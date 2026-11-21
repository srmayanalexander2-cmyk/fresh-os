.text
.global start
#ifdef _WIN64
#define P .quad
#define TLSMOV mov %gs:0x58,%rax; mov (%rax),%rax
start: TLSMOV; cmpl $0x4c535446,(%rax); jne bad; cmpl $8,4(%rax); jne bad; xor %eax,%eax; ret
bad: mov $17,%eax; ret
cb: test %edx,%edx; jz detach; cmp $1,%edx; jne badcb; TLSMOV; cmpl $0,4(%rax); jne badcb; movl $8,4(%rax); ret
detach: sub $40,%rsp; mov $-11,%ecx; call *__imp_GetStdHandle(%rip); mov %rax,%rcx; lea msg(%rip),%rdx; mov $12,%r8d; lea count(%rip),%r9; movq $0,32(%rsp); call *__imp_WriteFile(%rip); add $40,%rsp; ret
#else
#define P .long
#define TLSMOV mov %fs:0x2c,%eax; mov (%eax),%eax
start: TLSMOV; cmpl $0x4c535446,(%eax); jne bad; cmpl $8,4(%eax); jne bad; xor %eax,%eax; ret
bad: mov $17,%eax; ret
cb: cmpl $0,8(%esp); je detach; cmpl $1,8(%esp); jne badcb; TLSMOV; cmpl $0,4(%eax); jne badcb; movl $8,4(%eax); ret $12
detach: push $-11; call *__imp__GetStdHandle; push $0; push $count; push $12; push $msg; push %eax; call *__imp__WriteFile; ret $12
#endif
badcb: ud2
.data
idx: .long -1
count: .long 0
msg: .ascii "TLS detached"
.balign 8
tmpl: .long 0x4c535446
endtmpl:
.balign 8
callbacks: P cb; P 0
.section .rdata,"a"
.globl _tls_used, __tls_used
_tls_used:
__tls_used:
P tmpl; P endtmpl; P idx; P callbacks; .long 4,0
