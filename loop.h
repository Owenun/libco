#ifndef LOOP_H
#define LOOP_H

#include "co.h"

/*
 * 事件循环层: 用 poll 驱动协程。
 * 约定: 协程 yield 出的整数值就是它要等的 fd, 就绪时以 poll 的 revents 唤醒它。
 */
co_t *loop_spawn(void (*fn)(void *), void *arg);
int   co_wait_fd(int fd);       /* 协程内调用: 挂起, 返回 revents */
void  loop_run(void);           /* 主栈: 跑到没有协程可等 */

#endif
