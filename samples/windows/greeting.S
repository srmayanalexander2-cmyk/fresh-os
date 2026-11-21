.text
.global greeting
greeting: mov slot(%rip),%rax
 ret
.section .data
slot: .quad text
text: .asciz "Hello from a relocated application DLL."
