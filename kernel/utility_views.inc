static const struct {
    const char *title, *detail;
    int app;
} guides[] = {{"Keeping a document",
               "Open Writer, type your note, then press Ctrl+S to save to Fresh Home. Use "
               "Save as for a different file.",
               WRITER},
              {"Running a program",
               "Select an EXE in Files and choose Convert to IMP. Open the result to run it "
               "without the source EXE.",
               FILES},
              {"Build a checklist",
               "Type one item and press Enter. Click a row to check it off. Save writes the "
               "list to Home.",
               TODO},
              {"Capture the screen",
               "Press F8, choose Snip, then drag over the desktop. The selection is saved to "
               "snip.bmp on writable Home.",
               CENTER},
              {"File checks and archives",
               "In Terminal: sha3 NAME prints a SHA3-256 checksum. tar files.tar A B packs "
               "two files. gzip A.gz A wraps one file.",
               TERM},
              {"Find your devices",
               "System shows the boot mode, display and storage drivers. Open Device "
               "inventory for actual PCI IDs.",
               SYSTEM}};
#define NGUIDES ((int)(sizeof guides / sizeof *guides))
static int quicksel, quickfirst, quickorder[] = {0, 1, 2, 3, 4, 5, 6, 7}, magnify, saver,
                                 oldmotion, olddim, snipping, sx, sy;
static const char *const actions[] = {"Brightness", "Volume", "Motion",      "Energy saver",
                                      "Magnifier",  "Snip",   "Connections", "Event log"};
