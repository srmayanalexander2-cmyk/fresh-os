ENTRY(enter32)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS { . = 0x100000; .text : { *(.entry) *(.text*) } :text
.rodata : { *(.rodata*) } :text
. = ALIGN(4096); .data : { *(.data*) } :data
. = ALIGN(16); zero_from = .; .bss : { *(.bss*) *(COMMON) } :data
zero_to = .; ASSERT(zero_to < 0x1800000,"exceeds Lite RAM budget")
/DISCARD/ : { *(.note*) *(.comment*) *(.eh_frame*) } }
