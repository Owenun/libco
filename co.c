#define _GNU_SOURCE
#include "co.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define STACK_SIZE (64 * 1024)          /* 每协程 64KB; 线程默认 8MB */

extern void co_swap(struct ctx *from, struct ctx *to);   /* swap.S */

static struct co  sched;                /* 主栈那个"调用者"的现场 */
static struct co *cur;                  /* 当前在跑的协程; NULL 表示主栈在跑 */
static struct co *qhead, *qtail;        /* 就绪队列 (只有 co_spawn 会入队) */
static int        nswap;

static void push(struct co *c)
{
    c->next = NULL;
    if (qtail) qtail->next = c; else qhead = c;
    qtail = c;
}

static struct co *pop(void)
{
    struct co *c = qhead;
    if (!c) return NULL;
    qhead = c->next;
    if (!qhead) qtail = NULL;
    return c;
}

/*
 * 每个协程的起点。
 * 第一次被切入时, co_swap 的 ret 直接跳到这儿 —— 所以它是"入口", 不是回调。
 * 用户函数返回后只标记 DONE, 不在这里 free 自己的栈: free 完 rsp 还指着那块内存,
 * 切走的那一瞬间就在踩已释放内存。回收交给"resume 我的那个"。
 */
static void co_main(void) __attribute__((noreturn));
static void co_main(void)
{
    struct co *c  = cur;
    struct co *to = c->caller ? c->caller : &sched;

    c->fn(c->arg);
    c->state = CO_DONE;
    cur = c->caller;
    nswap++;
    co_swap(&c->ctx, &to->ctx);         /* 最后一跳, 永不回来 */
    __builtin_unreachable();
}

co_t *co_create(void (*fn)(void *), void *arg)
{
    struct co *c = calloc(1, sizeof *c);
    if (!c) abort();

    size_t page = (size_t)sysconf(_SC_PAGESIZE);
    c->map_len = page + STACK_SIZE;
    c->stack = mmap(NULL, c->map_len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (c->stack == MAP_FAILED) { perror("mmap"); abort(); }

    /* 栈底一页设成不可访问: 溢出时内核直接 SIGSEGV,
     * 而不是静默写坏隔壁协程的栈 */
    mprotect(c->stack, page, PROT_NONE);

    c->fn    = fn;
    c->arg   = arg;
    c->state = CO_READY;

    /* 铺初始栈, 让 co_swap 的 ret 正好落到 co_main 上 */
    uint64_t *sp = (uint64_t *)(c->stack + c->map_len);   /* 栈顶, 向下生长 */
    *--sp = 0;                                            /* ABI: 入口处 rsp % 16 == 8 */
    *--sp = (uint64_t)(uintptr_t)co_main;
    c->ctx.rsp = sp;

    return c;          /* 不入队: 谁驱动由调用者决定 */
}

co_t *co_spawn(void (*fn)(void *), void *arg)
{
    co_t *c = co_create(fn, arg);
    push(c);
    return c;
}

/*
 * 让出 CPU, 回到"resume 我的那个"—— 可能是主栈, 也可能是另一个协程。
 * 非对称就体现在这里: yield 不能挑目标, 只能回到叫我的人。
 */
int co_yield(int out)
{
    struct co *c  = cur;
    struct co *to = c->caller ? c->caller : &sched;

    c->out = out;
    cur = c->caller;
    nswap++;
    co_swap(&c->ctx, &to->ctx);
    return c->in;                       /* 恢复时读到调用者刚写好的 in */
}

/*
 * 切入 c 并塞给它 in, 等它让出或结束, 返回它交出的 out。
 * 调用者是主栈或者另一个协程都行 —— 后者就是生成器管道的形状。
 */
int co_resume(co_t *c, int in)
{
    if (c->state != CO_READY) return c->out;

    struct co *self = cur ? cur : &sched;      /* 谁在 resume */
    c->caller = (self != &sched) ? self : NULL;
    c->in = in;
    cur = c;
    nswap++;
    co_swap(&self->ctx, &c->ctx);
    cur = (self != &sched) ? self : NULL;
    return c->out;
}

int co_run(int (*on_yield)(co_t *c, int out))
{
    struct co *c;

    while ((c = pop())) {
        co_resume(c, c->in);

        if (c->state == CO_DONE) {       /* 自然结束: 由调度者收割 */
            co_destroy(c);
            continue;
        }

        /* 它刚让出, out 是这次交出的值; 现在决定下次恢复时塞给它的 in */
        c->in = on_yield ? on_yield(c, c->out) : 0;
        push(c);                        /* round-robin: 放回队尾 */
    }
    return nswap;
}

void co_destroy(co_t *c)
{
    munmap(c->stack, c->map_len);
    free(c);
}
