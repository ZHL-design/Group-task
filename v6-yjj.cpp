#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_UNDO 50
#define INIT_CAPACITY 10
#define LINE_BUFFER_SIZE 1024
#define MAX_WORDS 10000
#define MAX_HIGHLIGHT_KEYWORD 20

// ===================== 数据结构定义区 =====================
// V1 动态文本行数组
typedef struct {
    char** lines;
    int line_count;
    int capacity;
} Text;

// V2 撤销栈：单条删除记录
typedef struct {
    int line_index;
    char* content;
} UndoRecord;

// V2 数组实现栈
typedef struct {
    UndoRecord records[MAX_UNDO];
    int top;
} UndoStack;

// V4 词频结构体
typedef struct {
    char word[64];
    int count;
} WordFreq;

// V5 方案1：数组存储关键词
typedef struct {
    char* keywords[MAX_HIGHLIGHT_KEYWORD];
    int keyword_cnt;
} KeyArr;

// V5 方案2：单向链表存储关键词（数据结构考点）
typedef struct KeyNode {
    char* keyword;
    struct KeyNode* next;
} KeyNode;

// 切换宏：打开使用链表，注释使用数组
#define USE_LINKED_LIST_KEYWORD

// ===================== V1 动态文本操作函数 =====================
Text* create_text() {
    Text* new_text = (Text*)malloc(sizeof(Text));
    if (new_text == NULL) {
        perror("create_text malloc failed");
        return NULL;
    }
    new_text->line_count = 0;
    new_text->capacity = INIT_CAPACITY;
    new_text->lines = (char**)calloc(new_text->capacity, sizeof(char*));
    if (new_text->lines == NULL) {
        free(new_text);
        perror("lines calloc failed");
        return NULL;
    }
    return new_text;
}

void load_file(Text* text, const char* filename) {
    if (text == NULL) return;
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        printf("文件打开失败：%s\n", filename);
        return;
    }
    char buf[LINE_BUFFER_SIZE];
    while (fgets(buf, LINE_BUFFER_SIZE, fp)) {
        // 动态扩容
        if (text->line_count >= text->capacity) {
            int new_cap = text->capacity * 2;
            char** new_lines = (char**)realloc(text->lines, sizeof(char*) * new_cap);
            if (new_lines == NULL) {
                perror("realloc lines failed");
                break;
            }
            text->lines = new_lines;
            text->capacity = new_cap;
        }
        // 去除换行符
        size_t len = strlen(buf);
        if (len && buf[len - 1] == '\n') buf[--len] = '\0';
        if (len && buf[len - 1] == '\r') buf[--len] = '\0';
        // 拷贝行
        char* line = strdup(buf);
        text->lines[text->line_count++] = line;
    }
    fclose(fp);
    printf("加载完成，共 %d 行\n", text->line_count);
}

void display_text(const Text* text) {
    if (text == NULL || text->line_count == 0) {
        printf("文本为空\n");
        return;
    }
    printf("\n===== 文本内容 =====\n");
    for (int i = 0; i < text->line_count; i++) {
        printf("%3d | %s\n", i + 1, text->lines[i]);
    }
    printf("====================\n");
}

void free_text(Text* text) {
    if (text == NULL) return;
    for (int i = 0; i < text->line_count; i++) {
        free(text->lines[i]);
    }
    free(text->lines);
    free(text);
}

// ===================== V2 撤销栈（数组栈实现） =====================
void init_undo_stack(UndoStack* s) {
    s->top = -1;
    memset(s->records, 0, sizeof(s->records));
}

void free_undo_stack(UndoStack* s) {
    for (int i = 0; i <= s->top; i++) {
        free(s->records[i].content);
    }
}

int save_state(Text* t, UndoStack* s, int idx) {
    if (s->top >= MAX_UNDO - 1) {
        printf("撤销栈已满，无法保存操作\n");
        return 0;
    }
    s->top++;
    s->records[s->top].line_index = idx;
    s->records[s->top].content = strdup(t->lines[idx]);
    if (s->records[s->top].content == NULL) {
        s->top--;
        perror("strdup failed");
        return 0;
    }
    return 1;
}

