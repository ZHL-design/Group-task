#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_UNDO 50
#define INIT_CAPACITY 10
#define MAX_WORD_LEN 256
#define COLOR_RED   "\033[31m"
#define COLOR_RESET "\033[0m"

/* ---------- 数据结构 ---------- */
typedef struct {
    char** lines;
    int count;
    int cap;
} Text;

typedef struct {
    int line_idx;
    char* content;
} UndoRec;

typedef struct {
    UndoRec recs[MAX_UNDO];
    int top;
} UndoStack;

typedef struct TrieNode {
    struct TrieNode* child[26];
    int freq;
} TrieNode;

typedef struct {
    char word[MAX_WORD_LEN];
    int freq;
} WordFreq;

typedef struct KwNode {
    char* word;
    struct KwNode* next;
} KwNode;

typedef struct {
    KwNode* head;
    int cnt;
} KwList;

/* ---------- 辅助宏：安全扩容 ---------- */
#define GROW_ARRAY(ptr, old_cap, new_cap, type) do { \
    type* _tmp = realloc(ptr, sizeof(type) * (new_cap)); \
    if (!_tmp) { fprintf(stderr, "内存不足\n"); exit(1); } \
    ptr = _tmp; \
    old_cap = new_cap; \
} while(0)

/* ---------- Text 操作 ---------- */
Text* text_new(void) {
    Text* t = malloc(sizeof(Text));
    t->lines = calloc(INIT_CAPACITY, sizeof(char*));
    t->count = 0;
    t->cap = INIT_CAPACITY;
    return t;
}

void text_load(Text* t, const char* fn) {
    FILE* f = fopen(fn, "r");
    if (!f) { printf("无法打开文件：%s\n", fn); return; }
    char buf[1024];
    while (fgets(buf, sizeof buf, f)) {
        if (t->count >= t->cap)
            GROW_ARRAY(t->lines, t->cap, t->cap * 2, char*);
        buf[strcspn(buf, "\n")] = '\0';
        t->lines[t->count++] = strdup(buf);
    }
    fclose(f);
    printf("文件加载成功，共 %d 行。\n", t->count);
}

void text_show(const Text* t) {
    if (!t || t->count == 0) { printf("文本为空！\n"); return; }
    printf("=== 文本内容 [%d行] ===\n", t->count);
    for (int i = 0; i < t->count; i++)
        printf("%3d: %s\n", i + 1, t->lines[i]);
    puts("=== 结束 ===");
}

void text_free(Text* t) {
    if (!t) return;
    for (int i = 0; i < t->count; i++) free(t->lines[i]);
    free(t->lines);
    free(t);
}

/* ---------- 撤销栈 ---------- */
void undostack_init(UndoStack* s) { s->top = -1; memset(s->recs, 0, sizeof(s->recs)); }

void undostack_free(UndoStack* s) {
    for (int i = 0; i <= s->top; i++) free(s->recs[i].content);
}

static int save_state(Text* t, UndoStack* s, int idx) {
    if (s->top >= MAX_UNDO - 1) return 0;
    s->top++;
    s->recs[s->top].line_idx = idx;
    s->recs[s->top].content = strdup(t->lines[idx]);
    return 1;
}

int text_delete(Text* t, int lineno, UndoStack* s) {
    int idx = lineno - 1;
    if (idx < 0 || idx >= t->count) {
        printf("行号无效！有效范围：1~%d\n", t->count);
        return 0;
    }
    if (!save_state(t, s, idx)) return 0;
    free(t->lines[idx]);
    memmove(t->lines + idx, t->lines + idx + 1, (t->count - idx - 1) * sizeof(char*));
    t->count--;
    printf("已删除第 %d 行\n", lineno);
    return 1;
}

int text_undo(Text* t, UndoStack* s) {
    if (s->top < 0) { puts("没有可撤销的操作"); return 0; }
    UndoRec* r = &s->recs[s->top];
    if (r->line_idx < 0 || r->line_idx > t->count) {
        free(r->content); s->top--; return 0;
    }
    if (t->count >= t->cap)
        GROW_ARRAY(t->lines, t->cap, t->cap * 2, char*);
    memmove(t->lines + r->line_idx + 1, t->lines + r->line_idx,
            (t->count - r->line_idx) * sizeof(char*));
    t->lines[r->line_idx] = strdup(r->content);
    t->count++;
    free(r->content);
    s->top--;
    printf("撤销成功，已恢复第 %d 行\n", r->line_idx + 1);
    return 1;
}

