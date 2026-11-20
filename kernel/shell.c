#include "draw.h"
#include "packages.h"
#include "compat.h"
static u8 imported[FILE_LIMIT];
static PeResult pe_result;
static int pe_ran, pe_ok, apk_ran;
static char activity_text[320], activity_status[128];
#include "util.h"
#include "storage.h"
#ifndef EDITION
#define EDITION "Normal"
#endif
#ifndef LITE
#define LITE 0
#endif
/* All state belongs to the live session. Closing an app is distinct from hiding it. */
enum {
    FILES,
    WRITER,
    PAINT,
    CALC,
    CLOCK,
    SETTINGS,
    TERM,
    MINES,
    SYSTEM,
    ANDROID,
    CALENDAR,
    TASKMGR,
    WINDOWS,
    NAPPS
};
struct App {
    const char *name, *cmd, *mark;
    u32 color;
};
static const struct App apps[] = {
    {"Files", "files", "F", 0xf2c443},       {"Writer", "writer", "W", 0x79badf},
    {"Paint", "paint", "P", 0xff849e},       {"Calculator", "calc", "+", 0x83c88c},
    {"Clock", "clock", "C", 0x52cdde},       {"Settings", "settings", "S", 0x86a6bf},
    {"Terminal", "term", ">", 0x5684a2},     {"Minefield", "mines", "*", 0xf5a654},
    {"System", "system", "i", 0xb698d1},     {"Android", "android", "A", 0x9cda6d},
    {"Calendar", "calendar", "D", 0x6fd4bd}, {"Tasks", "monitor", "T", 0xc2b7f1},
    {"EXE Runner", "exe", "E", 0x60afe0}};
static int welcome = 1, active = -1, hidden[NAPPS], menu, tray, choice, cli, shutdown, widget,
           wall, color, motion = !LITE, dim = 10, vol = 5, dirty = 1;
