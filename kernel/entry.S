.section .entry,"ax"
.global _start
_start:
 cld
 mov $__bss_start,%edi
 mov $__end,%ecx
 sub %edi,%ecx
 xor %eax,%eax
 rep stosb
 call main
1: cli
 hlt
 jmp 1b
.section .text
.global pit_isr,keyboard_isr,mouse_isr
pit_isr:
 push %eax
 incl ticks
 mov $0x20,%al
 out %al,$0x20
 pop %eax
 iret
mouse_isr:
 push %eax
 mov $0x20,%al
 out %al,$0xa0
 jmp eoi
keyboard_isr:
 push %eax
 mov $0x20,%al
eoi: out %al,$0x20
 pop %eax
 iret
.section .note.GNU-stack,"",@progbits
