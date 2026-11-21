/* CPU-pushed error codes and synthetic zeros share one diagnostic frame.
 * PC-relative offsets also work after EFI relocates the image. No return. */
.text
.macro fault n
fault_\n:
 .if !(\n == 8 || (\n >= 10 && \n <= 14) || \n == 17 || \n == 21 || \n == 29 || \n == 30)
 push $0
 .endif
 push $\n
 jmp fault_common
.endm
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
 fault \n
.endr
fault_common:
 cli
 cld
#ifdef __x86_64__
 mov (%rsp),%rdi
 mov 8(%rsp),%rsi
 mov 16(%rsp),%rdx
 lea fault_stack_end(%rip),%rsp
#else
 mov %esp,%edi
 mov $fault_stack_end,%esp
 sub $4,%esp
 push 8(%edi)
 push 4(%edi)
 push (%edi)
#endif
 call kernel_fault
1: hlt
 jmp 1b
.balign 4
.global fault_offsets
fault_offsets:
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
 .long fault_\n-fault_offsets
.endr
.bss
.balign 16
.skip 16384
fault_stack_end:
