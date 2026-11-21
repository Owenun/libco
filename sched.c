/* 环形链表做就绪队列。tail->next 是队头, tail 本身是队尾。 */
#include "sched.h"
#include <stdlib.h>

struct task { co_t *co; struct task *next; };

static struct task *tail;
static co_t *waited;            /* 正在 join 的目标, 由 join 回收 */

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
    if (h == tail) tail = NULL;
    else           tail->next = h->next;
    return h;
}

static void step(void)
{
    struct task *t = pop();
    co_t *c = t->co;

    if (c->state != CO_DONE) co_resume(c, 0);       /* 到下一个 yield 或结束 */
    if (c->state != CO_DONE) { push(t); return; }

    free(t);
    if (c != waited) co_destroy(c);
}

void sched_run(void)
{
    while (tail) step();
}

void sched_join(co_t *c)
{
    waited = c;
    while (c->state != CO_DONE) step();
    waited = NULL;

    co_destroy(c);
}

co_t *sched_spawn(void (*fn)(void *), void *arg)
{
    struct task *t = malloc(sizeof *t);

    t->co = co_create(fn, arg);
    push(t);
    return t->co;
}
