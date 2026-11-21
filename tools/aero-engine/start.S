.section .start,"ax"
.global _start
_start:
 li sp,0x5fff0
 call worker
 ecall
