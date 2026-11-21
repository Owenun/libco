/*
 * void co_swap(struct ctx *from, struct ctx *to);
 *   rdi = from, rsi = to
 *
 * 全部内容: 存 7 个 callee-saved 寄存器 + rsp, 换过来, 然后 ret。
 * ret 弹出栈顶当 rip —— 所以 rip 根本不用单独存,
 * 只要保证切进去时栈顶放着它该执行的地址。
 */
        .text
        .globl  co_swap
        .type   co_swap, @function
co_swap:
        movq    %rsp,   0(%rdi)
        movq    %rbx,   8(%rdi)
        movq    %rbp,  16(%rdi)
        movq    %r12,  24(%rdi)
        movq    %r13,  32(%rdi)
        movq    %r14,  40(%rdi)
        movq    %r15,  48(%rdi)

        movq    0(%rsi), %rsp
        movq    8(%rsi), %rbx
        movq   16(%rsi), %rbp
        movq   24(%rsi), %r12
        movq   32(%rsi), %r13
        movq   40(%rsi), %r14
        movq   48(%rsi), %r15
        ret
        .size   co_swap, .-co_swap

        .section .note.GNU-stack,"",@progbits
