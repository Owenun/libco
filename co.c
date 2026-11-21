/* 切换走 swap.S 里的 co_swap, 调度策略在 sched.c。 */
#define _GNU_SOURCE
#include "co.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define STACK_SIZE (64 * 1024)

extern void co_swap(struct ctx *from, struct ctx *to);   /* swap.S */

static struct ctx main_ctx;
static struct co *cur;              /* NULL 表示主栈在跑 */

/*
 * 新栈栈顶是 [0][co_main], co_swap 的 ret 直接落到这里。
 * 它同时是 fn 的调用者: fn 返回后继续往下执行。
 */
static void co_main(void) __attribute__((noreturn));
static void co_main(void)
{
    struct co *self = cur;

    self->fn(self->arg);

    /* 不能在这里释放栈: 切走后 rsp 仍指向它。 */
    self->state = CO_DONE;
    cur = NULL;
    co_swap(&self->ctx, &main_ctx);
    __builtin_unreachable();
}

co_t *co_create(void (*fn)(void *), void *arg)
{
    struct co *c = calloc(1, sizeof *c);
    size_t page  = (size_t)sysconf(_SC_PAGESIZE);

    c->stack_len = page + STACK_SIZE;
    c->stack = mmap(NULL, c->stack_len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    /* 栈底一页不可访问, 溢出直接 SIGSEGV */
    mprotect(c->stack, page, PROT_NONE);

    c->fn    = fn;
    c->arg   = arg;
    c->state = CO_READY;

    uint64_t *sp = (uint64_t *)(c->stack + c->stack_len);
    *--sp = 0;                          /* 入口处 rsp % 16 == 8, 兼作 co_main 的假返回地址 */
    *--sp = (uint64_t)(uintptr_t)co_main;
    c->ctx.rsp = sp;

    return c;
}

int co_resume(co_t *c, int in)
{
    if (c->state != CO_READY) return c->out;

    c->in = in;
    cur = c;
    co_swap(&main_ctx, &c->ctx);
    cur = NULL;
    return c->out;
}

int co_yield(int out)
{
    struct co *self = cur;

    self->out = out;
    cur = NULL;
    co_swap(&self->ctx, &main_ctx);
    return self->in;
}

void co_destroy(co_t *c)
{
    munmap(c->stack, c->stack_len);
    free(c);
}
