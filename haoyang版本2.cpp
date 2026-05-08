#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INIT_CAPACITY 10

// 文本存储结构体
typedef struct {
    char** lines;
    int line_count;
    int capacity;
} Text;

// 动态扩容函数（模块化）
static int resize_text(Text* t) {
    int new_cap = t->capacity * 2;
    char** new_lines = (char**)realloc(t->lines, new_cap * sizeof(char*));
    if (!new_lines) return 0;
    t->lines = new_lines;
    t->capacity = new_cap;
    return 1;
}

// 初始化文本结构体
Text* create_text() {
    Text* t = (Text*)malloc(sizeof(Text));
    if (!t) return NULL;
    t->line_count = 0;
    t->capacity = INIT_CAPACITY;
    t->lines = (char**)calloc(t->capacity, sizeof(char*));
    if (!t->lines) { free(t); return NULL; }
    return t;
}

// 加载文件（最终优化版）
int load_file(Text* t, const char* filename) {
    if (!t || !filename) return -1;
    FILE* f = fopen(filename, "r");
    if (!f) return -2;

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        if (t->line_count >= t->capacity && !resize_text(t)) {
            fclose(f); return -3;
        }
        
        // 最终优化：跨平台去除换行符 \n 和 \r
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';

        char* copy = (char*)malloc(len + 1);
        if (!copy) { fclose(f); return -4; }
        strcpy(copy, buf);
        t->lines[t->line_count++] = copy;
    }
    fclose(f);
    return t->line_count;
}

// 格式化显示文本
void display_text(const Text* t) {
    if (!t || t->line_count == 0) {
        printf("文本为空\n");
        return;
    }
    printf("=== 文本内容（%d 行）===\n", t->line_count);
    for (int i = 0; i < t->line_count; i++)
        printf("%4d: %s\n", i + 1, t->lines[i]);
}

// 安全释放所有内存
void free_text(Text* t) {
    if (!t) return;
    for (int i = 0; i < t->line_count; i++)
        if (t->lines[i]) free(t->lines[i]);
    free(t->lines);
    free(t);
}

// 主函数
int main() {
    Text* txt = create_text();
    if (!txt) {
        printf("文本初始化失败！\n");
        return 1;
    }

    int result = load_file(txt, "input.txt");
    if (result < 0) {
        printf("文件读取失败，请检查 input.txt 是否存在！\n");
    } else {
        display_text(txt);
    }

    free_text(txt);
    return 0;
}