int delete_line(Text* t, int line_no, UndoStack* s) {
    int idx = line_no - 1;
    if (idx < 0 || idx >= t->line_count) {
        printf("行号超出范围 1~%d\n", t->line_count);
        return 0;
    }
    if (!save_state(t, s, idx)) return 0;
    free(t->lines[idx]);
    // 数组前移覆盖
    for (int i = idx; i < t->line_count - 1; i++) {
        t->lines[i] = t->lines[i + 1];
    }
    t->line_count--;
    printf("已删除第 %d 行\n", line_no);
    return 1;
}

int undo_delete(Text* t, UndoStack* s) {
    if (s->top < 0) {
        printf("无撤销记录\n");
        return 0;
    }
    UndoRecord rec = s->records[s->top];
    int restore_idx = rec.line_index;
    // 扩容
    if (t->line_count >= t->capacity) {
        int new_cap = t->capacity * 2;
        char** new_lines = (char**)realloc(t->lines, sizeof(char*) * new_cap);
        if (new_lines == NULL) {
            perror("undo realloc failed");
            return 0;
        }
        t->lines = new_lines;
        t->capacity = new_cap;
    }
    // 后移腾出插入位置
    for (int i = t->line_count; i > restore_idx; i--) {
        t->lines[i] = t->lines[i - 1];
    }
    t->lines[restore_idx] = strdup(rec.content);
    t->line_count++;
    free(rec.content);
    s->top--;
    printf("撤销成功，恢复第 %d 行\n", restore_idx + 1);
    return 1;
}

// ===================== V3 检索与文本统计 =====================
void find_text(const Text* text, const char* key) {
    if (text == NULL || text->line_count == 0) return;
    int cnt = 0;
    printf("\n===== 搜索【%s】结果 =====\n", key);
    for (int i = 0; i < text->line_count; i++) {
        if (strstr(text->lines[i], key)) {
            printf("行%3d: %s\n", i + 1, text->lines[i]);
            cnt++;
        }
    }
    printf("匹配共 %d 处\n", cnt);
}

long count_total_chars(const Text* text) {
    long sum = 0;
    for (int i = 0; i < text->line_count; i++) {
        sum += strlen(text->lines[i]);
    }
    return sum;
}

int count_total_lines(const Text* text) {
    return text ? text->line_count : 0;
}

void display_stats(const Text* text) {
    if (text == NULL || text->line_count == 0) {
        printf("无文本数据\n");
        return;
    }
    printf("\n===== 文本统计 =====\n");
    printf("总行数：%d\n", count_total_lines(text));
    printf("总字符数：%ld\n", count_total_chars(text));
    printf("====================\n");
}

// ===================== V4 词频统计 + qsort排序 =====================
static int is_delimiter(char c) {
    return isspace((unsigned char)c) || ispunct((unsigned char)c);
}

static void str_to_lower(char* s) {
    for (; *s; s++) {
        *s = tolower((unsigned char)*s);
    }
}

int count_word_frequency(const Text* text, WordFreq freq[], int max) {
    int word_cnt = 0;
    for (int i = 0; i < text->line_count; i++) {
        char buf[LINE_BUFFER_SIZE];
        strncpy(buf, text->lines[i], LINE_BUFFER_SIZE - 1);
        buf[LINE_BUFFER_SIZE - 1] = '\0';
        char* p = buf;
        while (*p) {
            // 跳过分隔符
            while (*p && is_delimiter(*p)) p++;
            if (!*p) break;
            // 截取单词
            char word[64] = {0};
            int j = 0;
            while (*p && !is_delimiter(*p) && j < 63) {
                word[j++] = *p++;
            }
            str_to_lower(word);
            // 查重
            int exist = 0;
            for (int k = 0; k < word_cnt; k++) {
                if (strcmp(freq[k].word, word) == 0) {
                    freq[k].count++;
                    exist = 1;
                    break;
                }
            }
            if (!exist && word_cnt < max) {
                strcpy(freq[word_cnt].word, word);
                freq[word_cnt].count = 1;
                word_cnt++;
            }
        }
    }
    return word_cnt;
}

static int freq_cmp(const void* a, const void* b) {
    WordFreq* wa = (WordFreq*)a;
    WordFreq* wb = (WordFreq*)b;
    if (wa->count != wb->count)
        return wb->count - wa->count; // 降序
    return strcmp(wa->word, wb->word);
}

void sort_frequency(WordFreq freq[], int total) {
    qsort(freq, total, sizeof(WordFreq), freq_cmp);
}

