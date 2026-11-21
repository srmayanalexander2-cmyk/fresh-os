static void software_draw(int x, int y) {
    text(x, y, "Installed programs", 2, INK);
    pill(x + 331, y, 126, 29, "Open Files", 0);
    for (int j = 0; j < 4; j++) {
        int i = pkgsel / 4 * 4 + j;
        if (i >= pkgcount)
            break;
        gel(x, y + 45 + j * 35, 456, 31, 10, WHITE, i == pkgsel ? 0x9ce8c5 : 0xe0f0ed, ICE);
        text(x + 13, y + 49 + j * 35, pkg_names[i], 1, INK);
    }
    if (!pkgcount)
        text(x, y + 55,
             home_ready() ? "Choose an IMP in Files and click Install."
                          : "Installation requires a writable Fresh Home disk.",
             1, INK);
    if (pkgcount) {
        pill(x, y + 210, 82, 30, "Run", 1);
        pill(x + 88, y + 210, 120, 30, confirm_remove ? "Confirm?" : "Uninstall", 0);
        pill(x + 279, y + 210, 84, 30, "Prev", 0);
        pill(x + 373, y + 210, 84, 30, "Next", 0);
    }
}
static void shelf_action(const char *verb) {
    if (!pkgcount)
        return;
    char cmd[64];
    copy(cmd, verb, 64);
    int n = len(cmd);
    copy(cmd + n, pkg_names[pkgsel], 64 - n);
    package_cmd(cmd);
    if (active == SOFTWARE)
        shelf_refresh();
}
static void shelf_click(int x, int y) {
    if (inside(x, y, 331, 0, 126, 29)) {
        launch(FILES);
        return;
    }
    int i = pkgsel / 4 * 4 + (y - 45) / 35;
    if (y >= 45 && y < 185 && x >= 0 && x < 456 && i < pkgcount) {
        pkgsel = i;
        confirm_remove = 0;
    }
    if (!pkgcount || y < 210 || y >= 240)
        return;
    if (x >= 0 && x < 82)
        shelf_action("run ");
    else if (x >= 88 && x < 208) {
        if (confirm_remove)
            shelf_action("remove ");
        else
            confirm_remove = 1;
    } else if (x >= 279 && x < 457) {
        pkgsel = clamp(pkgsel + (x < 364 ? -4 : 4), 0, pkgcount - 1);
        confirm_remove = 0;
    }
}
