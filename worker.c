/*
 * 场景: 8 个 worker 抢 100 个任务 —— 调度器驱动 (co_run 的 round-robin)。
 * 这个任务形态其实线程也合适, 放在这里是为了把"就绪队列 + 收割"跑一遍。
 */
#include "co.h"

#include <stdio.h>

#define NW  8
#define NJ  100

static int next_job;
static int rounds;

static void do_job(int job)
{
    volatile long s = 0;
    for (int i = 1; i <= job; i++) s += i;
}

/*
 * 整个函数只有一处 yield, 一次完成双向握手:
 *   带出去 (out): 已完成任务数
 *   拿回来 (in) : 下一个任务号, -1 表示收工
 * done 是栈上的局部变量, 要跨过上百次切换活下来 —— 这就是"独立栈"的意义。
 */
static void worker(void *arg)
{
    const char *name = arg;
    int done = 0, job;

    while ((job = co_yield(done)) >= 0) {
        do_job(job);
        done++;
    }
    printf("  %-2s 收工, 共完成 %d 个任务\n", name, done);
}

/* 调度策略: 看到它交出的进度, 决定下次恢复时给它哪个任务 (-1 = 收工) */
static int dispatch(co_t *c, int done)
{
    if (rounds < NW * 2)                /* 只打前两圈, 看出 round-robin */
        printf("  %-2s  yield(done=%d)  ->  resume(job=%d)\n",
               (const char *)c->arg, done, next_job);
    rounds++;
    return next_job < NJ ? next_job++ : -1;
}

int main(void)
{
    static const char *names[NW] = { "w0","w1","w2","w3","w4","w5","w6","w7" };

    printf("== %d 个 worker, %d 个任务, 每栈 64KB (合计 %d KB) ==\n",
           NW, NJ, NW * 64);

    for (int i = 0; i < NW; i++)
        co_spawn(worker, (void *)names[i]);

    int n = co_run(dispatch);
    printf("== 结束: %d 个任务, yield %d 次, co_swap %d 次 ==\n",
           NJ, rounds, n);
    return 0;
}
