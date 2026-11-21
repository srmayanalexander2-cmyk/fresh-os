static int visible_rows, rowcount, hitlink[40], selectedrow, reveal_link;
static void browser_draw(int x, int y, int w, int h) {
    int inner = w - 30;
    char tail[URL_CAP];
    const char *s = editing ? address : location;
    while (*s && tw(s, 1) > inner - 231)
        s++;
    copy(tail, s, sizeof tail);
    pill(x + 9, y + 5, 40, 28, "<", 0);
    pill(x + 53, y + 5, 40, 28, ">", 0);
    gel(x + 98, y + 4, w - 240, 30, 9, WHITE, editing ? 0xc6ffef : ICE, WHITE);
    text(x + 107, y + 8, tail, 1, INK);
    pill(x + w - 136, y + 5, 58, 28, "Go", 1);
    pill(x + w - 74, y + 5, 64, 28, "Star", 0);
    box(x + 9, y + 42, w - 18, h - 78, 0xf8fffd);
    fit_label(x + w / 2, y + 47, w - 54, title, 2);
    visible_rows = clamp((h - 117) / 21, 1, 40);
    for (int j = 0; j < 40; j++)
        hitlink[j] = 0;
    rowcount = 0;
    selectedrow = -1;
    for (int at = 0; at < bodylen;) {
        if (body[at] == '\n') {
            at++;
            continue;
        }
        int start = at, style = style_at[at], f = style < 0 ? 1 : 0, width = 0, space = -1;
        while (at < bodylen && body[at] != '\n' && style_at[at] == style &&
               at - start < 156) {
            unsigned c = (u8)body[at];
            int adv = fonts[f].adv[clamp(c - 32, 0, 94)];
            if (width + adv > w - 56)
                break;
            width += adv;
            if (c == ' ')
                space = at;
            at++;
        }
        if (at < bodylen && body[at] != '\n' && body[at] != ' ' && space > start)
            at = space;
        if (at == start)
            at++;
        int n = at - start;
        while (at < bodylen && body[at] == ' ')
            at++;
        int j = rowcount++ - scrollrow;
        if (style > 0 && style == linkfocus && selectedrow < 0)
            selectedrow = rowcount - 1;
        if (j < 0 || j >= visible_rows)
            continue;
        char row[160];
        copy(row, body + start, n + 1);
        int yy = y + 82 + j * 21;
        if (style > 0 && style == linkfocus)
            box(x + 16, yy, w - 32, 19, 0xc8f5e8);
        int end = text(x + 22, yy, row, f, style > 0 ? 0x08718a : INK);
        if (style > 0) {
            box(x + 22, yy + 15, end - x - 22, 1, 0x53afbd);
            hitlink[j] = style;
        }
    }
    if (reveal_link) {
        reveal_link = 0;
        if (selectedrow >= 0 &&
            (selectedrow < scrollrow || selectedrow >= scrollrow + visible_rows)) {
            scrollrow = clamp(selectedrow, 0, clamp(rowcount - visible_rows, 0, 999));
            browser_draw(x, y, w, h);
            return;
        }
    }
    if (scrollrow > clamp(rowcount - visible_rows, 0, 999)) {
        scrollrow = clamp(rowcount - visible_rows, 0, 999);
        dirty = 1;
    }
    pill(x + 10, y + h - 32, 57, 26, "Home", 0);
    pill(x + w - 130, y + h - 32, 55, 26, "Up", 0);
    pill(x + w - 70, y + h - 32, 58, 26, "Down", 0);
    fit_label(x + (w - 55) / 2, y + h - 26, w - 220,
              notice[0] ? notice
              : prefix(location, "http:")
                  ? "HTTP / unencrypted   |   Ctrl+L: address"
                  : "Local page   |   Tab: links   |   Ctrl+L: address",
              0);
}
static void travel(int d) {
    int k = hpos + d;
    if (k >= 0 && k < hcount) {
        hpos = k;
        navigate(paths[k], 0);
    }
}
static void star(void) {
    if (eq(location, "aero:favorite"))
        return;
    copy(starred, location, sizeof starred);
    copy(notice,
         home_save("aero.favorite", starred, len(starred))
             ? "Favorite saved to Home"
             : "Favorite saved for this session",
         sizeof notice);
}
static void browser_click(int x, int y, int w, int h) {
    if (y >= 4 && y < 35) {
        if (x < 50)
            travel(-1);
        else if (x < 94)
            travel(1);
        else if (x < w - 141) {
            editing = replace_address = 1;
        } else if (x < w - 77)
            navigate(address, 1);
        else
            star();
        return;
    }
    if (y >= h - 33 && y < h) {
        if (x < 70)
            navigate("aero:home", 1);
        else if (x > w - 132)
            scrollrow = clamp(scrollrow + (x > w - 72 ? 1 : -1) * visible_rows, 0,
                              clamp(rowcount - visible_rows, 0, 999));
        return;
    }
    int j = (y - 82) / 21;
    if (y >= 82 && j < visible_rows && hitlink[j])
        follow(hitlink[j]);
}
static void browser_key(int k, char c) {
    if (ctrl && k == 38) {
        editing = replace_address = 1;
        return;
    }
    if (ctrl && k == 19) {
        navigate(location, 0);
        return;
    }
    if (ctrl && k == 32) {
        star();
        return;
    }
    if (editing) {
        if (ctrl && k == 46) {
            copy(clip, address, sizeof clip);
            return;
        }
        if (ctrl && k == 47) {
            if (replace_address)
                address[0] = 0;
            replace_address = 0;
            int pos = len(address);
            paste(address, sizeof address, &pos, 0);
            return;
        }
        if (k == 28) {
            navigate(address, 1);
            return;
        }
        if (k == 1) {
            editing = 0;
            copy(address, location, sizeof address);
            return;
        }
        if (k == 14 || c >= 32) {
            if (replace_address)
                address[0] = 0;
            replace_address = 0;
            int n = len(address);
            if (k == 14) {
                if (n)
                    address[n - 1] = 0;
            } else
                append(address, sizeof address, c);
        }
        return;
    }
    if (k == 15 && nlinks) {
        linkfocus = 1 + linkfocus % nlinks;
        reveal_link = 1;
    }
    if (k == 28)
        follow(linkfocus);
    if (k == 331)
        travel(-1);
    if (k == 333)
        travel(1);
    int d = k == 328   ? -1
            : k == 336 ? 1
            : k == 329 ? -visible_rows
            : k == 337 ? visible_rows
                       : 0;
    if (d)
        scrollrow = clamp(scrollrow + d, 0, clamp(rowcount - visible_rows, 0, 999));
}
