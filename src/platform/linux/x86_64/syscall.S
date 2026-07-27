.text
.globl runes_linux_x86_64_syscall6
.type runes_linux_x86_64_syscall6, @function
runes_linux_x86_64_syscall6:
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    movq %rcx, %rdx
    movq %r8, %r10
    movq %r9, %r8
    movq 8(%rsp), %r9
    syscall
    ret
.size runes_linux_x86_64_syscall6, .-runes_linux_x86_64_syscall6

.section .note.GNU-stack,"",@progbits
