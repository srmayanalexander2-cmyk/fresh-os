#pragma once
#include "base.h"
static inline u8 in8(u16 p) {
    u8 v;
    __asm__ volatile("inb %w1,%0" : "=a"(v) : "Nd"(p));
    return v;
}
static inline void out8(u16 p, u8 v) { __asm__ volatile("outb %0,%w1" ::"a"(v), "Nd"(p)); }
static inline u16 in16(u16 p) {
    u16 v;
    __asm__ volatile("inw %w1,%0" : "=a"(v) : "Nd"(p));
    return v;
}
static inline void out16(u16 p, u16 v) { __asm__ volatile("outw %0,%w1" ::"a"(v), "Nd"(p)); }
extern volatile u32 ticks;
void irq_init(void);
void ps2_init(void);
void input_poll(void);
int key_event(void);
int pointer_event(int *, int *, int *);
void idle(void);
void tone(int);
void tone_freq(int hz, int vol);
