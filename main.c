/*
 * 生成器: 主栈和一条协程栈之间的乒乓。
 *   next(g) ──resume──▶ 协程跑到下一个 co_yield
 *           ◀──yield─── 回到 next() 之后, 把值交出来
 * 协程的循环变量留在自己的栈上, 所以它停在 yield 时, 进度就整个被保存下来了。
 */
#include "co.h"

#include <stdint.h>
#include <stdio.h>

static int next(co_t *g) { return co_resume(g, 0); }   /* 当迭代器用 */

static int advanced;            /* 已经产出到哪个数, 用来证明"没算过的不算" */

/* 源源不断产出 n, n+1, n+2...; 起点由 arg 给 */
static void nat(void *arg)
{
    int n = (int)(intptr_t)arg;

    for (;;) {
        advanced = n;
        co_yield(n);
        n++;
    }
}

int main(void)
{
    puts("一个无限序列, 只要前 5 个:");
    co_t *g = co_create(nat, (void *)(intptr_t)2);
    for (int i = 0; i < 5; i++)
        printf("  %d\n", next(g));
    printf("收手: 第 6 个从没算过 (只推进到 %d), 它还停在 yield 上\n\n", advanced);
    co_destroy(g);

    puts("两条协程交替取: 各自的 n 在自己的栈上, 互不干扰");
    co_t *a = co_create(nat, (void *)(intptr_t)1);
    co_t *b = co_create(nat, (void *)(intptr_t)100);
    for (int i = 0; i < 3; i++) {
        int x = next(a);
        int y = next(b);
        printf("  a=%-3d b=%d\n", x, y);
    }
    co_destroy(a);
    co_destroy(b);
    return 0;
}
