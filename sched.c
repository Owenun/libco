/*
 * 朴素 round-robin 调度: 一条环形链表, 每次取队头跑一步, 没完就回队尾。
 * 它不做别的判断 —— 队列空就是没人要跑了。
 */
#include "sched.h"
#include <stdlib.h>

struct task { co_t *co; struct task *next; };

static struct task *tail;       /* 环形队列的队尾; tail->next 是队头 */
static co_t *waited;            /* 正在 join 谁: 它完成后不由 step 回收 */

static void push(struct task *t)
{
    if (!tail) { t->next = t; tail = t; return; }
    t->next = tail->next;
    tail->next = t;
    tail = t;
}

static struct task *pop(void)
{
    struct task *h = tail->next;
    if (h == tail) tail = NULL;             /* 只剩一个 */
    else           tail->next = h->next;
    return h;
}

static void step(void)
{
    struct task *t = pop();
    co_t *c = t->co;

    if (c->state != CO_DONE) co_resume(c, 0);       /* 跑一步: 到下一个 yield 或结束 */
    if (c->state != CO_DONE) { push(t); return; }   /* 没完, 回队尾等下一轮 */

    free(t);
    if (c != waited) co_destroy(c);                 /* 等它的人负责回收它 */
}

void sched_run(void)
{
    while (tail) step();
}

void sched_join(co_t *c)
{
    waited = c;
    while (c->state != CO_DONE) step();   /* 它每让出一次, 就有别人被推进一次 */
    waited = NULL;

    co_destroy(c);      /* join 之后句柄失效, 和 libco 的 wait 一样 */
}

co_t *sched_spawn(void (*fn)(void *), void *arg)
{
    struct task *t = malloc(sizeof *t);

    t->co = co_create(fn, arg);
    push(t);
    return t->co;
}
