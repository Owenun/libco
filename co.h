#ifndef CO_H
#define CO_H

#include <stddef.h>

/*
 * 一份现场 = 6 个 callee-saved 寄存器 + rsp, 顺序必须与 swap.S 的偏移一致。
 * rip 不在里面: co_swap 最后那条 ret 会从栈上把它弹出来。
 */
struct ctx {
    void *rsp;                  /*  0 */
    void *rbx;                  /*  8 */
    void *rbp;                  /* 16 */
    void *r12;                  /* 24 */
    void *r13;                  /* 32 */
    void *r14;                  /* 40 */
    void *r15;                  /* 48 */
};

enum { CO_READY, CO_DONE };

/* 一个协程 = 一份现场 + 一条自己的栈 */
struct co {
    struct ctx ctx;             /* 停在哪 */
    void (*fn)(void *);         /* 做什么 */
    void *arg;
    char  *stack;
    size_t stack_len;
    int    state;
    int    in, out;             /* out: 让出时交出去; in: 恢复时收回来 */
};

typedef struct co co_t;

co_t *co_create(void (*fn)(void *), void *arg);  /* 分栈, 把"第一次运行"摆在 co_main 上 */
int   co_resume(co_t *c, int in);                /* 主栈 -> 协程, 返回它交出的 out */
int   co_yield(int out);                         /* 协程 -> 主栈, 返回下次的 in */
void  co_destroy(co_t *c);                       /* 只能拆停在 yield 上的协程 */

#endif
