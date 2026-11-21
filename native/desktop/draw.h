#pragma once
#include "base.h"
extern int sw, sh;
int canvas(struct Display *);
void present(int);
void clip(Rect);
void clear_clip(void);
void fill(Rect, u32);
void rounded(Rect, int, u32);
void glass(Rect, int, u32, int);
void wallpaper(Rect, int);
void stroke(int, int, int, int, int, u32);
int width(const char *, int);
void label(int, int, const char *, int, u32);
void shorten(int, int, const char *, int, u32, int);
struct TextPoint {
    short row, x;
};
int flow_map(Rect, const char *, int, u32, int, int, int *, struct TextPoint *);
int flow(Rect, const char *, int, u32, int, int, int *);
void shell_art(int, Rect);
void glyph(int, int, int, int);
void cursor(int, int);
#define INK 0x183b50
#define DIM 0x47677c
#define PAPER 0xf4fbff
#define WHITE 0xffffff
