#ifndef FRESH_PLATFORM_H
#define FRESH_PLATFORM_H
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#define W 1280
#define H 720
extern volatile u32 ticks;
extern u32 screen[H][W];
extern int mx, my, buttons;
int display_flip_bind(u32 bar);
const char *display_driver(void);
void display_fit(int fill);
void cursor_present(int visible, int fresh);
void hw_init(void), present(void), serial(const char *), halt(void), key(int, int),
    pointer(int, int, int), rtc(int *, int *, int *, int *, int *);
static inline int clamp(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
#endif
