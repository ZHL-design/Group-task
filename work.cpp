#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>

#define MAX_UNDO 50
#define INIT_CAPACITY 10
#define MAX_WORD_LEN 256
#define COLOR_RED   "\033[31m"
#define COLOR_RESET "\033[0m"

/* ---------- 自定义 strdup ---------- */
char* my_strdup(const char* s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s);
    char* copy = (char*)malloc(len + 1);
    if (copy == NULL) {
        printf("错误：内存分配失败！\n");
        exit(1);
    }
    strcpy(copy, s);
    return copy;
}

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

/* ---------- 新增：操作日志队列 ---------- */
#define LOG_QUEUE_SIZE 10

typedef struct {
    char* msgs[LOG_QUEUE_SIZE];
    int front, rear, count;
} LogQueue;

/* ---------- 新增：单词共现图 ---------- */
typedef struct EdgeNode {
    int adjvex;
    int weight;
    struct EdgeNode* next;
} EdgeNode;

typedef struct Vertex {
    char* word;
    EdgeNode* firstedge;
} Vertex;

typedef struct {
    Vertex* vertices;
    int capacity;
    int count;
} Graph;

/* ---------- 辅助宏：安全扩容 ---------- */
#define GROW_ARRAY(ptr, old_cap, new_cap, type) do { \
    type* _tmp = (type*)realloc(ptr, sizeof(type) * (new_cap)); \
    if (!_tmp) { fprintf(stderr, "内存不足\n"); exit(1); } \
    ptr = _tmp; \
    old_cap = new_cap; \
} while(0)

/* ---------- Text 操作 ---------- */
Text* text_new(void) {
    Text* t = (Text*)malloc(sizeof(Text));
    t->lines = (char**)calloc(INIT_CAPACITY, sizeof(char*));
    t->count = 0;
    t->cap = INIT_CAPACITY;
    return t;
}

