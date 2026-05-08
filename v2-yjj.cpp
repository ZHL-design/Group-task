#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_UNDO 50
#define INIT_CAPACITY 10
#define LINE_BUFFER_SIZE 1024

// 文本结构体：管理文件内容的动态数组
typedef struct {
    char** lines;      // 每一行字符串的指针数组
    int line_count;    // 当前总行数
    int capacity;      // 数组容量（动态扩容用）
} Text;

// 撤销记录结构体：保存被删除行的状态
typedef struct {
    int line_index;     // 被删除行在原文本中的索引
    char* content;      // 被删除行的完整内容
} UndoRecord;

// 撤销栈结构体：用数组实现栈，保存所有可撤销的删除操作
typedef struct {
    UndoRecord records[MAX_UNDO];
    int top;            // 栈顶指针，-1 表示栈空
} UndoStack;

// ---------------------- 基础工具函数（来自V1） ----------------------
// 创建并初始化文本对象
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
    
    // 初始化所有指针为NULL，避免野指针
    for (int i = 0; i < new_text->capacity; i++) {
        new_text->lines[i] = NULL;
    }
    
    return new_text;
}

// 加载文件到文本对象
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
        // 动态扩容：当行数量超过容量时，容量翻倍
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
        
        // 去掉换行符（兼容Windows的\r\n和Linux的\n）
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
            len--;
        }
        if (len > 0 && buffer[len-1] == '\r') {
            buffer[len-1] = '\0';
        }
        
        // 复制行内容到堆内存，避免buffer被覆盖
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

// 显示文本内容
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

// 释放文本对象的所有内存
void free_text(Text* text) {
    if (text == NULL) return;
    
    // 先释放每一行的字符串
    for (int i = 0; i < text->line_count; i++) {
        if (text->lines[i] != NULL) {
            free(text->lines[i]);
        }
    }
    // 再释放行指针数组和文本结构体
    free(text->lines);
    free(text);
    printf("文本内存已释放。\n");
}

// ---------------------- V2 新增：撤销栈核心函数 ----------------------
// 初始化撤销栈
void init_undo_stack(UndoStack* s) {
    s->top = -1;
    for (int i = 0; i < MAX_UNDO; i++) {
        s->records[i].content = NULL;
    }
}

// 释放撤销栈中保存的所有被删除行内容
void free_undo_stack(UndoStack* s) {
    for (int i = 0; i <= s->top; i++) {
        if (s->records[i].content) {
            free(s->records[i].content);
        }
    }
}

// 保存删除前的状态到撤销栈（压栈操作）
int save_state(Text* t, UndoStack* s, int line_no) {
    // 栈满时拒绝保存
    if (s->top >= MAX_UNDO - 1) {
        printf("撤销栈已满，无法保存更多操作！\n");
        return 0;
    }
    
    s->top++;
    s->records[s->top].line_index = line_no;
    // 复制被删除行的内容到栈中，避免原内容被释放
    s->records[s->top].content = (char*)malloc(strlen(t->lines[line_no]) + 1);
    
    if (!s->records[s->top].content) {
        printf("内存分配失败，无法保存撤销状态！\n");
        s->top--; // 回滚栈顶
        return 0;
    }
    
    strcpy(s->records[s->top].content, t->lines[line_no]);
    return 1;
}

// ---------------------- V2 核心功能：删除行 + 撤销 ----------------------
// 删除指定行（行号从1开始计数）
int delete_line(Text* t, int line_no, UndoStack* s) {
    int idx = line_no - 1;
    
    // 行号有效性检查
    if (idx < 0 || idx >= t->line_count) {
        printf("行号无效！有效范围：1~%d\n", t->line_count);
        return 0;
    }
    
    // 先保存状态，再删除
    if (!save_state(t, s, idx)) {
        return 0;
    }
    
    // 释放被删除行的内存
    free(t->lines[idx]);
    // 移动后续所有行，覆盖被删除的位置
    for (int i = idx; i < t->line_count - 1; i++) {
        t->lines[i] = t->lines[i+1];
    }
    
    t->line_count--;
    printf("已删除第 %d 行\n", line_no);
    return 1;
}

// 撤销上一次删除操作（弹栈操作）
int undo(Text* t, UndoStack* s) {
    // 栈空时无法撤销
    if (s->top < 0) {
        printf("没有可撤销的操作！\n");
        return 0;
    }
    
    UndoRecord* last = &s->records[s->top];
    int restore_idx = last->line_index;
    
    // 检查保存的索引是否还在有效范围内
    if (restore_idx < 0 || restore_idx > t->line_count) {
        printf("撤销失败：保存的行号已无效\n");
        free(last->content);
        s->top--;
        return 0;
    }
    
    // 文本扩容：如果插入后会超过容量，先扩容
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
    
    // 从后往前移动行，腾出插入位置
    for (int i = t->line_count; i > restore_idx; i--) {
        t->lines[i] = t->lines[i-1];
    }
    
    // 恢复被删除的行
    t->lines[restore_idx] = (char*)malloc(strlen(last->content) + 1);
    if (!t->lines[restore_idx]) {
        printf("内存分配失败，撤销失败！\n");
        return 0;
    }
    strcpy(t->lines[restore_idx], last->content);
    t->line_count++;
    
    // 释放栈中保存的内容，栈顶下移
    free(last->content);
    s->top--;
    
    printf("撤销成功，已恢复第 %d 行\n", restore_idx + 1);
    return 1;
}

// ---------------------- 主函数（V2交互界面） ----------------------
int main() {
    Text* my_text = create_text();
    if (my_text == NULL) {
        return 1;
    }
    
    // 初始化撤销栈
    UndoStack undo_stack;
    init_undo_stack(&undo_stack);
    
    // 加载文件（支持用户输入文件名，默认input.txt）
    char filename[100];
    printf("请输入文件名（默认 input.txt）: ");
    fgets(filename, sizeof(filename), stdin);
    filename[strcspn(filename, "\n")] = '\0';
    
    if (strlen(filename) == 0) {
        strcpy(filename, "input.txt");
    }
    
    load_file(my_text, filename);
    display_text(my_text);
    
    // 主交互循环
    char cmd[10];
    int line_no;
    
    while (1) {
        printf("\n--- 命令菜单 ---\n");
        printf("d <行号> : 删除指定行\n");
        printf("u       : 撤销上一次删除\n");
        printf("s       : 显示当前文本\n");
        printf("q       : 退出程序\n");
        printf("请输入命令: ");
        
        scanf("%s", cmd);
        
        if (strcmp(cmd, "d") == 0) {
            if (scanf("%d", &line_no) != 1) {
                printf("请输入有效的数字行号！\n");
                // 清空输入缓冲区，避免死循环
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
        else if (strcmp(cmd, "q") == 0) {
            printf("退出程序...\n");
            break;
        }
        else {
            printf("未知命令！请输入 d/u/s/q\n");
        }
        // 清空输入缓冲区，为下一次输入做准备
        while (getchar() != '\n');
    }
    
    // 释放所有资源
    free_undo_stack(&undo_stack);
    free_text(my_text);
    printf("程序结束，所有内存已释放。\n");
    
    return 0;
}
