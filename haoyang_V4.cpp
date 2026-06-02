#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 动态数组初始容量
#define INIT_CAPACITY 10

// 文本结构体：用于存储多行文本内容
typedef struct {
    char** lines;      // 指向每行字符串的指针数组
    int line_count;    // 当前实际行数
    int capacity;      // 当前最大容量（动态扩容使用）
} Text;

/**
 * @brief 动态扩容函数
 * @param t 文本结构体指针
 * @return 成功1，失败0
 */
static int resize_text(Text* t) {
    // 二倍扩容
    int new_cap = t->capacity * 2;
    char** new_lines = (char**)realloc(t->lines, new_cap * sizeof(char*));
    if (!new_lines) return 0;

    t->lines = new_lines;
    t->capacity = new_cap;
    return 1;
}

/**
 * @brief 创建并初始化文本结构体
 * @return 成功返回结构体指针，失败返回NULL
 */
Text* create_text() {
    Text* t = (Text*)malloc(sizeof(Text));
    if (!t) return NULL;

    t->line_count = 0;
    t->capacity = INIT_CAPACITY;

    // 初始化指针数组
    t->lines = (char**)calloc(t->capacity, sizeof(char*));
    if (!t->lines) {
        free(t);
        return NULL;
    }
    return t;
}

/**
 * @brief 从文件加载内容到文本结构体
 * @param t 文本指针
 * @param filename 文件名
 * @return 成功返回行数，失败返回负数
 */
int load_file(Text* t, const char* filename) {
    if (!t || !filename) return -1;

    FILE* f = fopen(filename, "r");
    if (!f) return -2;

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        // 容量不足则扩容
        if (t->line_count >= t->capacity && !resize_text(t)) {
            fclose(f);
            return -3;
        }

        // 去除换行符 \n 和 \r（兼容 Windows/Linux）
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';

        // 分配内存并复制行内容
        char* copy = (char*)malloc(len + 1);
        if (!copy) {
            fclose(f);
            return -4;
        }
        strcpy(copy, buf);
        t->lines[t->line_count++] = copy;
    }

    fclose(f);
    return t->line_count;
}

/**
 * @brief 将文本内容保存到文件
 * @param t 文本指针
 * @param filename 目标文件名
 * @return 成功1，失败-1/-2
 */
int save_file(Text* t, const char* filename) {
    if (!t || !filename) return -1;

    FILE* f = fopen(filename, "w");
    if (!f) return -2;

    for (int i = 0; i < t->line_count; i++) {
        fprintf(f, "%s\n", t->lines[i]);
    }

    fclose(f);
    return 1;
}

/**
 * @brief 在指定位置插入一行
 * @param t 文本指针
 * @param pos 行号（从1开始）
 * @param content 插入内容
 * @return 成功1，失败负数
 */
int insert_line(Text* t, int pos, const char* content) {
    // 行号越界判断
    if (!t || pos < 1 || pos > t->line_count + 1) return -1;
    int index = pos - 1;

    // 容量不足则扩容
    if (t->line_count >= t->capacity && !resize_text(t)) return -2;

    // 后面的行依次后移
    for (int i = t->line_count; i > index; i--) {
        t->lines[i] = t->lines[i - 1];
    }

    // 复制新行
    char* new_line = (char*)malloc(strlen(content) + 1);
    if (!new_line) return -3;
    strcpy(new_line, content);

    t->lines[index] = new_line;
    t->line_count++;
    return 1;
}

/**
 * @brief 删除指定行
 * @param t 文本指针
 * @param pos 行号
 * @return 成功1，失败-1
 */
int delete_line(Text* t, int pos) {
    if (!t || pos < 1 || pos > t->line_count) return -1;
    int index = pos - 1;

    // 释放该行内存
    free(t->lines[index]);

    // 后面的行依次前移
    for (int i = index; i < t->line_count - 1; i++) {
        t->lines[i] = t->lines[i + 1];
    }

    t->line_count--;
    t->lines[t->line_count] = NULL;
    return 1;
}

/**
 * @brief 修改指定行内容
 * @param t 文本指针
 * @param pos 行号
 * @param new_content 新内容
 * @return 成功1，失败负数
 */
int modify_line(Text* t, int pos, const char* new_content) {
    if (!t || pos < 1 || pos > t->line_count) return -1;
    int index = pos - 1;

    // 释放旧行
    free(t->lines[index]);

    // 分配新行
    char* new_line = (char*)malloc(strlen(new_content) + 1);
    if (!new_line) return -2;
    strcpy(new_line, new_content);

    t->lines[index] = new_line;
    return 1;
}

/**
 * @brief 查找关键字并显示所在行
 * @param t 文本指针
 * @param keyword 查找关键字
 */
void search_text(Text* t, const char* keyword) {
    if (!t || t->line_count == 0 || !keyword) {
        printf("文本为空或关键字无效！\n");
        return;
    }

    int found = 0;
    printf("==== 查找结果（关键字：%s）====\n", keyword);
    for (int i = 0; i < t->line_count; i++) {
        if (strstr(t->lines[i], keyword)) {
            printf("第 %d 行：%s\n", i + 1, t->lines[i]);
            found = 1;
        }
    }
    if (!found)
        printf("未找到匹配内容！\n");
}

/**
 * @brief 全局替换文本
 * @param t 文本指针
 * @param old_str 旧字符串
 * @param new_str 新字符串
 * @return 替换次数
 */