void text_load(Text* t, const char* fn) {
    FILE* f = fopen(fn, "rb");  // 使用二进制模式读取，防止中文乱码
    if (!f) { printf("无法打开文件：%s\n", fn); return; }
    char buf[1024];
    while (fgets(buf, sizeof buf, f)) {
        // 去掉行尾的 \r\n 或 \n
        buf[strcspn(buf, "\r\n")] = '\0';
        if (t->count >= t->cap)
            GROW_ARRAY(t->lines, t->cap, t->cap * 2, char*);
        t->lines[t->count++] = my_strdup(buf);
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

/* ---------- 保存到新文件 ---------- */
void text_save_as(const Text* t, const char* original_fn) {
    if (!t || t->count == 0) {
        printf("文本为空，无法保存。\n");
        return;
    }

    char new_fn[1024];
    const char* dot = strrchr(original_fn, '.');
    if (dot) {
        size_t prefix_len = dot - original_fn;
        strncpy(new_fn, original_fn, prefix_len);
        new_fn[prefix_len] = '\0';
        strcat(new_fn, "_processed");
        strcat(new_fn, dot);
    } else {
        snprintf(new_fn, sizeof new_fn, "%s_processed", original_fn);
    }

    FILE* f = fopen(new_fn, "w");
    if (!f) {
        printf("无法创建新文件：%s\n", new_fn);
        return;
    }

    for (int i = 0; i < t->count; i++) {
        fprintf(f, "%s\n", t->lines[i]);
    }
    fclose(f);
    printf("处理结果已保存至：%s\n", new_fn);
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
    s->recs[s->top].content = my_strdup(t->lines[idx]);
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
    t->lines[r->line_idx] = my_strdup(r->content);
    t->count++;
    free(r->content);
    s->top--;
    printf("撤销成功，已恢复第 %d 行\n", r->line_idx + 1);
    return 1;
}

/* ---------- 新增：行插入（线性表操作） ---------- */
int text_insert(Text* t, int lineno, const char* content) {
    int idx = lineno - 1;
    if (idx < 0 || idx > t->count) {
        printf("行号无效！有效范围：1~%d（也可在末尾插入，行号为%d）\n", t->count, t->count+1);
        return 0;
    }
    if (t->count >= t->cap)
        GROW_ARRAY(t->lines, t->cap, t->cap * 2, char*);
    memmove(t->lines + idx + 1, t->lines + idx, (t->count - idx) * sizeof(char*));
    t->lines[idx] = my_strdup(content);
    t->count++;
    printf("已在第 %d 行前插入一行。\n", lineno);
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
    TrieNode* n = (TrieNode*)calloc(1, sizeof(TrieNode));
    return n;
}

void trie_insert(TrieNode* root, const char* w) {
    TrieNode* cur = root;
    for (; *w; w++) {
        int idx = tolower(*w) - 'a';
        if (idx < 0 || idx >= 26) continue;
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
    const WordFreq* x = (const WordFreq*)a;
    const WordFreq* y = (const WordFreq*)b;
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
    KwList* k = (KwList*)malloc(sizeof(KwList));
    k->head = NULL; k->cnt = 0;
    return k;
}

void kwlist_add(KwList* k, const char* w) {
    for (KwNode* cur = k->head; cur; cur = cur->next)
        if (!strcmp(cur->word, w)) return;
    KwNode* n = (KwNode*)malloc(sizeof(KwNode));
    n->word = my_strdup(w);
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

/* ---------- 新增：操作日志队列 ---------- */
void logqueue_init(LogQueue* q) {
    q->front = q->rear = q->count = 0;
}

void logqueue_push(LogQueue* q, const char* msg) {
    if (q->count >= LOG_QUEUE_SIZE) {
        free(q->msgs[q->front]);
        q->front = (q->front + 1) % LOG_QUEUE_SIZE;
        q->count--;
    }
    q->msgs[q->rear] = my_strdup(msg);
    q->rear = (q->rear + 1) % LOG_QUEUE_SIZE;
    q->count++;
}

void logqueue_show(const LogQueue* q) {
    if (q->count == 0) {
        puts("日志为空。");
        return;
    }
    printf("\n=== 最近操作日志 (共%d条) ===\n", q->count);
    int idx = q->front;
    for (int i = 0; i < q->count; i++) {
        printf("  %s\n", q->msgs[idx]);
        idx = (idx + 1) % LOG_QUEUE_SIZE;
    }
    puts("==============================");
}

void logqueue_free(LogQueue* q) {
    while (q->count > 0) {
        free(q->msgs[q->front]);
        q->front = (q->front + 1) % LOG_QUEUE_SIZE;
        q->count--;
    }
}

/* ---------- 新增：单词共现图 ---------- */
void graph_init(Graph* g) {
    g->capacity = 100;
    g->count = 0;
    g->vertices = (Vertex*)calloc(g->capacity, sizeof(Vertex));
}

int graph_find_vertex(Graph* g, const char* word) {
    for (int i = 0; i < g->count; i++)
        if (strcmp(g->vertices[i].word, word) == 0)
            return i;
    return -1;
}

int graph_add_vertex(Graph* g, const char* word) {
    int idx = graph_find_vertex(g, word);
    if (idx != -1) return idx;
    if (g->count >= g->capacity) {
        g->capacity *= 2;
        g->vertices = (Vertex*)realloc(g->vertices, sizeof(Vertex) * g->capacity);
    }
    g->vertices[g->count].word = my_strdup(word);
    g->vertices[g->count].firstedge = NULL;
    return g->count++;
}

void graph_add_edge(Graph* g, int v1, int v2) {
    EdgeNode* e = (EdgeNode*)malloc(sizeof(EdgeNode));
    e->adjvex = v2;
    e->weight = 1;
    e->next = g->vertices[v1].firstedge;
    g->vertices[v1].firstedge = e;

    e = (EdgeNode*)malloc(sizeof(EdgeNode));
    e->adjvex = v1;
    e->weight = 1;
    e->next = g->vertices[v2].firstedge;
    g->vertices[v2].firstedge = e;
}

void graph_increase_weight(Graph* g, int v1, int v2) {
    for (EdgeNode* e = g->vertices[v1].firstedge; e; e = e->next)
        if (e->adjvex == v2) { e->weight++; return; }
    graph_add_edge(g, v1, v2);
}

typedef struct {
    char word1[256];
    char word2[256];
    int weight;
} CooccPair;

static int cmp_coocc(const void* a, const void* b) {
    return ((CooccPair*)b)->weight - ((CooccPair*)a)->weight;
}

void graph_build_from_text(Graph* g, const Text* t) {
    graph_init(g);
    if (!t || t->count == 0) return;
    for (int i = 0; i < t->count; i++) {
        char buf[1024];
        strcpy(buf, t->lines[i]);
        for (char* p = buf; *p; p++) if (ispunct(*p)) *p = ' ';
        char* words[256];
        int wn = 0;
        char* tok = strtok(buf, " \t");
        while (tok && wn < 256) {
            if (strlen(tok) > 0) words[wn++] = tok;
            tok = strtok(NULL, " \t");
        }
        for (int j = 0; j < wn - 1; j++) {
            int v1 = graph_add_vertex(g, words[j]);
            int v2 = graph_add_vertex(g, words[j+1]);
            graph_increase_weight(g, v1, v2);
        }
    }
}

void graph_show_top_coocc(Graph* g, int top_n) {
    if (g->count == 0) { puts("图为空。"); return; }
    int max_edges = g->count * 10;
    CooccPair* pairs = (CooccPair*)malloc(sizeof(CooccPair) * max_edges);
    int pcnt = 0;
    for (int v = 0; v < g->count; v++) {
        for (EdgeNode* e = g->vertices[v].firstedge; e; e = e->next) {
            if (v < e->adjvex) {
                if (pcnt >= max_edges) break;
                strcpy(pairs[pcnt].word1, g->vertices[v].word);
                strcpy(pairs[pcnt].word2, g->vertices[e->adjvex].word);
                pairs[pcnt].weight = e->weight;
                pcnt++;
            }
        }
    }
    qsort(pairs, pcnt, sizeof(CooccPair), cmp_coocc);
    printf("\n=== 单词共现图 Top %d ===\n", top_n);
    int show = pcnt < top_n ? pcnt : top_n;
    for (int i = 0; i < show; i++)
        printf("  %s -- %s  共现 %d 次\n", pairs[i].word1, pairs[i].word2, pairs[i].weight);
    puts("=========================");
    free(pairs);
}

void graph_free(Graph* g) {
    for (int i = 0; i < g->count; i++) {
        EdgeNode* e = g->vertices[i].firstedge;
        while (e) {
            EdgeNode* tmp = e;
            e = e->next;
            free(tmp);
        }
        free(g->vertices[i].word);
    }
    free(g->vertices);
}

/* ---------- 主程序 ---------- */
int main(void) {
    // 如果你希望控制台正确显示UTF-8，可以取消下面两行的注释
    // SetConsoleOutputCP(65001);
    // SetConsoleCP(65001);
    
    Text* txt = text_new();
    UndoStack undo;
    undostack_init(&undo);
    KwList* kw = kwlist_new();
    LogQueue logq;
    logqueue_init(&logq);

    char fn[1024];
    printf("请输入文件路径: ");
    if (!fgets(fn, sizeof fn, stdin)) return 1;
    fn[strcspn(fn, "\n")] = '\0';
    if (strlen(fn) == 0) {
        printf("未输入路径，退出。\n");
        text_free(txt);
        kwlist_free(kw);
        logqueue_free(&logq);
        return 1;
    }

    text_load(txt, fn);
    if (txt->count == 0) {
        printf("文件为空或加载失败，退出。\n");
        text_free(txt);
        kwlist_free(kw);
        logqueue_free(&logq);
        return 1;
    }
    text_show(txt);

    char logbuf[128];
    snprintf(logbuf, sizeof logbuf, "加载文件: %s (%d行)", fn, txt->count);
    logqueue_push(&logq, logbuf);

    char line[512];
    while (1) {
        printf("\n命令: d <行号>(删除), u(撤销), s(显示/高亮), f <词>(搜索), c(统计), w(词频), h(设置高亮关键词), p(保存到新文件), ins <行号> <内容>(插入), log(操作日志), co(共现图Top10), q(退出): ");
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
                snprintf(logbuf, sizeof logbuf, "删除第 %d 行", ln);
                logqueue_push(&logq, logbuf);
                text_show(txt);
            } else puts("请输入数字行号！");
        }
        else if (!strcmp(cmd, "u")) {
            text_undo(txt, &undo);
            snprintf(logbuf, sizeof logbuf, "撤销一次操作");
            logqueue_push(&logq, logbuf);
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
            snprintf(logbuf, sizeof logbuf, "设置高亮关键词 (%d个)", kw->cnt);
            logqueue_push(&logq, logbuf);
        }
        else if (!strcmp(cmd, "p")) {
            text_save_as(txt, fn);
            snprintf(logbuf, sizeof logbuf, "保存到新文件");
            logqueue_push(&logq, logbuf);
        }
        else if (!strcmp(cmd, "ins")) {
            int ln;
            char content[256];
            if (sscanf(line, "%*s %d %[^\n]", &ln, content) == 2) {
                if (text_insert(txt, ln, content)) {
                    snprintf(logbuf, sizeof logbuf, "插入行 %d: %s", ln, content);
                    logqueue_push(&logq, logbuf);
                    text_show(txt);
                }
            } else puts("用法: ins <行号> <内容>");
        }
        else if (!strcmp(cmd, "log")) {
            logqueue_show(&logq);
        }
        else if (!strcmp(cmd, "co")) {
            Graph coocc_graph;
            graph_build_from_text(&coocc_graph, txt);
            graph_show_top_coocc(&coocc_graph, 10);
            graph_free(&coocc_graph);
        }
        else if (!strcmp(cmd, "q")) break;
        else printf("未知命令！可用: d, u, s, f, c, w, h, p, ins, log, co, q\n");
    }

    kwlist_free(kw);
    undostack_free(&undo);
    logqueue_free(&logq);
    text_free(txt);
    return 0;
}
