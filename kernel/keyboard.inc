/* Interaction surface, not a keyboard emulator. Delivers the same scancodes
 * as physical typing; never used for credentials or permission prompts. */
static int oskshift;
static const struct {
    const char *lo, *hi;
    int start;
} keyrows[] = {{"1234567890-=", "!@#$%^&*()_+", 2},
               {"qwertyuiop[]", "QWERTYUIOP{}", 16},
               {"asdfghjkl;'", "ASDFGHJKL:\"", 30},
               {"zxcvbnm,./", "ZXCVBNM<>?", 44}};
#define KBX ((W - 480) / 2)
#define KBY (H - 190)
static int kb_visible(void) {
    return osk && !welcome && !setup && !secret && !modal && !savebox && !pending_doc[0] &&
           !overview && !snipping && !shutdown && !tray && !menu &&
           (active == WRITER || active == STICKY || active == TODO || active == TERM ||
            (cli && active < 0));
}
static void keyboard_draw(void) {
    glass(KBX - 10, KBY - 9, 500, 187, 20, ICE, 247);
    for (int r = 0; r < 4; r++) {
        const char *s = oskshift ? keyrows[r].hi : keyrows[r].lo;
        for (int i = 0; s[i]; i++) {
            char l[] = {s[i], 0};
            int x = KBX + i * 40, y = KBY + r * 34;
            gel(x, y, 37, 30, 8, WHITE, 0xb7e1dc, ICE);
            centered(x + 18, y + 3, l, 1, INK);
        }
    }
    pill(KBX, KBY + 136, 68, 28, "Shift", oskshift);
    pill(KBX + 72, KBY + 136, 164, 28, "Space", 0);
    pill(KBX + 240, KBY + 136, 80, 28, "Bksp", 0);
    pill(KBX + 324, KBY + 136, 82, 28, "Enter", 0);
    pill(KBX + 410, KBY + 136, 70, 28, "Hide", 0);
}
static int keyboard_click(int x, int y, int click) {
    x -= KBX;
    y -= KBY;
    if (!inside(x, y, -10, -9, 500, 187))
        return 0;
    if (!click || x < 0 || y < 0)
        return 1;
    int k = 0;
    if (y < 136) {
        int r = y / 34, i = x / 40;
        if (i < len(keyrows[r].lo) && x % 40 < 37 && y % 34 < 30)
            k = keyrows[r].start + i;
    } else if (y < 165) {
        if (x < 70)
            oskshift = !oskshift;
        else if (x < 240)
            k = 57;
        else if (x < 324)
            k = 14;
        else if (x < 410)
            k = 28;
        else
            osk = 0;
    }
    if (k) {
        int old = shift;
        shift = oskshift;
        key(k, 1);
        shift = old;
        oskshift = 0;
    }
    return 1;
}
