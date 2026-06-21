#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_UNDO 50
#define INIT_CAPACITY 10
#define LINE_BUFFER_SIZE 1024
#define MAX_WORDS 10000  // 最大统计单词数
#define MAX_HIGHLIGHT_KEYWORD 20 // 数组版关键词最大数量

// 文本结构体：管理文件内容的动态数组(V1)
typedef struct {
    char** lines;      // 每一行字符串的指针数组
    int line_count;    // 当前总行数
    int capacity;      // 数组容量（动态扩容用）
} Text;

// 撤销记录结构体：保存被删除行的状态(V2)
typedef struct {
    int line_index;     // 被删除行在原文本中的索引
    char* content;      // 被删除行的完整内容
} UndoRecord;

// 撤销栈结构体：用数组实现栈，保存所有可撤销的删除操作(V2栈结构)
typedef struct {
    UndoRecord records[MAX_UNDO];
    int top;            // 栈顶指针，-1 表示栈空
} UndoStack;

// V4词频统计结构体
typedef struct {
    char word[64];   // 单词本身（最多63个字符）
    int count;       // 出现次数
} WordFreq;

// ========== V5新增：两种关键词存储结构，二选一使用 ==========
// 方案1：指针数组（简单版，无需链表，适合快速实现）
typedef struct {
    char* keywords[MAX_HIGHLIGHT_KEYWORD];
    int keyword_cnt;
} KeyArr;

// 方案2：单向链表（数据结构考点版，练链表增删遍历）
typedef struct KeyNode {
    char* keyword;
    struct KeyNode* next;
} KeyNode;

// 全局切换宏：注释掉就是数组版，打开就是链表版
#define USE_LINKED_LIST_KEYWORD

// ---------------------- V1基础工具函数 ----------------------
Text* create_text() {
    Text* new_text = (Text*)malloc(sizeof(Text));
    if (new_text == NULL) {
        printf("内存分配失败！\n");
        return NULL;
    }
    
    new_text->line_count = 0;
    new_text->capacity = INIT_CAPACITY;
    new_text->lines = (char**)malloc(sizeof(char*) * new_text->capacity);
    
    if (new_text->lines == NULL) {
        free(new_text);
        printf("内存分配失败！\n");
        return NULL;
    }
    
    for (int i = 0; i < new_text->capacity; i++) {
        new_text->lines[i] = NULL;
    }
    return new_text;
}

void load_file(Text* text, const char* filename) {
    if (text == NULL) {
        printf("Text对象为空！\n");
        return;
    }
    
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("无法打开文件：%s\n", filename);
        return;
    }
    
    char buffer[LINE_BUFFER_SIZE];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        if (text->line_count >= text->capacity) {
            int new_capacity = text->capacity * 2;
            char** new_lines = (char**)realloc(text->lines, sizeof(char*) * new_capacity);
            if (new_lines == NULL) {
                printf("内存扩容失败！\n");
                break;
            }
            text->lines = new_lines;
            text->capacity = new_capacity;
        }
        
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
            len--;
        }
        if (len > 0 && buffer[len-1] == '\r') {
            buffer[len-1] = '\0';
        }
        
        char* line_copy = (char*)malloc(strlen(buffer) + 1);
        if (line_copy == NULL) {
            printf("行内存分配失败！\n");
            break;
        }
        strcpy(line_copy, buffer);
        text->lines[text->line_count] = line_copy;
        text->line_count++;
    }
    fclose(file);
    printf("文件加载成功，共 %d 行。\n", text->line_count);
}

void display_text(const Text* text) {
    if (text == NULL || text->line_count == 0) {
        printf("文本为空！\n");
        return;
    }
    printf("=== 文本内容 [%d行] ===\n", text->line_count);
    for (int i = 0; i < text->line_count; i++) {
        printf("%3d: %s\n", i+1, text->lines[i]);
    }
    printf("=== 结束 ===\n");
}

void free_text(Text* text) {
    if (text == NULL) return;
    for (int i = 0; i < text->line_count; i++) {
        if (text->lines[i] != NULL) free(text->lines[i]);
    }
    free(text->lines);
    free(text);
    printf("文本内存已释放。\n");
}

// ---------------------- V2撤销栈函数 ----------------------
void init_undo_stack(UndoStack* s) {
    s->top = -1;
    for (int i = 0; i < MAX_UNDO; i++) {
        s->records[i].content = NULL;
    }
}

void free_undo_stack(UndoStack* s) {
    for (int i = 0; i <= s->top; i++) {
        if (s->records[i].content) free(s->records[i].content);
    }
}

