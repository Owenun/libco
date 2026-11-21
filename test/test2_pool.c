/* test2: 3 个 worker 抢 9 个任务; main join 全部 */
#include "sched.h"
#include <stdio.h>

static int jobs = 9;

static void worker(void *arg)
{
    int id = *(int *)arg;

    while (jobs > 0) {
        printf("  worker %d 拿走 job %d\n", id, jobs);
        jobs--;
        co_yield(id);
    }
    printf("  worker %d 收工\n", id);
}

int main(void)
{
    int id[3] = {1, 2, 3};
    co_t *w[3];

    puts("3 个 worker 抢 9 个任务:");
    for (int i = 0; i < 3; i++)
        w[i] = sched_spawn(worker, &id[i]);

    sched_join(w[0]);
    sched_join(w[1]);
    sched_join(w[2]);

    puts("队列空");
    return 0;
}
