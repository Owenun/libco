# co —— 一个有栈协程的最小实现

x86-64 / Linux / SysV ABI。分两层:

- **机制层** `co.c` + `swap.S`: 四个接口, 只回答"谁握下一条指令"。
- **策略层** `sched.c`(可选): 一条就绪队列 + 轮转, 只调 `co_resume`, 不碰汇编。

## 文件

| 文件 | 内容 |
|---|---|
| `swap.S` | 15 条指令: 存 6 个 callee-saved 寄存器 + 换 rsp + ret |
| `co.h` `co.c` | 栈 / guard page / resume / yield / destroy |
| `main.c` | 生成器: 一个无限序列 + 两条协程交替取 |
| `sched.h` `sched.c` | 外壳: spawn / run / join |
| `pool.c` | 3 个 worker 抢 9 个活, main 只 join 1 号 |

## 接口

```c
/* 机制层: 方向写在函数名里, 所以可以指定目标 */
co_t *co_create(void (*fn)(void *), void *arg);  /* 铺栈, 摆好"第一次运行" */
int   co_resume(co_t *c, int in);                /* 主栈 -> 协程, 返回它交出的 out */
int   co_yield (int out);                        /* 协程 -> 主栈, 返回下次的 in */
void  co_destroy(co_t *c);                       /* 拆栈 */

/* 策略层: 谁该跑、等谁 */
co_t *sched_spawn(void (*fn)(void *), void *arg);
void  sched_run(void);                           /* 轮转到队列空 */
void  sched_join(co_t *c);                       /* 只等这一个, 期间别人照跑 */
```

## 运行

```sh
make run     # ./gen && ./pool
```

## 为什么 join 只能在外壳里

`join` 的前提是**能把 CPU 交给别人**: 我歇着, 谁接着跑, 谁回头叫醒我。
机制层没有这张网 —— 那里 CPU 只会回到调用者, 所以"等"根本无从表达。
外壳是唯一知道"还有谁可跑"的角色, 于是 `run`/`join` 只能住在它里面。
这是分层的原因, 不是省事。

## 三个必须记住的点

- **rip 不用存**。`co_swap` 最后一条是 `ret`, 它从栈顶弹目标地址。新栈栈顶被人工铺成 `[填充][co_main 地址]`, 那个 8 字节填充同时满足 `rsp % 16 == 8` 并充当假返回地址 —— 于是"第一次运行"和"第 n 次恢复"走**同一条路**, 代码里没有任何分支去区分它们。
- **只有两个变量**: `main_ctx`(主栈的现场) 和 `cur`(现在谁在跑)。`co_resume` 存主栈、换协程; `co_yield` 存协程、换主栈。两句镜像, 没有别的控制流。
- **不自己回收**。结束时只置 `DONE`, 由调用者 `co_destroy`; 自己释放自己的栈, 会在切走那一瞬间踩已释放内存。

## 生成器就是这两句话

一个协程是一个**可以暂停的循环**, 循环变量就在它自己的栈上; 消费者 = 反复 `co_resume`, 收手时 `co_destroy`, 于是"无限序列"只被算到用得到的地方。

## 和 libco 的取舍

| | libco | 这里 |
|---|---|---|
| 谁挑下一个 | 库内随机 `rand()` | 调用者点名 (机制层) / 确定性 FIFO (外壳) |
| 多少个协程 | 定长表 128, 处处检查 | 链表, 无上限, 无需检查 |
| 切换 | `setjmp/longjmp` + `stack_switch_call`, 散在三处 | 只有 `co_swap` 一个出口 |
| 栈 | 内嵌 64KB, 手动对齐 | `mmap` + `PROT_NONE` 保护页 |
| 让出 | 一个 `co_yield` 同时"让出 + 挑人 + 恢复" | `resume`/`yield` 拆开, 方向显式 |
| 等待 | `co_wait`, 单 `waiter` 字段 | 外壳 `join`, 等不到的人继续跑 |

## 一个已知的简化

只有一个消费者(主栈), 所以 `co_yield` 总是回到主栈 —— 这是"非对称 + 单消费者"的退化形态。
要支持"协程里再 resume 另一个协程"(管道、嵌套), 需要给每个协程记一笔"谁叫的我", `co_yield` 才能回到上一层而不是主栈。
