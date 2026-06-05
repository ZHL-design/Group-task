#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define INIT_CAPACITY 10
#define STATE_STACK_INIT_CAPACITY 5
#define MAX_WORD_LEN 50
#define MAX_WORDS 1000

/* ================= 数据结构 ================= */

// 文本结构
typedef struct {
    char** lines;      // 行指针数组
    int line_count;    // 行数
    int capacity;      // 容量
} Text;

// 状态结构（用于撤销）
typedef struct {
    char** lines;
    int line_count;
    int capacity;
} State;

// 状态栈
typedef struct {
    State* states;
    int top;
    int capacity;
} StateStack;

// 单词频率结构（V4新增）
typedef struct {
    char word[MAX_WORD_LEN];
    int count;
} WordFreq;

/* ================= 函数声明 ================= */

Text* create_text();
void load_file(Text* text, const char* filename);
void display_text(const Text* text);
void free_text(Text* text);

StateStack* create_state_stack();
void push_state(StateStack* stack, const Text* text);
State* pop_state(StateStack* stack);
void save_state(Text* text, StateStack* stack);
void undo_delete(Text* text, StateStack* stack);
void delete_line(Text* text, int line_num, StateStack* stack);
void free_state_stack(StateStack* stack);
void free_state(State* state);

/* ================= V3 功能 ================= */
int find_text(const Text* text, const char* keyword);
int count_total_chars(const Text* text);
int count_total_lines(const Text* text);

/* ================= V4 新增功能 ================= */
void count_word_frequency(const Text* text, WordFreq* word_list, int* word_count);
void sort_frequency(WordFreq* word_list, int word_count);
void display_word_frequency(const WordFreq* word_list, int word_count);

/* ================= 基本功能实现 ================= */

Text* create_text() {
    Text* t = (Text*)malloc(sizeof(Text));
    if (!t) return NULL;

    t->line_count = 0;
    t->capacity = INIT_CAPACITY;
    t->lines = (char**)malloc(sizeof(char*) * t->capacity);
    if (!t->lines) {
        free(t);
        return NULL;
    }

    for (int i = 0; i < t->capacity; i++)
        t->lines[i] = NULL;

    return t;
}

void load_file(Text* text, const char* filename) {
    if (!text) return;

    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("无法打开文件：%s\n", filename);
        return;
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), fp)) {
        if (text->line_count >= text->capacity) {
            text->capacity *= 2;
            text->lines = realloc(text->lines, sizeof(char*) * text->capacity);
        }

        buf[strcspn(buf, "\n")] = '\0';
        text->lines[text->line_count] = strdup(buf);
        text->line_count++;
    }

    fclose(fp);
    printf("文件 %s 加载完成，共 %d 行。\n", filename, text->line_count);
}

void display_text(const Text* text) {
    if (!text || !text->line_count) {
        printf("文本为空。\n");
        return;
    }

    printf("\n=== 文本预览（%d 行）===\n", text->line_count);
    for (int i = 0; i < text->line_count; i++)
        printf("%3d: %s\n", i + 1, text->lines[i]);
    printf("=======================\n");
}

void free_text(Text* text) {
    if (!text) return;
    for (int i = 0; i < text->line_count; i++)
        free(text->lines[i]);
    free(text->lines);
    free(text);
}

/* ================= V2 撤销机制 ================= */

StateStack* create_state_stack() {
    StateStack* s = (StateStack*)malloc(sizeof(StateStack));
    s->capacity = STATE_STACK_INIT_CAPACITY;
    s->states = malloc(sizeof(State) * s->capacity);
    s->top = -1;
    return s;
}

void push_state(StateStack* stack, const Text* text) {
    if (!stack || !text) return;

    if (++stack->top >= stack->capacity) {
        stack->capacity *= 2;
        stack->states = realloc(stack->states, sizeof(State) * stack->capacity);
    }

    State* st = &stack->states[stack->top];
    st->line_count = text->line_count;
    st->capacity = text->capacity;
    st->lines = malloc(sizeof(char*) * st->capacity);

    for (int i = 0; i < text->line_count; i++)
        st->lines[i] = strdup(text->lines[i]);
}

State* pop_state(StateStack* stack) {
    if (stack->top < 0) return NULL;
    return &stack->states[stack->top--];
}

void save_state(Text* text, StateStack* stack) {
    push_state(stack, text);
}

void undo_delete(Text* text, StateStack* stack) {
    State* st = pop_state(stack);
    if (!st) {
        printf("无可撤销操作。\n");
        return;
    }

    free_text(text);

    text->line_count = st->line_count;
    text->capacity = st->capacity;
    text->lines = malloc(sizeof(char*) * text->capacity);

    for (int i = 0; i < st->line_count; i++)
        text->lines[i] = strdup(st->lines[i]);

    printf("撤销成功。\n");
}

void delete_line(Text* text, int line_num, StateStack* stack) {
    if (!text || line_num < 1 || line_num > text->line_count) {
        printf("行号无效。\n");
        return;
    }

    save_state(text, stack);

    int idx = line_num - 1;
    free(text->lines[idx]);

    for (int i = idx; i < text->line_count - 1; i++)
        text->lines[i] = text->lines[i + 1];

    text->lines[--text->line_count] = NULL;
    printf("已删除第 %d 行。\n", line_num);
}

void free_state_stack(StateStack* stack) {
    while (stack->top >= 0)
        free_state(&stack->states[stack->top--]);
    free(stack->states);
    free(stack);
}

