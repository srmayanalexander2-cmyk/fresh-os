/* Relocate before reading the exact same CD loader from a USB/disk image. */
.intel_syntax noprefix
.code16
.global _start
_start: cli
 xor ax,ax
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov sp,0x7000
 cld
 mov si,0x7c00
 mov di,0x600
 mov cx,256
 rep movsw
 ljmp 0,relocated
relocated: sti
 mov dword ptr [0x504],0x46524553
 mov byte ptr [0x508],0x99
 mov si,offset dap
 mov ah,0x42
 int 0x13
 jc fail
 ljmp 0,0x7c00
fail: cli
 hlt
 jmp fail
.org 120
dap: .byte 16,0
 .word 4,0x7c00,0
.org 128
 .quad 0
