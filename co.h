#ifndef CO_H
#define CO_H

#include <stddef.h>

/* 偏移与 swap.S 一致。rip 不在其中: co_swap 的 ret 从栈顶取。 */
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

/* 单消费者: co_yield 恒回主栈。 */
struct co {
    struct ctx ctx;
    void (*fn)(void *);
    void *arg;
    char  *stack;
    size_t stack_len;
    int    state;
    int    in, out;             /* in 由 resume 传入, out 由 yield 传出 */
};

typedef struct co co_t;

co_t *co_create(void (*fn)(void *), void *arg);
int   co_resume(co_t *c, int in);      /* -> out */
int   co_yield(int out);               /* -> 下次的 in */
void  co_destroy(co_t *c);             /* 仅限停在 yield 上的协程 */

#endif
