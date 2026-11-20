/* El Torito no-emulation entry; loader immediately precedes the kernel on CD. */
.intel_syntax noprefix
.code16
.global _start
_start: jmp 0:setup
.org 8
.long 0
image_lba: .long 0
image_bytes: .long 0
.org 64
setup: cli
 xor ax,ax
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov sp,0x7bf0
 cld
 sti
 mov [drive],dl
 mov eax,[image_lba]
 inc eax
 mov edx,[image_bytes]
 dec edx
 shr edx,11
 cmp byte ptr [0x508],0x99
 jne optical
 cmp dword ptr [0x504],0x46524553
 jne optical
 shl eax,2
 shl edx,2
 mov word ptr [block],512
optical:
 mov [lba],eax
 mov [remaining],edx
 in al,0x92
 or al,2
 and al,0xfe
 out 0x92,al
more: mov eax,[remaining]
 test eax,eax
 jz video
 cmp eax,16
 jbe chunk
 mov eax,16
chunk: mov [count],ax
 sub [remaining],eax
 mov dl,[drive]
 mov si,offset dap
 mov ah,0x42
 int 0x13
 jc fail
 movzx eax,word ptr [count]
 add [lba],eax
 mul word ptr [block]
 movzx ecx,ax
 shr ecx,2
 cli
 lgdt [gdtr]
 mov eax,cr0
 or al,1
 mov cr0,eax
 ljmp 8,copy
.code32
copy: mov ax,16
 mov ds,ax
 mov es,ax
 mov esi,0x10000
 mov edi,[dest]
 rep movsd
 mov [dest],edi
 ljmp 24,back16
.code16
back16: mov ax,32
 mov ds,ax
 mov es,ax
 mov eax,cr0
 and al,0xfe
 mov cr0,eax
 ljmp 0,real
real: xor ax,ax
 mov ds,ax
 mov es,ax
 sti
 jmp more
video:
 mov di,0x5000
 mov dword ptr [di],0x32454256
 mov ax,0x4f00
 int 0x10
 cmp ax,0x4f
 jne fail
 mov si,[0x500e]
 mov ax,[0x5010]
 mov fs,ax
scan: mov cx,fs:[si]
 add si,2
 cmp cx,0xffff
 je found
 push si
 push cx
 push fs
 mov di,0x5200
 mov ax,0x4f01
 int 0x10
 pop fs
 pop cx
 pop si
 cmp ax,0x4f
 jne scan
 mov ax,[0x5200]
 and ax,0x91
 cmp ax,0x91
 jne scan
 cmp byte ptr [0x5219],32
 jne scan
 cmp word ptr [0x521f],0x1008
 jne scan
 cmp byte ptr [0x521b],6
 jne scan
 movzx eax,word ptr [0x5212]
 cmp eax,640
 jb scan
 cmp eax,1280
 ja scan
 movzx edx,word ptr [0x5214]
 cmp edx,480
 jb scan
 cmp edx,1024
 ja scan
 /* Prefer 16:9 over taller modes, then the widest supported. */
 imul ebx,eax,9
 imul edx,16
 sub ebx,edx
 jns positive
 neg ebx
positive: imul ebx,4
 sub eax,ebx
 add eax,0x4000
 cmp eax,[score]
 jle scan
 mov [score],eax
 mov [mode],cx
 jmp scan
found: mov cx,[mode]
 test cx,cx
 jz fail
 mov di,0x5200
 mov ax,0x4f01
 int 0x10
 mov bx,[mode]
 or bx,0x4000
 mov ax,0x4f02
 int 0x10
 cmp ax,0x4f
 jne fail
 cli
 lgdt [gdtr]
 mov eax,cr0
 or al,1
 mov cr0,eax
 ljmp 8,launch
.code32
launch: mov ax,16
 mov ds,ax
 mov es,ax
 mov ss,ax
 mov fs,ax
 mov gs,ax
 mov esp,0x90000
 mov eax,0x100000
 jmp eax
.code16
fail: mov ax,3
 int 0x10
 mov si,offset msg
print: lodsb
 test al,al
 jz halt
 mov ah,0x0e
 int 0x10
 jmp print
halt: cli
 hlt
 jmp halt
msg: .asciz "FreshOS: cannot load live media or find a 32-bit VBE display."
.align 8
gdt: .quad 0,0x00cf9a000000ffff,0x00cf92000000ffff,0x00009a000000ffff,0x000092000000ffff
gdtr: .word .-gdt-1
 .long gdt
.align 4
dap: .byte 16,0
count: .word 0
 .word 0,0x1000
lba: .quad 0
dest: .long 0x100000
remaining: .long 0
score: .long -2147483648
mode: .word 0
block: .word 2048
drive: .byte 0
.org 2048
