#include "platform.h"
#include "art.h"
#define INK 0x154156
#define MINT 0x16db9c
#define ICE 0xeafdff
#define WHITE 0xffffff
static inline u32 mix(u32 a, u32 b, int t) {
    unsigned k = 256 - t;
    return ((((a & 0xff00ff) * k + (b & 0xff00ff) * t) >> 8) & 0xff00ff) |
           ((((a & 0xff00) * k + (b & 0xff00) * t) >> 8) & 0xff00);
}
static void box(int x, int y, int w, int h, u32 c) {
    int x1 = clamp(x + w, 0, W), y1 = clamp(y + h, 0, H);
    for (int j = clamp(y, 0, H); j < y1; j++)
        for (int i = clamp(x, 0, W); i < x1; i++)
            screen[j][i] = c;
}
static int root(int v) {
    int n = 0;
    for (int bit = 1 << 14; bit; bit >>= 2) {
        if (v >= n + bit) {
            v -= n + bit;
            n = (n >> 1) + bit;
        } else
            n >>= 1;
    }
    return n;
}
static void gel(int x, int y, int w, int h, int r, u32 top, u32 mid, u32 low) {
    r = clamp(r, 0, (w < h ? w : h) / 2);
    for (int i = 0; i < h; i++) {
        int q = i < r ? r - i - 1 : i >= h - r ? i - (h - r) : 0;
        int pad = q ? r - root(r * r - q * q) : 0;
        int t = i * 256 / h;
        box(x + pad, y + i, w - 2 * pad, 1,
            t < 145 ? mix(top, mid, t * 256 / 145) : mix(mid, low, (t - 145) * 256 / 111));
    }
}
static inline u32 rgb(u16 p) {
    return ((p & 0xf800) << 8) | ((p & 0x7e0) << 5) | ((p & 31) << 3);
}
static void sprite(const u16 *p, int sw, int sh, int x, int y, int w, int h, int mask) {
    if (w <= 0 || h <= 0)
        return;
    int xm[W];
    for (int i = clamp(-x, 0, w); i < w && x + i < W; i++)
        xm[x + i] = i * sw / w;
    for (int j = clamp(-y, 0, h); j < h && y + j < H; j++) {
        const u16 *row = p + j * sh / h * sw;
        for (int i = clamp(x, 0, W); i < x + w && i < W; i++) {
            u16 c = row[xm[i]];
            if (!mask || c != 0xf81f)
                screen[y + j][i] = rgb(c);
        }
    }
}
struct Font {
    int w, h;
    const u8 *adv, *bitmap;
};
static const struct Font fonts[] = {small_font, body_font, head_font};
static int text(int x, int y, const char *s, int f, u32 color) {
    const struct Font *z = fonts + f;
    for (; *s; s++) {
        int n = (u8)*s - 32;
        if (n < 0 || n > 94)
            continue;
        const u8 *p = z->bitmap + n * z->w;
        for (int j = 0; j < z->h; j++) {
            if (y + j >= 0 && y + j < H)
                for (int i = 0; i < z->w; i++) {
                    int a = p[i];
                    if (a && x + i >= 0 && x + i < W)
                        screen[y + j][x + i] = mix(screen[y + j][x + i], color, a);
                }
            p += z->w * 95;
        }
        x += z->adv[n];
    }
    return x;
}
static void badge(int x, int y, const char *s, int on) {
    gel(x, y, 114, 31, 15, WHITE, on ? 0x42f3a4 : 0xa9eaf2, on ? 0x99ffd0 : 0xdff8fc);
    text(x + 13, y + 5, s, 1, INK);
}
