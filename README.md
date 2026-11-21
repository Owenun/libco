# co

x86-64 有栈协程, Linux only, 无外部依赖。

一个协程 = 一条自己的栈 + 56 字节现场。切换就是存 6 个 callee-saved 寄存器加 `rsp`,
再 `ret` 跳到另一条栈上 —— 全部内容在 `swap.S` 的 15 条指令里。

它不提供并行, 也不加速计算; 它把"等"从代码里拿掉: 等待中的协程只占一条栈, 不占线程。

核心 106 行(`co.h` / `co.c` / `swap.S`)。调度不在核心里 —— 轮转和事件循环是两个可选扩展,
各自跟用到它的 demo 放在一起。

## 结构

| 路径 | 内容 |
| --- | --- |
| `co.h` `co.c` `swap.S` | 核心: `co_create` / `co_resume` / `co_yield` / `co_destroy`, `co_swap` |
| `test/test1_gen/` | 生成器。只用核心 |
| `test/test2_pool/` | `sched.c` `sched.h` + 3 个 worker 抢 9 个任务 |
| `test/test3_fetch/` | `loop.c` `loop.h` + 100 个慢请求, 串行 vs 协程 |

## Quick start

```sh
make
./test/test1_gen/test1_gen
./test/test2_pool/test2_pool
./test/test3_fetch/test3_fetch
```

`make run` 依次跑这三个。

`test1_gen`:

```
无限序列, 取前 5 个:
  2
  3
  4
  5
  6
停在 yield 上, 只推进到 6

两个生成器交替取, n 各自留在自己的栈上
  a=1   b=100
  a=2   b=101
  a=3   b=102
```

`test3_fetch`(服务端每个请求睡 50ms):

```
100 个请求, 服务端每个睡 50 ms:
  串行阻塞      5122 ms   (完成 100 个)
  单线程协程      61 ms   (完成 100 个)
```

数字随机器浮动, 量级是八十倍。它证明的是等待被重叠, 不是计算变快。

## API

```c
/* 核心 */
co_t *co_create(void (*fn)(void *), void *arg);
int   co_resume(co_t *c, int in);     /* 主栈 -> 协程, 返回 out */
int   co_yield(int out);              /* 协程 -> 主栈, 返回下次的 in */
void  co_destroy(co_t *c);            /* 仅限停在 yield 上的协程 */

/* test2_pool/sched.h */
co_t *sched_spawn(void (*fn)(void *), void *arg);
void  sched_run(void);                /* 队列跑空后回收 */
void  sched_join(co_t *c);

/* test3_fetch/loop.h */
co_t *loop_spawn(void (*fn)(void *), void *arg);
int   co_wait_fd(int fd);             /* 挂起直到 fd 可读, 返回 revents */
void  loop_run(void);
```

## 实现要点

`struct ctx` 只有 6 个 callee-saved 寄存器加 `rsp`, 因为其余状态已经在栈上: 编译器在 `call co_swap` 之前把活跃的 caller-saved 值写进了栈帧, `call` 压入了返回地址。存 `rsp` 等于存下整条栈。

`co_swap` 不保存 `rip`, 它以 `ret` 结尾, 目标地址取自栈顶。新栈栈顶被铺成 `[0][co_main]`, 首次运行和恢复因此走同一条路径, 代码里没有分支区分两者。那个 `0` 是 `co_main` 的假返回地址, 并保证入口处 `rsp % 16 == 8`。

协程结束时只置 `CO_DONE`, 不释放自己的栈 —— 切走之后 `rsp` 仍指向它。由调用者 `co_destroy`。

## 两条扩展的差别

`test2_pool/sched.c` 是轮转: 所有可运行的协程排队, 每次取队头跑一步。

`test3_fetch/loop.c` 是等待: 协程 `yield` 出的整数值被解释为它要等的 fd, 一起塞进 `poll`, 就绪的拿 `revents` 唤醒, 等待中的协程不在可运行队列里。

同一对 `co_resume` / `co_yield` 支撑两种推进方式, 是因为机制层只回答"谁握下一条指令", 不回答"下一步该谁跑"。

## 限制

- 仅 x86-64 SysV。Win64 下 `rdi` / `rsi` / `xmm6-15` 也是 callee-saved, 需加进 `struct ctx`。
- 只能在 `co_yield` 这类 call 边界切换。
- `errno` 是线程局部的, 协程之间共享。
- 单消费者, `co_yield` 恒回主栈。嵌套(协程里等另一条协程)需在 `struct co` 中记录 caller。
- 同一条协程不能既产出值又等 fd: `int` 通道在两个扩展里含义不同, 混用要加标记位。
- `loop.c` 只有 `poll`, 没有超时, 不能"睡 3 秒"。
- 单线程, 一条协程里做计算会拖住所有其他协程。
