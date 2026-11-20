/* The Windows x64 ABI uses RCX/RDX/R8/R9 and 32 bytes of shadow space. */
.text
.globl start
start:
    sub $40,%rsp
    xor %ecx,%ecx
    lea message(%rip),%rdx
    lea caption(%rip),%r8
    xor %r9d,%r9d
    call *__imp_MessageBoxW(%rip)
    sub $1,%eax
    mov %eax,%ecx
    call *__imp_ExitProcess(%rip)
.section .rodata
caption: .short 'F','r','e','s','h','O','S',0
message: .short 'H','e','l','l','o',' ','f','r','o','m',' ','a',' ','6','4','-','b','i','t',' ','W','i','n','d','o','w','s',' ','E','X','E','.',0