static int wx = 218, wy = 64, dgx, dgy, drag, shift, caps, progress, velocity, goal;
static int setup, installing, finished;
static int lift, lift_speed;
static int year, month, day, hour, minute;
static char tim[8], date[12];
static const u32 tones[] = {0xbbffcf, 0x8defff, 0xffff96, WHITE};
static char note[1400], line[100], history[12][105], lastcmd[100];
static int rows;
static u32 paint[198][466];
static int ink;
static const u32 paints[] = {0x177996, 0x26c67e, 0xff7891, 0xf7c24a, 0x262e3d, WHITE};
static char expr[32] = "0";
static int acc, oper, fresh = 1;
static int grid[64], seen[64], lost, won, cell, seeded;
static u32 seed = 1;
static void say(const char *s) {
    if (rows == 12) {
        for (int i = 1; i < 12; i++)
            copy(history[i - 1], history[i], 105);
        rows--;
    }
    copy(history[rows++], s, 105);
}
static void clockread(void) {
    rtc(&year, &month, &day, &hour, &minute);
    char *p = num(tim, hour);
    *p++ = ':';
    two(p, minute);
    p = num(date, month);
    *p++ = '/';
    num(p, day);
}
static void runapk(const u8 *data, u32 size) {
    const char *error = 0, *message;
    if (size >= 4 && data[0] == 'd' && data[1] == 'e' && data[2] == 'x' && data[3] == 10)
        message = android_activity(data, size); /* pre-R1 Home records */
    else
        message = apk_activity(data, size, &error);
    copy(activity_text, message ? message : "", sizeof activity_text);
    copy(activity_status,
         message ? "Launcher Activity completed"
         : error ? error
                 : "Unsupported legacy Activity",
         sizeof activity_status);
    apk_ran = 1;
    serial(message ? "DEX Activity rendered\r\n" : "APK rejected\r\n");
}
static void runexe(const u8 *data, u32 size) {
    pe_ok = pe_run(data, size, &pe_result);
    pe_ran = 1;
    serial(pe_ok ? (pe_result.bits == 64 ? "PE64 completed\r\n" : "PE32 completed\r\n")
                 : "EXE rejected\r\n");
}
static void launch(int id) {
    if (id < 0 || id >= NAPPS)
        return;
    if (active >= 0 && active != id)
        hidden[active] = 1;
    active = id;
    lift = motion ? 22 : 0;
    lift_speed = 0;
    hidden[id] = 0;
    if (id == ANDROID && !apk_ran)
        runapk(base_apk, BASE_APK_SIZE);
    if (id == WINDOWS && !pe_ran)
        runexe(hello32, HELLO32_SIZE);
    menu = tray = widget = shutdown = 0;
    goal = progress = velocity = 0;
    dirty = 1;
    serial("app: ");
    serial(apps[id].cmd);
    serial("\r\n");
}
static void hide(int keep) {
    if (active >= 0)
        hidden[active] = keep;
    active = -1;
    drag = 0;
    dirty = 1;
}
static void launcher(void) {
    tray = widget = 0;
    menu = !menu;
    goal = menu ? 1024 : 0;
    choice = 0;
    dirty = 1;
}
static void tasks(void) {
    menu = widget = 0;
    progress = goal = velocity = 0;
    tray = !tray;
    choice = 0;
    dirty = 1;
}
static int nth(int n) {
    for (int i = 0; i < NAPPS; i++)
        if (hidden[i] && n-- == 0)
            return i;
    return -1;
}
static void enter(void) {
    welcome = 0;
    dirty = 1;
    serial("desktop ready\r\n");
}
static void cli_set(int mode) {
    cli = mode;
    hide(1);
    menu = tray = 0;
    progress = goal = velocity = 0;
}
static void newgame(void) {
    for (int i = 0; i < 64; i++)
        grid[i] = seen[i] = 0;
    lost = won = seeded = cell = 0;
}
static int near(int a, int b) {
    int x = a % 8 - b % 8, y = a / 8 - b / 8;
    return x >= -1 && x <= 1 && y >= -1 && y <= 1;
}
static void reveal(int k) {
    if (lost || won || seen[k])
        return;
    if (!seeded) {
        seed ^= ticks;
        for (int n = 0; n < 10;) {
            seed = 1664525 * seed + 1013904223;
            int j = (seed >> 16) % 64;
            if (near(k, j) || grid[j])
                continue;
            grid[j] = -1;
            n++;
        }
        for (int i = 0; i < 64; i++)
            if (grid[i] >= 0)
                for (int j = 0; j < 64; j++)
                    if (grid[j] == -1 && near(i, j))
                        grid[i]++;
        seeded = 1;
    }
    seen[k] = 1;
    if (grid[k] < 0) {
        lost = 1;
        return;
    }
    int more;
    do {
        more = 0;
        for (int i = 0; i < 64; i++)
            if (seen[i] && grid[i] == 0)
                for (int j = 0; j < 64; j++)
                    if (!seen[j] && near(i, j)) {
                        seen[j] = 1;
                        more++;
                    }
    } while (more);
    int count = 0;
    for (int i = 0; i < 64; i++)
        count += seen[i];
    won = count == 54;
}
static int value(void) {
    int n = 0, i = expr[0] == '-';
    for (int j = i; expr[j] >= '0' && expr[j] <= '9'; j++)
        n = n * 10 + expr[j] - '0';
    return i ? -n : n;
}
static void calc(char c) {
    if (c == 'c' || c == 'C') {
        copy(expr, "0", 32);
        oper = acc = 0;
        fresh = 1;
        return;
    }
    if (c >= '0' && c <= '9') {
        if (fresh || eq(expr, "0"))
            expr[0] = 0;
        if (len(expr) < 8)
            append(expr, 32, c);
        fresh = 0;
        return;
    }
    if (!c || !(c == '+' || c == '-' || c == '*' || c == '/' || c == '='))
        return;
    int n = value();
    long long v = n;
    if (oper && (!fresh || c == '=')) {
        if (oper == '+')
            v = (long long)acc + n;
        if (oper == '-')
            v = (long long)acc - n;
        if (oper == '*')
            v = (long long)acc * n;
        if (oper == '/') {
            if (!n) {
                copy(expr, "Divide by zero", 32);
                goto error;
            }
            v = acc / n;
        }
    }
    if (v > 99999999 || v < -99999999) {
        copy(expr, "Out of range", 32);
        goto error;
    }
    acc = (int)v;
    num(expr, acc);
    oper = c == '=' ? 0 : c;
    fresh = 1;
    return;
error:
    oper = acc = 0;
    fresh = 1;
}
static const char *saveprefs(void) {
    u8 prefs[] = {1, cli, wall, color, motion, dim, vol};
    return home_save("profile", prefs, sizeof prefs)
               ? "Settings saved."
               : "Needs writable Fresh Home on primary IDE.";
}
static void loadhome(void) {
    u8 p[7];
    if (home_read("profile", p, 7) == 7 && p[0] == 1) {
        cli = p[1] & 1;
        wall = p[2] % 3;
        color = p[3] % 4;
        motion = p[4] & 1;
        dim = clamp(p[5], 1, 10);
        vol = clamp(p[6], 0, 10);
    }
    int n = home_read("note.txt", note, sizeof(note) - 1);
    note[n >= 0 ? n : 0] = 0;
    if (home_read("canvas", paint, sizeof paint) != (int)sizeof paint)
        for (int j = 0; j < 198; j++)
            for (int i = 0; i < 466; i++)
                paint[j][i] = WHITE;
}
static void open_setup(void) {
    if (!installing) {
        setup = 1;
        finished = 0;
        install_probe();
    }
    dirty = 1;
}
static void begin_setup(void) {
    if (install_begin()) {
        setup = 0;
        welcome = 0;
        installing = 1;
        serial("installer copying\r\n");
    }
    dirty = 1;
}
static void command(void) {
    copy(lastcmd, line, sizeof lastcmd);
    char s[100];
    copy(s, line, 100);
    line[0] = 0;
    if (!*s)
        return;
    say(s);
    if (prefix(s, "open ")) {
        for (int i = 5, n = len(s); i <= n; i++)
            s[i - 5] = s[i];
    }
    for (int i = 0; i < NAPPS; i++)
        if (eq(s, apps[i].cmd)) {
            launch(i);
            return;
        }
    int sl = len(s);
    int forcedapk = prefix(s, "apk "), forcedexe = prefix(s, "exe ");
    int apk = forcedapk || (!forcedexe && sl > 4 && eq(s + sl - 4, ".apk"));
    int exe = forcedexe || (!forcedapk && sl > 4 && eq(s + sl - 4, ".exe"));
    if (apk || exe) {
        const char *file = forcedapk || forcedexe ? s + 4 : s;
        int demo = eq(file, "demo");
        int n = demo ? -2 : home_read(file, imported, sizeof imported);
        if (n > 0) {
            if (apk)
                runapk(imported, n);
            else
                runexe(imported, n);
        } else if (n == -2 && apk && (demo || eq(file, "base.apk")))
            runapk(base_apk, BASE_APK_SIZE);
        else if (n == -2 && exe && (demo || eq(file, "hello32.exe")))
            runexe(hello32, HELLO32_SIZE);
        else if (n == -2 && exe && eq(file, "hello64.exe"))
            runexe(hello64, HELLO64_SIZE);
        else {
            say(n == -2 ? "Package not found. Type storage."
                        : "Package empty, corrupt, unreadable, or over 2 MiB.");
            return;
        }
        launch(apk ? ANDROID : WINDOWS);
        return;
    }
    if (eq(s, "start") || eq(s, "apps")) {
        say("START  |  Type an app name, then Enter.");
        say("files   writer   paint   calc   clock   settings   term   mines   system");
        say("android   calendar   monitor   exe");
    } else if (eq(s, "help")) {
        say("start / apps   open NAME   tasks   resume NAME   clear   gui   cli");
        say("wall sky|lagoon|paint   color mint|ice|sun|white   time   cat note   storage");
    } else if (eq(s, "storage")) {
        say(disk_status());
        say("save NAME / load NAME    savecanvas    prefs    ls    apk/exe NAME    install");
    } else if (eq(s, "install"))
        open_setup();
    else if (eq(s, "prefs"))
        say(saveprefs());
    else if (eq(s, "savecanvas"))
        say(home_save("canvas", paint, sizeof paint) ? "Canvas saved."
                                                     : "Canvas not saved. Type storage.");
    else if (eq(s, "ls")) {
        if (!home_list(say))
            say("No Fresh Home mounted.");
    } else if (prefix(s, "save ")) {
        if (eq(s + 5, "profile") || eq(s + 5, "canvas"))
            say("Reserved name.");
        else
            say(home_save(s + 5, note, len(note)) ? "Note saved."
                                                  : "Not saved. Use a-z, digits, dot, dash, "
                                                    "underscore; 31 max. Check storage.");
    } else if (prefix(s, "load ")) {
        char tmp[sizeof note];
        int n = home_read(s + 5, tmp, sizeof(tmp) - 1);
        if (n >= 0) {
            tmp[n] = 0;
            copy(note, tmp, sizeof note);
            say("Loaded into Writer.");
        } else
            say("Note not found or unreadable.");
    } else if (eq(s, "tasks")) {
        int any = 0;
        for (int i = 0; i < NAPPS; i++)
            if (hidden[i]) {
                say(apps[i].cmd);
                any = 1;
            }
        if (!any)
            say("No minimized apps.");
    } else if (prefix(s, "resume ")) {
        for (int i = 0; i < NAPPS; i++)
            if (hidden[i] && eq(s + 7, apps[i].cmd)) {
                launch(i);
                return;
            }
        say("No such minimized app.");
    } else if (eq(s, "clear"))
        rows = 0;
    else if (eq(s, "cli"))
        cli_set(1);
    else if (eq(s, "gui"))
        cli_set(0);
    else if (eq(s, "time")) {
        say(tim);
        say(date);
    } else if (eq(s, "cat note"))
        say(note[0] ? note : "(empty note)");
    else if (eq(s, "wall sky"))
        wall = 0;
    else if (eq(s, "wall lagoon"))
        wall = 1;
    else if (eq(s, "wall paint"))
        wall = 2;
    else if (eq(s, "color mint"))
        color = 0;
    else if (eq(s, "color ice"))
        color = 1;
    else if (eq(s, "color sun"))
        color = 2;
    else if (eq(s, "color white"))
        color = 3;
    else
        say("Unknown command. Type help or start.");
}
static void bg(void) {
    if (wall == 0)
        sprite(SKY, SKY_W, SKY_H, 0, 0, W, H, 0);
    else if (wall == 1) {
        for (int y = 0; y < H; y++)
            box(0, y, W, 1, mix(0x055872, 0xa7efd9, y * 256 / H));
    } else
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                screen[y][x] = paint[y * 198 / H][x * 466 / W];
}
static void icon(int x, int y, int i, int r) {
    static const u16 *const glyphs[] = {ICON0, ICON1, ICON2, ICON3,  ICON4,  ICON5, ICON6,
                                        ICON7, ICON8, ICON9, ICON10, ICON11, ICON6};
    sprite(glyphs[i], 64, 64, x, y, r, r, 1);
}
static void rail(void) { /* Keep the reference rail silhouette; don't paint old sky over a new
                            wallpaper. */
    for (int y = 0; y < 333; y++)
        for (int x = 0; x < 34; x++) {
            int ok = y < 221   ? x < 31
                     : y < 241 ? ((x - 15) * (x - 15) + (y - 221) * (y - 221) < 360)
                               : ((x + 24) * (x + 24) + (y - 282) * (y - 282) < 3300);
            if (ok)
                screen[y][x] = rgb(RAIL[y * 34 + x]);
        }
    gel(2, 2, 27, 24, 12, WHITE, 0x5baaf7, 0x156cc0);
    text(10, 4, "F", 1, WHITE);
    box(2, 27, 27, 12, 0x9cefff);
    text(3, 26, "Files", 0, INK);
    text(1, 147, tim, 0, INK);
    text(1, 159, date, 0, INK);
}
static void bubble(int x, int y, int w, int h, u32 mid) {
    gel(x, y, w, h, h / 2, WHITE, mid, mix(mid, WHITE, 154));
    for (int j = 0; j < 13; j++)
        box(x + w / 4, y + h - 2 + j, 17 - j, 1, mix(mid, WHITE, 154));
}
static void startmenu(void) {
    int p = clamp(progress, 1, 1130), w = 236 * p / 1024, h = 185 * p / 1024, x = 173 - w / 2,
        y = 276 - w / 2;
    sprite(BURST, 236, 236, x, y, w, w, 1);
    sprite(BURST, 236, 236, 331 - h / 2, 146 - h / 2, h, h, 1);
    if (p < 940 || !menu)
        return;
    text(130, 173, "All Apps", 2, INK);
    text(295, 122, tim, 2, INK);
    text(311, 157, date, 1, INK);
    for (int i = choice / 9 * 9; i < NAPPS && i < choice / 9 * 9 + 9; i++) {
        int a = 95 + i % 3 * 60, b = 205 + (i % 9) / 3 * 46;
        if (choice == i)
            gel(a - 6, b - 1, 51, 42, 10, 0xd4fbff, 0x86eaff, 0xcefaff);
        icon(a + 8, b, i, 23);
        text(a - 4, b + 23, apps[i].name, 0, INK);
    }
    text(184, 349, choice < 9 ? "More >" : "< Back", 0, INK);
    gel(164, 368, 18, 18, 9, WHITE, 0x485965, 0xb4f9b7);
    text(169, 368, "o", 0, WHITE);
    bubble(2, 100, 91, 61, 0x40f490);
    text(12, 116, nth(0) < 0 ? "No apps" : "Apps ready", 1, INK);
    text(19, 137, "in Tray", 0, INK);
}
static void traybox(void) {
    int n = 0;
    while (nth(n) >= 0)
        n++;
    int x = cli ? 125 : 39, y = 106;
    gel(x, y, 186, 56 + n * 30, 18, WHITE, 0x94f5d6, 0xb2fbec);
    text(x + 14, y + 8, "Minimized apps", 1, INK);
    if (!n)
        text(x + 14, y + 32, "The tray is empty.", 0, INK);
    for (int i = 0; i < n; i++) {
        if (choice == i)
            gel(x + 7, y + 31 + i * 30, 170, 27, 10, WHITE, 0x41dfa9, 0xaffcdd);
        text(x + 16, y + 34 + i * 30, apps[nth(i)].name, 1, INK);
    }
}
static void console(int x, int y, int w, int h, int full) {
    gel(x, y, w, h, 12, 0x164556, 0x103e50, 0x123c4a);
    text(x + 16, y + 12, "FRESH SHELL    /    LIVE SESSION", 0, 0x8defff);
    int max = full ? 12 : 8, start = rows > max ? rows - max : 0;
    for (int i = start; i < rows; i++) {
        char b[105];
        copy(b, history[i], full ? 102 : 58);
        text(x + 16, y + 36 + (i - start) * 19, b, 0, tones[color]);
    }
    int yy = y + 39 + (rows - start) * 19;
    text(x + 16, yy, "fresh >", 1, tones[color]);
    int off = len(line) > (full ? 78 : 43) ? len(line) - (full ? 78 : 43) : 0;
    int end = text(x + 78, yy, line + off, 1, WHITE);
    box(end + 1, yy + 3, 2, 15, tones[color]);
    const char *query = prefix(line, "open ") ? line + 5 : line;
    if (*query) {
        int tx = x + 16;
        for (int i = 0; i < NAPPS; i++)
            if (prefix(apps[i].cmd, query))
                tx = text(tx, yy + 28, apps[i].cmd, 0, tones[color]) + 12;
    }
}
static void settings(int x, int y) {
    const char *labels[] = {"Desktop shell", "Wallpaper",  "Shell text",
                            "Motion",        "Brightness", "Sound level"};
    const char *opts[] = {cli ? "Command line" : "Graphical",
                          wall == 0   ? "Sky Crisp"
                          : wall == 1 ? "Lagoon"
                                      : "My canvas",
                          (const char *[]){"Mint", "Ice", "Sun", "White"}[color],
                          motion ? "Spring" : "Still",
                          "",
                          ""};
    for (int i = 0; i < 6; i++) {
        int yy = y + i * 34;
        text(x, yy + 4, labels[i], 1, INK);
        char b[10];
        if (i >= 4) {
            num(b, (i == 4 ? dim : vol) * 10);
            append(b, 10, '%');
        }
        badge(x + 307, yy, i >= 4 ? b : opts[i], i == 0);
    }
    text(x, y + 212, "Click a value to cycle it. F3 toggles the shell from anywhere.", 0,
         INK);
}
static int monthoff;
static void calendar(int x, int y) {
    static const char *names[] = {"January",   "February", "March",    "April",
                                  "May",       "June",     "July",     "August",
                                  "September", "October",  "November", "December"};
    int m = month + monthoff, yr = year;
    while (m < 1) {
        m += 12;
        yr--;
    }
    while (m > 12) {
        m -= 12;
        yr++;
    }
    int end = text(x, y, names[m - 1], 2, INK);
    char b[12];
    num(b, yr);
    text(end + 8, y, b, 2, INK);
    text(x + 333, y + 8, "<  month  >", 1, INK);
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int leap = yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0);
    int cnt = days[m - 1] + (m == 2 && leap);
    static const int off[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int z = yr - (m < 3);
    int start = (z + z / 4 - z / 100 + z / 400 + off[m - 1] + 1) % 7;
    const char *week[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (int i = 0; i < 7; i++)
        text(x + i * 64, y + 45, week[i], 0, INK);
    for (int i = 1; i <= cnt; i++) {
        int k = start + i - 1, xx = x + k % 7 * 64, yy = y + 70 + k / 7 * 27;
        if (!monthoff && i == day)
            gel(xx - 4, yy - 1, 31, 22, 10, WHITE, MINT, 0xb4ffdd);
        num(b, i);
        text(xx, yy, b, 1, INK);
    }
}
static void wrapped(int x, int y, int width, const char *s) {
    int tx = x;
    for (int i = 0, top = y; s && s[i] && i < 320 && y < top + 113; i++) {
        char b[] = {s[i], 0};
        if (tx > x + width || s[i] == '\n') {
            tx = x;
            y += 22;
        }
        if (s[i] != '\n')
            tx = text(tx, y, b, 1, INK);
    }
}
static void window(void) {
    int x = wx, y = clamp(wy + lift, 0, H - 311), a = active;
    gel(x + 3, y + 6, 506, 311, 15, 0x769ca6, 0x637f8b, 0xa4bcc1);
    gel(x, y, 506, 311, 15, WHITE, 0x35efbf, 0xeafffb);
    gel(x + 2, y + 30, 502, 279, 12, 0xf9ffff, 0xeaf7f9, 0xf7fdfd);
    icon(x + 9, y + 3, a, 23);
    text(x + 38, y + 5, apps[a].name, 1, INK);
    text(x + 412, y + 7, "_", 1, INK);
    text(x + 469, y + 4, "x", 1, INK);
    int xx = x + 19, yy = y + 45;
    if (a == ANDROID) {
        text(xx, yy, "Android Activity", 2, INK);
        text(xx, yy + 39, "DALVIK BYTECODE    /    TEXTVIEW", 0, 0x347e65);
        wrapped(xx, yy + 83, 444, activity_text[0] ? activity_text : activity_status);
        text(xx, yy + 212, "R1   /   Standalone APK loader and TextView runtime", 0, INK);
    }
    if (a == WINDOWS) {
        text(xx, yy, "Windows executable", 2, INK);
        text(xx, yy + 39, pe_result.bits == 64 ? "PE32+    /    x86-64" : "PE    /    x86", 0,
             0x347e65);
        wrapped(xx, yy + 70, 445, pe_ok ? pe_result.output : pe_result.detail);
        char code[16];
        num(code, (int)pe_result.exit_code);
        int tx = text(xx, yy + 209,
                      pe_ok ? "Program exited with code "
                            : "Stopped: unsupported or invalid program.",
                      1, INK);
        if (pe_ok)
            text(tx, yy + 209, code, 1, INK);
    }
    if (a == CALENDAR)
        calendar(xx, yy);
    if (a == TASKMGR) {
        text(xx, yy, "App sessions", 2, INK);
        text(xx, yy + 38, "Tasks   /   Foreground", 1, INK);
        int n = 0;
        for (int i = 0; i < NAPPS; i++)
            if (hidden[i]) {
                text(xx + (n / 6) * 215, yy + 73 + n % 6 * 24, apps[i].name, 1, INK);
                text(xx + 96 + (n / 6) * 215, yy + 77 + n % 6 * 24, "Minimized", 0, INK);
                n++;
            }
        if (!n)
            text(xx, yy + 73, "Nothing minimized.", 1, INK);
    }
    if (a == TERM)
        console(x + 9, y + 36, 488, 265, 0);
    if (a == SETTINGS)
        settings(xx, yy);
    if (a == SYSTEM) {
        text(xx, yy, "FreshOS Release 1", 2, INK);
        text(xx, yy + 36, EDITION " edition   /   native live kernel", 1, INK);
        text(xx, yy + 72,
#ifdef EFI
             "64-bit x86 long mode  /  UEFI GOP framebuffer"
#else
             "32-bit x86 protected mode  /  VBE framebuffer"
#endif
             ,
             1, INK);
        text(xx, yy + 99, "Input: i8042 keyboard and relative PS/2 mouse", 1, INK);
        text(xx, yy + 126, disk_status(), 1, INK);
        text(xx, yy + 172, "F1  Launcher     F2 / Win  Tray     F3  GUI / CLI", 1, INK);
        text(xx, yy + 198, "F4  Minimize     Esc  Back     F6 / F7  Sound / Light", 1, INK);
    }
    if (a == FILES) {
        text(xx, yy, "Live workspace", 2, INK);
        text(xx, yy + 36,
             home_ready() ? "Fresh Home mounted. Use Terminal to save your work."
                          : "Session files stay in memory until you power off.",
             1, INK);
        badge(xx, yy + 81, "Note.txt", 0);
        badge(xx + 130, yy + 81, "Canvas", 0);
        text(xx, yy + 134, "Release 1   /   Included packages", 0, INK);
        badge(xx, yy + 158, "base.apk", 0);
        badge(xx + 130, yy + 158, "hello32.exe", 0);
        badge(xx + 260, yy + 158, "hello64.exe", 0);
    }
    if (a == WRITER) {
        // Find the last twelve visual lines so the insertion point never scrolls offscreen.
        int starts[12] = {0}, row = 0, width = 0;
        for (int i = 0; note[i]; i++) {
            unsigned ch = (u8)note[i];
            int adv = ch >= 32 && ch <= 126 ? fonts[0].adv[ch - 32] : 0;
            if (ch == '\n' || width + adv > 451) {
                starts[++row % 12] = i + (ch == '\n');
                width = 0;
            }
            width += adv;
        }
        int tx = xx, ty = yy;
        for (int i = row >= 12 ? starts[(row - 11) % 12] : 0; note[i]; i++) {
            unsigned ch = (u8)note[i];
            int adv = ch >= 32 && ch <= 126 ? fonts[0].adv[ch - 32] : 0;
            if (ch == '\n' || tx + adv > xx + 451) {
                tx = xx;
                ty += 17;
            }
            if (ch != '\n') {
                char c[2] = {ch, 0};
                tx = text(tx, ty, c, 0, INK);
            }
        }
        box(tx, ty + 2, 2, 12, MINT);
        text(xx, y + 278, "Note.txt   /   Session memory   /   Backspace to edit", 0, INK);
    }
    if (a == PAINT) {
        for (int j = 0; j < 198; j++)
            for (int i = 0; i < 466; i++)
                screen[yy + j][xx + i] = paint[j][i];
        for (int i = 0; i < 6; i++) {
            if (i == ink)
                box(xx + i * 32 - 2, yy + 207, 28, 27, INK);
            box(xx + i * 32, yy + 209, 24, 23, paints[i]);
        }
        badge(xx + 315, yy + 205, "Set wallpaper", 1);
    }
    if (a == CALC) {
        gel(xx, yy, 466, 54, 10, 0xbbe7df, 0xdffdf3, 0xc8efdf);
        text(xx + 15, yy + 10, expr, 2, INK);
        const char *k = "789/456*123-0C=+";
        for (int i = 0; i < 16; i++) {
            int bx = xx + (i % 4) * 117, by = yy + 62 + i / 4 * 42;
            gel(bx, by, 108, 36, 12, WHITE, 0x9ae2e2, 0xe2ffff);
            char b[] = {k[i], 0};
            text(bx + 45, by + 6, b, 1, INK);
        }
    }
    if (a == CLOCK) {
        text(xx, yy, "Local hardware clock", 2, INK);
        text(xx, yy + 45, tim, 2, INK);
        char b[16];
        num(b, year);
        int end = text(xx, yy + 85, b, 1, INK);
        text(end + 8, yy + 85, date, 1, INK);
        text(xx, yy + 133, "Time comes from the PC real-time clock.", 1, INK);
    }
    if (a == MINES) {
        for (int i = 0; i < 64; i++) {
            int bx = xx + (i % 8) * 28, by = yy + (i / 8) * 28;
            u32 c = seen[i] ? 0xe3f3f4 : 0x65cfc6;
            box(bx, by, 26, 26, i == cell ? 0x19879b : c);
            box(bx + 2, by + 2, 22, 22, c);
            if (seen[i] || lost) {
                char b[] = {grid[i] < 0 ? '*' : grid[i] ? '0' + grid[i] : ' ', 0};
                text(bx + 8, by + 2, b, 1, INK);
            }
        }
        text(xx + 247, yy + 6, "Minefield", 2, INK);
        text(xx + 247, yy + 47,
             lost  ? "Hit a mine!"
             : won ? "Field cleared!"
                   : "Find 54 clear tiles.",
             1, INK);
        text(xx + 247, yy + 83, "Arrows + Enter or click.", 0, INK);
        badge(xx + 247, yy + 132, "New field", 0);
    }
}
static void render(void) {
    if (welcome) {
        sprite(WELCOME, WELCOME_W, WELCOME_H, 0, 0, W, H, 0);
        gel(12, H - 29, 221, 21, 10, 0xf9ffff, 0xb1eaf1, 0xdfffff);
        text(20, H - 26, "Release 1  /  " EDITION " live  /  Enter to begin", 0, INK);
        badge(W - 132, H - 44, "Install  /  I", 0);
    } else {
        bg();
        if (!cli)
            rail();
        else
            console(68, 53, 824, 401, 1);
        if (active >= 0)
            window();
        if (progress && !cli)
            startmenu();
        if (tray) {
            if (cli) {
                gel(110, 85, 550, 252, 9, 0x153a46, 0x164551, 0x123844);
                text(132, 105, "TRAY  /  Arrows to select  /  Enter to restore", 1,
                     tones[color]);
                int n = 0;
                for (int j; (j = nth(n)) >= 0; n++) {
                    text(132, 143 + n * 19, n == choice ? ">" : " ", 0, tones[color]);
                    text(156, 143 + n * 19, apps[j].cmd, 0, WHITE);
                }
                if (!n)
                    text(132, 143, "No minimized apps. Esc to return.", 0, WHITE);
            } else
                traybox();
        }
        if (widget) {
            bubble(508, 420, 132, 66, 0xfff53e);
            text(524, 429, widget == 1 ? "Volume" : "Brightness", 1, INK);
            gel(520, 451, 105, 21, 10, WHITE, 0x19cff4, 0xdcfbff);
            int level = widget == 1 ? vol : dim;
            if (level)
                gel(520, 451, level * 10, 21, 9, WHITE, 0x65f700, 0xccff9c);
            gel(481, 491, 63, 61, 28, WHITE, 0xffee20, 0xfff472);
            text(499, 502, ":)", 2, INK);
        }
        if (shutdown) {
            gel(295, 195, 370, 136, 20, WHITE, 0xbefff0, 0xf7ffff);
            text(315, 210, "End live session?", 2, INK);
            text(315, 250, "Your note and canvas will be lost.", 1, INK);
            badge(316, 280, "Cancel", 0);
            badge(449, 280, "Halt", 1);
        }
    }
    if (installing) {
        gel(W - 241, 10, 231, 38, 16, WHITE, MINT, 0xbaffdd);
        char pct[8];
        num(pct, install_percent());
        append(pct, 8, '%');
        text(W - 226, 18, "Installing FreshOS", 1, INK);
        text(W - 81, 18, pct, 1, INK);
    }
    if (setup) {
        gel(198, 148, 570, 244, 26, WHITE, 0x98fce0, 0xebfffa);
        text(222, 168, finished ? "FreshOS setup result" : "Install FreshOS", 2, INK);
        text(222, 212, disk_status(), 1, INK);
        text(222, 252, "Source: FreshOS CD (secondary IDE master)", 1, INK);
        text(222, 280, "Destination: specially prepared primary IDE disk only.", 1, INK);
        text(222, 311, "Never formats or migrates an existing Windows drive.", 0, INK);
        badge(225, 341, "Back / Esc", 0);
        if (!finished)
            badge(611, 341, "Install / I", 1);
    }
    if (dim < 10) {
        int k = 90 + dim * 16;
        for (int j = 0; j < H; j++)
            for (int i = 0; i < W; i++)
                screen[j][i] = mix(0, screen[j][i], k);
    }
    if (welcome || setup || !cli || active >= 0) {
        gel(mx - 4, my - 4, 10, 10, 5, WHITE, 0x00c2e8, 0xc0ffff);
    }
    present();
}
static void sound(void);
static void setting(int i) {
    switch (i) {
    case 0:
        cli_set(!cli);
        break;
    case 1:
        wall = (wall + 1) % 3;
        break;
    case 2:
        color = (color + 1) % 4;
        break;
    case 3:
        motion = !motion;
        break;
    case 4:
        dim = dim <= 2 ? 10 : dim - 2;
        break;
    case 5:
        vol = (vol + 1) % 11;
        sound();
        break;
    }
}
static int inside(int x, int y, int a, int b, int w, int h) {
    return x >= a && x < a + w && y >= b && y < b + h;
}
void pointer(int old, int x, int y) {
    if (cli && active < 0 && !setup && !welcome)
        return;
    dirty = 1;
    int click = (buttons & 1) && !(old & 1);
    if (!(buttons & 1))
        drag = 0;
    if (setup) {
        if (click && inside(x, y, 225, 341, 114, 31))
            setup = 0;
        else if (click && !finished && inside(x, y, 611, 341, 114, 31))
            begin_setup();
        return;
    }
    if (welcome) {
        if (click && inside(x, y, W - 132, H - 44, 114, 31))
            open_setup();
        else if (click && inside(x, y, 456, 367, 108, 173))
            enter();
        return;
    }
    if (shutdown) {
        if (click) {
            if (inside(x, y, 449, 280, 114, 31) && !installing)
                halt();
            shutdown = 0;
        }
        return;
    }
    if (drag) {
        wx = clamp(x - dgx, 0, W - 510);
        wy = clamp(y - dgy, 0, H - 320);
        return;
    }
    if (widget && click) {
        if (inside(x, y, 516, 447, 119, 32)) {
            int v = clamp((x - 520 + 5) / 10, 0, 10);
            if (widget == 1) {
                vol = v;
                sound();
            } else
                dim = clamp(v, 1, 10);
            return;
        }
        widget = 0;
    }
    if (tray && click) {
        int j = (y - 137) / 30;
        if (inside(x, y, 45, 137, 174, NAPPS * 30) && nth(j) >= 0)
            launch(nth(j));
        else
            tray = 0;
        return;
    }
    if (menu && click) {
        if (inside(x, y, 175, 342, 62, 25)) {
            choice = choice < 9 ? 9 : 0;
            return;
        }
        for (int i = choice / 9 * 9; i < NAPPS && i < choice / 9 * 9 + 9; i++)
            if (inside(x, y, 87 + i % 3 * 60, 202 + (i % 9) / 3 * 46, 59, 46)) {
                launch(i);
                return;
            }
        if (inside(x, y, 155, 359, 38, 33)) {
            shutdown = 1;
        }
        launcher();
        return;
    }
    if (!cli && click && x < 35) {
        if (y < 42)
            launch(FILES);
        else if (y >= 140 && y < 171)
            launch(CLOCK);
        else if (y >= 171 && y < 194)
            tasks();
        else if (y >= 194 && y < 218)
            widget = 2;
        else if (y >= 218 && y < 243)
            widget = 1;
        else if (y >= 244 && y < 332)
            launcher();
        return;
    }
    if (active < 0)
        return;
    int rx = x - wx, ry = y - clamp(wy + lift, 0, H - 311);
    if (click && inside(rx, ry, 0, 0, 506, 30)) {
        if (rx > 449)
            hide(0);
        else if (rx > 389)
            hide(1);
        else {
            dgx = rx;
            dgy = ry;
            drag = 1;
        }
        return;
    }
    int a = rx - 19, b = ry - 45;
    if (active == PAINT && (buttons & 1) && inside(a, b, 0, 0, 466, 198)) {
        for (int j = -3; j < 4; j++)
            for (int i = -3; i < 4; i++)
                if (i * i + j * j <= 9 && inside(a + i, b + j, 0, 0, 466, 198))
                    paint[b + j][a + i] = paints[ink];
        return;
    }
    if (!click)
        return;
    if (active == CALENDAR && inside(a, b, 326, 0, 132, 38))
        monthoff = clamp(monthoff + (a < 375 ? -1 : 1), -1200, 1200);
    if (active == SETTINGS && inside(a, b, 307, 0, 114, 204))
        setting(b / 34);
    if (active == FILES && b >= 158 && b < 189 && a >= 0 && a < 374 && a % 130 < 114) {
        int i = a / 130;
        if (!i)
            runapk(base_apk, BASE_APK_SIZE);
        else if (i == 1)
            runexe(hello32, HELLO32_SIZE);
        else
            runexe(hello64, HELLO64_SIZE);
        launch(i ? WINDOWS : ANDROID);
        return;
    }
    if (active == FILES && b >= 81 && b < 112) {
        if (a >= 0 && a < 114)
            launch(WRITER);
        if (a >= 130 && a < 244)
            launch(PAINT);
    }
    if (active == PAINT && b >= 205 && b < 239) {
        if (a >= 0 && a < 192)
            ink = a / 32;
        if (a >= 315 && a < 429)
            wall = 2;
    }
    if (active == CALC && inside(a, b, 0, 62, 466, 168))
        calc("789/456*123-0C=+"[(b - 62) / 42 * 4 + a / 117]);
    if (active == MINES) {
        if (inside(a, b, 0, 0, 224, 224)) {
            cell = b / 28 * 8 + a / 28;
            reveal(cell);
        } else if (inside(a, b, 247, 132, 114, 31))
            newgame();
    }
}
static const char plain[] =
    "\0\0"
    "1234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char upper[] =
    "\0\0"
    "!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
void key(int k, int down) {
    if (k == 42 || k == 54) {
        shift = down;
        return;
    }
    if (!down)
        return;
    dirty = 1;
    if (setup) {
        if (k == 1 || k == 28)
            setup = 0;
        else if (k == 23 && !finished)
            begin_setup();
        return;
    }
    if (welcome) {
        if (k == 23)
            open_setup();
        if (k == 28 || k == 57)
            enter();
        return;
    }
    if (shutdown) {
        if (k == 28 && !installing)
            halt();
        shutdown = 0;
        return;
    }
    if (k == 58) {
        caps = !caps;
        return;
    }
    if (k == 61) {
        cli_set(!cli);
        return;
    }
    if (k == 60 || k == 347 || k == 348) {
        tasks();
        return;
    }
    if (k == 62) {
        hide(1);
        return;
    }
    if (k == 64 || k == 65) {
        widget = k == 64 ? 1 : 2;
        return;
    }
    if (widget) {
        if (k == 331 || k == 333) {
            int v = k == 331 ? -1 : 1;
            if (widget == 1) {
                vol = clamp(vol + v, 0, 10);
                sound();
            } else
                dim = clamp(dim + v, 1, 10);
        } else
            widget = 0;
        return;
    }
    if (k == 59) {
        if (cli) {
            hide(1);
            copy(line, "start", 100);
            command();
        } else
            launcher();
        return;
    }
    if (k == 1) {
        if (tray)
            tray = 0;
        else if (menu)
            launcher();
        else if (active >= 0)
            hide(1);
        return;
    }
    if (tray) {
        int n = 0;
        while (nth(n) >= 0)
            n++;
        if (k == 336 && n)
            choice = (choice + 1) % n;
        if (k == 328 && n)
            choice = (choice + n - 1) % n;
        if (k == 28 && nth(choice) >= 0)
            launch(nth(choice));
        return;
    }
    if (menu) {
        if (k == 328)
            choice = (choice + NAPPS - 3) % NAPPS;
        if (k == 336)
            choice = (choice + 3) % NAPPS;
        if (k == 331)
            choice = (choice + NAPPS - 1) % NAPPS;
        if (k == 333 || k == 15)
            choice = (choice + 1) % NAPPS;
        if (k == 28)
            launch(choice);
        return;
    }
    char c = k < (int)sizeof plain ? (shift ? upper[k] : plain[k]) : 0;
    if (caps && c >= 'a' && c <= 'z')
        c -= 32;
    else if (caps && c >= 'A' && c <= 'Z')
        c += 32;
    if (active == TERM || (active < 0 && cli)) {
        if (k == 328) {
            copy(line, lastcmd, 100);
            return;
        }
        if (k == 15) {
            if (len(line))
                for (int i = 0; i < NAPPS; i++)
                    if (prefix(apps[i].cmd, line)) {
                        copy(line, apps[i].cmd, 100);
                        break;
                    }
            return;
        }
        if (c == '\n')
            command();
        else if (c == '\b') {
            if (len(line))
                line[len(line) - 1] = 0;
        } else if (c >= 32)
            append(line, 100, c);
        return;
    }
    if (active == WRITER) {
        if (c == '\b') {
            if (len(note))
                note[len(note) - 1] = 0;
        } else if (c == '\n' || c >= 32)
            append(note, sizeof note, c);
        return;
    }
    if (active == CALC) {
        calc(c == '\n' ? '=' : c);
        return;
    }
    if (active == MINES) {
        if (k == 331)
            cell = clamp(cell - 1, 0, 63);
        if (k == 333)
            cell = clamp(cell + 1, 0, 63);
        if (k == 336)
            cell = clamp(cell + 8, 0, 63);
        if (k == 328)
            cell = clamp(cell - 8, 0, 63);
        if (k == 28)
            reveal(cell);
        if (c == 'r')
            newgame();
        return;
    }
    if (active == CALENDAR && (k == 331 || k == 333))
        monthoff = clamp(monthoff + (k == 331 ? -1 : 1), -1200, 1200);
    if (active == SETTINGS && c >= '1' && c <= '6') {
        setting(c - '1');
        return;
    }
    if (active < 0 && k == 28)
        launcher();
}
extern void poll(void), beep(int), silence(void);
static u32 quiet;
static void sound(void) {
    if (vol) {
        beep(450 + vol * 50);
        quiet = ticks + 1 + vol / 2;
    } else
        silence();
}
void main(void) {
    hw_init();
    clockread();
    for (int j = 0; j < 198; j++)
        for (int i = 0; i < 466; i++)
            paint[j][i] = WHITE;
    storage_init();
    if (home_ready())
        loadhome();
    newgame();
    say("FreshOS Shell  /  Type start for apps or help for commands.");
    serial("welcome ready: " EDITION "\r\n");
    u32 last = 0, clocktick = ticks;
    for (;;) {
        poll();
        u32 now = ticks;
        if (quiet && (int)(now - quiet) >= 0) {
            silence();
            quiet = 0;
        }
        if (now - clocktick >= 60) {
            int before = minute;
            clockread();
            clocktick = now;
            if (before != minute)
                dirty = 1;
        }
        if (now != last) {
            last = now;
            if (installing) {
                int res = install_step();
                if (res) {
                    installing = 0;
                    finished = setup = 1;
                    serial(res == 1 ? "installation verified\r\n"
                                    : "installation failed\r\n");
                }
                dirty = 1;
            }
            if (lift || lift_speed) {
                lift_speed = (lift_speed * 67 - lift * 24) / 100;
                lift += lift_speed;
                if (!lift_speed && lift >= -3 && lift <= 3)
                    lift = 0;
                dirty = 1;
            }
            if (progress != goal || velocity) {
                if (!motion) {
                    progress = goal;
                    velocity = 0;
                } else {
                    velocity = (velocity * 71 + (goal - progress) * 14) / 100;
                    progress += velocity;
                    if ((progress - goal < 6 && goal - progress < 6 && velocity < 4 &&
                         velocity > -4) ||
                        (!velocity && progress != goal)) {
                        progress = goal;
                        velocity = 0;
                    }
                }
                dirty = 1;
            }
            if (dirty) {
                dirty = 0;
                render();
            }
        }
        __asm__ volatile("hlt");
    }
}
