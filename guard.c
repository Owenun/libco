/*
 * guard page: 一个故意把 64KB 栈用穿的协程。
 * 栈底那一页被 mprotect(PROT_NONE), 于是溢出时内核直接 SIGSEGV,
 * 而不是静默写坏隔壁协程的栈。
 */
#include "co.h"

#include <stdio.h>

static void blow_stack(void *arg)
{
    volatile char buf[68 * 1024];       /* 比 64KB 的栈还大 */
    buf[0] = 1;                         /* 这一写落在 guard page 上 */
    printf("到不了这里 %d\n", buf[0]);
    (void)arg;
}

int main(void)
{
    /* 写到 stderr: 进程马上就要 SIGSEGV, stdout 的缓冲会被丢掉 */
    fputs("== guard page: 故意溢出 64KB 的栈 ==\n", stderr);

    co_spawn(blow_stack, NULL);
    co_run(NULL);
    return 0;
}