void free_state(State* state) {
    if (!state) return;
    for (int i = 0; i < state->line_count; i++)
        free(state->lines[i]);
    free(state->lines);
}

/* ================= V3 功能 ================= */

int find_text(const Text* text, const char* keyword) {
    if (!text || !keyword) return 0;
    for (int i = 0; i < text->line_count; i++) {
        if (strstr(text->lines[i], keyword))
            return i + 1;
    }
    return 0;
}

int count_total_chars(const Text* text) {
    int total = 0;
    if (!text) return 0;
    for (int i = 0; i < text->line_count; i++)
        total += strlen(text->lines[i]);
    return total;
}

int count_total_lines(const Text* text) {
    return text ? text->line_count : 0;
}

/* ================= V4 新增功能 ================= */

// 判断字符是否是单词的一部分
int is_word_char(char c) {
    return isalpha(c) || isdigit(c);
}

// 统计词频
void count_word_frequency(const Text* text, WordFreq* word_list, int* word_count) {
    if (!text || !text->line_count) {
        *word_count = 0;
        return;
    }
    
    *word_count = 0;
    
    for (int i = 0; i < text->line_count; i++) {
        const char* line = text->lines[i];
        int len = strlen(line);
        
        for (int j = 0; j < len; j++) {
            // 跳过非单词字符
            if (!is_word_char(line[j])) continue;
            
            // 提取一个单词
            char word[MAX_WORD_LEN] = {0};
            int k = 0;
            
            // 复制单词（转换为小写）
            while (j < len && is_word_char(line[j]) && k < MAX_WORD_LEN - 1) {
                word[k++] = tolower(line[j++]);
            }
            word[k] = '\0';
            
            if (strlen(word) < 1) continue;  // 跳过空单词
            
            // 在词频列表中查找单词
            int found = 0;
            for (int w = 0; w < *word_count; w++) {
                if (strcmp(word_list[w].word, word) == 0) {
                    word_list[w].count++;
                    found = 1;
                    break;
                }
            }
            
            // 如果没找到，添加到列表
            if (!found && *word_count < MAX_WORDS) {
                strcpy(word_list[*word_count].word, word);
                word_list[*word_count].count = 1;
                (*word_count)++;
            }
            
            j--;  // 回退一个字符，因为外层循环会递增
        }
    }
}

// 比较函数，用于排序
int compare_frequency(const void* a, const void* b) {
    const WordFreq* w1 = (const WordFreq*)a;
    const WordFreq* w2 = (const WordFreq*)b;
    return w2->count - w1->count;  // 降序排列
}

// 排序词频
void sort_frequency(WordFreq* word_list, int word_count) {
    if (word_count < 2) return;  // 不需要排序
    
    qsort(word_list, word_count, sizeof(WordFreq), compare_frequency);
}

// 显示词频统计结果
void display_word_frequency(const WordFreq* word_list, int word_count) {
    if (word_count == 0) {
        printf("没有找到任何单词。\n");
        return;
    }
    
    printf("\n=== 词频统计（共 %d 个不同单词）===\n", word_count);
    printf("%-5s %-20s %s\n", "排名", "单词", "出现次数");
    printf("------------------------------------\n");
    
    int display_count = word_count < 20 ? word_count : 20;
    for (int i = 0; i < display_count; i++) {
        printf("%-5d %-20s %d\n", i + 1, word_list[i].word, word_list[i].count);
    }
    
    if (word_count > 20) {
        printf("... 还有 %d 个单词未显示\n", word_count - 20);
    }
    printf("====================================\n");
}

/* ================= 主函数 ================= */

int main() {
    Text* text = create_text();
    StateStack* stack = create_state_stack();
    WordFreq word_list[MAX_WORDS];
    int word_count = 0;

    // 使用 input.txt
    load_file(text, "input.txt");

    int choice, line;
    char key[128];

    while (1) {
        printf("\n====== 文本编辑器 V4 ======\n");
        printf("1. 显示文本\n");
        printf("2. 删除行\n");
        printf("3. 撤销删除\n");
        printf("4. 查找字符串\n");
        printf("5. 统计字符数\n");
        printf("6. 统计行数\n");
        printf("7. 统计词频（不排序）\n");
        printf("8. 统计词频（排序后）\n");
        printf("9. 退出\n");
        printf("请选择：");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                display_text(text);
                break;
            case 2:
                printf("输入要删除的行号：");
                scanf("%d", &line);
                delete_line(text, line, stack);
                break;
            case 3:
                undo_delete(text, stack);
                break;
            case 4:
                printf("输入要查找的字符串：");
                scanf("%s", key);
                line = find_text(text, key);
                if (line)
                    printf("找到，位于第 %d 行。\n", line);
                else
                    printf("未找到。\n");
                break;
            case 5:
                printf("总字符数：%d\n", count_total_chars(text));
                break;
            case 6:
                printf("总行数：%d\n", count_total_lines(text));
                break;
            case 7:
                count_word_frequency(text, word_list, &word_count);
                printf("\n=== 词频统计（原始顺序）===\n");
                for (int i = 0; i < word_count && i < 20; i++) {
                    printf("%-20s: %d\n", word_list[i].word, word_list[i].count);
                }
                printf("共 %d 个不同单词\n", word_count);
                break;
            case 8:
                // 先统计，然后排序，最后显示
                count_word_frequency(text, word_list, &word_count);
                sort_frequency(word_list, word_count);
                display_word_frequency(word_list, word_count);
                break;
            case 9:
                free_text(text);
                free_state_stack(stack);
                return 0;
            default:
                printf("无效选项。\n");
        }
    }
}
