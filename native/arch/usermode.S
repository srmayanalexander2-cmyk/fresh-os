/* Long-mode user entry. CR3, stack and privilege transitions happen only here.
 * No higher-level kernel code runs with a user address space installed. */
#ifdef __x86_64__
.text
.global user_resume
user_resume:
 pushfq
 cli
 push %rbx
 push %rbp
 push %r12
 push %r13
 push %r14
 push %r15
 mov %cr3,%rax
 mov %rax,host_root(%rip)
 push %rax
 mov %rsp,ksp(%rip)
 fxsave64 host_fp(%rip)
 fxrstor64 user_fp(%rip)
 mov user_root(%rip),%rax
 mov %rax,%cr3
 pushq $0x1b
 pushq user_regs+32(%rip)
 pushq user_regs+136(%rip)
 pushq user_cs(%rip)
 pushq user_regs+128(%rip)
 mov $0x1b,%eax
 mov %ax,%ds
 mov %ax,%es
.macro reg op,r,n
.ifc \op,load
 mov user_regs+8*\n(%rip),%\r
.else
 mov %\r,user_regs+8*\n(%rip)
.endif
.endm
.macro allregs op
 reg \op,rax,0
 reg \op,rcx,1
 reg \op,rdx,2
 reg \op,rbx,3
 reg \op,rbp,5
 reg \op,rsi,6
 reg \op,rdi,7
 reg \op,r8,8
 reg \op,r9,9
 reg \op,r10,10
 reg \op,r11,11
 reg \op,r12,12
 reg \op,r13,13
 reg \op,r14,14
 reg \op,r15,15
.endm
 /* Compatibility-mode PE uses a data descriptor; 64-bit PE uses GS.base. */
 xor %eax,%eax
 cmpq $0,user_teb(%rip)
 je 1f
 cmpq $0x3b,user_cs(%rip)
 jne 1f
 mov $0x43,%eax
1: mov %ax,%fs
 mov $0xc0000101,%ecx
 xor %eax,%eax
 cmpq $0x23,user_cs(%rip)
 jne 2f
 mov user_teb(%rip),%rax
2: mov %rax,%rdx
 shr $32,%rdx
 wrmsr
 mov $0xc0000100,%ecx
 mov user_fs(%rip),%rax
 mov %rax,%rdx
 shr $32,%rdx
 wrmsr
 allregs load
 iretq
/* SYSCALL doesn't switch stacks. Construct the same tail as a CPL3 trap.
 * IA32_FMASK clears IF, TF, NT, DF and AC before this first instruction. */
.global user_syscall
user_syscall:
 mov %rsp,user_regs+32(%rip)
 lea user_irq_top(%rip),%rsp
 pushq $0x1b
 pushq user_regs+32(%rip)
 push %r11
 pushq user_cs(%rip)
 push %rcx
 pushq $0
 pushq $256
 jmp user_trap
.global user_int80
user_int80:
 pushq $0
 pushq $258
 jmp user_trap
.global user_winapi
user_winapi:
 pushq $0
 pushq $257
.global user_trap
user_trap:
 cld
 allregs save
 pop %r10   /* vector */
 add $8,%rsp
 popq user_regs+128(%rip)
 add $8,%rsp
 popq user_regs+136(%rip)
 popq user_regs+32(%rip)
 mov $0xc0000100,%ecx
 xor %eax,%eax
 xor %edx,%edx
 wrmsr
 inc %ecx
 wrmsr
 mov %ax,%fs
 mov %ax,%gs
 mov ksp(%rip),%rsp
 pop %rax
 mov %rax,%cr3
 fxsave64 user_fp(%rip)
 fxrstor64 host_fp(%rip)
 mov $16,%eax
 mov %ax,%ds
 mov %ax,%es
 mov %ax,%ss
 mov %r10d,%eax
 pop %r15
 pop %r14
 pop %r13
 pop %r12
 pop %rbp
 pop %rbx
 popfq
 ret
.bss
.balign 16
host_fp: .skip 512
ksp: .skip 8
.balign 16
.skip 16384
.global user_irq_top
user_irq_top:
#endif
.section .note.GNU-stack,"",@progbits
