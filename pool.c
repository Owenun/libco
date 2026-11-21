/*
 * 3 个 worker 轮流抢同一叠活。
 * main 只 join 1 号: 1 号收工时 main 就拿回控制权, 2、3 号还停在 yield 上。
 */
#include "sched.h"
#include <stdio.h>

static int jobs = 9;            /* 共享的活; 单线程, 所以"共享"天然安全 */

static void worker(void *arg)
{
    int id = *(int *)arg;

    while (jobs > 0) {
        printf("  worker %d 拿走 job %d\n", id, jobs);
        jobs--;
        co_yield(id);           /* 交还 CPU: 回到调度循环, 别人接着跑 */
    }
    printf("  worker %d 收工\n", id);
}

int main(void)
{
    int id[3] = {1, 2, 3};
    co_t *w[3];

    puts("3 个 worker 抢 9 个活:");
    for (int i = 0; i < 3; i++)
        w[i] = sched_spawn(worker, &id[i]);

    sched_join(w[0]);
    puts("1 号收工, main 拿回控制权 —— 此时 2、3 号还没跑完");

    sched_run();                    /* 把队列跑空, 顺手回收 */
    puts("队列空了");
    return 0;
}
