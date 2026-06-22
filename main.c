#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_UNDO 50
#define INIT_CAPACITY 10
#define MAX_WORD_LEN 256

// ==================== V1: 文本结构体 ====================
typedef struct {
    char** lines;
    int line_count;
    int capacity;
} Text;

// ==================== V2: 撤销栈（数组实现） ====================
typedef struct {
    int line_index;
    char* content;
} UndoRecord;

typedef struct {
    UndoRecord records[MAX_UNDO];
    int top;
} UndoStack;

// ==================== V4: Trie 树节点（词频统计） ====================
typedef struct TrieNode {
    struct TrieNode* children[26];
    int freq;
} TrieNode;

// ==================== V4: 词频统计结果结构 ====================
typedef struct {
    char word[MAX_WORD_LEN];
    int freq;
} WordFreq;

// ==================== V5: 关键词链表 ====================
typedef struct KeywordNode {
    char* word;
    struct KeywordNode* next;
} KeywordNode;

typedef struct {
    KeywordNode* head;
    int count;
} KeywordList;

// -------------------- ANSI 颜色宏 --------------------
#define COLOR_RED   "\033[31m"
#define COLOR_RESET "\033[0m"

// ==================== V1 函数实现 ====================
Text* create_text() {
    Text* new_text = (Text*)malloc(sizeof(Text));
    if (!new_text) return NULL;
    new_text->line_count = 0;
    new_text->capacity = INIT_CAPACITY;
    new_text->lines = (char**)malloc(sizeof(char*) * new_text->capacity);
    if (!new_text->lines) {
        free(new_text);
        return NULL;
    }
    for (int i = 0; i < new_text->capacity; i++) new_text->lines[i] = NULL;
    return new_text;
}

void load_file(Text* text, const char* filename) {
    if (!text) return;
    FILE* file = fopen(filename, "r");
    if (!file) {
        printf("无法打开文件：%s\n", filename);
        return;
    }
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        if (text->line_count >= text->capacity) {
            int new_cap = text->capacity * 2;
            char** new_lines = (char**)realloc(text->lines, sizeof(char*) * new_cap);
            if (!new_lines) break;
            text->lines = new_lines;
            text->capacity = new_cap;
        }
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';
        char* copy = (char*)malloc(strlen(buffer) + 1);
        if (!copy) break;
        strcpy(copy, buffer);
        text->lines[text->line_count++] = copy;
    }
    fclose(file);
    printf("文件加载成功，共 %d 行。\n", text->line_count);
}

void display_text(const Text* text) {
    if (!text || text->line_count == 0) {
        printf("文本为空！\n");
        return;
    }
    printf("=== 文本内容 [%d行] ===\n", text->line_count);
    for (int i = 0; i < text->line_count; i++)
        printf("%3d: %s\n", i+1, text->lines[i]);
    printf("=== 结束 ===\n");
}

void free_text(Text* text) {
    if (!text) return;
    for (int i = 0; i < text->line_count; i++) free(text->lines[i]);
    free(text->lines);
    free(text);
}

// ==================== V2 函数实现 ====================
void init_undo_stack(UndoStack* s) {
    s->top = -1;
    for (int i = 0; i < MAX_UNDO; i++) s->records[i].content = NULL;
}

void free_undo_stack(UndoStack* s) {
    for (int i = 0; i <= s->top; i++)
        if (s->records[i].content) free(s->records[i].content);
}

