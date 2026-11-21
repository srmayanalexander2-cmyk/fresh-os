#pragma once
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uintptr_t ptr;
/* BIOS and GOP publish the same channel shifts, stride and actual dimensions. */
struct Display {
    ptr base;
    u32 w, h, stride, r, g, b;
};
struct Rect {
    int x, y, w, h;
};
typedef struct Rect Rect;
static inline int low(int a, int b) { return a < b ? a : b; }
static inline int high(int a, int b) { return a > b ? a : b; }
static inline int bound(int v, int a, int b) { return high(a, low(v, b)); }
static inline int hit(Rect r, int x, int y) {
    return x >= r.x && y >= r.y && x - r.x < r.w && y - r.y < r.h;
}
static inline Rect rect(int x, int y, int w, int h) { return (Rect){x, y, w, h}; }
void *memcpy(void *, const void *, size_t);
void *memset(void *, int, size_t);
int equal(const char *, const char *);
int length(const char *);
void strset(char *, unsigned, const char *);
int add(char *, unsigned, char);
void decimal(char *, int);
void hex32(char *, u32);
void logline(const char *);
void uart_init(void);
void uart(const char *);
void desktop(struct Display *);
void panic_cpu(ptr vector, ptr error, ptr pc) __attribute__((noreturn));
