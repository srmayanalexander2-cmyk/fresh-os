.text
.altmacro
.macro stub n
.global trap\n
trap\n:
.if !(\n==8 || \n==10 || \n==11 || \n==12 || \n==13 || \n==14 || \n==17 || \n==21 || \n==29 || \n==30)
 push $0
.endif
 push $\n
 jmp fault
.endm
.set idx,0
.rept 32
stub %idx
.set idx,idx+1
.endr
fault: cli
 cld
#ifdef __x86_64__
 cmpq $2,(%rsp)
 je kernel_fault
 cmpq $8,(%rsp)
 je kernel_fault
 cmpq $18,(%rsp)
 je kernel_fault
 testb $3,24(%rsp)
 jnz user_trap
kernel_fault:
 mov %cr3,%rax
 cmp user_root(%rip),%rax
 jne 1f
 mov host_root(%rip),%rax
 mov %rax,%cr3
1: mov (%rsp),%rdi
 mov 8(%rsp),%rsi
 mov 16(%rsp),%rdx
 and $-16,%rsp
 call panic_cpu
#else
 mov %esp,%esi
 push 8(%esi)
 push 4(%esi)
 push (%esi)
 call panic_cpu
#endif
.global timer_irq
timer_irq:
#ifdef __x86_64__
 push %rax
 incl ticks(%rip)
 mov $32,%al
 out %al,$32
 pop %rax
 testb $3,8(%rsp)
 jz 1f
 pushq $0
 pushq $32
 jmp user_trap
1: iretq
#else
 push %eax
 incl ticks
 mov $32,%al
 out %al,$32
 pop %eax
 iret
#endif
.section .rodata
.global traplist
traplist:
.macro addr n
#ifdef __x86_64__
.quad trap\n
#else
.long trap\n
#endif
.endm
.set idx,0
.rept 32
addr %idx
.set idx,idx+1
.endr
.section .note.GNU-stack,"",@progbits
