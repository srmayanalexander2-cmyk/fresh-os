/* Decode uncompressed Windows BMP from Home into a bounded RGB565 preview.
   The import buffer can then be reused by another app without losing the photo. */
#define PHOTO_W 456
#define PHOTO_H 174
static u16 photo[PHOTO_W * PHOTO_H];
static int photo_w, photo_h;
static char photo_name[32];
static u32 bmp_u32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }
static int load_photo(const char *name) {
    const u8 *p;
    int n = get_program(name, &p);
    if (n < 54 || p[0] != 'B' || p[1] != 'M' || bmp_u32(p + 14) < 40 || p[26] != 1 || p[27] ||
        p[29] || bmp_u32(p + 30))
        return 0;
    int w = (int)bmp_u32(p + 18), h = (int)bmp_u32(p + 22), bits = p[28];
    u32 off = bmp_u32(p + 10);
    if (w < 1 || w > 32768 || h == 0 || h < -32768 || h > 32768 ||
        (bits != 16 && bits != 24 && bits != 32) || off < 54 || off > (u32)n ||
        bmp_u32(p + 14) > off - 14)
        return 0;
    int top = h < 0;
    if (top)
        h = -h;
    u32 stride = ((w * bits + 31) / 32) * 4;
    if ((u32)h > ((u32)n - off) / stride)
        return 0;
    photo_w = PHOTO_W;
    photo_h = h * photo_w / w;
    if (photo_h > PHOTO_H) {
        photo_h = PHOTO_H;
        photo_w = w * photo_h / h;
    }
    if (photo_h < 1)
        photo_h = 1;
    if (photo_w < 1)
        photo_w = 1;
    for (int j = 0; j < photo_h; j++) {
        int y = j * h / photo_h;
        const u8 *r = p + off + (top ? y : h - 1 - y) * stride;
        for (int i = 0; i < photo_w; i++) {
            const u8 *q = r + (i * w / photo_w) * (bits / 8);
            u16 c;
            if (bits == 16) {
                u16 b = q[0] | q[1] << 8;
                c = (b & 0x7fe0) << 1 | (b & 31) | ((b >> 4) & 32);
            } else
                c = (q[2] >> 3) << 11 | (q[1] >> 2) << 5 | (q[0] >> 3);
            photo[j * photo_w + i] = c;
        }
    }
    copy(photo_name, name, sizeof photo_name);
    return 1;
}
static void photo_draw(int x, int y) {
    text(x, y, photo_w ? photo_name : "Photos", 2, INK);
    box(x, y + 34, PHOTO_W, PHOTO_H, 0x204354);
    if (photo_w)
        sprite(photo, photo_w, photo_h, x + (PHOTO_W - photo_w) / 2,
               y + 34 + (PHOTO_H - photo_h) / 2, photo_w, photo_h, 0);
    else
        centered(x + PHOTO_W / 2, y + 101,
                 "Open a BMP in Files or capture a selection with F10.", 1, WHITE);
    pill(x, y + 215, 98, 28, "Files", 0);
    text(x + 117, y + 220, "BMP   /   16, 24 or 32-bit RGB", 0, INK);
}
