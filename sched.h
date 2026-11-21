#ifndef SCHED_H
#define SCHED_H

#include "co.h"

/*
 * 可选的调度外壳: 自己维护一条就绪队列, 轮转推进。
 * 只用 co_resume / co_yield, 不碰汇编, 队列无上限, 顺序确定。
 */
co_t *sched_spawn(void (*fn)(void *), void *arg);   /* 建一个, 挂到队尾 */
void  sched_run(void);                              /* 轮转到队列空, 顺手回收 */
void  sched_join(co_t *c);                          /* 只等这一个; 期间别人照跑, 完后它被回收 */

#endif