int save_state(Text* t, UndoStack* s, int line_no) {
    if (s->top >= MAX_UNDO - 1) return 0;
    s->top++;
    s->records[s->top].line_index = line_no;
    s->records[s->top].content = (char*)malloc(strlen(t->lines[line_no]) + 1);
    if (!s->records[s->top].content) {
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
    for (int i = idx; i < t->line_count - 1; i++)
        t->lines[i] = t->lines[i+1];
    t->line_count--;
    printf("已删除第 %d 行\n", line_no);
    return 1;
}

int undo(Text* t, UndoStack* s) {
    if (s->top < 0) {
        printf("没有可撤销的操作\n");
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
        if (!new_lines) return 0;
        t->lines = new_lines;
        t->capacity = new_cap;
    }
    for (int i = t->line_count; i > restore_idx; i--)
        t->lines[i] = t->lines[i-1];
    t->lines[restore_idx] = (char*)malloc(strlen(last->content) + 1);
    if (!t->lines[restore_idx]) return 0;
    strcpy(t->lines[restore_idx], last->content);
    t->line_count++;
    free(last->content);
    s->top--;
    printf("撤销成功，已恢复第 %d 行\n", restore_idx + 1);
    return 1;
}

// ==================== V3 函数实现 ====================
void find_text(const Text* text, const char* pattern) {
    if (!text || text->line_count == 0) {
        printf("文本为空，无法搜索。\n");
        return;
    }
    int found = 0;
    printf("\n=== 搜索 \"%s\" 的结果 ===\n", pattern);
    for (int i = 0; i < text->line_count; i++) {
        if (strstr(text->lines[i], pattern)) {
            printf("第 %d 行: %s\n", i+1, text->lines[i]);
            found++;
        }
    }
    if (!found) printf("未找到包含 \"%s\" 的行。\n", pattern);
    printf("========================\n");
}

int count_total_chars(const Text* text) {
    if (!text) return 0;
    int total = 0;
    for (int i = 0; i < text->line_count; i++)
        total += strlen(text->lines[i]);
    return total;
}

int count_total_lines(const Text* text) {
    return text ? text->line_count : 0;
}

void show_statistics(const Text* text) {
    int lines = count_total_lines(text);
    int chars = count_total_chars(text);
    printf("\n========== 统计信息 ==========\n");
    printf("总行数: %d\n", lines);
    printf("总字符数: %d (不含换行符)\n", chars);
    if (lines > 0)
        printf("平均每行字符数: %.1f\n", (double)chars / lines);
    printf("==============================\n");
}

// ==================== V4 辅助函数 ====================
void to_lowercase(char* str) {
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

char** extract_words(const char* line, int* word_count) {
    if (!line || !*line) {
        *word_count = 0;
        return NULL;
    }
    char* temp = (char*)malloc(strlen(line) + 1);
    strcpy(temp, line);
    for (int i = 0; temp[i]; i++)
        if (ispunct((unsigned char)temp[i])) temp[i] = ' ';
    
    char** words = NULL;
    int capacity = 0;
    *word_count = 0;
    char* token = strtok(temp, " \t");
    while (token) {
        if (strlen(token)) {
            to_lowercase(token);
            if (*word_count >= capacity) {
                capacity = capacity == 0 ? 10 : capacity * 2;
                char** new_words = (char**)realloc(words, sizeof(char*) * capacity);
                if (!new_words) {
                    for (int i = 0; i < *word_count; i++) free(words[i]);
                    free(words);
                    free(temp);
                    *word_count = 0;
                    return NULL;
                }
                words = new_words;
            }
            words[*word_count] = (char*)malloc(strlen(token) + 1);
            strcpy(words[*word_count], token);
            (*word_count)++;
        }
        token = strtok(NULL, " \t");
    }
    free(temp);
    return words;
}

TrieNode* create_trie_node() {
    TrieNode* node = (TrieNode*)malloc(sizeof(TrieNode));
    if (node) {
        node->freq = 0;
        for (int i = 0; i < 26; i++) node->children[i] = NULL;
    }
    return node;
}

void insert_word(TrieNode* root, const char* word) {
    TrieNode* curr = root;
    for (int i = 0; word[i]; i++) {
        int idx = word[i] - 'a';
        if (idx < 0 || idx >= 26) continue;
        if (!curr->children[idx])
            curr->children[idx] = create_trie_node();
        curr = curr->children[idx];
    }
    curr->freq++;
}

void collect_freq(TrieNode* node, char* prefix, int depth, WordFreq** list, int* count, int* capacity) {
    if (!node) return;
    if (node->freq > 0) {
        if (*count >= *capacity) {
            *capacity = (*capacity == 0) ? 64 : (*capacity * 2);
            *list = (WordFreq*)realloc(*list, sizeof(WordFreq) * (*capacity));
        }
        prefix[depth] = '\0';
        strcpy((*list)[*count].word, prefix);
        (*list)[*count].freq = node->freq;
        (*count)++;
    }
    for (int i = 0; i < 26; i++) {
        if (node->children[i]) {
            prefix[depth] = 'a' + i;
            collect_freq(node->children[i], prefix, depth + 1, list, count, capacity);
        }
    }
}

void free_trie(TrieNode* node) {
    if (!node) return;
    for (int i = 0; i < 26; i++) free_trie(node->children[i]);
    free(node);
}

int compare_word_freq(const void* a, const void* b) {
    const WordFreq* wa = (const WordFreq*)a;
    const WordFreq* wb = (const WordFreq*)b;
    if (wa->freq != wb->freq)
        return wb->freq - wa->freq;
    return strcmp(wa->word, wb->word);
}

void count_word_frequency(const Text* text, WordFreq** result, int* result_count) {
    if (!text || text->line_count == 0) {
        *result = NULL;
        *result_count = 0;
        return;
    }
    TrieNode* root = create_trie_node();
    for (int i = 0; i < text->line_count; i++) {
        int word_cnt = 0;
        char** words = extract_words(text->lines[i], &word_cnt);
        if (!words) continue;
        for (int j = 0; j < word_cnt; j++) {
            insert_word(root, words[j]);
            free(words[j]);
        }
        free(words);
    }
    WordFreq* freq_list = NULL;
    int capacity = 0, count = 0;
    char prefix[MAX_WORD_LEN];
    collect_freq(root, prefix, 0, &freq_list, &count, &capacity);
    free_trie(root);
    
    qsort(freq_list, count, sizeof(WordFreq), compare_word_freq);
    
    *result = freq_list;
    *result_count = count;
}

void display_word_frequency(const Text* text) {
    WordFreq* list = NULL;
    int count = 0;
    count_word_frequency(text, &list, &count);
    if (count == 0) {
        printf("文本中没有单词或文本为空。\n");
        return;
    }
    printf("\n========== 词频统计结果（基于Trie树） ==========\n");
    printf("共 %d 个不同的单词\n", count);
    printf("单词\t\t频率\n");
    printf("------------------------\n");
    for (int i = 0; i < count; i++)
        printf("%-20s %d\n", list[i].word, list[i].freq);
    printf("================================================\n");
    free(list);
}

// ==================== V5 函数实现（链表版） ====================
void init_keyword_list(KeywordList* kl) {
    kl->head = NULL;
    kl->count = 0;
}

void add_keyword(KeywordList* kl, const char* kw) {
    if (!kw || strlen(kw) == 0) return;
    // 去重
    KeywordNode* cur = kl->head;
    while (cur) {
        if (strcmp(cur->word, kw) == 0) return;
        cur = cur->next;
    }
    KeywordNode* new_node = (KeywordNode*)malloc(sizeof(KeywordNode));
    new_node->word = (char*)malloc(strlen(kw) + 1);
    strcpy(new_node->word, kw);
    new_node->next = kl->head;
    kl->head = new_node;
    kl->count++;
}

void free_keyword_list(KeywordList* kl) {
    KeywordNode* cur = kl->head;
    while (cur) {
        KeywordNode* tmp = cur;
        cur = cur->next;
        free(tmp->word);
        free(tmp);
    }
    kl->head = NULL;
    kl->count = 0;
}

void highlight_keywords(const Text* text, const KeywordList* kl) {
    if (!text || text->line_count == 0) {
        printf("文本为空！\n");
        return;
    }
    if (kl->head == NULL) {
        display_text(text);
        return;
    }

    printf("=== 多关键词高亮显示 [%d行] ===\n", text->line_count);
    for (int i = 0; i < text->line_count; i++) {
        const char* line = text->lines[i];
        printf("%3d: ", i + 1);
        
        int pos = 0;
        int len = strlen(line);
        while (pos < len) {
            int best_pos = -1;
            int best_len = 0;
            KeywordNode* cur = kl->head;
            while (cur) {
                const char* kw = cur->word;
                int kw_len = strlen(kw);
                if (kw_len == 0) { cur = cur->next; continue; }
                const char* found = strstr(line + pos, kw);
                if (found) {
                    int offset = (int)(found - (line + pos));
                    if (best_pos == -1 || offset < best_pos) {
                        best_pos = offset;
                        best_len = kw_len;
                    }
                }
                cur = cur->next;
            }
            if (best_pos == -1) {
                printf("%s", line + pos);
                break;
            } else {
                if (best_pos > 0) {
                    printf("%.*s", best_pos, line + pos);
                }
                printf("%s%.*s%s", COLOR_RED, best_len, line + pos + best_pos, COLOR_RESET);
                pos += best_pos + best_len;
            }
        }
        printf("\n");
    }
    printf("=== 结束 ===\n");
}

// ==================== 主程序（交互菜单，无命令历史） ====================
int main() {
    Text* my_text = create_text();
    if (!my_text) return 1;
    
    UndoStack undo_stack;
    init_undo_stack(&undo_stack);
    
    KeywordList keyword_list;
    init_keyword_list(&keyword_list);
    
    char filename[100];
    printf("请输入文件名（input.txt）: ");
    fgets(filename, sizeof(filename), stdin);
    filename[strcspn(filename, "\n")] = '\0';
    if (strlen(filename) == 0) strcpy(filename, "input.txt");
    
    load_file(my_text, filename);
    display_text(my_text);
    
    char cmd[20];
    int line_no;
    char search_str[256];
    
    while (1) {
        printf("\n命令: d <行号>(删除), u(撤销), s(显示/高亮), f <词>(搜索), c(统计), w(词频), h(设置高亮关键词), q(退出): ");
        scanf("%s", cmd);
        
        if (strcmp(cmd, "d") == 0) {
            if (scanf("%d", &line_no) != 1) {
                printf("请输入数字行号！\n");
                while (getchar() != '\n');
                continue;
            }
            delete_line(my_text, line_no, &undo_stack);
            display_text(my_text);
        }
        else if (strcmp(cmd, "u") == 0) {
            undo(my_text, &undo_stack);
            display_text(my_text);
        }
        else if (strcmp(cmd, "s") == 0) {
            highlight_keywords(my_text, &keyword_list);
        }
        else if (strcmp(cmd, "f") == 0) {
            if (scanf("%255s", search_str) != 1) {
                printf("请输入搜索关键词！\n");
                while (getchar() != '\n');
                continue;
            }
            find_text(my_text, search_str);
        }
        else if (strcmp(cmd, "c") == 0) {
            show_statistics(my_text);
        }
        else if (strcmp(cmd, "w") == 0) {
            display_word_frequency(my_text);
        }
        else if (strcmp(cmd, "h") == 0) {
            free_keyword_list(&keyword_list);
            init_keyword_list(&keyword_list);
            printf("请输入多个关键词（空格分隔，回车结束）: ");
            char input[512];
            getchar();  // 吸收之前的换行符
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = '\0';
            char* token = strtok(input, " ");
            while (token) {
                add_keyword(&keyword_list, token);
                token = strtok(NULL, " ");
            }
            printf("已设置 %d 个高亮关键词。\n", keyword_list.count);
        }
        else if (strcmp(cmd, "q") == 0) {
            break;
        }
        else {
            printf("未知命令！可用: d, u, s, f, c, w, h, q\n");
        }
        while (getchar() != '\n');  // 清空输入缓冲
    }
    
    free_keyword_list(&keyword_list);
    free_undo_stack(&undo_stack);
    free_text(my_text);
    return 0;
}