int save_state(Text* t, UndoStack* s, int line_no) {
    if (s->top >= MAX_UNDO - 1) {
        printf("撤销栈已满，无法保存更多操作！\n");
        return 0;
    }
    s->top++;
    s->records[s->top].line_index = line_no;
    s->records[s->top].content = (char*)malloc(strlen(t->lines[line_no]) + 1);
    if (!s->records[s->top].content) {
        printf("内存分配失败，无法保存撤销状态！\n");
        s->top--;
        return 0;
    }
    strcpy(s->records[s->top].content, t->lines[line_no]);
    return 1;
}

int delete_line(Text* t, int line_no, UndoStack* s) {
    int idx = line_no - 1;
    if (idx < 0 || idx >= t->line_count) {
        printf("行号无效！有效范围：1~%d\n", t->line_count);
        return 0;
    }
    if (!save_state(t, s, idx)) return 0;
    free(t->lines[idx]);
    for (int i = idx; i < t->line_count - 1; i++) {
        t->lines[i] = t->lines[i+1];
    }
    t->line_count--;
    printf("已删除第 %d 行\n", line_no);
    return 1;
}

int undo(Text* t, UndoStack* s) {
    if (s->top < 0) {
        printf("没有可撤销的操作！\n");
        return 0;
    }
    UndoRecord* last = &s->records[s->top];
    int restore_idx = last->line_index;
    if (restore_idx < 0 || restore_idx > t->line_count) {
        printf("撤销失败：保存的行号已无效\n");
        free(last->content);
        s->top--;
        return 0;
    }
    if (t->line_count + 1 > t->capacity) {
        int new_cap = t->capacity * 2;
        char** new_lines = (char**)realloc(t->lines, sizeof(char*) * new_cap);
        if (!new_lines) {
            printf("内存不足，撤销失败！\n");
            return 0;
        }
        t->lines = new_lines;
        t->capacity = new_cap;
    }
    for (int i = t->line_count; i > restore_idx; i--) {
        t->lines[i] = t->lines[i-1];
    }
    t->lines[restore_idx] = (char*)malloc(strlen(last->content) + 1);
    if (!t->lines[restore_idx]) {
        printf("内存分配失败，撤销失败！\n");
        return 0;
    }
    strcpy(t->lines[restore_idx], last->content);
    t->line_count++;
    free(last->content);
    s->top--;
    printf("撤销成功，已恢复第 %d 行\n", restore_idx + 1);
    return 1;
}

// ---------------------- V3搜索&统计函数 ----------------------
void find_text(const Text* text, const char* keyword) {
    if (text == NULL || text->line_count == 0) {
        printf("文本为空，无法搜索！\n");
        return;
    }
    if (keyword == NULL || strlen(keyword) == 0) {
        printf("关键词不能为空！\n");
        return;
    }
    int found_count = 0;
    printf("=== 搜索结果（关键词：%s） ===\n", keyword);
    for (int i = 0; i < text->line_count; i++) {
        if (strstr(text->lines[i], keyword) != NULL) {
            printf("行号 %3d: %s\n", i+1, text->lines[i]);
            found_count++;
        }
    }
    printf("共找到 %d 处匹配\n", found_count);
}

long count_total_chars(const Text* text) {
    if (text == NULL || text->line_count == 0) return 0;
    long total = 0;
    for (int i = 0; i < text->line_count; i++) total += strlen(text->lines[i]);
    return total;
}

int count_total_lines(const Text* text) {
    if (text == NULL) return 0;
    return text->line_count;
}

void display_stats(const Text* text) {
    if (text == NULL || text->line_count == 0) {
        printf("文本为空，无统计信息！\n");
        return;
    }
    int lines = count_total_lines(text);
    long chars = count_total_chars(text);
    printf("=== 文本统计信息 ===\n");
    printf("总行数：%d\n", lines);
    printf("总字符数（不含换行）：%ld\n", chars);
    printf("====================\n");
}

// ---------------------- V4词频统计+qsort排序 ----------------------
static int is_delimiter(char c) {
    return isspace(c) || ispunct(c);
}
static void to_lower_str(char* str) {
    for (; *str; str++) *str = tolower((unsigned char)*str);
}

int count_word_frequency(const Text* text, WordFreq freq[], int max_words) {
    if (text == NULL || text->line_count == 0) return 0;
    int word_count = 0;
    for (int i = 0; i < text->line_count; i++) {
        char line[LINE_BUFFER_SIZE];
        strncpy(line, text->lines[i], sizeof(line) - 1);
        line[sizeof(line) - 1] = '\0';
        char* ptr = line;
        while (*ptr) {
            while (*ptr && is_delimiter(*ptr)) ptr++;
            if (!*ptr) break;
            char word_buf[64];
            int j = 0;
            while (*ptr && !is_delimiter(*ptr) && j < sizeof(word_buf) - 1) {
                word_buf[j++] = *ptr++;
            }
            word_buf[j] = '\0';
            to_lower_str(word_buf);
            int found = 0;
            for (int k = 0; k < word_count; k++) {
                if (strcmp(freq[k].word, word_buf) == 0) {
                    freq[k].count++; found = 1; break;
                }
            }
            if (!found && word_count < max_words) {
                strcpy(freq[word_count].word, word_buf);
                freq[word_count].count = 1;
                word_count++;
            }
        }
    }
    return word_count;
}

