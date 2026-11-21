#ifndef CO_H
#define CO_H

#include <stddef.h>

/*
 * 上下文 = 8 个 64 位寄存器, 共 64 字节。
 * 字段顺序必须与 swap.S 里的偏移一致 (0, 8, 16, ... , 48)。
 * rip 不在这里 —— 它由 co_swap 最后的 ret 从栈上弹出。
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

/* 协程自己的 "task_struct" */
struct co {
    struct ctx ctx;             /* 调度现场 */
    void (*fn)(void *);
    void *arg;                  /* 起点: 函数 + 参数 */
    struct co *caller;          /* 谁 resume 了我; NULL = 主栈 */
    char  *stack;               /* mmap 基址 (最低一页是 guard page) */
    size_t map_len;
    int    state;
    int    in, out;             /* out: 让出时带出去; in: 恢复时带回来 */
    struct co *next;            /* 就绪队列 */
};

typedef struct co co_t;

/* 只铺栈和现场, 不入队。生成器 / 手动驱动用这个。 */
co_t *co_create(void (*fn)(void *), void *arg);

/* 创建 + 进就绪队列, 交给 co_run 的 round-robin 驱动。 */
co_t *co_spawn(void (*fn)(void *), void *arg);

/* 协程内: 带 out 让出 CPU, 回到"resume 我的那个"; 恢复时拿到它塞的 in */
int co_yield(int out);

/* 切入 c 并塞给它 in, 等它让出或结束, 返回它交出的 out。
 * 调用者可以是主栈, 也可以是另一个协程 (生成器管道)。 */
int co_resume(co_t *c, int in);

/* 对 co_spawn 出来的协程做 round-robin, 跑完自动回收。
 * 每次让出后回调 on_yield(c, out), 返回值就是它下次被恢复时收到的 in。
 * 返回 co_swap 被调用的总次数。 */
int co_run(int (*on_yield)(co_t *c, int out));

/* 拆栈回收。只能对停在 yield 上的协程调用 —— 它此刻不在跑, 栈可以安全 munmap。 */
void co_destroy(co_t *c);

#endif
