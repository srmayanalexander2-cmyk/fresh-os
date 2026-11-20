#include "platform.h"
#include "drivers.h"
static inline u8 in(u16 p) {
    u8 v;
    __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p));
    return v;
}
static inline void out(u16 p, u8 v) { __asm__ volatile("outb %0,%1" ::"a"(v), "Nd"(p)); }
void serial(const char *s) {
    for (; *s; s++) {
        int wait = 20000;
        while (!(in(0x3fd) & 32) && --wait)
            ;
        out(0x3f8, *s);
    }
}
volatile u32 ticks;
static struct __attribute__((packed)) {
    u16 lo, cs;
    u8 zero, flags;
    u16 hi;
#ifdef EFI
    u32 high, reserved;
#endif
} idt[256];
#pragma GCC visibility push(hidden)
extern void pit_isr(void), keyboard_isr(void), mouse_isr(void);
#pragma GCC visibility pop
static void gate(int i, void (*f)(void)) {
    unsigned long a = (unsigned long)f;
    idt[i].lo = a;
    idt[i].hi = a >> 16;
    idt[i].cs = 8;
    idt[i].flags = 142;
#ifdef EFI
    idt[i].high = a >> 32;
#endif
}
static int ready(void) {
    int n = 65535;
    while ((in(0x64) & 2) && --n)
        ;
    return n;
}
static void cmd(int c) {
    if (ready())
        out(0x64, c);
}
static void data(int c) {
    if (ready())
        out(0x60, c);
}
static int recv(void) {
    int n = 65535;
    while (!(in(0x64) & 1) && --n)
        ;
    return n ? in(0x60) : -1;
}
static void aux(int c) {
    cmd(0xd4);
    data(c);
    recv();
}
int sw, sh, pitch, red_first;
volatile u32 *fb;
static int dw, dh, offx, offy, xmap[1920];
static volatile u32 *dispi_base;
static int page;
static void dispi(int reg, int val) {
    __asm__ volatile("outw %%ax,%%dx" ::"a"((u16)reg), "d"((u16)0x1ce));
    __asm__ volatile("outw %%ax,%%dx" ::"a"((u16)val), "d"((u16)0x1cf));
}
static u16 dispi_read(int reg) {
    u16 v;
    __asm__ volatile("outw %%ax,%%dx" ::"a"((u16)reg), "d"((u16)0x1ce));
    __asm__ volatile("inw %%dx,%%ax" : "=a"(v) : "d"((u16)0x1cf));
    return v;
}
/* Firmware remains the fallback. Bind only when the firmware already set the
 * matching BGR32 linear mode and two whole pages fit the reported BAR. */
int display_flip_bind(u32 bar) {
    if (bar < 0x1000000 || sw != W || sh != H || pitch != W || red_first ||
        (unsigned long)fb != bar || bar > 0xffffffffu - 8 * 1024 * 1024)
        return 0;
    u16 id = dispi_read(0);
    if ((id != 0xb0c4 && id != 0xb0c5) || dispi_read(1) != W || dispi_read(2) != H ||
        dispi_read(3) != 32 || !(dispi_read(4) & 0x40) ||
        dispi_read(10) * 65536u < 2u * 4 * W * H || dispi_read(6) != W ||
        dispi_read(7) < 2 * H)
        return 0;
    dispi_base = fb;
    dispi(8, 0);
    dispi(9, 0);
    serial("DISPI double buffer active\r\n");
    return 1;
}
const char *display_driver(void) {
    return dispi_base           ? "Bochs DISPI / native 720p page flip"
           : sw == W && sh == H ? "Firmware framebuffer / native 720p"
                                : "Firmware framebuffer / scaled viewport";
}
u32 screen[H][W];
int mx = W / 2, my = H / 2, buttons;
#define VI(o, T) (*(volatile T *)(0x5200 + (o)))
void hw_init(void) {
    out(0x3f9, 0);
    out(0x3fb, 0x80);
    out(0x3f8, 1);
    out(0x3f9, 0);
    out(0x3fb, 3);
    out(0x3fa, 0xc7);
    out(0x3fc, 0xb);
    serial("FreshOS kernel entered\r\n");
#ifndef EFI
    sw = VI(18, u16);
    sh = VI(20, u16);
    pitch = VI(16, u16) / 4;
    fb = (void *)VI(40, u32);

#endif
    dw = sw;
    dh = sw * H / W;
    if (dh > sh) {
        dh = sh;
        dw = sh * W / H;
    }
    offx = (sw - dw) / 2;
    offy = (sh - dh) / 2;
    for (int i = 0; i < dw; i++)
        xmap[i] = i * W / dw;
    cmd(0xa8);
    cmd(0x20);
    int cfg = recv();
    if (cfg >= 0) {
        cmd(0x60);
        data((cfg | 0x43) & ~0x30);
        aux(0xf6);
        aux(0xf4);
    }
    gate(32, pit_isr);
    gate(33, keyboard_isr);
    gate(44, mouse_isr);
    struct __attribute__((packed)) {
        u16 size;
        unsigned long addr;
    } ptr = {sizeof(idt) - 1, (unsigned long)idt};
    __asm__ volatile("lidt %0" ::"m"(ptr));
    out(0x20, 17);
    out(0xa0, 17);
    out(0x21, 32);
    out(0xa1, 40);
    out(0x21, 4);
    out(0xa1, 2);
    out(0x21, 1);
    out(0xa1, 1);
    out(0x21, 0xf8);
    out(0xa1, 0xef);
    out(0x43, 0x36);
    out(0x40, 19886 & 255);
    out(0x40, 19886 >> 8);
    __asm__ volatile("sti");
}
/* The backing store never contains the pointer. Restore only its old and new
 * bounds on motion. Full frames at native 720p copy contiguous scan lines. */
