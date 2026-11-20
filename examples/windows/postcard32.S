.text
.globl _start
_start:
 push $version
 call *__imp__GetVersionExA
 test %eax,%eax
 jz fail
 cmpl $22631,version+12
 jne fail
 push $product
 push $0
 push $0
 push $0
 push $10
 call *__imp__GetProductInfo
 cmpl $48,product
 jne fail
 push $0
 push $0
 push $2
 push $0
 push $0
 push $0x40000000
 push $name
 call *__imp__CreateFileA
 mov %eax,%ebx
 push $0
 push $written
 push $end-msg
 push $msg
 push %ebx
 call *__imp__WriteFile
 test %eax,%eax
 jz fail
 push %ebx
 call *__imp__CloseHandle
 test %eax,%eax
 jz fail
 push $0
 call *__imp__ExitProcess
fail: push $1
 call *__imp__ExitProcess
.data
name: .asciz "postcard.txt"
msg: .ascii "A postcard from the Windows 11 Pro compatibility profile.\n"
end:
written: .long 0
product: .long 0
version: .long 148
.space 144
