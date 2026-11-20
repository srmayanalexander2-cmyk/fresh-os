ENTRY(_start)
PHDRS { code PT_LOAD FLAGS(5); ram PT_LOAD FLAGS(6); }
SECTIONS {
    . = 0x100000;
    .text : { *(.entry) *(.text*) } :code
    .rodata : { *(.rodata*) } :code
    . = ALIGN(4096);
    .data : { *(.data*) } :ram
    . = ALIGN(16); __bss_start = .;
    .bss : { *(.bss*) *(COMMON) } :ram
    __end = .;
    ASSERT(__end <= 0x1000000, "kernel exceeds the 16 MiB boot budget")
    /DISCARD/ : { *(.note*) *(.eh_frame*) *(.comment*) }
}