void show_word_freq(WordFreq freq[], int total, int top) {
    if (total == 0) {
        printf("未统计到单词\n");
        return;
    }
    if (top <= 0 || top > total) top = total;
    printf("\n===== 高频词TOP%d =====\n", top);
    printf("%-16s | 次数\n", "单词");
    printf("------------------------\n");
    for (int i = 0; i < top; i++) {
        printf("%-16s | %d\n", freq[i].word, freq[i].count);
    }
}

// ===================== V5 关键词高亮（链表/数组双版本） =====================
#ifdef USE_LINKED_LIST_KEYWORD
KeyNode* create_key_node(const char* s) {
    KeyNode* n = (KeyNode*)malloc(sizeof(KeyNode));
    n->keyword = strdup(s);
    n->next = NULL;
    return n;
}

void add_key_link(KeyNode** head, const char* s) {
    KeyNode* new_node = create_key_node(s);
    if (*head == NULL) {
        *head = new_node;
        return;
    }
    KeyNode* p = *head;
    while (p->next) p = p->next;
    p->next = new_node;
}

void free_key_link(KeyNode* head) {
    KeyNode* p = head;
    while (p) {
        KeyNode* tmp = p;
        p = p->next;
        free(tmp->keyword);
        free(tmp);
    }
}

void highlight_keywords(const Text* text, KeyNode* head) {
    if (text->line_count == 0 || head == NULL) {
        printf("无文本或无高亮关键词\n");
        return;
    }
    printf("\033[0m\n===== 高亮文本(红色匹配) =====\n");
    for (int i = 0; i < text->line_count; i++) {
        char buf[LINE_BUFFER_SIZE];
        strcpy(buf, text->lines[i]);
        char* cur = buf;
        printf("%3d | ", i + 1);
        while (*cur) {
            int hit = 0;
            KeyNode* pkey = head;
            while (pkey) {
                int len = strlen(pkey->keyword);
                if (strncmp(cur, pkey->keyword, len) == 0) {
                    printf("\033[31m%s\033[0m", pkey->keyword);
                    cur += len;
                    hit = 1;
                    break;
                }
                pkey = pkey->next;
            }
            if (!hit) putchar(*cur++);
        }
        putchar('\n');
    }
    printf("==============================\033[0m\n");
}

#else
void init_key_arr(KeyArr* arr) {
    arr->keyword_cnt = 0;
    memset(arr->keywords, 0, sizeof(arr->keywords));
}

int add_key_arr(KeyArr* arr, const char* s) {
    if (arr->keyword_cnt >= MAX_HIGHLIGHT_KEYWORD) return 0;
    arr->keywords[arr->keyword_cnt++] = strdup(s);
    return 1;
}

void free_key_arr(KeyArr* arr) {
    for (int i = 0; i < arr->keyword_cnt; i++) {
        free(arr->keywords[i]);
    }
    arr->keyword_cnt = 0;
}

void highlight_keywords(const Text* text, KeyArr* arr) {
    if (text->line_count == 0 || arr->keyword_cnt == 0) {
        printf("无文本或无高亮关键词\n");
        return;
    }
    printf("\033[0m\n===== 高亮文本(红色匹配) =====\n");
    for (int i = 0; i < text->line_count; i++) {
        char buf[LINE_BUFFER_SIZE];
        strcpy(buf, text->lines[i]);
        char* cur = buf;
        printf("%3d | ", i + 1);
        while (*cur) {
            int hit = 0;
            for (int k = 0; k < arr->keyword_cnt; k++) {
                int len = strlen(arr->keywords[k]);
                if (strncmp(cur, arr->keywords[k], len) == 0) {
                    printf("\033[31m%s\033[0m", arr->keywords[k]);
                    cur += len;
                    hit = 1;
                    break;
                }
            }
            if (!hit) putchar(*cur++);
        }
        putchar('\n');
    }
    printf("==============================\033[0m\n");
}
#endif

