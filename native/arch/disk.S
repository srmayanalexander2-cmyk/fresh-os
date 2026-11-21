/* Relocate before loading the larger VBE/EDD loader at 0000:7c00. */
.code16
.global disk
disk: cli
 xor %ax,%ax
 mov %ax,%ds
 mov %ax,%es
 mov %ax,%ss
 mov $0x7a00,%sp
 mov $0x7c00,%si
 mov $0x0600,%di
 mov $256,%cx
 cld
 rep movsw
 ljmp $0,$go
go: sti
 mov $dap,%si
 mov $0x42,%ah
 int $0x13
 jc fail
 ljmp $0,$0x7c00
fail: mov $0xe21,%ax
 int $0x10
 cli
1: hlt
 jmp 1b
.balign 4
dap: .byte 16,0
.word 4,0x7c00,0
.quad 1
.org 400
.ascii "STRATUS/BOOT/002"
.org 416
.long 0 /* sectors in system area, filled by media builder */
.org 510
.word 0xaa55
