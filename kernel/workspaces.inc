/* Desktops group singleton app sessions. Launching a session on a different
 * desktop explicitly brings it there; browsing Task View does not move it. */
static void switch_desk(int n) {
    if (n != desk) {
        foreground[desk] = active;
        deskx[desk] = wx;
        desky[desk] = wy;
        desk = n;
        active = foreground[n];
        wx = deskx[n];
        wy = desky[n];
    }
    overview = menu = tray = drag = widget = lift = lift_speed = goal = velocity = progress =
        0;
    dirty = 1;
    serial("workspace: ");
    char s[4];
    num(s, n + 1);
    serial(s);
    serial("\r\n");
}
static void task_view(void) {
    overview = !overview;
    focusdesk = desk;
    menu = tray = widget = goal = progress = velocity = 0;
    dirty = 1;
}
static void workspace_draw(void) {
    glass(W / 2 - 306, H / 2 - 139, 612, 278, 28, ICE, 250);
    text(W / 2 - 276, H / 2 - 116, "Task view", 2, INK);
    text(W / 2 - 276, H / 2 - 75, "Select a desktop   /   Arrows and Enter", 1, INK);
    for (int i = 0; i < NDESKS; i++) {
        int x = W / 2 - 278 + 141 * i, y = H / 2 - 33;
        gel(x, y, 132, 104, 14, WHITE, i == focusdesk ? 0x91e7cc : 0xb7ddea, ICE);
        int a = i == desk ? active : foreground[i];
        if (a >= 0)
            icon(x + 51, y + 9, a, 30);
        char n[] = "Desktop 1";
        n[8] += i;
        centered(x + 66, y + 44, n, 1, INK);
        centered(x + 66, y + 70, a >= 0 ? apps[a].name : "Desktop", 0, INK);
    }
    text(W / 2 - 276, H / 2 + 92,
         "Win+Tab: task view     /     Ctrl+Win+Left or Right: switch", 0, INK);
}