/* ---------- 搜索与统计 ---------- */
void text_find(const Text* t, const char* pat) {
    if (!t || t->count == 0) { puts("文本为空，无法搜索。"); return; }
    int found = 0;
    printf("\n=== 搜索 \"%s\" 的结果 ===\n", pat);
    for (int i = 0; i < t->count; i++)
        if (strstr(t->lines[i], pat))
            printf("第 %d 行: %s\n", i + 1, t->lines[i]), found++;
    if (!found) printf("未找到包含 \"%s\" 的行。\n", pat);
    puts("========================");
}

void text_stats(const Text* t) {
    int lines = t ? t->count : 0;
    int chars = 0;
    for (int i = 0; i < lines; i++) chars += strlen(t->lines[i]);
    printf("\n========== 统计信息 ==========\n");
    printf("总行数: %d\n", lines);
    printf("总字符数: %d (不含换行符)\n", chars);
    if (lines > 0) printf("平均每行字符数: %.1f\n", (double)chars / lines);
    puts("==============================");
}

/* ---------- 词频统计（Trie） ---------- */
TrieNode* trie_new(void) {
    TrieNode* n = calloc(1, sizeof(TrieNode));
    return n;
}

void trie_insert(TrieNode* root, const char* w) {
    TrieNode* cur = root;
    for (; *w; w++) {
        int idx = tolower(*w) - 'a';
        if (idx < 0 || idx >= 25) continue;
        if (!cur->child[idx]) cur->child[idx] = trie_new();
        cur = cur->child[idx];
    }
    cur->freq++;
}

static void trie_collect(TrieNode* n, char* pre, int d, WordFreq** list, int* cnt, int* cap) {
    if (!n) return;
    if (n->freq > 0) {
        if (*cnt >= *cap) GROW_ARRAY(*list, *cap, *cap ? *cap * 2 : 64, WordFreq);
        pre[d] = '\0';
        strcpy((*list)[*cnt].word, pre);
        (*list)[*cnt].freq = n->freq;
        (*cnt)++;
    }
    for (int i = 0; i < 26; i++)
        if (n->child[i]) {
            pre[d] = 'a' + i;
            trie_collect(n->child[i], pre, d + 1, list, cnt, cap);
        }
}

void trie_free(TrieNode* n) {
    if (!n) return;
    for (int i = 0; i < 26; i++) trie_free(n->child[i]);
    free(n);
}

static int cmp_freq(const void* a, const void* b) {
    const WordFreq* x = a, * y = b;
    if (x->freq != y->freq) return y->freq - x->freq;
    return strcmp(x->word, y->word);
}

void text_wordfreq(const Text* t, WordFreq** out, int* out_n) {
    *out = NULL; *out_n = 0;
    if (!t || t->count == 0) return;
    TrieNode* root = trie_new();
    for (int i = 0; i < t->count; i++) {
        char buf[1024];
        strcpy(buf, t->lines[i]);
        for (char* p = buf; *p; p++) if (ispunct(*p)) *p = ' ';
        char* tok = strtok(buf, " \t");
        while (tok) {
            if (*tok) trie_insert(root, tok);
            tok = strtok(NULL, " \t");
        }
    }
    int cap = 0, cnt = 0;
    char pre[MAX_WORD_LEN];
    trie_collect(root, pre, 0, out, &cnt, &cap);
    trie_free(root);
    qsort(*out, cnt, sizeof(WordFreq), cmp_freq);
    *out_n = cnt;
}

void text_showfreq(const Text* t) {
    WordFreq* list; int n;
    text_wordfreq(t, &list, &n);
    if (n == 0) { puts("文本中没有单词或文本为空。"); return; }
    printf("\n========== 词频统计结果（基于Trie树） ==========\n");
    printf("共 %d 个不同的单词\n", n);
    puts("单词\t\t频率\n------------------------");
    for (int i = 0; i < n; i++) printf("%-20s %d\n", list[i].word, list[i].freq);
    puts("================================================");
    free(list);
}

