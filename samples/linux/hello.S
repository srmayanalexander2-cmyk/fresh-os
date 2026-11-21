/* Freestanding Linux x86-64 executable. Runs unchanged on Linux and FreshOS. */
.section .text
.global _start
_start:
 mov $1,%eax
 mov $1,%edi
 lea hello(%rip),%rsi
 mov $end-hello,%edx
 syscall
 mov $60,%eax
 xor %edi,%edi
 syscall
.section .rodata
hello: .ascii "Hello from a static Linux ELF!\n"
end:
