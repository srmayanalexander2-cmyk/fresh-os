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
static int hover[32][4], nhover;
static int hovered(int x, int y) {
    for (int i = nhover - 1; i >= 0; i--)
        if (inside(x, y, hover[i][0], hover[i][1], hover[i][2], hover[i][3]))
            return i;
    return -1;
}
static void pill(int x, int y, int w, int h, const char *label, int primary) {
    int hot = inside(mx, my, x, y, w, h);
    if (nhover < 32) {
        hover[nhover][0] = x;
        hover[nhover][1] = y;
        hover[nhover][2] = w;
        hover[nhover++][3] = h;
    }
    gel(x, y + 2, w, h, h / 2, 0x85aaae, 0x58919f, 0x7eb2b5);
    gel(x, y, w, h, h / 2, WHITE,
        primary ? (hot ? 0x67f6b6 : 0x42e7b4) : (hot ? 0xc8ffff : 0xbbe6ee),
        primary ? 0x98ffce : 0xe1f8fb);
    centered(x + w / 2, y + (h - 19) / 2, label, 1, INK);
}
/* Hit geometry below is shared with painting and mouse routing. */
#define CARD_X (W * 54 / 100)
#define START_X (CARD_X + 46)
#define START_Y (H * 60 / 100)
static void welcome_view(void) {
    sky();
    int x = CARD_X, c = x + 150;
    glass(x - 18, -80, 337, H + 160, 156, 0xaff4fc, 156);
    glass(x + 8, -75, 281, H + 160, 126, 0xe3ffff, 65);
    glass(x - 162, 85, 181, 50, 25, 0x72f3fc, 235);
    centered(x - 72, 97, "Desktop   /   " EDITION, 1, INK);
    sprite(GL_LEAF, GL_LEAF_W, GL_LEAF_H, x - 86, 145, 79, 101, 1);
    box(x - 52, 254, 2, 400, 0xc9ffff);
    box(x - 73, 284, 1, 305, 0x92ffff);
    centered(c + 1, 247, "Welcome!", 3, 0x387783);
    centered(c, 243, "Welcome!", 3, 0xe0ffd4);
    // Reflect the live glyphs in the glass, not a screenshot of the lettering.
    for (int j = 0; j < 26; j++)
        for (int i = c - 134; i < c + 134; i++)
            screen[307 + j][i] = mix(screen[307 + j][i], screen[290 - j][i], 120 - 4 * j);
    centered(c, 350, "FreshOS   /   Sky desktop", 1, 0x205b6c);
    pill(START_X, START_Y, 208, 42, "Start session", 1);
    pill(START_X + 17, START_Y + 57, 174, 32, "Install to disk", 0);
    glass(18, H - 58, 239, 36, 18, 0xe2ffff, 190);
    text(37, H - 47, "ENTER   Start session     /     I   Install", 0, INK);
}
enum {
    DOCK_APPS,
    DOCK_FILES,
    DOCK_WRITER,
    DOCK_SETTINGS,
    DOCK_CLOCK,
    DOCK_TRAY,
    DOCK_LIGHT,
    DOCK_SOUND,
    DOCK_NET,
    DOCK_COUNT
};
static const struct {
    int x, y, w, h;
    const char *name;
} dock[] = {{0, 303, 53, 82, "Applications"}, {2, 6, 40, 41, "Files"},
            {2, 48, 40, 37, "Writer"},        {2, 86, 40, 37, "Settings"},
            {2, 125, 40, 32, "Clock"},        {2, 158, 40, 30, "App tray"},
            {2, 256, 40, 33, "Brightness"},   {2, 223, 40, 32, "Volume"},
            {2, 190, 40, 32, "Connections"}};
static int dock_at(int x, int y) {
    for (int i = 0; i < DOCK_COUNT; i++)
        if (inside(x, y, dock[i].x, dock[i].y, dock[i].w, dock[i].h))
            return i;
    return -1;
}
#define GLYPH(name, x, y, w, h) sprite(GL_##name, GL_##name##_W, GL_##name##_H, x, y, w, h, 1)
static void sun(int x, int y) {
    gel(x - 8, y - 8, 16, 16, 8, WHITE, 0xffff43, 0xf1b234);
    for (int i = -1; i <= 1; i++) {
        box(x + i * 10, y - 12, 2, 3, 0xefa72a);
        box(x + i * 10, y + 9, 2, 3, 0xefa72a);
    }
    box(x - 13, y, 4, 2, 0xefa72a);
    box(x + 9, y, 4, 2, 0xefa72a);
}
static void rail(void) {
    glass(-19, -22, 65, 320, 27, 0x7eefff, 220);
    for (int x = 0; x < 43; x++) {
        unsigned shade = 255 - clamp((x > 21 ? x - 21 : 21 - x) * 11, 0, 240);
        for (int y = 0; y < 282; y++)
            screen[y][x] = mix(screen[y][x], 0x08d2ff, shade / 2);
    }
    int over = dock_at(mx, my);
    for (int i = 0; i < DOCK_COUNT; i++) {
        int x = dock[i].x, y = dock[i].y;
        if (i == over && i != DOCK_APPS)
            gel(x + 2, y, 36, dock[i].h, 12, WHITE, 0x83efd9, 0xbef8ed);
        switch (i) {
        case DOCK_APPS:
            gel(-70, y - 23, 123, 122, 61, 0xcdfdff, 0x06dcff, 0x60e3ff);
            GLYPH(APPS, 0, y + 23, 52, 42);
            break;
        case DOCK_FILES:
        case DOCK_WRITER:
        case DOCK_SETTINGS:
        case DOCK_CLOCK:
            icon(6, y + 3,
                 i == DOCK_FILES      ? FILES
                 : i == DOCK_WRITER   ? WRITER
                 : i == DOCK_SETTINGS ? SETTINGS
                                      : CLOCK,
                 i == DOCK_CLOCK ? 26 : 32);
            break;
        case DOCK_TRAY:
            GLYPH(TRAY, 11, y + 3, 22, 24);
            break;
        case DOCK_NET:
            GLYPH(NETWORK, 7, y + 2, 30, 28);
            break;
        case DOCK_SOUND:
            GLYPH(SOUND, 6, y + 5, 32, 23);
            break;
        case DOCK_LIGHT:
            sun(22, y + 16);
            break;
        }
    }
    if (over >= 0 && !menu && !widget && !tray) {
        int yy = dock[over].y + 3;
        glass(53, yy, tw(dock[over].name, 1) + 24, 27, 13, 0xe3ffff, 228);
        text(65, yy + 3, dock[over].name, 1, INK);
    }
}
#define SL_X 102
#define SL_LEN 180
static int slider_y(void) { return widget == 1 ? 248 : 282; }
static int slide_grab;
static void slider_draw(void) {
    int y = slider_y(), v = widget == 1 ? vol : dim;
    char pct[6];
    num(pct, v * 10);
    append(pct, 6, '%');
    glass(53, y - 58, 269, 103, 34, 0xaff5f0, 238);
    text(86, y - 44, widget == 1 ? "Volume" : "Brightness", 2, INK);
    text(250, y - 39, pct, 1, INK);
    gel(SL_X, y, SL_LEN, 9, 4, 0x679ca6, 0x7eb0bc, 0xbcdfe6);
    if (v)
        gel(SL_X, y, v * 18, 9, 4, 0xd9ffd0, 0x46dca6, 0x25bfac);
    gel(SL_X + v * 18 - 10, y - 6, 20, 21, 10, WHITE, 0xc0ffff, 0x63dddc);
    text(91, y + 19, widget == 1 ? audio_name() : "Display luminance", 0, INK);
}
