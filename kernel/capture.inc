static void store_le(u8 *p, u32 v, int n) {
    for (int i = 0; i < n; i++)
        p[i] = v >> (8 * i);
}
/* RGB555 BMP is accepted by standard desktop viewers and fits the existing
 * import scratch area even for a full 1280x720 capture. No second framebuffer. */
static void save_snip(int x, int y) {
    int left = x < sx ? x : sx, top = y < sy ? y : sy;
    int w = (x > sx ? x : sx) - left + 1, h = (y > sy ? y : sy) - top + 1;
    snipping = 0;
    render();
    if (w < 2 || h < 2) {
        notify("Selection too small. Capture cancelled.");
        return;
    }
    u32 stride = (2 * w + 3) & ~3u, size = 54 + h * stride;
    if (size > sizeof imported) {
        notify("Selection too large.");
        return;
    }
    u8 *p = imported;
    for (int i = 0; i < 54; i++)
        p[i] = 0;
    p[0] = 'B';
    p[1] = 'M';
    store_le(p + 2, size, 4);
    p[10] = 54;
    p[14] = 40;
    store_le(p + 18, w, 4);
    store_le(p + 22, h, 4);
    p[26] = 1;
    p[28] = 16;
    store_le(p + 34, h * stride, 4);
    for (int j = 0; j < h; j++) {
        u8 *row = p + 54 + (h - 1 - j) * stride;
        for (int i = 0; i < w; i++) {
            u32 c = screen[top + j][left + i];
            store_le(row + 2 * i, ((c >> 9) & 0x7c00) | ((c >> 6) & 0x3e0) | ((c >> 3) & 31),
                     2);
        }
        for (u32 i = 2 * w; i < stride; i++)
            row[i] = 0;
    }
    notify(home_save("snip.bmp", p, size) ? "Selection saved as snip.bmp."
                                          : "Capture not saved: check Home.");
}
static void capture_overlay(void) {
    if (snipping == 1) {
        glass(W / 2 - 220, 12, 440, 38, 18, ICE, 238);
        centered(W / 2, 20, "Drag to capture a rectangle   /   Esc cancels", 1, INK);
    } else if (snipping == 2) {
        int l = mx < sx ? mx : sx, t = my < sy ? my : sy, w = (mx > sx ? mx : sx) - l + 1,
            h = (my > sy ? my : sy) - t + 1;
        box(l, t, w, 2, MINT);
        box(l, t + h - 1, w, 1, MINT);
        box(l, t, 2, h, MINT);
        box(l + w - 1, t, 1, h, MINT);
    }
}
static void lens(void) {
    int x = mx < W / 2 ? W - 210 : 10, y = 10;
    int left = clamp(mx - 48, 0, W - 96), top = clamp(my - 32, 0, H - 64);
    box(x, y, 200, 158, INK);
    for (int j = 0; j < 128; j++)
        for (int i = 0; i < 192; i++)
            screen[y + 4 + j][x + 4 + i] = screen[top + j / 2][left + i / 2];
    text(x + 10, y + 135, "2x magnifier   /   F9 to close", 0, WHITE);
}
