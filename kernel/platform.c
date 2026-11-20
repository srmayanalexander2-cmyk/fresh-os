#include "platform.h"
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
void present(void) {
    for (int y = 0; y < dh; y++) {
        u32 *row = screen[y * H / dh];
        volatile u32 *d = fb + (offy + y) * pitch + offx;
        for (int x = 0; x < dw; x++) {
            u32 c = row[xmap[x]];
            d[x] = red_first ? ((c & 255) << 16 | (c & 0xff00) | (c >> 16)) : c;
        }
    }
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
                if (!(packet[0] & 192)) {
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
