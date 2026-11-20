/* Measurement and painting use the same wrapped lines as mouse and caret navigation. */
static int starts[1402], lines, firstrow, caretrow;
static int advance(char c) { return c >= 32 && c <= 126 ? fonts[0].adv[(int)c - 32] : 0; }
static void layout_note(void) {
    int width = 0, n = len(note);
    editpos = clamp(editpos, 0, n);
    lines = 1;
    starts[0] = 0;
    for (int i = 0; i < n; i++) {
        int adv = advance(note[i]);
        if (width + adv > 439) {
            starts[lines++] = i;
            width = 0;
        }
        if (note[i] == '\n') {
            starts[lines++] = i + 1;
            width = 0;
        } else
            width += adv;
    }
    starts[lines] = n;
    caretrow = 0;
    while (caretrow + 1 < lines && starts[caretrow + 1] <= editpos)
        caretrow++;
    firstrow = clamp(caretrow - 9, 0, lines > 10 ? lines - 10 : 0);
}
static int line_end(int row) {
    int end = starts[row + 1];
    if (row + 1 < lines && end > starts[row] && note[end - 1] == '\n')
        end--;
    return end;
}
static int pixel_pos(int row, int x) {
    int pos = starts[row], end = line_end(row);
    while (pos < end) {
        int w = advance(note[pos]);
        if (x < w / 2)
            break;
        x -= w;
        pos++;
    }
    return pos;
}
static void writer_place(int x, int y) {
    if (x < 0 || x > 466 || y < 30 || y > 212)
        return;
    layout_note();
    int row = clamp(firstrow + (y - 34) / 17, 0, lines - 1);
    editpos = pixel_pos(row, x - 11);
}
static void writer_draw(int x, int y) {
    layout_note();
    int right = text(x, y, document, 1, INK);
    if (modified)
        text(right + 6, y, "*", 1, INK);
    char n[16];
    num(n, len(note));
    text(text(x + 321, y + 4, n, 0, INK) + 5, y + 4, "/ 1399 bytes", 0, INK);
    gel(x, y + 27, 466, 184, 9, 0xc5e2e2, 0xeaf3f0, WHITE);
    box(x + 2, y + 29, 462, 179, 0xfcfffd);
    for (int r = firstrow; r < lines && r < firstrow + 10; r++) {
        int tx = x + 11, yy = y + 34 + (r - firstrow) * 17;
        for (int p = starts[r]; p < line_end(r); p++) {
            if (editpos == p)
                box(tx - 1, yy + 1, 2, 14, 0x159684);
            char ch[] = {note[p], 0};
            tx = text(tx, yy, ch, 0, INK);
        }
        if (caretrow == r && editpos == line_end(r))
            box(tx, yy + 1, 2, 14, 0x159684);
    }
    text(x, y + 228, "Plain text  /  Ctrl+S saves", 0, INK);
    pill(x + 226, y + 221, 114, 27, "Save", 1);
    pill(x + 350, y + 221, 114, 27, "Save as...", 0);
}
static void writer_key(int k, char c) {
    int n = len(note);
    layout_note();
    if (ctrl && shift && k == 31) {
        savebox = 1;
        savename[0] = 0;
        return;
    }
    if (ctrl && k == 31) {
        write_document(document);
        return;
    }
    if (k == 331 || k == 333) {
        editpos = clamp(editpos + (k == 331 ? -1 : 1), 0, n);
        return;
    }
    if (k == 327 || k == 335) {
        editpos = k == 327 ? starts[caretrow] : line_end(caretrow);
        return;
    }
    if (k == 328 || k == 336) {
        int xp = 0;
        for (int i = starts[caretrow]; i < editpos; i++)
            xp += advance(note[i]);
        editpos = pixel_pos(clamp(caretrow + (k == 328 ? -1 : 1), 0, lines - 1), xp);
        return;
    }
    if ((c == '\b' && editpos > 0) || (k == 339 && editpos < n)) {
        modified = 1;
        if (c == '\b')
            editpos--;
        for (int i = editpos; i < n; i++)
            note[i] = note[i + 1];
    } else if ((c == '\n' || c >= 32) && n + 1 < (int)sizeof note) {
        for (int i = n; i >= editpos; i--)
            note[i + 1] = note[i];
        note[editpos++] = c;
        modified = 1;
    }
}
static void files_draw(int x, int y) {
    text(x, y, "Workspace", 2, INK);
    pill(x + 235, y, 106, 28, "Library", !filetab);
    pill(x + 350, y, 115, 28, "Recent", filetab);
    text(x, y + 34,
         filetab
             ? (home_ready() ? "FRESH HOME   /   RECENT FILES" : "SESSION   /   RECENT FILES")
             : "BUILT-IN DOCUMENTS & PROGRAMS",
         0, 0x3a737c);
    int n = filecount();
    for (int j = 0; j < 4; j++) {
        int i = fileidx / 4 * 4 + j;
        if (i >= n)
            break;
        const char *name = filetab ? recent[i] : library[i];
        int yy = y + 55 + j * 34;
        gel(x, yy, 466, 30, 8, WHITE, i == fileidx ? 0xaceede : 0xe1eff2,
            i == fileidx ? 0xd4fff1 : 0xf2f8f9);
        int app = suffix(name, ".imp") || suffix(name, ".exe") ? WINDOWS
                  : eq(name, "canvas")                         ? PAINT
                                                               : WRITER;
        icon(x + 6, yy + 2, app, 25);
        text(x + 42, yy + 4, name, 1, INK);
        text(x + 345, yy + 7,
             suffix(name, ".exe")   ? "Windows EXE"
             : suffix(name, ".imp") ? "IMP image"
             : app == PAINT         ? "Artwork"
                                    : "Document",
             0, INK);
    }
    if (!n)
        text(x + 12, y + 84, "No recent files. Convert an EXE or save a document.", 1, INK);
    if (n) {
        pill(x, y + 211, 85, 30, "Open", 1);
        if (suffix(selection(), ".exe"))
            pill(x + 95, y + 211, 135, 30, "Convert to IMP", 0);
        if (n > 4) {
            pill(x + 295, y + 211, 78, 30, "Prev", 0);
            pill(x + 383, y + 211, 82, 30, "Next", 0);
        }
    }
}
static void file_click(int x, int y) {
    if (y >= 0 && y < 29 && x >= 235 && x < 465) {
        filetab = x >= 350;
        file_refresh();
        fileidx = 0;
        return;
    }
    if (y >= 55 && y < 191 && x >= 0 && x < 466) {
        int i = fileidx / 4 * 4 + (y - 55) / 34;
        if (i < filecount()) {
            fileidx = i;
            if (last_file == i && ticks - last_click < 28) {
                last_file = -1;
                open_file(selection());
                return;
            }
            last_file = i;
            last_click = ticks;
        }
        return;
    }
    if (y >= 211 && y < 242 && filecount()) {
        if (x >= 0 && x < 85)
            open_file(selection());
        else if (x >= 95 && x < 230 && suffix(selection(), ".exe"))
            convert_file(selection());
        else if (filecount() > 4 && x >= 295 && x < 465) {
            int p = (fileidx / 4 + (x < 380 ? -1 : 1));
            fileidx = clamp(p, 0, (filecount() - 1) / 4) * 4;
        }
    }
}
