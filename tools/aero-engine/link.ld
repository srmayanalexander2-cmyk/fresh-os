ENTRY(_start)
SECTIONS {
 . = 0x1000;
 .text : { *(.start) *(.text*) }
 .rodata : { *(.rodata*) *(.srodata*) }
 .data : { *(.data*) *(.sdata*) *(.got*) }
 .bss : { *(.bss*) *(.sbss*) *(COMMON) }
 ASSERT(. < 0x34000,"worker overlaps JS heap")
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
