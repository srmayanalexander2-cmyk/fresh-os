/* Import a function from the application-local DLL, then pass its result to a Win32 API. */
.text
.global start
start: sub $40,%rsp
 call *__imp_greeting(%rip)
 xor %ecx,%ecx
 mov %rax,%rdx
 lea caption(%rip),%r8
 xor %r9d,%r9d
 call *__imp_MessageBoxA(%rip)
 xor %ecx,%ecx
 call *__imp_ExitProcess(%rip)
.section .rodata
caption: .asciz "Stratus"
