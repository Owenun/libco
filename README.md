# co —— 一个有栈协程的最小实现

x86-64 / Linux / SysV ABI。核心只有一句话: **存 8 个寄存器, 换掉 rsp, 然后 ret**。

## 文件

| 文件 | 内容 |
|---|---|
| `co.h` `co.c` `swap.S` | 机制: 上下文 64 字节 / 15 条汇编 / 栈 + guard page / 就绪队列 |
| `main.c` → `gen` | 生成器: 无限序列 + 素数管道, **调用者驱动** |
| `worker.c` → `worker` | 8 个 worker 抢 100 个任务, **调度器驱动** |
| `guard.c` → `guard` | 故意溢出 64KB 栈, 看 guard page 生效 |

## 接口

```c
co_t *co_create(void (*fn)(void *), void *arg);  /* 只在栈上造现场, 不入队 */
co_t *co_spawn (void (*fn)(void *), void *arg);  /* create + 入队, 给 co_run */
int   co_yield (int out);                        /* 让出, 回到 resume 我的那个 */
int   co_resume(co_t *c, int in);                /* 切入一步, 拿回它交出的值 */
int   co_run   (int (*on_yield)(co_t*, int));    /* round-robin 跑完并收割 */
void  co_destroy(co_t *c);                       /* 拆栈 (只能对停在 yield 的协程) */
```

## 运行

```sh
make          # 编出 gen / worker / guard
./gen         # 生成器管道: 取前 10 个素数, 上游只推进到 29
./worker      # round-robin: 100 个任务, 108 次 yield, 232 次 co_swap
./guard       # 期待 SIGSEGV (exit 139)
```

## 关键点

- **rip 不用存**: `co_swap` 最后的 `ret` 从栈顶弹出目标地址。于是"新协程第一次运行"和"第 n 次恢复"走同一条路径, 只是新栈被人工铺成 `[对齐填充][co_main 地址]`; 那个 8 字节填充还兼任 `co_main` 的假返回地址槽。
- **栈顶对齐**: 入口处必须 `rsp % 16 == 8`; 少写那 8 字节填充, `co_main` 里一条 SSE 指令就会崩。
- **`yield` 回到"resume 我的那个"**, 不是"回到调度器"。调用者可能是主栈, 也可能是另一个协程 —— 后者就是生成器管道。非对称的真正含义是: `yield` 不能挑目标。
- **不自己回收**: 协程结束时只置 `DONE`, 由调用者 `munmap`。自己 `free` 自己的栈, 会在"切走的那一瞬间"踩已释放内存。
- **`in` 在让出之后才写**: 让出时交 `out`, 调用者看到 `out` 后才决定下次塞什么 `in`。写成"切入前才决定"会让 `in` 被覆盖, 静默丢数据。
- **生成器**: 无限序列 + 按需推进, 状态全在栈上。消费者收手就 `co_destroy`, 停在 yield 的协程可以直接拆栈 —— 这就是"无限"可以安全使用的原因。
- **代价**: 同一线程上的协程, 一个阻塞就全停。所以协程必须配非阻塞 IO + 事件循环。
