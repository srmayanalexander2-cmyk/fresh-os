/* Benign process-lifecycle / memory-protection fixture, static Linux ABI. */
.global _start
_start:
 mov %cs,%ax
 and $3,%eax
 cmp $3,%eax
 jne bad
 cmpq $2,(%rsp)
 jne bad
 mov 16(%rsp),%r12
 cmpb $'t',(%r12)
 je timeout
 cmpb $'f',(%r12)
 je fault
 mov $9,%eax
 xor %edi,%edi
 mov $8192,%esi
 mov $3,%edx
 mov $0x22,%r10d
 mov $-1,%r8
 xor %r9d,%r9d
 syscall
 test %rax,%rax
 js bad
 mov %rax,%r13
 movq $42,(%r13)
 mov $10,%eax
 mov %r13,%rdi
 mov $4096,%esi
 mov $1,%edx
 syscall
 test %rax,%rax
 jne bad
 cmpq $42,(%r13)
 jne bad
 cmpb $'p',(%r12)
 jne free
 movq $0,(%r13) /* deliberately exercise application read-only page */
 jmp bad
free:
 mov $11,%eax
 mov %r13,%rdi
 mov $8192,%esi
 syscall
 test %rax,%rax
 jne bad
 xor %edi,%edi
 jmp exit
bad: mov $9,%edi
exit: mov $60,%eax
 syscall
fault: ud2
timeout: pause
 jmp timeout