static int compare_freq(const void* a, const void* b) {
    const WordFreq* wa = (const WordFreq*)a;
    const WordFreq* wb = (const WordFreq*)b;
    if (wa->count != wb->count) return wb->count - wa->count;
    else return strcmp(wa->word, wb->word);
}

void sort_frequency(WordFreq freq[], int count) {
    if (count <= 0) return;
    qsort(freq, count, sizeof(WordFreq), compare_freq);
}

void display_word_freq(const WordFreq freq[], int count, int top_n) {
    if (count == 0) {
        printf("没有统计到任何单词！\n");
        return;
    }
    if (top_n <= 0 || top_n > count) top_n = count;
    printf("=== 词频统计（前%d个高频词） ===\n", top_n);
    printf("  单词           出现次数\n");
    printf("----------------------------\n");
    for (int i = 0; i < top_n; i++) {
        printf("%-15s %d\n", freq[i].word, freq[i].count);
    }
    printf("============================\n");
}

// ========== V5核心新增：多关键词高亮全套函数 ==========
#ifdef USE_LINKED_LIST_KEYWORD
// 链表版：新增关键词节点
KeyNode* create_key_node(const char* s) {
    KeyNode* node = (KeyNode*)malloc(sizeof(KeyNode));
    node->keyword = malloc(strlen(s)+1);
    strcpy(node->keyword, s);
    node->next = NULL;
    return node;
}
// 链表尾部插入关键词
void add_keyword_link(KeyNode** head, const char* s) {
    KeyNode* new_node = create_key_node(s);
    if(*head == NULL) {*head = new_node; return;}
    KeyNode* p = *head;
    while(p->next) p = p->next;
    p->next = new_node;
}
// 释放关键词链表
void free_key_link(KeyNode* head) {
    KeyNode* p = head;
    while(p) {
        KeyNode* tmp = p;
        p = p->next;
        free(tmp->keyword);
        free(tmp);
    }
}
// 高亮主函数：遍历链表关键词，ANSI红色标记匹配内容
void highlight_keywords(const Text* text, KeyNode* key_head) {
    if(text->line_count ==0 || key_head == NULL) {
        printf("无文本/无高亮关键词\n"); return;
    }
    printf("\033[0m=== 关键词高亮文本(匹配内容红色) ===\n");
    for(int i=0; i<text->line_count; i++) {
        char line_buf[LINE_BUFFER_SIZE];
        strcpy(line_buf, text->lines[i]);
        char* cur = line_buf;
        printf("%3d: ",i+1);
        while(*cur != '\0') {
            int match_flag = 0;
            KeyNode* pkey = key_head;
            // 逐个关键词匹配
            while(pkey) {
                int keylen = strlen(pkey->keyword);
                if(strncmp(cur, pkey->keyword, keylen)==0) {
                    // ANSI转义码：红色高亮
                    printf("\033[31m%s\033[0m", pkey->keyword);
                    cur += keylen;
                    match_flag =1;
                    break;
                }
                pkey = pkey->next;
            }
            if(!match_flag) putchar(*cur++);
        }
        putchar('\n');
    }
    printf("==================================\033[0m\n");
}

#else
// 数组指针版：关键词数组初始化、新增、释放
void init_key_arr(KeyArr* ka) { ka->keyword_cnt =0; }
int add_keyword_arr(KeyArr* ka, const char* s) {
    if(ka->keyword_cnt >= MAX_HIGHLIGHT_KEYWORD) return 0;
    ka->keywords[ka->keyword_cnt] = malloc(strlen(s)+1);
    strcpy(ka->keywords[ka->keyword_cnt], s);
    ka->keyword_cnt++;
    return 1;
}
void free_key_arr(KeyArr* ka) {
    for(int i=0; i<ka->keyword_cnt; i++) free(ka->keywords[i]);
    ka->keyword_cnt=0;
}
// 高亮主函数：遍历指针数组关键词
void highlight_keywords(const Text* text, KeyArr* ka) {
    if(text->line_count ==0 || ka->keyword_cnt ==0) {
        printf("无文本/无高亮关键词\n"); return;
    }
    printf("\033[0m=== 关键词高亮文本(匹配内容红色) ===\n");
    for(int i=0; i<text->line_count; i++) {
        char line_buf[LINE_BUFFER_SIZE];
        strcpy(line_buf, text->lines[i]);
        char* cur = line_buf;
        printf("%3d: ",i+1);
        while(*cur != '\0') {
            int match_flag = 0;
            for(int k=0;k<ka->keyword_cnt;k++) {
                int keylen = strlen(ka->keywords[k]);
                if(strncmp(cur, ka->keywords[k], keylen)==0) {
                    printf("\033[31m%s\033[0m", ka->keywords[k]);
                    cur += keylen;
                    match_flag =1; break;
                }
            }
            if(!match_flag) putchar(*cur++);
        }
        putchar('\n');
    }
    printf("==================================\033[0m\n");
}
#endif

