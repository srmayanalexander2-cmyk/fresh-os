.text
.global native64
native64:
 cli
 cld
 lea stack_end(%rip),%rsp
 lea gdt64(%rip),%rax
 push %rax
 pushw $23
 lgdt (%rsp)
 add $10,%rsp
 pushq $8
 lea 1f(%rip),%rax
 push %rax
 lretq
1: mov $16,%eax
 mov %eax,%ds
 mov %eax,%es
 mov %eax,%ss
 call main
2: cli
 hlt
 jmp 2b
.global pit_isr,mouse_isr,keyboard_isr
pit_isr:
 push %rax
 incl ticks(%rip)
 mov $32,%al
 out %al,$32
 pop %rax
 iretq
mouse_isr:
 push %rax
 mov $32,%al
 out %al,$0xa0
 jmp eoi
keyboard_isr:
 push %rax
 mov $32,%al
eoi: out %al,$32
 pop %rax
 iretq
.section .rodata
.balign 8
gdt64: .quad 0,0x00af9b000000ffff,0x00cf93000000ffff
.bss
.balign 16
.skip 65536
stack_end:
