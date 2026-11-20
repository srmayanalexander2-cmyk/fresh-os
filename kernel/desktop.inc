/* Native shell materials. Geometry and hit targets share the same coordinate system.
 * Wallpaper contributes the background only; all lettering and chrome are live. */
static int inside(int, int, int, int, int, int);
static int tw(const char *s, int f) {
    int n = 0;
    for (; *s; s++)
        if ((u8)*s >= 32 && (u8)*s <= 126)
            n += fonts[f].adv[(u8)*s - 32];
    return n;
}
static void centered(int cx, int y, const char *s, int f, u32 c) {
    text(cx - tw(s, f) / 2, y, s, f, c);
}
static void glass(int x, int y, int w, int h, int r, u32 tint, int alpha) {
    for (int j = 0; j < h; j++) {
        int q = j < r ? r - j - 1 : j >= h - r ? j - h + r : 0;
        int pad = q ? r - root(r * r - q * q) : 0;
        for (int i = pad; i < w - pad; i++) {
            int sx = x + i, sy = y + j;
            if (sx < 0 || sx >= W || sy < 0 || sy >= H)
                continue;
            int edge = i - pad < w - pad - i ? i - pad : w - pad - i;
            int shine = j < 29 ? (29 - j) * 4 : 0;
            int side = edge < 18 ? (18 - edge) * 5 : 0;
            u32 color = mix(tint, WHITE, clamp(shine + side, 0, 235));
            int al = (edge < 2 || j < 2 || j == h - 1) ? 220 : alpha;
            screen[sy][sx] = mix(screen[sy][sx], color, al);
        }
    }
}
static void pill(int x, int y, int w, int h, const char *label, int primary) {
    int hot = inside(mx, my, x, y, w, h);
    gel(x, y + 2, w, h, h / 2, 0x85aaae, 0x58919f, 0x7eb2b5);
    gel(x, y, w, h, h / 2, WHITE,
        primary ? (hot ? 0x67f6b6 : 0x42e7b4) : (hot ? 0xc8ffff : 0xbbe6ee),
        primary ? 0x98ffce : 0xe1f8fb);
    centered(x + w / 2, y + (h - 19) / 2, label, 1, INK);
}
static void leaf(int x, int y, int s) { /* Two tapered leaves grow from a shared stem. */
    for (int j = 0; j < s; j++) {
        int breadth = 1 + 4 * j * (s - j) / s / 3;
        int cx = x + s - j;
        box(cx - breadth / 2, y + j, breadth, 1, mix(0xbbf365, 0x27976b, j * 255 / s));
        box(cx, y + j, 1, 1, 0xbff594);
    }
    for (int j = 0; j < s / 2; j++) {
        int w = 1 + 4 * j * (s / 2 - j) / s;
        box(x - s / 2 + j * 2 - w / 2, y + s / 2 + j, w, 1,
            mix(0x96dd55, 0x3eac74, j * 340 / s));
    }
}
static void welcome_view(void) {
    sprite(SKY, SKY_W, SKY_H, 0, 0, W, H, 0);
    glass(24, 20, 216, 34, 17, 0xdcf9ff, 170);
    leaf(43, 27, 17);
    text(69, 27, "FreshOS   /   Current", 1, INK);
    glass(328, 55, 330, 430, 78, 0xbdeef2, 210);
    glass(341, 68, 304, 401, 65, 0xd9f9f6, 80);
    gel(463, 101, 60, 60, 30, WHITE, 0x6ae1cd, 0x24afbe);
    leaf(480, 115, 29);
    centered(493, 174, "F R E S H O S     /     W O R K S P A C E", 0, 0x337781);
    centered(493, 201, "Welcome!", 3, 0x1a5262);
    centered(493, 267, "Your personal sky desktop.", 1, 0x285e67);
    pill(395, 329, 196, 43, "Use FreshOS    >", 1);
    pill(418, 389, 150, 32, "Install to disk", 0);
    centered(493, 444, EDITION " edition", 0, INK);
    glass(24, 484, 191, 32, 16, 0xe4ffff, 190);
    text(40, 492, "ENTER   Start     /     I   Install", 0, INK);
}
enum {
    DOCK_APPS,
    DOCK_FILES,
    DOCK_WRITER,
    DOCK_SETTINGS,
    DOCK_CLOCK,
    DOCK_TRAY,
    DOCK_LIGHT,
    DOCK_SOUND
};
static const struct {
    int x, y, w, h;
    const char *name;
} dock[] = {{10, 17, 59, 46, "Apps"},    {10, 68, 59, 46, "Files"},
            {10, 119, 59, 46, "Writer"}, {10, 170, 59, 46, "Settings"},
            {10, 225, 59, 41, "Clock"},  {10, 272, 59, 37, "Tray"},
            {10, 315, 59, 31, "Light"},  {10, 350, 59, 31, "Sound"}};
static int dock_at(int x, int y) {
    for (int i = 0; i < 8; i++)
        if (inside(x, y, dock[i].x, dock[i].y, dock[i].w, dock[i].h))
            return i;
    return -1;
}
static void rail(void) {
    glass(2, 5, 74, 389, 36, 0xb8f1f6, 206);
    int over = dock_at(mx, my);
    for (int i = 0; i < 8; i++) {
        int x = dock[i].x, y = dock[i].y, w = dock[i].w, h = dock[i].h;
        if (over == i || (!i && menu))
            gel(x, y, w, h, 15, WHITE, 0x9ceddc, 0xcaf8f4);
        if (i < 4) {
            if (!i) {
                gel(x + 17, y + 1, 26, 26, 13, WHITE, 0x67e5b6, MINT);
                leaf(x + 24, y + 5, 14);
            } else
                icon(x + 16, y - 1, i == 1 ? FILES : i == 2 ? WRITER : SETTINGS, 29);
            centered(x + w / 2, y + 27, dock[i].name, 0, INK);
        } else if (i == DOCK_CLOCK) {
            centered(x + w / 2, y, tim, 1, INK);
            centered(x + w / 2, y + 21, date, 0, INK);
        } else {
            gel(x + 2, y + 2, w - 4, h - 5, 13, WHITE, i == 5 ? 0x8ceac7 : 0xace6ed, ICE);
            centered(x + w / 2, y + (h - 15) / 2, dock[i].name, 0, INK);
        }
    }
}
