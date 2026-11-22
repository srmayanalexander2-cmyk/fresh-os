/* A normal static i386 executable. Read a file from the IMP resource directory. */
.text
.global _start
_start:
 /* Exercise the bounded brk contract before opening the package resource. */
 mov $45,%eax
 xor %ebx,%ebx
 int $0x80
 test %eax,%eax
 jz bad
 mov %eax,%ebp
 lea 8192(%ebp),%ebx
 mov $45,%eax
 int $0x80
 cmp %ebx,%eax
 jne bad
 movb $0x51,(%ebp)
 movb $0x5a,4096(%ebp)
 lea 4096(%ebp),%ebx
 mov $45,%eax
 int $0x80
 cmp %ebx,%eax
 jne bad
 lea 8192(%ebp),%ebx
 mov $45,%eax
 int $0x80
 cmp %ebx,%eax
 jne bad
 cmpb $0,4096(%ebp)
 jne bad
 cmpb $0x51,(%ebp)
 jne bad
 mov %ebp,%ebx
 mov $45,%eax
 int $0x80
 cmp %ebx,%eax
 jne bad
 mov $5,%eax
 mov $path,%ebx
 xor %ecx,%ecx
 int $0x80
 mov %eax,%ebx
 mov $3,%eax
 mov $buf,%ecx
 mov $100,%edx
 int $0x80
 cmp $0,%eax
 jle bad
 mov %eax,%edx
 mov $4,%eax
 mov $1,%ebx
 int $0x80
 xor %ebx,%ebx
 jmp done
bad: mov $1,%ebx
done: mov $1,%eax
 int $0x80
.section .rodata
path: .asciz "greeting.txt"
.bss
buf: .skip 100
