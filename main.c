#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_UNDO 50
#define INIT_CAPACITY 10

// 定义文本结构体。
typedef struct {
    char** lines;      // 字符串数组指针
    int line_count;    // 总行数
    int capacity;      // 数组容量
} Text;
typedef struct {
    int line_index;     // 被删除行的原始索引
    char* content;      // 被删除行的内容
} UndoRecord;

typedef struct {
    UndoRecord records[MAX_UNDO];
    int top;            // 栈顶指针，-1 表示空
} UndoStack;
// 创建文本对象
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
        return NULL;
    }
    
    for (int i = 0; i < new_text->capacity; i++) {
        new_text->lines[i] = NULL;
    }
    
    return new_text;
}

// 加载文件
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
    
    char buffer[1024];
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
        
        // 去掉换行符
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
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

// 显示文本
void display_text(const Text* text) {
    if (text == NULL || text->line_count == 0) {
        printf("文本为空.！\n");
        return;
    }
    
    printf("=== 文本内容 [%d行] ===\n", text->line_count);
    for (int i = 0; i < text->line_count; i++) {
        printf("%3d: %s\n", i+1, text->lines[i]);
    }
    printf("=== 结束 ===\n");
}

// 释放内存
void free_text(Text* text) {
    if (text == NULL) return;
    
    for (int i = 0; i < text->line_count; i++) {
        if (text->lines[i] != NULL) {
            free(text->lines[i]);
        }
    }
    
    free(text->lines);
    free(text);
    printf("内存已释放。\n");
}

void init_undo_stack(UndoStack* s) {
    s->top = -1;
    for (int i = 0; i < MAX_UNDO; i++) {
        s->records[i].content = NULL;
    }
}

void free_undo_stack(UndoStack* s) {
    for (int i = 0; i <= s->top; i++) {
        if (s->records[i].content) {
            free(s->records[i].content);
        }
    }
}

int save_state(Text* t, UndoStack* s, int line_no) {
    if (s->top >= MAX_UNDO - 1) {
        printf("撤销栈已满，无法保存\n");
        return 0;
    }
    
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
    
    if (!save_state(t, s, idx)) {
        return 0;
    }
    
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
        if (!new_lines) {
            printf("内存不足，撤销失败\n");
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
        printf("内存分配失败\n");
        return 0;
    }
    strcpy(t->lines[restore_idx], last->content);
    t->line_count++;
    
    free(last->content);
    s->top--;
    
    printf("撤销成功，已恢复第 %d 行\n", restore_idx + 1);
    return 1;
}
void find_text(const Text* text, const char* pattern) {
 if (text == NULL || text->line_count == 0) {
        printf("文本为空，无法搜索。\n");
        return;
    }
    
    int found = 0;
    printf("\n=== 搜索 \"%s\" 的结果 ===\n", pattern);
    for (int i = 0; i < text->line_count; i++) {
        if (strstr(text->lines[i], pattern) != NULL) {
            printf("第 %d 行: %s\n", i+1, text->lines[i]);
            found++;
        }
    }
    if (!found) {
        printf("未找到包含 \"%s\" 的行。\n", pattern);
    }
    printf("========================\n");
}

// 统计总字符数（不包括换行符，因为加载时已去掉换行）
int count_total_chars(const Text* text) {   // +++ V3新增 +++
    if (text == NULL || text->line_count == 0) {
        return 0;
    }
    
    int total = 0;
    for (int i = 0; i < text->line_count; i++) {
        total += strlen(text->lines[i]);
    }
    return total;
}

// 统计总行数（直接返回行数）
int count_total_lines(const Text* text) {   // +++ V3新增 +++
    if (text == NULL) return 0;
    return text->line_count;
}

// 显示统计信息（封装调用）
void show_statistics(const Text* text) {   // +++ V3新增 +++
    int lines = count_total_lines(text);
    int chars = count_total_chars(text);
    printf("\n========== 统计信息 ==========\n");
    printf("总行数: %d\n", lines);
    printf("总字符数: %d (不含换行符)\n", chars);
    if (lines > 0) {
        printf("平均每行字符数: %.1f\n", (double)chars / lines);
    }
    printf("==============================\n");
}
   int main(){
    Text* my_text = create_text();
    if (my_text == NULL) {
        return 1;
    }
    
    // 初始化撤销栈
    UndoStack undo_stack;
    init_undo_stack(&undo_stack);
    
    // 加载文件
    char filename[100];
    printf("请输入文件名（默认 input.txt）: ");
    fgets(filename, sizeof(filename), stdin);
    filename[strcspn(filename, "\n")] = '\0';
    
    if (strlen(filename) == 0) {
        strcpy(filename, "input.txt");
    }
    
    load_file(my_text, filename);
    display_text(my_text);
    
    // 主循环
    char cmd[10];
    int line_no;
    char search_str[256];
    while (1) {
        
        printf("\n命令: d <行号>(删除), u(撤销), s(显示), f <关键词>(搜索), c(统计), q(退出): ");
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
            display_text(my_text);
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
        else if (strcmp(cmd, "q") == 0) {
            break;
        }
        else {
            
            printf("未知命令！可用命令: d, u, s, f, c, q\n");
        }
        // 清空输入缓冲区
        while (getchar() != '\n');
    }
    
    free_undo_stack(&undo_stack);
    free_text(my_text);
    
    return 0;
}