static void quick_action(int a) {
    switch (a) {
    case 0:
        dim = dim >= 10 ? 2 : clamp(dim + 2, 1, 10);
        break;
    case 1:
        vol = (vol + 1) % 11;
        sound();
        break;
    case 2:
        motion = !motion;
        break;
    case 3:
        if (!saver) {
            oldmotion = motion;
            olddim = dim;
            motion = 0;
            dim = clamp(dim, 1, 6);
        } else {
            motion = oldmotion;
            dim = olddim;
        }
        saver = !saver;
        break;
    case 4:
        magnify = !magnify;
        break;
    case 5:
        if (!home_ready()) {
            notify("Snipping needs writable Fresh Home.");
            break;
        }
        if (active == CENTER) {
            hide(1);
            if (actions_from >= 0 && owner[actions_from] == desk && hidden[actions_from])
                launch(actions_from);
        }
        menu = tray = widget = goal = progress = velocity = 0;
        feedback[0] = 0;
        snipping = 1;
        break;
    case 6:
        launch(CONNECTIONS);
        break;
    case 7:
        launch(EVENTS);
        break;
    }
}
static void quick_move(int d, int reorder) {
    int next = clamp(quicksel + d, 0, 7);
    if (reorder) {
        int a = quickorder[next];
        quickorder[next] = quickorder[quicksel];
        quickorder[quicksel] = a;
    }
    quicksel = next;
    if (next < quickfirst)
        quickfirst = next;
    if (next > quickfirst + 3)
        quickfirst = next - 3;
}
static void utility_draw(int a, int x, int y) {
    char n[16];
    if (a == STICKY) {
        text(x, y, "Sticky notes", 2, INK);
        for (int i = 0; i < 3; i++) {
            char b[] = {'1' + i, 0};
            pill(x + 268 + i * 62, y, 53, 27, b, i == card);
        }
        gel(x, y + 42, 460, 142, 12, 0xfff8ad, 0xffef97, 0xffefb9);
        wrapped(x + 12, y + 50, 430, cards[card]);
        text(x, y + 193, "Type to append   /   Ctrl+C copies   /   Ctrl+V pastes", 0, INK);
        pill(x + 346, y + 211, 114, 29, "Save", 1);
    }
    if (a == TODO) {
        text(x, y, "My checklist", 2, INK);
        text(x + 285, y + 6, "Click a row to toggle", 0, INK);
        for (int j = 0; j < 4; j++) {
            int i = todopage * 4 + j;
            if (i >= ntodo)
                break;
            int yy = y + 38 + 30 * j;
            gel(x, yy, 460, 27, 8, WHITE, i == todosel ? 0xb8efde : 0xdcecf3, ICE);
            text(x + 10, yy + 4, checked & (1u << i) ? "[x]" : "[ ]", 1, INK);
            text(x + 45, yy + 4, todos[i], 1, INK);
        }
        box(x, y + 167, 360, 28, 0xe6f5f3);
        text(x + 6, y + 170, draft[0] ? draft : "Type a new task...", 1, INK);
        pill(x + 370, y + 165, 90, 30, "Add", 1);
        pill(x, y + 211, 78, 28, "Prev", 0);
        pill(x + 83, y + 211, 78, 28, "Next", 0);
        pill(x + 176, y + 211, 143, 28, "Clear done", 0);
        pill(x + 346, y + 211, 114, 28, "Save", 1);
    }
    if (a == CHARMAP) {
        text(x, y, "Character map", 2, INK);
        text(x + 270, y + 6, "Printable ASCII   /   95 glyphs", 0, INK);
        for (int i = 0; i < 95; i++) {
            int bx = x + i % 19 * 24, by = y + 43 + i / 19 * 30;
            box(bx, by, 22, 27, i == chidx ? MINT : 0xe3f1f3);
            char ch[] = {32 + i, 0};
            text(bx + 5, by + 4, ch, 1, INK);
        }
        text(x, y + 213, "Character code:", 1, INK);
        num(n, chidx + 32);
        text(x + 136, y + 213, n, 1, INK);
        pill(x + 292, y + 205, 164, 30, "Copy character", 1);
    }
    if (a == TIPS) {
        text(x, y, "FreshOS field guide", 2, INK);
        num(n, helpindex + 1);
        text(x + 397, y + 6, n, 1, INK);
        text(x + 414, y + 6, "/ 6", 1, INK);
        text(x, y + 50, guides[helpindex].title, 1, INK);
        wrapped(x, y + 83, 445, guides[helpindex].detail);
        pill(x, y + 210, 88, 29, "Back", 0);
        pill(x + 95, y + 210, 88, 29, "Next", 0);
        pill(x + 297, y + 210, 159, 29, "Open tool", 1);
    }
    if (a == EVENTS) {
        text(x, y, "Session events", 2, INK);
        text(x + 300, y + 5, "Seconds since boot", 0, INK);
        for (int j = 0; j < 7 && evpage * 7 + j < ecount; j++) {
            int i = (ehead + 31 - evpage * 7 - j) % 32;
            num(n, eventtime[i]);
            text(x, y + 42 + 22 * j, n, 0, INK);
            char b[64];
            copy(b, events[i], 64);
            text(x + 67, y + 41 + 22 * j, b, 0, INK);
        }
        pill(x, y + 210, 100, 29, "Newer", 0);
        pill(x + 109, y + 210, 100, 29, "Older", 0);
    }
    if (a == CENTER) {
        text(x, y, "Action center", 2, INK);
        text(x + 239, y + 7, "Arrows: scroll   /   Shift+arrows: reorder", 0, INK);
        for (int j = 0; j < 4; j++) {
            int i = quickfirst + j, v = quickorder[i];
            int yy = y + 42 + j * 37;
            gel(x, yy, 460, 32, 10, WHITE, i == quicksel ? 0xa5efda : 0xd9ecf1, ICE);
            text(x + 13, yy + 4, actions[v], 1, INK);
            const char *d = v == 0 || v == 1
                                ? (num(n, (v == 0 ? dim : vol) * 10), append(n, 16, '%'), n)
                            : v == 2 ? (motion ? "On" : "Off")
                            : v == 3 ? (saver ? "On" : "Off")
                            : v == 4 ? (magnify ? "On" : "Off")
                                     : "Open";
            text(x + 370, yy + 4, d, 1, INK);
        }
        pill(x, y + 211, 90, 28, "Up", 0);
        pill(x + 95, y + 211, 90, 28, "Down", 0);
        pill(x + 202, y + 211, 120, 28, "Move up", 0);
        pill(x + 330, y + 211, 125, 28, "Move down", 0);
    }
}
static void copychar(void) {
    clip[0] = chidx + 32;
    clip[1] = 0;
    notify("Character copied. Paste with Ctrl+V.");
}
static int utility_click(int a, int x, int y) {
    if (x < 0 || x >= 460)
        return 0;
    int bottom = y >= 205 && y < 244;
    if (a == STICKY) {
        if (y >= 0 && y < 30 && x >= 268)
            card = clamp((x - 268) / 62, 0, 2);
        if (bottom && x >= 346)
            card_save();
    } else if (a == TODO) {
        if (y >= 38 && y < 158) {
            int i = todopage * 4 + (y - 38) / 30;
            if (i < ntodo) {
                todosel = i;
                checked ^= 1u << i;
            }
        }
        if (y >= 165 && y < 195 && x >= 370)
            add_task();
        if (bottom) {
            if (x < 162) {
                todopage =
                    clamp(todopage + (x < 80 ? -1 : 1), 0, ntodo ? (ntodo - 1) / 4 : 0);
                todosel = todopage * 4;
            } else if (x >= 346)
                todo_save();
            else if (x >= 176 && x < 319)
                clear_done();
        }
    } else if (a == CHARMAP) {
        if (y >= 43 && y < 193 && x < 456)
            chidx = (y - 43) / 30 * 19 + x / 24;
        if (bottom && x >= 292)
            copychar();
    } else if (a == TIPS) {
        if (bottom) {
            if (x < 183)
                helpindex = (helpindex + NGUIDES + (x < 90 ? -1 : 1)) % NGUIDES;
            else if (x >= 297)
                launch(guides[helpindex].app);
        }
    } else if (a == EVENTS) {
        if (bottom && x < 210)
            evpage = clamp(evpage + (x < 104 ? -1 : 1), 0, ecount ? (ecount - 1) / 7 : 0);
    } else if (a == CENTER) {
        if (y >= 42 && y < 190) {
            quicksel = quickfirst + (y - 42) / 37;
            quick_action(quickorder[quicksel]);
        } else if (bottom)
            quick_move((x < 95 || (x >= 202 && x < 330)) ? -1 : 1, x >= 202);
    } else
        return 0;
    return 1;
}
static int utility_key(int a, int k, char c) {
    if (a == CENTER) {
        if (k == 328 || k == 336)
            quick_move(k == 328 ? -1 : 1, shift);
        else if (k == 28)
            quick_action(quickorder[quicksel]);
    } else if (a == STICKY || a == TODO) {
        char *s = a == STICKY ? cards[card] : draft;
        int cap = a == STICKY ? 241 : 41, n = len(s);
        if (ctrl && k == 31) {
            if (a == STICKY)
                card_save();
            else
                todo_save();
        } else if (ctrl && k == 46)
            copy(clip, s, sizeof clip);
        else if (ctrl && k == 47)
            paste(s, cap, &n, a == STICKY);
        else if (a == STICKY && k == 15)
            card = (card + 1) % 3;
        else if (a == TODO && k == 28)
            add_task();
        else if (a == TODO && (k == 328 || k == 336) && ntodo) {
            todosel = clamp(todosel + (k == 328 ? -1 : 1), 0, ntodo - 1);
            todopage = todosel / 4;
        } else if (a == TODO && k == 15 && ntodo)
            checked ^= 1u << todosel;
        else if (c == '\b' && n)
            s[n - 1] = 0;
        else if (c >= 32 || (a == STICKY && c == '\n'))
            append(s, cap, c);
    } else if (a == CHARMAP) {
        int delta = k == 331 ? -1 : k == 333 ? 1 : k == 328 ? -19 : k == 336 ? 19 : 0;
        chidx = clamp(chidx + delta, 0, 94);
        if (k == 28 || (ctrl && k == 46))
            copychar();
    } else if (a == TIPS) {
        if (k == 331 || k == 333)
            helpindex = (helpindex + NGUIDES + (k == 331 ? -1 : 1)) % NGUIDES;
        if (k == 28)
            launch(guides[helpindex].app);
    } else if (a == EVENTS) {
        if (k == 328 || k == 336)
            evpage = clamp(evpage + (k == 328 ? -1 : 1), 0, ecount ? (ecount - 1) / 7 : 0);
    } else
        return 0;
    return 1;
}
