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
 /* Validate realtime, monotonic, clock resolution, gettimeofday and time. */
 sub $192,%rsp
 mov $228,%eax
 xor %edi,%edi
 mov %rsp,%rsi
 syscall
 test %rax,%rax
 jne bad
 mov (%rsp),%r13
 test %r13,%r13
 jz bad
 mov 8(%rsp),%r13
 cmp $1000000000,%r13d
 jae bad
 mov $229,%eax
 xor %edi,%edi
 mov %rsp,%rsi
 syscall
 test %rax,%rax
 jne bad
 mov (%rsp),%r13
 mov 8(%rsp),%r14
 test %r13,%r13
 jnz .Lrealtime_resolution_second
 test %r14,%r14
 jz bad
 cmp $1000000000,%r14d
 jae bad
 jmp .Lrealtime_resolution_ok
.Lrealtime_resolution_second:
 cmp $1,%r13
 jne bad
 test %r14,%r14
 jnz bad
.Lrealtime_resolution_ok:
 mov $228,%eax
 mov $1,%edi
 mov %rsp,%rsi
 syscall
 test %rax,%rax
 jne bad
 mov 8(%rsp),%r13
 cmp $1000000000,%r13d
 jae bad
 mov $229,%eax
 mov $1,%edi
 mov %rsp,%rsi
 syscall
 test %rax,%rax
 jne bad
 cmpq $0,(%rsp)
 jne bad
 mov 8(%rsp),%r13
 test %r13,%r13
 jz bad
 cmp $1000000000,%r13d
 jae bad
 mov $96,%eax
 mov %rsp,%rdi
 xor %esi,%esi
 syscall
 test %rax,%rax
 jne bad
 mov (%rsp),%r13
 test %r13,%r13
 jz bad
 mov 8(%rsp),%r13
 cmp $1000000,%r13d
 jae bad
 mov $201,%eax
 xor %edi,%edi
 syscall
 test %rax,%rax
 jz bad
 mov $228,%eax
 xor %edi,%edi
 xor %esi,%esi
 syscall
 cmp $-14,%rax
 jne bad
 /* Open a packaged resource and validate fstat/newfstatat metadata. */
 mov $257,%eax
 mov $-100,%edi
 lea greeting_path(%rip),%rsi
 xor %edx,%edx
 xor %r10d,%r10d
 syscall
 cmp $3,%rax
 jne bad
 mov $262,%eax
 mov $-100,%edi
 lea greeting_path(%rip),%rsi
 lea 32(%rsp),%rdx
 xor %r10d,%r10d
 syscall
 test %rax,%rax
 jne bad
 mov 56(%rsp),%r13d
 and $0170000,%r13d
 cmp $0100000,%r13d
 jne bad
 mov 80(%rsp),%r15
 test %r15,%r15
 jz bad
 cmp $16384,%r15
 ja bad
 mov $5,%eax
 mov $3,%edi
 lea 32(%rsp),%rsi
 syscall
 test %rax,%rax
 jne bad
 mov 56(%rsp),%r13d
 and $0170000,%r13d
 cmp $0100000,%r13d
 jne bad
 cmp %r15,80(%rsp)
 jne bad
 mov $5,%eax
 mov $3,%edi
 xor %esi,%esi
 syscall
 cmp $-14,%rax
 jne bad
 mov $262,%eax
 mov $-100,%edi
 xor %esi,%esi
 lea 32(%rsp),%rdx
 xor %r10d,%r10d
 syscall
 cmp $-14,%rax
 jne bad
 add $192,%rsp
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
greeting_path: .asciz "greeting.txt"