/* ---------- 关键词高亮（链表） ---------- */
KwList* kwlist_new(void) {
    KwList* k = malloc(sizeof(KwList));
    k->head = NULL; k->cnt = 0;
    return k;
}

void kwlist_add(KwList* k, const char* w) {
    for (KwNode* cur = k->head; cur; cur = cur->next)
        if (!strcmp(cur->word, w)) return;
    KwNode* n = malloc(sizeof(KwNode));
    n->word = strdup(w);
    n->next = k->head;
    k->head = n;
    k->cnt++;
}

void kwlist_free(KwList* k) {
    KwNode* cur = k->head;
    while (cur) {
        KwNode* tmp = cur;
        cur = cur->next;
        free(tmp->word);
        free(tmp);
    }
    k->head = NULL; k->cnt = 0;
}

void text_highlight(const Text* t, const KwList* k) {
    if (!t || t->count == 0) { puts("文本为空！"); return; }
    if (!k->head) { text_show(t); return; }
    printf("=== 多关键词高亮显示 [%d行] ===\n", t->count);
    for (int i = 0; i < t->count; i++) {
        const char* s = t->lines[i];
        printf("%3d: ", i + 1);
        int pos = 0, len = strlen(s);
        while (pos < len) {
            int best = -1, blen = 0;
            for (KwNode* cur = k->head; cur; cur = cur->next) {
                const char* kw = cur->word;
                int kwl = strlen(kw);
                if (kwl == 0) continue;
                const char* found = strstr(s + pos, kw);
                if (found) {
                    int off = found - (s + pos);
                    if (best == -1 || off < best) { best = off; blen = kwl; }
                }
            }
            if (best == -1) { printf("%s", s + pos); break; }
            if (best > 0) printf("%.*s", best, s + pos);
            printf("%s%.*s%s", COLOR_RED, blen, s + pos + best, COLOR_RESET);
            pos += best + blen;
        }
        putchar('\n');
    }
    puts("=== 结束 ===");
}

/* ---------- 主程序 ---------- */
int main(void) {
    Text* txt = text_new();
    UndoStack undo;
    undostack_init(&undo);
    KwList* kw = kwlist_new();

    char fn[100] = "input.txt";
    printf("请输入文件名（默认 input.txt）: ");
    if (fgets(fn, sizeof fn, stdin)) {
        fn[strcspn(fn, "\n")] = '\0';
        if (strlen(fn) == 0) strcpy(fn, "input.txt");
    }
    text_load(txt, fn);
    text_show(txt);

    char line[512];
    while (1) {
        printf("\n命令: d <行号>(删除), u(撤销), s(显示/高亮), f <词>(搜索), c(统计), w(词频), h(设置高亮关键词), q(退出): ");
        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        char cmd[32] = "";
        char arg[256] = "";
        int n = sscanf(line, "%31s %255s", cmd, arg);
        if (n < 1) continue;

        if (!strcmp(cmd, "d")) {
            int ln;
            if (sscanf(arg, "%d", &ln) == 1) {
                text_delete(txt, ln, &undo);
                text_show(txt);
            } else puts("请输入数字行号！");
        }
        else if (!strcmp(cmd, "u")) {
            text_undo(txt, &undo);
            text_show(txt);
        }
        else if (!strcmp(cmd, "s")) {
            text_highlight(txt, kw);
        }
        else if (!strcmp(cmd, "f")) {
            if (strlen(arg)) text_find(txt, arg);
            else puts("请输入搜索关键词！");
        }
        else if (!strcmp(cmd, "c")) {
            text_stats(txt);
        }
        else if (!strcmp(cmd, "w")) {
            text_showfreq(txt);
        }
        else if (!strcmp(cmd, "h")) {
            kwlist_free(kw);
            printf("请输入多个关键词（空格分隔）: ");
            if (fgets(line, sizeof line, stdin)) {
                line[strcspn(line, "\n")] = '\0';
                char* tok = strtok(line, " ");
                while (tok) { kwlist_add(kw, tok); tok = strtok(NULL, " "); }
            }
            printf("已设置 %d 个高亮关键词。\n", kw->cnt);
        }
        else if (!strcmp(cmd, "q")) break;
        else printf("未知命令！可用: d, u, s, f, c, w, h, q\n");
    }

    kwlist_free(kw);
    undostack_free(&undo);
    text_free(txt);
    return 0;
}