static void blit(int left, int top, int right, int bottom, int cursor) {
    int x0 = (clamp(left, 0, W) * dw + W - 1) / W, x1 = (clamp(right, 0, W) * dw + W - 1) / W;
    int y0 = (clamp(top, 0, H) * dh + H - 1) / H, y1 = (clamp(bottom, 0, H) * dh + H - 1) / H;
    for (int y = y0; y < y1; y++) {
        int sy = y * H / dh;
        const u32 *row = screen[sy];
        volatile u32 *d = fb + (offy + y) * pitch + offx;
        if (!cursor && dw == W && !red_first) {
            const u32 *s = row + x0;
            volatile u32 *dst = d + x0;
            unsigned long count = x1 - x0;
            __asm__ volatile("rep movsl" : "+S"(s), "+D"(dst), "+c"(count)::"memory");
            continue;
        }
        for (int x = x0; x < x1; x++) {
            int sx = xmap[x];
            u32 c = row[sx];
            if (cursor) {
                int j = sy - my, i = sx - mx;
                if (j >= 0 && j < 17 && i >= 0 && i <= j * 2 / 3 && i < 9 &&
                    (i < 3 || j < 12))
                    c = (i == 0 || i == j * 2 / 3 || j == 16 || j == 11) ? 0x154455
                                                                         : 0xf6ffff;
            }
            d[x] = red_first ? ((c & 255) << 16 | (c & 0xff00) | (c >> 16)) : c;
        }
    }
}
void present(void) {
    if (dispi_base) {
        page ^= 1;
        fb = dispi_base + page * W * H;
    }
    blit(0, 0, W, H, 0);
    if (dispi_base) {
        __sync_synchronize();
        dispi(9, page * H);
    }
}
void cursor_present(int visible, int fresh) {
    static int ox = -32, oy = -32, was;
    if (!fresh && visible == was && mx == ox && my == oy)
        return;
    if (!fresh && was)
        blit(ox, oy, ox + 11, oy + 18, 0);
    if (visible)
        blit(mx, my, mx + 11, my + 18, 1);
    ox = mx;
    oy = my;
    was = visible;
}
void poll(void) {
    static u8 packet[3];
    static int n, ext;
    for (int i = 0; i < 96; i++) {
        int s = in(0x64);
        if (!(s & 1))
            break;
        int b = in(0x60);
        if (s & 32) {
            if (!n && !(b & 8))
                continue;
            packet[n++] = b;
            if (n == 3) {
                n = 0;
                int ax, ay, b, absolute = guest_pointer(&ax, &ay, &b);
                if (absolute == 1) {
                    int old = buttons;
                    mx = clamp((ax * sw / 65535 - offx) * W / dw, 0, W - 1);
                    my = clamp((ay * sh / 65535 - offy) * H / dh, 0, H - 1);
                    buttons = b;
                    pointer(old, mx, my);
                } else if (absolute < 0 && !(packet[0] & 192)) {
                    mx = clamp(mx + packet[1] - (packet[0] & 16 ? 256 : 0), 0, W - 1);
                    my = clamp(my - packet[2] + (packet[0] & 32 ? 256 : 0), 0, H - 1);
                    int old = buttons;
                    buttons = packet[0] & 7;
                    pointer(old, mx, my);
                }
            }
        } else if (b == 224)
            ext = 1;
        else {
            key((ext ? 256 : 0) | (b & 127), !(b & 128));
            ext = 0;
        }
    }
}
static int cmos(int r) {
    out(0x70, r);
    return in(0x71);
}
static int cv(int x, int bin) { return bin ? x : (x >> 4) * 10 + (x & 15); }
void rtc(int *y, int *m, int *d, int *h, int *min) {
    int n = 2000;
    while ((cmos(10) & 128) && --n)
        ;
    int b = cmos(11), bin = b & 4, v = cmos(4);
    *min = cv(cmos(2), bin);
    *h = cv(v & 127, bin);
    if (!(b & 2))
        *h = *h % 12 + (v & 128 ? 12 : 0);
    *d = cv(cmos(7), bin);
    *m = cv(cmos(8), bin);
    *y = 2000 + cv(cmos(9), bin);
}
void halt(void) {
    serial("FreshOS halted\r\n");
    for (;;)
        __asm__ volatile("cli; hlt");
}
void beep(int hz) {
    int n = 1193180 / hz;
    out(0x43, 0xb6);
    out(0x42, n);
    out(0x42, n >> 8);
    out(0x61, in(0x61) | 3);
}
void silence(void) { out(0x61, in(0x61) & ~3); }
