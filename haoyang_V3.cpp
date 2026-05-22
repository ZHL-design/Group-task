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

// 加载文件
int load_file(Text* t, const char* filename) {
    if (!t || !filename) return -1;
    FILE* f = fopen(filename, "r");
    if (!f) return -2;

    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        if (t->line_count >= t->capacity && !resize_text(t)) {
            fclose(f); return -3;
        }
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

// 保存文件（版本3新增）
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

// 插入一行（版本3新增）
int insert_line(Text* t, int pos, const char* content) {
    if (!t || pos < 1 || pos > t->line_count + 1) return -1;
    int index = pos - 1;

    if (t->line_count >= t->capacity && !resize_text(t)) {
        return -2;
    }
    for (int i = t->line_count; i > index; i--) {
        t->lines[i] = t->lines[i - 1];
    }
    char* new_line = (char*)malloc(strlen(content) + 1);
    if (!new_line) return -3;
    strcpy(new_line, content);
    t->lines[index] = new_line;
    t->line_count++;
    return 1;
}

// 删除一行（版本3新增）
int delete_line(Text* t, int pos) {
    if (!t || pos < 1 || pos > t->line_count) return -1;
    int index = pos - 1;

    free(t->lines[index]);
    for (int i = index; i < t->line_count - 1; i++) {
        t->lines[i] = t->lines[i + 1];
    }
    t->line_count--;
    t->lines[t->line_count] = NULL;
    return 1;
}

// 修改一行（版本3新增）
int modify_line(Text* t, int pos, const char* new_content) {
    if (!t || pos < 1 || pos > t->line_count) return -1;
    int index = pos - 1;

    free(t->lines[index]);
    char* new_line = (char*)malloc(strlen(new_content) + 1);
    if (!new_line) return -2;
    strcpy(new_line, new_content);
    t->lines[index] = new_line;
    return 1;
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

// 菜单（版本3新增）
void show_menu() {
    printf("\n========== 文本编辑器菜单 ==========\n");
    printf("1. 显示文本内容\n");
    printf("2. 插入一行\n");
    printf("3. 删除一行\n");
    printf("4. 修改一行\n");
    printf("5. 保存到文件\n");
    printf("0. 退出程序\n");
    printf("===================================\n");
    printf("请输入操作序号：");
}

// 主函数（版本3：带交互菜单）
int main() {
    Text* txt = create_text();
    if (!txt) {
        printf("文本初始化失败！\n");
        return 1;
    }

    // 自动加载 input.txt
    load_file(txt, "input.txt");

    int choice;
    int line;
    char content[1024];

    while (1) {
        show_menu();
        scanf("%d", &choice);
        getchar(); // 吸收换行

        switch (choice) {
            case 1:
                display_text(txt);
                break;
            case 2:
                printf("请输入要插入的行号：");
                scanf("%d", &line);
                getchar();
                printf("请输入内容：");
                fgets(content, 1024, stdin);
                content[strcspn(content, "\n")] = '\0';
                insert_line(txt, line, content);
                printf("插入成功！\n");
                break;
            case 3:
                printf("请输入要删除的行号：");
                scanf("%d", &line);
                delete_line(txt, line);
                printf("删除成功！\n");
                break;
            case 4:
                printf("请输入要修改的行号：");
                scanf("%d", &line);
                getchar();
                printf("请输入新内容：");
                fgets(content, 1024, stdin);
                content[strcspn(content, "\n")] = '\0';
                modify_line(txt, line, content);
                printf("修改成功！\n");
                break;
            case 5:
                save_file(txt, "output.txt");
                printf("已保存到 output.txt！\n");
                break;
            case 0:
                printf("程序退出，释放内存...\n");
                free_text(txt);
                return 0;
            default:
                printf("输入无效，请重新选择！\n");
        }
    }
    free_text(txt);
    return 0;
}
