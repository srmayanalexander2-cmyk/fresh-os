/* El Torito no-emulation CD. Firmware fills the boot information table. */
.code16
.global optical
optical: ljmp $0,$start
.org 8
.long 0
image_lba: .long 0
image_len: .long 0
.org 64
start: cli
 xor %ax,%ax
 mov %ax,%ds
 mov %ax,%es
 mov %ax,%ss
 mov $0x7b00,%sp
 sti
 cld
 mov %dl,drive
 mov image_lba,%eax
#ifdef HARD_DISK
 shl $2,%eax
 add $4,%eax
#else
 inc %eax
#endif
 mov %eax,dlba
 mov image_len,%eax
#ifdef HARD_DISK
 sub $1537,%eax
 shr $9,%eax
 cmp $8192,%eax
#else
 dec %eax
 shr $11,%eax
 cmp $2048,%eax
#endif
 ja fail
 test %eax,%eax
 jz fail
 mov %ax,sectors
 in $0x92,%al
 or $2,%al
 out %al,$0x92
read: mov sectors,%ax
 cmp $16,%ax
 jbe 1f
 mov $16,%ax
1: mov %ax,chunk
 mov %ax,requested
 mov $dap,%si
 mov drive,%dl
 mov $0x42,%ah
 int $0x13
 jc fail
 cli
 lgdt gdtr
 mov %cr0,%eax
 or $1,%al
 mov %eax,%cr0
 ljmpl $8,$copy_chunk
.code32
copy_chunk: mov $16,%ax
 mov %ax,%ds
 mov %ax,%es
 mov $0x10000,%esi
 mov destination,%edi
 movzwl requested,%ecx
#ifdef HARD_DISK
 shl $7,%ecx
#else
 shl $9,%ecx
#endif
 cld
 rep movsl
 mov %edi,destination
 ljmp $24,$to_real
.code16
to_real: mov %cr0,%eax
 and $0xfffffffe,%eax
 mov %eax,%cr0
 ljmp $0,$again
again: xor %ax,%ax
 mov %ax,%ds
 mov %ax,%es
 mov %ax,%ss
 sti
 movzwl requested,%eax
 add %eax,dlba
 sub %ax,sectors
 jnz read
 mov $0x5400,%di
 movl $0x32454256,(%di)
 mov $0x4f00,%ax
 int $0x10
 cmp $0x4f,%ax
 jne fail
 mov 0x540e,%si
 mov 0x5410,%ax
 mov %ax,%fs
scan: mov %fs:(%si),%cx
 add $2,%si
 cmp $-1,%cx
 je pick
 push %si
 push %cx
 push %fs
 mov $0x5800,%di
 mov $0x4f01,%ax
 int $0x10
 pop %fs
 pop %cx
 pop %si
 cmp $0x4f,%ax
 jne scan
 mov 0x5800,%ax
 and $0x91,%ax
 cmp $0x91,%ax
 jne scan
 cmpb $32,0x5819
 jne scan
 cmpb $6,0x581b
 jne scan
 movzwl 0x5812,%eax
 cmp $640,%eax
 jb scan
 cmp $TARGET_W,%eax
 ja scan
 movzwl 0x5814,%edx
 cmp $480,%edx
 jb scan
 cmp $TARGET_H,%edx
 ja scan
 imul %edx,%eax
 cmp score,%eax
 jbe scan
 mov %eax,score
 mov %cx,mode
 jmp scan
pick: mov mode,%cx
 test %cx,%cx
 jz fail
 mov $0x5800,%di
 mov $0x4f01,%ax
 int $0x10
 mov mode,%bx
 or $0x4000,%bx
 mov $0x4f02,%ax
 int $0x10
 cmp $0x4f,%ax
 jne fail
 cli
 in $0x92,%al
 or $2,%al
 out %al,$0x92
 lgdt gdtr
 mov %cr0,%eax
 or $1,%al
 mov %eax,%cr0
 ljmpl $8,$protected
fail: mov $0xe21,%ax
 int $0x10
 cli
1: hlt
 jmp 1b
.balign 8
gdt: .quad 0,0xcf9a000000ffff,0xcf92000000ffff,0x00009a000000ffff
gdtr: .word 31
.long gdt
drive: .byte 0
mode: .word 0
score: .long 0
sectors: .word 0
requested: .word 0
destination: .long 0x100000
.balign 4
dap: .byte 16,0
chunk: .word 1
.word 0
seg: .word 0x1000
dlba: .quad 0
.code32
protected: mov $16,%ax
 mov %ax,%ds
 mov %ax,%es
 mov %ax,%ss
 mov $0x90000,%esp
 cld
 mov $0x100000,%eax
 jmp *%eax
.org 2048
