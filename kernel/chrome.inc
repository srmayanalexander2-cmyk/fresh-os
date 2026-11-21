/* Keep layout and hit-testing on the same rectangle, including when maximized. */
struct Bounds {
    int x, y, w, h;
};
static struct Bounds frame(void) {
    if (active >= 0 && maximized[active])
        return (struct Bounds){cli ? 0 : 54, 0, cli ? W : W - 54, H};
    return (struct Bounds){wx, clamp(wy + lift, 0, H - 311), 506, 311};
}
static void toggle_max(void) {
    if (active >= 0) {
        maximized[active] = !maximized[active];
        drag = lift = lift_speed = 0;
        dirty = 1;
        serial(maximized[active] ? "window: maximized\r\n" : "window: restored\r\n");
    }
}
static const char *const context_labels[] = {"Copy text",     "Paste text", "Copy diagnostic",
                                             "Open Terminal", "Settings",   "Task Manager",
                                             "Show desktop"};
static void context_action(int row) {
    if (row == 2) {
        const char *s = active == WINDOWS && pe_ran && !pe_ok ? pe_result.detail
                        : lastnotice[0]                       ? lastnotice
                                                              : disk_status();
        copy(clip, s, sizeof clip);
        say("Diagnostic copied.");
        return;
    }
    if (row <= 1) {
        int isterm = active == TERM || (active < 0 && cli);
        if (active == WRITER || active == STICKY || active == TODO || isterm ||
            (row == 0 && active == CHARMAP)) {
            int c = ctrl, h = shift;
            ctrl = 1;
            shift = isterm && !row;
            key(row ? 47 : 46, 1);
            shift = h;
            ctrl = c;
        } else if (!row && active == WINDOWS && pe_ran) {
            copy(clip, pe_ok ? pe_result.output : pe_result.detail, sizeof clip);
        } else
            notify("Copy and paste work in text apps and Terminal.");
        return;
    }
    if (row == 3)
        launch(TERM);
    else if (row == 4)
        launch(SETTINGS);
    else if (row == 5)
        launch(TASKMGR);
    else if (row == 6)
        cli_set(0);
}
static void context_draw(void) {
    glass(cx, cy, 204, 241, 15, ICE, 251);
    for (int i = 0; i < 7; i++) {
        int y = cy + 5 + i * 33;
        if (inside(mx, my, cx + 4, y, 196, 33))
            gel(cx + 5, y, 194, 32, 9, WHITE, 0xadebd8, ICE);
        text(cx + 17, y + 5, context_labels[i], 1, INK);
    }
}
