#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_CAPACITY 10

// 定义文本结构体。
typedef struct {
    char** lines;      // 字符串数组指针
    int line_count;    // 总行数
    int capacity;      // 数组容量
} Text;

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
        printf("文本为空！\n");
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

// 主函数
int main() {
    Text* my_text = create_text();
    if (my_text == NULL) {
        return 1;
    }
    
    load_file(my_text, "input.txt");
    display_text(my_text);
    free_text(my_text);
    
    return 0;
}
