#ifndef SCHED_H
#define SCHED_H

#include "co.h"

/* 就绪队列 + round-robin。只调 co_resume, 不碰汇编。 */
co_t *sched_spawn(void (*fn)(void *), void *arg);
void  sched_run(void);          /* 队列跑空后回收 */
void  sched_join(co_t *c);      /* 等这一个, 期间其余照跑; 返回后 c 失效 */

#endif
