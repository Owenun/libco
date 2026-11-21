/*
 * 生成器: 调用者驱动, 没有调度器。
 * 一个协程就是一个"可以暂停的循环", 它的全部状态就在自己的栈上。
 */
#include "co.h"

#include <stdio.h>

#define NP 10                   /* 取前 10 个素数 */

static int next(co_t *g) { return co_resume(g, 0); }   /* 当迭代器用 */

/* ---------------- 例 1: 一个无限序列 ---------------- */

static int pushed;              /* 上游推进到哪里, 用来证明"没算过的不算" */

static void nat(void *arg)
{
    (void)arg;
    for (int n = 2;; n++) {     /* 永不结束 */
        pushed = n;
        co_yield(n);
    }
}

/* ---------------- 例 2: 素数管道 ---------------- */

struct layer {
    co_t *up;                   /* 上游 */
    int   p;                    /* 本层滤掉的素数 */
};

static void filter(void *arg)
{
    struct layer *l = arg;

    for (;;) {
        int x = next(l->up);        /* 向上游要一个数 */
        if (x % l->p) co_yield(x);  /* 除得尽就吞掉, 除不尽就传给下游 */
    }
}

int main(void)
{
    puts("例 1: 无限自然数, 只要前 5 个");
    co_t *g = co_create(nat, NULL);
    for (int i = 0; i < 5; i++)
        printf("  %d\n", next(g));
    printf("  收手: 第 6 个从没算过 (上游只推进到 %d), 它还停在 yield 上\n\n",
           pushed);
    co_destroy(g);

    pushed = 0;

    puts("例 2: 素数管道 nat -> 滤2 -> 滤3 -> ... 每挖到一个素数就再套一层");
    static struct layer l[NP];
    static co_t *chain[NP + 1];
    int n = 0;

    co_t *head = chain[n++] = co_create(nat, NULL);
    for (int i = 0; i < NP; i++) {
        int p = next(head);                 /* 从管道头挖出一个素数 */
        printf("  第 %2d 个素数 = %d\n", i + 1, p);
        l[i].up = head;                     /* 新的一层把整条管道包住 */
        l[i].p  = p;
        head = chain[n++] = co_create(filter, &l[i]);
    }
    printf("  %d 个协程 / %d 条栈; 上游只推进到 %d, 没算过的数一个都没碰\n",
           n, n, pushed);

    for (int i = 0; i < n; i++) co_destroy(chain[i]);
    puts("  全部拆栈: 停在 yield 上的协程可以直接回收, 不需要它配合");
    return 0;
}
