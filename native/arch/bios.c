#include "base.h"
#define V16(off) (*(volatile u16 *)(0x5800 + (off)))
#define V8(off) (*(volatile u8 *)(0x5800 + (off)))
void bios_display(void) {
    struct Display d = {
        *(volatile u32 *)0x5828, V16(18), V16(20), V16(16) / 4, V8(32), V8(34), V8(36)};
    desktop(&d);
}
