/* test1: 生成器。主栈与一条协程栈交替推进。 */
#include "co.h"

#include <stdint.h>
#include <stdio.h>

static int next(co_t *g) { return co_resume(g, 0); }

static int advanced;            /* 已产出到哪个数 */

/* 产出 n, n+1, n+2 ... 起点由 arg 给 */
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
    puts("无限序列, 取前 5 个:");
    co_t *g = co_create(nat, (void *)(intptr_t)2);
    for (int i = 0; i < 5; i++)
        printf("  %d\n", next(g));
    printf("停在 yield 上, 只推进到 %d\n\n", advanced);
    co_destroy(g);

    puts("两个生成器交替取, n 各自留在自己的栈上");
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
