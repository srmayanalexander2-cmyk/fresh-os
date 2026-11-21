/* PE32 fastcall GuardCF check. GNU ld does not populate the load-config
 * directory; qa/guard32.py sets that directory on this QA fixture only. */
.text
.global start
start:
    cmpl $0xbb40e64e, cookie
    je bad
    cmpl $0,cookie
    je bad
    mov $target,%ecx
#ifdef REJECTED
    mov $other,%ecx
#endif
    call *check_ptr
    call *%ecx
    ret
bad: mov $13,%eax; ret
.balign 16
target: xor %eax,%eax; ret
other: mov $19,%eax; ret
.data
cookie: .long 0xbb40e64e
.section .rdata,"a"
check_ptr: .long bad
.section .cfg,"a"
.long 92
.zero 56
.long cookie,0,0,check_ptr,0,targets,1,0x500
targets: .long target-0x400000
