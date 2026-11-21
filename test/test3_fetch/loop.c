/*
 * 可运行队列 + 等待表。每轮先把能跑的都跑一步, 再 poll 全部等待中的 fd。
 * 等待中的协程不在可运行队列里 —— 这一点和 sched.c 不同, sched.c 是轮转。
 */
#include "loop.h"

#include <stdlib.h>
#include <poll.h>

struct task   { co_t *co; int in; struct task *next; };
struct waiter { co_t *co; int fd; int revents; struct waiter *next; };

static struct task   *tail;         /* 可运行队列, 环形; tail->next 是队头 */
static struct waiter *waiters;

static void push(co_t *c, int in)
{
    struct task *t = malloc(sizeof *t);

    t->co = c;
    t->in = in;
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

static void add_wait(co_t *c, int fd)
{
    struct waiter *w = malloc(sizeof *w);

    w->co  = c;
    w->fd  = fd;
    w->next = waiters;
    waiters = w;
}

static void step(void)
{
    struct task *t = pop();
    co_t *c = t->co;
    int out = co_resume(c, t->in);      /* out = 它要等的 fd */

    free(t);
    if (c->state == CO_DONE) { co_destroy(c); return; }
    add_wait(c, out);
}

static void poll_waiters(void)
{
    int n = 0, i;
    struct waiter *w;

    for (w = waiters; w; w = w->next) n++;

    struct pollfd pfd[n];
    struct waiter *ws[n];

    i = 0;
    for (w = waiters; w; w = w->next, i++) {
        ws[i] = w;
        pfd[i].fd      = w->fd;
        pfd[i].events  = POLLIN;
        pfd[i].revents = 0;
    }

    if (poll(pfd, n, -1) < 0) return;   /* 被打断就下一轮重来 */

    /* 就绪的搬回可运行队列, 用 co = NULL 标记已搬走 */
    for (i = 0; i < n; i++) {
        if (!pfd[i].revents) continue;
        push(ws[i]->co, pfd[i].revents);
        ws[i]->co = NULL;
    }

    struct waiter *keep = NULL;
    while (waiters) {
        w = waiters;
        waiters = w->next;
        if (w->co) { w->next = keep; keep = w; }
        else free(w);
    }
    waiters = keep;
}

void loop_run(void)
{
    while (tail || waiters) {
        while (tail) step();
        if (waiters) poll_waiters();
    }
}

co_t *loop_spawn(void (*fn)(void *), void *arg)
{
    co_t *c = co_create(fn, arg);

    push(c, 0);
    return c;
}

int co_wait_fd(int fd)
{
    return co_yield(fd);
}
