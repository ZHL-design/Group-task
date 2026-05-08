#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_CAPACITY 10
#define MAX_UNDO 5      // 最大撤销步数

// 文本结构体
typedef struct {
    char** lines;
    int line_count;
    int capacity;
} Text;

// 撤销记录（保存被删除的行）
typedef struct {
    int line_index;     // 删除前的行号（0-based）
    char* content;      // 被删除行的内容
} UndoRecord;

// 栈结构
typedef struct {
    UndoRecord records[MAX_UNDO];
    int top;            // 栈顶索引，-1 表示空
} UndoStack;

// 函数声明
Text* create_text();
void load_file(Text* text, const char* filename);
void display_text(const Text* text);
void free_text(Text* text);
void delete_line(Text* text, int line_no, UndoStack* stack);
void undo_delete(Text* text, UndoStack* stack);
void save_state(Text* text, UndoStack* stack, int line_no);
void init_undo_stack(UndoStack* stack);
void free_undo_stack(UndoStack* stack);

// ------------------------- 实现 -------------------------

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

void load_file(Text* t, const char* filename) {
    if (!t) return;
    // 清空原有内容
    for (int i = 0; i < t->line_count; i++) {
        free(t->lines[i]);
        t->lines[i] = NULL;
    }
    t->line_count = 0;

    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("无法打开文件 %s\n", filename);
        return;
    }
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';

        if (t->line_count >= t->capacity) {
            int new_cap = t->capacity * 2;
            char** new_lines = (char**)realloc(t->lines, sizeof(char*) * new_cap);
            if (!new_lines) break;
            t->lines = new_lines;
            t->capacity = new_cap;
        }
        char* copy = (char*)malloc(strlen(buf) + 1);
        if (!copy) break;
        strcpy(copy, buf);
        t->lines[t->line_count] = copy;
        t->line_count++;
    }
    fclose(fp);
    printf("加载完成，共 %d 行\n", t->line_count);
}

void display_text(const Text* t) {
    if (!t || t->line_count == 0) {
        printf("文本为空\n");
        return;
    }
    printf("\n=== 文本内容 ===\n");
    for (int i = 0; i < t->line_count; i++) {
        printf("%3d: %s\n", i+1, t->lines[i]);
    }
    printf("===============\n");
}

void free_text(Text* t) {
    if (!t) return;
    for (int i = 0; i < t->line_count; i++)
        free(t->lines[i]);
    free(t->lines);
    free(t);
}

// 初始化撤销栈
void init_undo_stack(UndoStack* s) {
    s->top = 0;                 
    for (int i = 0; i < MAX_UNDO; i++) {
        s->records[i].content = NULL;
    }
}

// 释放栈内动态内存
void free_undo_stack(UndoStack* s) {
    for (int i = 0; i < s->top; i++) {   
        if (s->records[i].content)
            free(s->records[i].content);
    }
}

// 保存当前删除行的状态（压栈）
void save_state(Text* t, UndoStack* s, int line_no) {
    if (line_no < 0 || line_no >= t->line_count) {
        printf("save_state: 行号无效\n");
        return;
    }
    
    s->top++;
    if (s->top >= MAX_UNDO) {
        printf("撤销栈已满，无法保存本次删除状态\n");
        s->top--;
        return;
    }
    s->records[s->top].line_index = line_no;
    
    s->records[s->top].content = (char*)malloc(strlen(t->lines[line_no]) + 1);
    strcpy(s->records[s->top].content, t->lines[line_no]);
}

// 删除指定行（行号从1开始）
void delete_line(Text* t, int line_no, UndoStack* s) {
    int idx = line_no - 1;
    if (idx < 0 || idx >= t->line_count) {
        printf("删除失败：行号无效 (1~%d)\n", t->line_count);
        return;
    }
    
    save_state(t, s, idx);

    free(t->lines[idx]);
    for (int i = idx; i < t->line_count - 1; i++) {
        t->lines[i] = t->lines[i+1];
    }
    t->line_count--;
    printf("已删除第 %d 行\n", line_no);
}

// 撤销上一次删除
void undo_delete(Text* t, UndoStack* s) {
    if (s->top < 0) {
        printf("没有可撤销的操作\n");
        return;
    }
    UndoRecord* last = &s->records[s->top];
    int restore_idx = last->line_index;

    
    if (restore_idx < 0 || restore_idx >= t->line_count) {
        printf("撤销失败：保存的行号已无效\n");
        free(last->content);
        s->top--;
        return;
    }

    if (t->line_count + 1 > t->capacity) {
        int new_cap = t->capacity * 2;
        char** new_lines = (char**)realloc(t->lines, sizeof(char*) * new_cap);
        if (!new_lines) {
            printf("内存不足，撤销失败\n");
            return;
        }
        t->lines = new_lines;
        t->capacity = new_cap;
    }

    for (int i = t->line_count; i > restore_idx; i--) {
        t->lines[i] = t->lines[i-1];
    }

    t->lines[restore_idx] = (char*)malloc(strlen(last->content) + 1);
    strcpy(t->lines[restore_idx], last->content);
    t->line_count++;

    free(last->content);
    

    printf("撤销成功，已恢复第 %d 行\n", restore_idx + 1);
}

// ------------------------- 主程序 -------------------------
int main() {
    Text* my_text = create_text();
    if (!my_text) return 1;

    UndoStack undo_stack;
    init_undo_stack(&undo_stack);

    load_file(my_text, "input.txt");
    display_text(my_text);

    char cmd[10];
    int line_num;
    while (1) {
        printf("\n命令: d <行号> (删除), u (撤销), q (退出): ");
        scanf("%s", cmd);
        if (strcmp(cmd, "d") == 0) {
            if (scanf("%d", &line_num) != 1) {
                printf("请输入数字行号\n");
                while (getchar() != '\n');
                continue;
            }
            delete_line(my_text, line_num, &undo_stack);
            display_text(my_text);
        }
        else if (strcmp(cmd, "u") == 0) {
            undo_delete(my_text, &undo_stack);
            display_text(my_text);
        }
        else if (strcmp(cmd, "q") == 0) {
            break;
        }
        else {
            printf("未知命令\n");
        }
        while (getchar() != '\n');
    }

  
    free_text(my_text);
    return 0;
}
