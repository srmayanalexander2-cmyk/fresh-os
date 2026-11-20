/* Ordinary i386 Win32 console program; imported functions use stdcall. */
.text
.globl _start
_start:
    push $-11
    call *__imp__GetStdHandle
    push $0
    push $written
    push $end-message
    push $message
    push %eax
    call *__imp__WriteFile
    test %eax,%eax
    jz failure
    mov written,%eax
    cmp $end-message,%eax
    jne failure
    push $0
    call *__imp__ExitProcess
failure:
    push $1
    call *__imp__ExitProcess
.data
message: .ascii "Hello from a 32-bit Windows EXE.\r\n"
end:
written: .long 0