// ---------------------- V5主交互菜单，兼容全部功能 ----------------------
int main() {
    Text* my_text = create_text();
    if (my_text == NULL) return 1;
    UndoStack undo_stack;
    init_undo_stack(&undo_stack);

    // 初始化关键词存储结构
    #ifdef USE_LINKED_LIST_KEYWORD
    KeyNode* key_head = NULL;
    #else
    KeyArr key_arr;
    init_key_arr(&key_arr);
    #endif

    char filename[100];
    printf("请输入文件名（默认 input.txt）: ");
    fgets(filename, sizeof(filename), stdin);
    filename[strcspn(filename, "\n")] = '\0';
    if (strlen(filename) == 0) strcpy(filename, "input.txt");
    load_file(my_text, filename);
    display_text(my_text);

    char cmd[10];
    int line_no;
    char keyword[LINE_BUFFER_SIZE];
    while (1) {
        printf("\n--- V5完整功能菜单 ---\n");
        printf("d <行号> : 删除指定行\n");
        printf("u       : 撤销上一次删除\n");
        printf("s       : 显示当前文本\n");
        printf("f <关键词> : 搜索文本\n");
        printf("t       : 文本字符行数统计\n");
        printf("w [n]   : 词频排序统计\n");
        printf("a <关键词>: 添加高亮关键词\n");
        printf("h       : 渲染高亮文本\n");
        printf("c       : 清空所有高亮关键词\n");
        printf("q       : 退出程序\n");
        printf("请输入命令: ");
        
        scanf("%s", cmd);
        if (strcmp(cmd, "d") == 0) {
            if (scanf("%d", &line_no) != 1) {
                printf("请输入有效的数字行号！\n");
                while (getchar() != '\n'); continue;
            }
            delete_line(my_text, line_no, &undo_stack);
            display_text(my_text);
        }
        else if (strcmp(cmd, "u") == 0) {
            undo(my_text, &undo_stack); display_text(my_text);
        }
        else if (strcmp(cmd, "s") == 0) display_text(my_text);
        else if (strcmp(cmd, "f") == 0) {
            scanf(" %[^\n]", keyword); find_text(my_text, keyword);
        }
        else if (strcmp(cmd, "t") == 0) display_stats(my_text);
        else if (strcmp(cmd, "w") == 0) {
            int top_n = 0;
            if (scanf("%d", &top_n) != 1) top_n = 0;
            WordFreq freq[MAX_WORDS] = {0};
            int total_words = count_word_frequency(my_text, freq, MAX_WORDS);
            sort_frequency(freq, total_words);
            display_word_freq(freq, total_words, top_n);
        }
        // V5新增交互命令
        else if (strcmp(cmd, "a") == 0) {
            scanf(" %[^\n]", keyword);
            #ifdef USE_LINKED_LIST_KEYWORD
            add_keyword_link(&key_head, keyword);
            #else
            add_keyword_arr(&key_arr, keyword);
            #endif
            printf("已添加高亮关键词: %s\n", keyword);
        }
        else if (strcmp(cmd, "h") == 0) {
            #ifdef USE_LINKED_LIST_KEYWORD
            highlight_keywords(my_text, key_head);
            #else
            highlight_keywords(my_text, &key_arr);
            #endif
        }
        else if (strcmp(cmd, "c") == 0) {
            #ifdef USE_LINKED_LIST_KEYWORD
            free_key_link(key_head); key_head = NULL;
            #else
            free_key_arr(&key_arr); init_key_arr(&key_arr);
            #endif
            printf("已清空全部高亮关键词\n");
        }
        else if (strcmp(cmd, "q") == 0) {
            printf("退出程序...\n"); break;
        }
        else printf("未知命令！\n");
        while (getchar() != '\n');
    }

    // 全部内存销毁
    #ifdef USE_LINKED_LIST_KEYWORD
    free_key_link(key_head);
    #else
    free_key_arr(&key_arr);
    #endif
    free_undo_stack(&undo_stack);
    free_text(my_text);
    printf("程序结束，所有内存已释放。\n");
    return 0;
}
  
