/*
 * 一个有栈协程的最小实现, 只服务单消费者场景: yield 恒回主栈。
 * 这里只回答"谁握下一条指令"; "谁该跑" 是策略, 放在可选的 sched.c。
 */
#define _GNU_SOURCE
#include "co.h"

#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define STACK_SIZE (64 * 1024)      /* 每条栈 64KB; 线程默认 8MB */

extern void co_swap(struct ctx *from, struct ctx *to);   /* swap.S */

static struct ctx main_ctx;         /* 主栈的现场 */
static struct co *cur;              /* 现在在跑哪个协程; NULL = 主栈在跑 */

/*
 * 每个协程的第一条指令, 同时也是 fn 的调用者:
 * 新栈栈顶被铺成 [填充][co_main 地址], 所以 co_swap 的 ret 会直接跳到这里,
 * fn 干完 return 回来就落在下面一行。
 */
static void co_main(void) __attribute__((noreturn));
static void co_main(void)
{
    struct co *self = cur;

    self->fn(self->arg);

    /* 只标记, 不在这里释放自己的栈: 释放完 rsp 还指着那块内存,
     * 切走那一瞬间就在踩已释放内存。回收交给主栈。 */
    self->state = CO_DONE;
    cur = NULL;
    co_swap(&self->ctx, &main_ctx);             /* 最后一跳, 回主栈 */
    __builtin_unreachable();
}

co_t *co_create(void (*fn)(void *), void *arg)
{
    struct co *c = calloc(1, sizeof *c);
    size_t page  = (size_t)sysconf(_SC_PAGESIZE);

    c->stack_len = page + STACK_SIZE;
    c->stack = mmap(NULL, c->stack_len, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    /* 栈底一页设成不可访问: 溢出时内核直接 SIGSEGV, 而不是静默写坏堆 */
    mprotect(c->stack, page, PROT_NONE);

    c->fn    = fn;
    c->arg   = arg;
    c->state = CO_READY;

    /* 铺初始栈: 栈顶往下放两格 */
    uint64_t *sp = (uint64_t *)(c->stack + c->stack_len);
    *--sp = 0;                                  /* 填充: 让入口处 rsp % 16 == 8, 也当假返回地址 */
    *--sp = (uint64_t)(uintptr_t)co_main;       /* co_swap 的 ret 会弹它 */
    c->ctx.rsp = sp;

    return c;
}

int co_resume(co_t *c, int in)
{
    if (c->state != CO_READY) return c->out;    /* 已经结束的, 不再进入 */

    c->in = in;
    cur = c;
    co_swap(&main_ctx, &c->ctx);    /* 存主栈, 换上它的 */
    cur = NULL;
    return c->out;                  /* 它这次交出的值 */
}

int co_yield(int out)
{
    struct co *self = cur;

    self->out = out;
    cur = NULL;
    co_swap(&self->ctx, &main_ctx); /* 存它的, 换回主栈 */
    return self->in;                /* 下次被叫醒, 在这里收下新的 in */
}

void co_destroy(co_t *c)
{
    munmap(c->stack, c->stack_len);
    free(c);
}
