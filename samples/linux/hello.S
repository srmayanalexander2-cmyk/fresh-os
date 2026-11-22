/* Freestanding Linux x86-64 executable. Runs unchanged on Linux and FreshOS. */
.section .text
.global _start
_start:
 /* The compact interpreter and UEFI native path both exercise the Linux brk
  * contract before the sample writes to stdout. */
 mov $12,%eax
 xor %edi,%edi
 syscall
 test %rax,%rax
 jz bad
 mov %rax,%r12
 lea 8192(%r12),%rdi
 mov $12,%eax
 syscall
 cmp %rdi,%rax
 jne bad
 movb $0x51,(%r12)
 movb $0x5a,4096(%r12)
 lea 4096(%r12),%rdi
 mov $12,%eax
 syscall
 cmp %rdi,%rax
 jne bad
 lea 8192(%r12),%rdi
 mov $12,%eax
 syscall
 cmp %rdi,%rax
 jne bad
 cmpb $0,4096(%r12)
 jne bad
 cmpb $0x51,(%r12)
 jne bad
 mov %r12,%rdi
 mov $12,%eax
 syscall
 cmp %rdi,%rax
 jne bad
 mov $1,%eax
 mov $1,%edi
 lea hello(%rip),%rsi
 mov $end-hello,%edx
 syscall
 mov $60,%eax
 xor %edi,%edi
 syscall
bad:
 mov $60,%eax
 mov $1,%edi
 syscall
.section .rodata
hello: .ascii "Hello from a static Linux ELF!\n"
end:
