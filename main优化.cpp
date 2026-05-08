#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INIT_CAPACITY 10  // 初始容量

// 文本结构体定义
typedef struct {
    char** lines;        // 存储每一行内容
    int line_count;      // 当前行数
    int capacity;        // 总容量
} Text;

// 独立扩容函数（模块化核心优化）
static int resize_text(Text* t) {
    int new_cap = t->capacity * 2;  // 二倍扩容
    char** new_lines = (char**)realloc(t->lines, new_cap * sizeof(char*));
    if (!new_lines) return 0;       // 扩容失败返回0
    t->lines = new_lines;
    t->capacity = new_cap;
    return 1;
}

// 创建文本结构体
Text* create_text() {
    Text* t = (Text*)malloc(sizeof(Text));
    if (!t) return NULL;
    t->line_count = 0;
    t->capacity = INIT_CAPACITY;
    t->lines = (char**)calloc(t->capacity, sizeof(char*));
    if (!t->lines) { free(t); return NULL; }
    return t;
}

// 从文件加载文本
int load_file(Text* t, const char* filename) {
    if (!t || !filename) return -1;
    FILE* f = fopen(filename, "r");
    if (!f) return -2;

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        // 容量不足则扩容
        if (t->line_count >= t->capacity && !resize_text(t)) {
            fclose(f); return -3;
        }
        // 去除换行符
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
        // 复制字符串
        char* copy = (char*)malloc(len + 1);
        strcpy(copy, buf);
        t->lines[t->line_count++] = copy;
    }
    fclose(f);
    return t->line_count;
}

// 显示文本内容
void display_text(const Text* t) {
    if (!t || t->line_count == 0) {
        printf("文本为空\n");
        return;
    }
    printf("=== 文本内容（%d 行）===\n", t->line_count);
    for (int i = 0; i < t->line_count; i++)
        printf("%4d: %s\n", i + 1, t->lines[i]);
}

// 释放内存
void free_text(Text* t) {
    if (!t) return;
    for (int i = 0; i < t->line_count; i++)
        free(t->lines[i]);
    free(t->lines);
    free(t);
}

// 主函数
int main() {
    Text* txt = create_text();
    int res = load_file(txt, "input.txt");
    if (res < 0) printf("文件加载失败！\n");
    else display_text(txt);
    free_text(txt);
    return 0;
}
