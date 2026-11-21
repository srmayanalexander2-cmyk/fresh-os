#include "io.h"
volatile u32 ticks;
static u32 silence_at;
void uart_init(void) {
    out8(0x3f9, 0);
    out8(0x3fb, 128);
    out8(0x3f8, 1);
    out8(0x3f9, 0);
    out8(0x3fb, 3);
    out8(0x3fa, 0xc7);
    out8(0x3fc, 3);
}
void uart(const char *s) {
    while (*s) {
        for (int i = 0; i < 20000 && !(in8(0x3fd) & 32); i++)
            ;
        out8(0x3f8, *s++);
    }
}
void logline(const char *s) {
    uart(s);
    uart("\r\n");
}
extern const ptr traplist[];
extern void timer_irq(void) __attribute__((visibility("hidden")));
struct Gate {
    u16 lo, cs;
    u8 ist, flags;
    u16 mid;
#ifdef __x86_64__
    u32 hi, pad;
#endif
} __attribute__((packed));
static struct Gate idt[256] __attribute__((aligned(16)));
static void gate(int n, ptr h) {
    idt[n] = (struct Gate){.lo = h,
                           .cs = 8,
                           .flags = 0x8e,
                           .mid = h >> 16
#ifdef __x86_64__
                           ,
                           .hi = h >> 32
#endif
    };
}
void irq_init(void) {
    for (int i = 0; i < 32; i++)
        gate(i, traplist[i]);
    gate(32, (ptr)timer_irq);
    struct {
        u16 size;
        ptr base;
    } __attribute__((packed)) desc = {sizeof idt - 1, (ptr)idt};
    __asm__ volatile("lidt %0" ::"m"(desc));
    out8(0x20, 17);
    out8(0xa0, 17);
    out8(0x21, 32);
    out8(0xa1, 40);
    out8(0x21, 4);
    out8(0xa1, 2);
    out8(0x21, 1);
    out8(0xa1, 1);
    out8(0x21, 254);
    out8(0xa1, 255);
    out8(0x43, 0x36);
    out8(0x40, 0xae);
    out8(0x40, 0x4d);
    __asm__ volatile("sti");
}
void idle(void) {
    if (silence_at && (int)(ticks - silence_at) >= 0) {
        out8(0x61, in8(0x61) & ~3);
        silence_at = 0;
    }
    __asm__ volatile("hlt");
}
void tone(int amount) {
    if (amount <= 0)
        return;
    u16 hz = 1193182 / (330 + amount * 2);
    out8(0x43, 0xb6);
    out8(0x42, hz);
    out8(0x42, hz >> 8);
    out8(0x61, in8(0x61) | 3);
    silence_at = ticks + 2;
}
static int wait_in(void) {
    for (int t = 0; t < 80000; t++)
        if (!(in8(0x64) & 2))
            return 1;
    return 0;
}
static void cmd(int v) {
    if (wait_in())
        out8(0x64, v);
}
static void data(int v) {
    if (wait_in())
        out8(0x60, v);
}
static int recv(void) {
    for (int t = 0; t < 120000; t++)
        if (in8(0x64) & 1)
            return in8(0x60);
    return -1;
}
void ps2_init(void) {
    cmd(0xad);
    cmd(0xa7);
    for (int i = 0; i < 64 && (in8(0x64) & 1); i++)
        in8(0x60);
    cmd(0x20);
    int c = recv();
    if (c < 0)
        return;
    cmd(0x60);
    data((c | 64) & ~0x33);
    cmd(0xae);
    cmd(0xa8);
    cmd(0xd4);
    data(0xf6);
    recv();
    cmd(0xd4);
    data(0xf4);
    recv();
}
/* Poll PS/2 once per tick. Single producer/single consumer, no allocation in IRQ. */
static int keys[64], kr, kw, extended, stage, mouse[3], dx, dy, buttons, updated;
void input_poll(void) {
    for (int n = 0; n < 128; n++) {
        int s = in8(0x64);
        if (!(s & 1))
            break;
        int v = in8(0x60);
        if (s & 0xc0) {
            stage = extended = 0;
            continue;
        }
        if (s & 32) {
            if (!stage && !(v & 8))
                continue;
            mouse[stage++] = v;
            if (stage == 3) {
                stage = 0;
                if (!(mouse[0] & 192)) {
                    dx += mouse[1] - (mouse[0] & 16 ? 256 : 0);
                    dy -= mouse[2] - (mouse[0] & 32 ? 256 : 0);
                }
                buttons = mouse[0] & 7;
                updated = 1;
            }
        } else if (v == 224)
            extended = 1;
        else {
            int next = (kw + 1) & 63;
            if (next != kr) {
                keys[kw] = v | (extended << 8);
                kw = next;
            }
            extended = 0;
        }
    }
}
int key_event(void) {
    if (kr == kw)
        return -1;
    int v = keys[kr];
    kr = (kr + 1) & 63;
    return v;
}
int pointer_event(int *x, int *y, int *b) {
    if (!updated)
        return 0;
    *x = dx;
    *y = dy;
    *b = buttons;
    dx = dy = updated = 0;
    return 1;
}
