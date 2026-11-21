#ifdef __x86_64__
.text
.global enter64
enter64: cli
 lea top(%rip),%rsp
 lea segments(%rip),%rax
 push %rax
 pushw $23
 lgdt (%rsp)
 add $10,%rsp
 pushq $8
 lea run(%rip),%rax
 push %rax
 lretq
run: mov $16,%eax
 mov %ax,%ss
 mov %ax,%ds
 mov %ax,%es
 cld
 call desktop
1: hlt
 jmp 1b
.balign 8
segments: .quad 0,0x00af9a000000ffff,0x00cf92000000ffff
#else
.section .entry,"ax"
.global enter32
enter32: cld
 mov $zero_from,%edi
 mov $zero_to,%ecx
 sub %edi,%ecx
 xor %eax,%eax
 rep stosb
 mov $top,%esp
 call bios_display
1: hlt
 jmp 1b
#endif
.bss
.balign 16
.skip 65536
top:
.section .note.GNU-stack,"",@progbits
