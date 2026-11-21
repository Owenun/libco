/*
 * void co_swap(struct ctx *from, struct ctx *to);   rdi = from, rsi = to
 *
 * 存 6 个 callee-saved + rsp, 载入 to, ret。
 * rip 由 ret 从栈顶取出, 不单独保存。
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
