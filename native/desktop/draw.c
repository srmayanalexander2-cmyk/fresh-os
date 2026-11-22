#include "draw.h"
#include "atlas.h"
int sw, sh;
int gfx_fast;
int text_sel_lo = -1, text_sel_hi = -1;
static struct Display target;
static u32 buffer[TARGET_W * TARGET_H];
static Rect limits;
static int sky_column[TARGET_W];
int canvas(struct Display *d) {
    if (!d->base || d->w < 640 || d->h < 480 || d->w > TARGET_W || d->h > TARGET_H ||
        d->stride < d->w || d->stride > 8192 || d->r > 24 || d->g > 24 || d->b > 24)
        return 0;
    target = *d;
    sw = d->w;
    sh = d->h;
    for (int i = 0; i < sw; i++)
        sky_column[i] = i * SKY_W / sw;
    clear_clip();
    return 1;
}
static Rect intersect(Rect a, Rect b) {
    int x = high(a.x, b.x), y = high(a.y, b.y);
    return rect(x, y, high(0, low(a.x + a.w, b.x + b.w) - x),
                high(0, low(a.y + a.h, b.y + b.h) - y));
}
void clip(Rect a) { limits = intersect(a, rect(0, 0, sw, sh)); }
void clear_clip(void) { clip(rect(0, 0, sw, sh)); }
static u32 blend(u32 a, u32 b, int alpha) {
    int inv = 256 - alpha;
    return ((((a & 0xff00ff) * inv + (b & 0xff00ff) * alpha) >> 8) & 0xff00ff) |
           ((((a & 0xff00) * inv + (b & 0xff00) * alpha) >> 8) & 0xff00);
}
void fill(Rect r, u32 c) {
    r = intersect(r, limits);
    for (int y = r.y; y < r.y + r.h; y++) {
        u32 *p = buffer + y * sw + r.x;
        for (int x = 0; x < r.w; x++)
            p[x] = c;
    }
}
/* Fast path for callers that already know the rect is inside the clip. */
void rect_direct(Rect r, u32 c) {
    r = intersect(r, rect(0, 0, sw, sh));
    for (int y = r.y; y < r.y + r.h; y++) {
        u32 *p = buffer + y * sw + r.x;
        for (int x = 0; x < r.w; x++)
            p[x] = c;
    }
}
static int inset(int y, int h, int radius) {
    int j = low(y, h - 1 - y);
    if (j >= radius)
        return 0;
    int d = radius - j - 1, x = radius;
    while (x * x + d * d > radius * radius)
        x--;
    return radius - x;
}
void rounded(Rect r, int z, u32 c) {
    z = low(z, low(r.w, r.h) / 2);
    for (int y = 0; y < r.h; y++) {
        int dx = inset(y, r.h, z);
        fill(rect(r.x + dx, r.y + y, r.w - 2 * dx, 1), c);
    }
}
void glass(Rect r, int z, u32 color, int opacity) {
    if (gfx_fast) {
        rounded(r, z, color);
        return;
    }
    z = low(z, low(r.w, r.h) / 2);
    for (int y = 0; y < r.h; y++) {
        int d = inset(y, r.h, z);
        Rect line = intersect(rect(r.x + d, r.y + y, r.w - 2 * d, 1), limits);
        if (!line.h)
            continue;
        u32 tint = blend(color, WHITE, 80 * (r.h - y) / high(1, r.h));
        for (int x = line.x; x < line.x + line.w; x++) {
            u32 *p = buffer + line.y * sw + x;
            *p = blend(*p, tint, opacity);
        }
    }
}
extern const u8 art[] __attribute__((visibility("hidden")));
void wallpaper(Rect r, int kind) {
    const u16 *p = (const u16 *)(kind == 3 ? art + WELCOME_OFF : art);
    Rect visible = intersect(r, limits);
    for (int y = visible.y; y < visible.y + visible.h; y++) {
        if (kind == 1 || kind == 2) {
            u32 a = kind == 1 ? 0x17646f : 0x3c456f, b = kind == 1 ? 0xa3e2cf : 0xf0c5bc;
            fill(rect(visible.x, y, visible.w, 1), blend(a, b, (y - r.y) * 255 / r.h));
            continue;
        }
        int base = (y - r.y) * (kind == 3 ? WELCOME_H : SKY_H) / r.h *
                   (kind == 3 ? WELCOME_W : SKY_W);
        for (int x = visible.x; x < visible.x + visible.w; x++) {
            int sx = kind == 3   ? (x - r.x) * WELCOME_W / r.w
                     : r.w == sw ? sky_column[x]
                                 : (x - r.x) * SKY_W / r.w;
            u32 c = p[base + sx];
            buffer[y * sw + x] = ((c & 0xf800) << 8) | ((c & 0x7e0) << 5) | ((c & 31) << 3);
        }
    }
}
void stroke(int x, int y, int xx, int yy, int w, u32 color) {
    int steps = high(high(xx - x, x - xx), high(yy - y, y - yy));
    for (int i = 0; i <= steps; i++)
        fill(rect(x + (xx - x) * i / high(1, steps) - w / 2,
                  y + (yy - y) * i / high(1, steps) - w / 2, w, w),
             color);
}
static int advance(int c, int f) { return advances[f][bound(c, 32, 126) - 32]; }
int width(const char *s, int f) {
    int v = 0;
    while (*s)
        v += advance((u8)*s++, f);
    return v;
}
static void letter(int x, int y, int c, int f, u32 ink) {
    c = bound(c, 32, 126) - 32;
    int w = font_w[f], h = font_h[f];
    const u8 *src = art + font_off[f] + c * w * h;
    Rect r = intersect(rect(x, y, w, h), limits);
    for (int j = r.y; j < r.y + r.h; j++)
        for (int i = r.x; i < r.x + r.w; i++) {
            int a = src[(j - y) * w + i - x];
            if (a) {
                u32 *p = buffer + j * sw + i;
                *p = blend(*p, ink, a + 1);
            }
        }
}
void label(int x, int y, const char *s, int f, u32 c) {
    while (*s) {
        letter(x, y, (u8)*s, f, c);
        x += advance((u8)*s++, f);
    }
}
void shorten(int x, int y, const char *s, int f, u32 c, int max) {
    if (width(s, f) <= max) {
        label(x, y, s, f, c);
        return;
    }
    int end = x + max - width("...", f);
    while (*s && x + advance((u8)*s, f) <= end) {
        letter(x, y, (u8)*s, f, c);
        x += advance((u8)*s++, f);
    }
    label(x, y, "...", f, c);
}
/* Identical measurement for viewport paint and caret tracking. skip counts visual lines. */
int flow_map(Rect r, const char *s, int f, u32 color, int skip, int caret, int *caretline,
             struct TextPoint *points) {
    int x = 0, row = 0, step = font_h[f] + 3;
    Rect old = limits;
    limits = intersect(limits, r);
    for (int i = 0;; i++) {
        int ch = (u8)s[i], adv = advance(ch, f);
        int needed = adv;
        if (ch > ' ' && i > 0 && s[i - 1] == ' ') {
            int word = adv;
            for (int j = i + 1; s[j] > ' ' && word <= r.w; j++)
                word += advance(s[j], f);
            if (word <= r.w)
                needed = word;
        }
        if (ch && ch != '\n' && x && x + needed > r.w) {
            row++;
            x = 0;
        }
        if (points)
            points[i] = (struct TextPoint){row, x};
        int y = r.y + (row - skip) * step;
        if (i == caret) {
            if (caretline)
                *caretline = row;
            fill(rect(r.x + x, y + 2, 2, step - 6), 0x088e88);
        }
        if (!ch)
            break;
        if (ch == '\n') {
            row++;
            x = 0;
            continue;
        }
        if (y < r.y + r.h && y + step > r.y) {
            if (text_sel_lo >= 0 && i >= text_sel_lo && i < text_sel_hi)
                fill(rect(r.x + x, y, adv, step), 0xbfe3f2);
            letter(r.x + x, y, ch, f, color);
        }
        x += adv;
    }
    limits = old;
    return row + 1;
}
int flow(Rect r, const char *s, int f, u32 color, int skip, int caret, int *line) {
    return flow_map(r, s, f, color, skip, caret, line, 0);
}
void glyph(int id, int x, int y, int size) {
    const u8 *src = art + ICON_OFFSET + id * 32 * 32 * 4;
    for (int j = 0; j < size; j++)
        for (int i = 0; i < size; i++) {
            if (!hit(limits, x + i, y + j))
                continue;
            const u8 *q = src + 4 * (j * 32 / size * 32 + i * 32 / size);
            if (q[3]) {
                u32 *p = buffer + (y + j) * sw + x + i;
                *p = blend(*p, (q[0] << 16) | (q[1] << 8) | q[2], q[3] + 1);
            }
        }
}
void shell_art(int id, Rect r) {
    const u8 *src = art + shell_off[id];
    Rect b = intersect(r, limits);
    for (int y = b.y; y < b.y + b.h; y++)
        for (int x = b.x; x < b.x + b.w; x++) {
            const u8 *p = src + 4 * ((y - r.y) * shell_h[id] / r.h * shell_w[id] +
                                     (x - r.x) * shell_w[id] / r.w);
            if (p[3])
                buffer[y * sw + x] = (p[0] << 16) | (p[1] << 8) | p[2];
        }
}
static int brightness = 100, cx = -20, cy = -20;
static u32 native_color(u32 c) {
    if (brightness < 100)
        c = blend(0, c, brightness * 256 / 100);
    return ((c >> 16 & 255) << target.r) | ((c >> 8 & 255) << target.g) |
           ((c & 255) << target.b);
}
void cursor(int x, int y) {
    volatile u32 *fb = (void *)target.base;
    for (int j = high(0, cy); j < low(sh, cy + 19); j++)
        for (int i = high(0, cx); i < low(sw, cx + 11); i++)
            fb[j * target.stride + i] = native_color(buffer[j * sw + i]);
    cx = x;
    cy = y;
    for (int j = 0; j < 11 && y + j < sh; j++)
        for (int i = 0; i < 11 && x + i < sw; i++) {
            int dx = i - 5, dy = j - 5;
            if (dx * dx + dy * dy <= 25)
                fb[(y + j) * target.stride + x + i] =
                    native_color(i == 0 || i == 10 ? 0x60e5ff
                                 : i < 5           ? 0xb6f5ff
                                 : i < 8           ? 0x13cfff
                                                   : WHITE);
        }
}
void present(int light) {
    brightness = light;
    volatile u32 *fb = (void *)target.base;
    int direct = target.r == 16 && target.g == 8 && target.b == 0 && light == 100;
    if (direct && target.stride == (u32)sw) {
        memcpy((void *)fb, buffer, (size_t)sw * sh * 4);
        return;
    }
    for (int y = 0; y < sh; y++) {
        volatile u32 *dst = fb + y * target.stride;
        u32 *src = buffer + y * sw;
        if (direct) {
            for (int x = 0; x < sw; x++)
                dst[x] = src[x];
        } else
            for (int x = 0; x < sw; x++)
                dst[x] = native_color(src[x]);
    }
}
