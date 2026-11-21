# co

x86-64 有栈协程, Linux only。机制层四个接口一份汇编, 调度与事件循环作为可选外壳。

- `co.c` / `swap.S` — `co_create` / `co_resume` / `co_yield` / `co_destroy`, 切换点只有 `co_swap`。
- `sched.c` — 就绪队列 + round-robin, 谁该跑。
- `loop.c` — `poll` 驱动, 谁在等。

## 结构

| 文件 | 内容 |
| --- | --- |
| `swap.S` | `co_swap`: 存 6 个 callee-saved + rsp, 载入目标, `ret` |
| `co.h` `co.c` | 机制层: 上下文、栈分配 (mmap + guard page)、生命周期 |
| `sched.h` `sched.c` | 轮转调度: `sched_spawn` / `sched_run` / `sched_join` |
| `loop.h` `loop.c` | 事件循环: `loop_spawn` / `co_wait_fd` / `loop_run` |
| `test/test1_gen.c` | 生成器 |
| `test/test2_pool.c` | 3 worker 抢 9 个任务, main 逐个 join |
| `test/test3_fetch.c` | 100 个慢请求, 串行 vs 单线程协程 |

## 构建

```sh
make
./test/test1_gen
./test/test2_pool
./test/test3_fetch
```

`make run` 依次运行三个 demo。

## API

```c
co_t *co_create(void (*fn)(void *), void *arg);
int   co_resume(co_t *c, int in);     /* 主栈 -> 协程, 返回 out */
int   co_yield(int out);              /* 协程 -> 主栈, 返回下次的 in */
void  co_destroy(co_t *c);            /* 仅限停在 yield 上的协程 */

co_t *sched_spawn(void (*fn)(void *), void *arg);
void  sched_run(void);                /* 队列跑空后回收 */
void  sched_join(co_t *c);

co_t *loop_spawn(void (*fn)(void *), void *arg);
int   co_wait_fd(int fd);             /* 挂起直到 fd 可读, 返回 revents */
void  loop_run(void);
```

## 实现要点

`struct ctx` 只有 6 个 callee-saved 寄存器加 `rsp`, 因为其余状态已经在栈上: 编译器在 `call co_swap` 之前把活跃的 caller-saved 值写进了栈帧, `call` 压入了返回地址。存 `rsp` 等于存下整条栈。

`co_swap` 不保存 `rip`, 它以 `ret` 结尾, 目标地址取自栈顶。新栈栈顶被铺成 `[0][co_main]`, 首次运行和恢复因此走同一条路径, 代码里没有分支区分两者。那个 `0` 是 `co_main` 的假返回地址, 并保证入口处 `rsp % 16 == 8`。

协程结束时只置 `CO_DONE`, 不释放自己的栈 —— 切走之后 `rsp` 仍指向它。由调用者 `co_destroy`。

## 事件循环层

`loop.c` 约定: 协程 `yield` 出的整数值就是它要等的 fd。事件循环把等待中的 fd 一起塞进 `poll`, 就绪的拿 `revents` 唤醒。于是协程里可以写顺序代码:

```c
int n = read(fd, buf, 8);
if (n < 0 && errno == EAGAIN) { co_wait_fd(fd); continue; }
```

`sched.c` 和 `loop.c` 是两种不同的推进方式: 前者轮转所有可运行的协程, 后者只在 fd 就绪时唤醒对应的那一个。等待中的协程不在可运行队列里, 这是两者的关键差别。

`test3_fetch` 的量: 100 个请求, 服务端每个睡 50ms, 串行 5.1s, 单线程 100 个协程 70ms 上下。它证明的是等待被重叠, 不是计算变快。

## join 为什么在机制层之外

`sched_join` 需要把 CPU 交给别人再拿回来, 而机制层里控制流只会回到调用者, 无法表达"等"。就绪队列由外壳持有, 所以等待只能发生在外壳。

## 与 libco 的差异

- 目标由调用者指定; 外壳是 FIFO, 不用随机。
- 协程数量无上限。libco 用 `MAX_CO=128` 定长表, 调用点都要判 `co_start` 返回值。
- 切换只经 `co_swap`。libco 有 `setjmp` / `longjmp` 与 `stack_switch_call` 两套, 并在 `co_yield` 里靠 `setjmp` 的返回值区分首次与恢复。
- 栈是 `mmap` 加 `PROT_NONE` 保护页。libco 内嵌 64KB 数组, 溢出静默写坏堆。
- 没有 `WAITING` 状态。libco 的死协程必须被 `co_wait` 才释放, 这里可以随时 `co_destroy`。

## 限制

- 仅 x86-64 SysV。Win64 下 `rdi` / `rsi` / `xmm6-15` 也是 callee-saved, 需加进 `struct ctx`。
- 只能在 `co_yield` 这类 call 边界切换。
- `errno` 是线程局部的, 协程之间共享。
- 单消费者, `co_yield` 恒回主栈。嵌套 (协程里等另一条协程) 需在 `struct co` 中记录 caller。
- `loop.c` 只有 `poll`, 没有超时: 协程不能"睡 3 秒", 也不能等一条不是 fd 的事件。
- 单线程, 一条协程里做计算会拖住所有其他协程。