// ===================== V6 核心：独立交互式菜单函数 =====================
void interactive_menu(Text* text, UndoStack* undo_stk
#ifdef USE_LINKED_LIST_KEYWORD
, KeyNode** key_head
#else
, KeyArr* key_arr
#endif
) {
    char cmd[32];
    while (1) {
        printf("\n========== 纯文本编辑器 V6 主菜单 ==========\n");
        printf("1. show    显示全部文本\n");
        printf("2. del n   删除第n行\n");
        printf("3. undo    撤销删除操作\n");
        printf("4. find s  搜索字符串s\n");
        printf("5. stat    文本行数/字符统计\n");
        printf("6. freq [n] 词频统计(可选输出前n个)\n");
        printf("7. addkey s 添加高亮关键词s\n");
        printf("8. highlight 渲染带关键词高亮的文本\n");
        printf("9. clearkey 清空所有高亮关键词\n");
        printf("0. exit    退出程序\n");
        printf("=============================================\n");
        printf("请输入命令：");
        scanf("%s", cmd);

        if (strcmp(cmd, "show") == 0) {
            display_text(text);
        }
        else if (strcmp(cmd, "del") == 0) {
            int n;
            if (scanf("%d", &n) == 1) delete_line(text, n, undo_stk);
            else printf("行号格式错误\n");
        }
        else if (strcmp(cmd, "undo") == 0) {
            undo_delete(text, undo_stk);
        }
        else if (strcmp(cmd, "find") == 0) {
            char s[LINE_BUFFER_SIZE];
            scanf(" %[^\n]", s);
            find_text(text, s);
        }
        else if (strcmp(cmd, "stat") == 0) {
            display_stats(text);
        }
        else if (strcmp(cmd, "freq") == 0) {
            int top = 0;
            scanf("%d", &top);
            WordFreq freq[MAX_WORDS] = {0};
            int total = count_word_frequency(text, freq, MAX_WORDS);
            sort_frequency(freq, total);
            show_word_freq(freq, total, top);
        }
        else if (strcmp(cmd, "addkey") == 0) {
            char s[LINE_BUFFER_SIZE];
            scanf(" %[^\n]", s);
#ifdef USE_LINKED_LIST_KEYWORD
            add_key_link(key_head, s);
#else
            add_key_arr(key_arr, s);
#endif
            printf("已添加关键词：%s\n", s);
        }
        else if (strcmp(cmd, "highlight") == 0) {
#ifdef USE_LINKED_LIST_KEYWORD
            highlight_keywords(text, *key_head);
#else
            highlight_keywords(text, key_arr);
#endif
        }
        else if (strcmp(cmd, "clearkey") == 0) {
#ifdef USE_LINKED_LIST_KEYWORD
            free_key_link(*key_head);
            *key_head = NULL;
#else
            free_key_arr(key_arr);
            init_key_arr(key_arr);
#endif
            printf("已清空所有关键词\n");
        }
        else if (strcmp(cmd, "exit") == 0) {
            printf("即将退出编辑器...\n");
            break;
        }
        else {
            printf("无效命令，请重新输入\n");
        }
        // 清空输入缓冲区
        while (getchar() != '\n');
    }
}

// ===================== 程序入口main(V6整合所有模块) =====================
int main() {
    // 1. 初始化文本容器
    Text* editor_text = create_text();
    if (editor_text == NULL) return EXIT_FAILURE;

    // 2. 初始化撤销栈
    UndoStack undo_stack;
    init_undo_stack(&undo_stack);

    // 3. 初始化关键词存储结构
#ifdef USE_LINKED_LIST_KEYWORD
    KeyNode* keyword_list = NULL;
#else
    KeyArr keyword_arr;
    init_key_arr(&keyword_arr);
#endif

    // 4. 加载文件
    char file_name[128];
    printf("请输入要打开的文件名(默认input.txt)：");
    fgets(file_name, sizeof(file_name), stdin);
    file_name[strcspn(file_name, "\n")] = '\0';
    if (strlen(file_name) == 0) strcpy(file_name, "input.txt");
    load_file(editor_text, file_name);

    // 5. V6核心：调用独立菜单交互函数
#ifdef USE_LINKED_LIST_KEYWORD
    interactive_menu(editor_text, &undo_stack, &keyword_list);
#else
    interactive_menu(editor_text, &undo_stack, &keyword_arr);
#endif

    // 6. 统一释放全部内存，杜绝泄漏
#ifdef USE_LINKED_LIST_KEYWORD
    free_key_link(keyword_list);
#else
    free_key_arr(&keyword_arr);
#endif
    free_undo_stack(&undo_stack);
    free_text(editor_text);

    printf("内存全部释放，程序正常结束\n");
    return EXIT_SUCCESS;
}