int replace_text(Text* t, const char* old_str, const char* new_str) {
    if (!t || !old_str || !new_str) return -1;

    int count = 0;
    int old_len = strlen(old_str);
    int new_len = strlen(new_str);

    for (int i = 0; i < t->line_count; i++) {
        char* pos = strstr(t->lines[i], old_str);
        while (pos != NULL) {
            int line_len = strlen(t->lines[i]);
            int new_line_len = line_len - old_len + new_len;

            // 生成新行
            char* new_line = (char*)malloc(new_line_len + 1);
            if (!new_line) return -2;

            // 拼接三部分：前 + 新串 + 后
            strncpy(new_line, t->lines[i], pos - t->lines[i]);
            new_line[pos - t->lines[i]] = '\0';
            strcat(new_line, new_str);
            strcat(new_line, pos + old_len);

            // 替换并释放旧行
            free(t->lines[i]);
            t->lines[i] = new_line;
            count++;

            // 继续向后查找
            pos = strstr(t->lines[i] + (pos - t->lines[i]) + new_len, old_str);
        }
    }
    return count;
}

/**
 * @brief 显示文本统计信息（行数、字符数）
 */
void show_statistics(Text* t) {
    if (!t) return;

    int total_chars = 0;
    for (int i = 0; i < t->line_count; i++) {
        total_chars += strlen(t->lines[i]);
    }

    printf("==== 文本统计信息 ====\n");
    printf("总行数：%d\n", t->line_count);
    printf("总字符数（不含换行）：%d\n", total_chars);
}

/**
 * @brief 显示所有文本内容
 */
void display_text(const Text* t) {
    if (!t || t->line_count == 0) {
        printf("文本为空！\n");
        return;
    }

    printf("=== 文本内容（%d 行）===\n", t->line_count);
    for (int i = 0; i < t->line_count; i++)
        printf("%4d: %s\n", i + 1, t->lines[i]);
}

/**
 * @brief 安全释放所有动态内存
 */
void free_text(Text* t) {
    if (!t) return;

    // 逐行释放
    for (int i = 0; i < t->line_count; i++)
        if (t->lines[i]) free(t->lines[i]);

    // 释放数组和结构体
    free(t->lines);
    free(t);
}

/**
 * @brief 显示功能菜单
 */
void show_menu() {
    printf("\n========== 文本编辑器 V4 菜单 ==========\n");
    printf("1. 显示文本内容\n");
    printf("2. 插入一行\n");
    printf("3. 删除一行\n");
    printf("4. 修改一行\n");
    printf("5. 查找关键字\n");
    printf("6. 替换文本\n");
    printf("7. 显示统计信息\n");
    printf("8. 保存到文件\n");
    printf("0. 退出程序\n");
    printf("=======================================\n");
    printf("请输入操作序号：");
}

// 主函数：程序入口
int main() {
    // 创建文本结构体
    Text* txt = create_text();
    if (!txt) {
        printf("文本初始化失败！\n");
        return 1;
    }

    // 加载 input.txt
    load_file(txt, "input.txt");

    int choice, line;
    char content[1024], key[1024], new_key[1024];
    int cnt;  // 替换次数变量

    // 循环菜单
    while (1) {
        show_menu();
        scanf("%d", &choice);
        getchar();  // 吸收缓冲区换行符

        switch (choice) {
            case 1:
                display_text(txt);
                break;
            case 2:
                printf("输入插入行号：");
                scanf("%d", &line);
                getchar();
                printf("输入内容：");
                fgets(content, 1024, stdin);
                content[strcspn(content, "\n")] = '\0';
                if (insert_line(txt, line, content) == 1)
                    printf("插入成功！\n");
                else
                    printf("插入失败！\n");
                break;
            case 3:
                printf("输入删除行号：");
                scanf("%d", &line);
                if (delete_line(txt, line) == 1)
                    printf("删除成功！\n");
                else
                    printf("删除失败！\n");
                break;
            case 4:
                printf("输入修改行号：");
                scanf("%d", &line);
                getchar();
                printf("输入新内容：");
                fgets(content, 1024, stdin);
                content[strcspn(content, "\n")] = '\0';
                if (modify_line(txt, line, content) == 1)
                    printf("修改成功！\n");
                else
                    printf("修改失败！\n");
                break;
            case 5:
                printf("请输入要查找的关键字：");
                fgets(key, 1024, stdin);
                key[strcspn(key, "\n")] = '\0';
                search_text(txt, key);
                break;
            case 6:
                printf("请输入要替换的旧文本：");
                fgets(key, 1024, stdin);
                key[strcspn(key, "\n")] = '\0';
                printf("请输入新文本：");
                fgets(new_key, 1024, stdin);
                new_key[strcspn(new_key, "\n")] = '\0';
                cnt = replace_text(txt, key, new_key);
                printf("替换完成，共替换 %d 处！\n", cnt);
                break;
            case 7:
                show_statistics(txt);
                break;
            case 8:
                save_file(txt, "output.txt");
                printf("已保存到 output.txt！\n");
                break;
            case 0:
                printf("程序退出，释放内存...\n");
                free_text(txt);
                return 0;
            default:
                printf("输入无效，请重新选择！\n");
                break;
        }
    }

    // 安全释放
    free_text(txt);
    return 0;
}
