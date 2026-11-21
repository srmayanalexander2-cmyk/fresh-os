#include "draw.h"
#include "packages.h"
#include "compat.h"
#include "filetools.h"
#include "aerojs.h"
#include "webnet.h"
static u8 imported[IMP_LIMIT], converted[IMP_LIMIT];
static PeResult pe_result;
static int pe_ran, pe_ok, last_imp;
#include "util.h"
#include "panic.inc"
#include "storage.h"
#include "drivers.h"
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
    CALENDAR,
    TASKMGR,
    WINDOWS,
    CONNECTIONS,
    STICKY,
    TODO,
    CHARMAP,
    TIPS,
    EVENTS,
    CENTER,
    PHOTOS,
    SOFTWARE,
    BROWSER,
    NAPPS
};
struct App {
    const char *name, *cmd, *mark;
    u32 color;
};
static const struct App apps[] = {{"Files", "files", "F", 0xf2c443},
                                  {"Writer", "writer", "W", 0x79badf},
                                  {"Paint", "paint", "P", 0xff849e},
                                  {"Calculator", "calc", "+", 0x83c88c},
                                  {"Clock", "clock", "C", 0x52cdde},
                                  {"Settings", "settings", "S", 0x86a6bf},
                                  {"Terminal", "term", ">", 0x5684a2},
                                  {"Minefield", "mines", "*", 0xf5a654},
                                  {"System", "system", "i", 0xb698d1},
                                  {"Calendar", "calendar", "D", 0x6fd4bd},
                                  {"Tasks", "monitor", "T", 0xc2b7f1},
                                  {"Programs", "programs", "E", 0x60afe0},
                                  {"Connections", "connections", "N", 0x63d5b3},
                                  {"Stickies", "sticky", "P", 0xffec85},
                                  {"To Do", "todo", "T", 0x65ceab},
                                  {"Glyphs", "charmap", "A", 0x72a3ce},
                                  {"Tips", "tips", "?", 0xcbd756},
                                  {"Events", "events", "E", 0x67bbca},
                                  {"Actions", "actions", "+", 0x81cba9},
                                  {"Photos", "photos", "", 0x96d4ba},
                                  {"Software", "software", "", 0xb1dcea},
                                  {"Aero Browser", "browser", "", 0x55aade}};
static int welcome = 1, active = -1, hidden[NAPPS], menu, tray, choice, cli, shutdown, widget,
           wall, color, motion = !LITE, dim = 10, vol = 5, dirty = 1;
static int wx = (W - 506) / 2, wy = (H - 311) / 2, dgx, dgy, drag, shift, caps, progress,
           velocity, goal;
static int setup, installing, finished;
static int lift, lift_speed;
static int year, month, day, hour, minute;
static char tim[8], date[12];
static const u32 tones[] = {0xbbffcf, 0x8defff, 0xffff96, WHITE};
static char note[1400], line[100], history[96][105], lastcmd[100];
static int rows, scroll, cmdpos, recall, ncmd;
static char commands[16][100];
static int hwpage = -1, hwsetup, netrow;
static char netinfo[2][64];
static void netline(const char *s) {
    if (netrow < 2)
        copy(netinfo[netrow++], s, 64);
}
static int modal, consent;
static char modaltext[768];
static void render(void);
static void sound(void);
extern void poll(void);
static u32 paint[198][466];
static int ink;
static const u32 paints[] = {0x177996, 0x26c67e, 0xff7891, 0xf7c24a, 0x262e3d, WHITE};
static char expr[32] = "0";
static int acc, oper, fresh = 1;
static int grid[64], seen[64], lost, won, cell, seeded;
static u32 seed = 1;
static void history_row(const char *s) {
    scroll = 0;
    if (rows == 96) {
        for (int i = 1; i < 96; i++)
            copy(history[i - 1], history[i], 105);
        rows--;
    }
    copy(history[rows++], s, 105);
}
/* Size scrollback rows for the narrower Terminal so no digest or command is clipped. */
static void say(const char *s) {
    do {
        char row[105];
        int n = 0, w = 0, gap = 0;
        while (s[n] && s[n] != '\n' && n < 104) {
            unsigned ch = (u8)s[n];
            int adv = ch >= 32 && ch <= 126 ? fonts[0].adv[ch - 32] : 0;
            if (w + adv > 454)
                break;
            if (s[n] == ' ')
                gap = n + 1;
            w += adv;
            n++;
        }
        if (s[n] && s[n] != '\n' && gap)
            n = gap;
        for (int i = 0; i < n; i++)
            row[i] = s[i];
        row[n] = 0;
        history_row(row);
        s += n;
        if (*s == '\n')
            s++;
    } while (*s);
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
static void runprogram(const u8 *p, u32 n, int imp) {
    last_imp = imp;
    if (!imp) {
        // Launch-time linking is temporary. Only an explicit Convert writes a Home record.
        n = imp_convert(p, n, imported, sizeof imported, &pe_result);
        p = imported;
    }
    pe_ok = n && imp_run(p, n, &pe_result);
    pe_ran = 1;
    serial(pe_ok
               ? (imp ? (pe_result.bits == 64 ? "IMP64 completed\r\n" : "IMP32 completed\r\n")
                      : (pe_result.bits == 64 ? "EXE to IMP64 completed\r\n"
                                              : "EXE to IMP32 completed\r\n"))
               : "Program rejected\r\n");
}
static int autohide, railshown = 1, fill = 1, quiet_ui, setpage, maximized[NAPPS], context,
                     cx, cy;
static int actions_from = -1;
#define NDESKS 4
static int desk, owner[NAPPS], foreground[NDESKS] = {-1, -1, -1, -1}, deskx[NDESKS],
                               desky[NDESKS], overview, focusdesk, winheld, winused, osk;
static void launch(int);
static void navigate(const char *, int);
#include "workspace.inc"
#include "organizer.inc"
#include "archive.inc"
#include "alarms.inc"
#include "packages.inc"
#include "commands.inc"
#include "aeropage.inc"
static void launch(int id) {
    if (id < 0 || id >= NAPPS)
        return;
    if (id == CENTER && active != CENTER)
        actions_from = active;
    if (active >= 0 && active != id)
        hidden[active] = 1;
    if (owner[id] != desk && foreground[owner[id]] == id)
        foreground[owner[id]] = -1;
    owner[id] = desk;
    active = id;
    log_event(apps[id].name);
    lift = motion ? 22 : 0;
    lift_speed = 0;
    hidden[id] = 0;
    if (id == BROWSER && !location[0])
        navigate("aero:home", 1);
    if (id == SOFTWARE)
        shelf_refresh();
    if (id == FILES) {
        file_refresh();
        fileidx = 0;
    }
    if (id == WRITER)
        editpos = clamp(editpos, 0, len(note));
    if (id == WINDOWS && !pe_ran)
        runprogram(imp32, IMP32_SIZE, 1);
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
        if (owner[i] == desk && hidden[i] && n-- == 0)
            return i;
    return -1;
}
static void enter(void) {
    welcome = 0;
    dirty = 1;
    serial("desktop ready\r\n");
}
static void cli_set(int mode) {
    serial(mode ? "shell: cli\r\n" : "shell: desktop\r\n");
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
    u8 prefs[] = {2, cli, wall, color, motion, dim, vol, autohide, fill, quiet_ui};
    return home_save("profile", prefs, sizeof prefs) ? "Settings saved."
                                                     : "Needs a writable Fresh Home disk.";
}
static void loadhome(void) {
    u8 p[10];
    int sz = home_read("profile", p, sizeof p);
    if ((sz == 7 && p[0] == 1) || (sz == 10 && p[0] == 2)) {
        if (sz == 10) {
            autohide = p[7] & 1;
            fill = p[8] & 1;
            quiet_ui = p[9] & 1;
            railshown = !autohide;
            display_fit(fill);
        }
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
#include "radio.inc"
static int guest_read(const char *name, void *b, u32 cap) {
    return suffix(name, ".txt") ? home_read(name, b, cap) : -1;
}
static void guest_box(const char *msg) {
    copy(modaltext, msg, sizeof modaltext);
    modal = 1;
    serial("Win32 modal awaiting OK\r\n");
    render();
    while (modal) {
        poll();
        cursor_present(1, 0);
        __asm__ volatile("hlt");
    }
    dirty = 1;
}
static int guest_save(const char *name, const void *data, u32 size) {
    if (!home_ready() || !suffix(name, ".txt"))
        return 0;
    copy(modaltext, "Allow this application to save a new version of ", sizeof modaltext);
    copy(modaltext + len(modaltext), name, 32);
    append(modaltext, sizeof modaltext, '?');
    modal = 2;
    consent = 0;
    serial("Document permission required\r\n");
    render();
    while (modal) {
        poll();
        cursor_present(1, 0);
        __asm__ volatile("hlt");
    }
    dirty = 1;
    return consent && home_save(name, data, size);
}
static void task_view(void);
static void command(void) {
    cmdpos = 0;
    scroll = 0;
    if (line[0]) {
        if (ncmd == 16) {
            for (int i = 1; i < 16; i++)
                copy(commands[i - 1], commands[i], 100);
            ncmd--;
        }
        copy(commands[ncmd++], line, 100);
    }
    recall = ncmd;
    copy(lastcmd, line, sizeof lastcmd);
    char s[100];
    copy(s, line, 100);
    line[0] = 0;
    if (!*s)
        return;
    say(s);
    int lo = 0, hi = len(s);
    while (lo < hi && s[lo] == ' ')
        lo++;
    while (hi > lo && s[hi - 1] == ' ')
        hi--;
    for (int i = 0; i < hi - lo; i++)
        s[i] = s[i + lo];
    s[hi - lo] = 0;
    for (int i = 0; s[i] && s[i] != ' '; i++)
        if (s[i] >= 'A' && s[i] <= 'Z')
            s[i] += 32;
    if (eq(s, "exit") || eq(s, "desktop") || eq(s, "logout")) {
        cli_set(0);
        return;
    }
    static const struct {
        const char *alias, *command;
    } aliases[] = {
        {"cls", "clear"},           {"ps", "tasks"},         {"tasklist", "tasks"},
        {"date", "time"},           {"explorer", "files"},   {"notepad", "writer"},
        {"appwiz.cpl", "software"}, {"control", "settings"}, {"msinfo32", "system"},
        {"taskmgr", "monitor"},     {"ipconfig", "net"},     {"ifconfig", "net"},
        {"lspci", "drivers"},       {"calc.exe", "calc"},    {"charmap.exe", "charmap"}};
    for (unsigned i = 0; i < sizeof aliases / sizeof *aliases; i++)
        if (eq(s, aliases[i].alias)) {
            copy(s, aliases[i].command, sizeof s);
            break;
        }
    if (prefix(s, "type ")) {
        int n = len(s);
        for (int i = 3; i < n; i++)
            s[i] = s[i + 1];
        s[0] = 'c';
        s[1] = 'a';
        s[2] = 't';
    }
    if (prefix(s, "browse ")) {
        launch(BROWSER);
        navigate(s + 7, 1);
        return;
    }
    if (prefix(s, "js ") || prefix(s, "jsfile ")) {
        const char *code = s + 3;
        if (prefix(s, "jsfile ")) {
            int n = home_read(s + 7, script, JS_SOURCE_MAX);
            if (n < 0) {
                say("Script missing or too large.");
                return;
            }
            script[n] = 0;
            code = script;
        }
        const char *err =
            aero_eval(imported, AERO_ENGINE, AERO_ENGINE_SIZE, code, jsout, JS_OUT_CAP);
        if (err)
            say(err);
        if (jsout[0])
            say(jsout);
        return;
    }
    if (eq(s, "pageinfo")) {
        say(location);
        say(title);
        say(notice);
        say(body);
        return;
    }
    if (prefix(s, "savepage ")) {
        const char *dest = s + 9;
        u8 unused;
        if (!bodylen || !suffix(dest, ".txt") || home_read(dest, &unused, 0) != -2 ||
            !home_save(dest, body, bodylen))
            say("Use a new .txt name on writable Home and open a page first.");
        else
            notify("Page text saved to Fresh Home.");
        return;
    }
    if (shell_tools(s))
        return;
    if (eq(s, "desktops")) {
        task_view();
        return;
    }
    if (eq(s, "osk")) {
        osk = !osk;
        return;
    }
    if (clock_command(s) || archive_cmd(s))
        return;
    if (eq(s, "utilities")) {
        say("sticky | todo | charmap | tips | events | actions | desktops | osk (F11)");
        say("Clock: alarm HH:MM | timer SECONDS | alarm off | timer off");
        say("F8 Quick actions | F9 Magnifier | F10 Snip | sha3 FILE | tar OUT.tar A B | gzip "
            "OUT.gz A");
        return;
    }
    if (eq(s, "net") || eq(s, "net dhcp")) {
        network_cmd(eq(s, "net dhcp"), say);
        return;
    }
    if (eq(s, "help keys")) {
        say("Home/End, arrows: edit | Up/Down: history | PgUp/PgDn: scroll");
        say("Ctrl+L: clear | Ctrl+C: discard input | F2: tray | F3: GUI/CLI");
        return;
    }
    if (eq(s, "help drivers")) {
        say("drivers - PCI function IDs and bound drivers. vmtools - guest status.");
        say("net dhcp | net - PCnet or Intel 82540EM Ethernet. wifi scan/join/ip/leave - "
            "COM2 ESP-AT.");
        say("bt info | bt scan - Bluetooth COM3 H4. open chime.wav - PCM output.");
        return;
    }
    if (eq(s, "uname") || eq(s, "ver")) {
        say("FreshOS 1.5 / " EDITION " / native x86 kernel");
        return;
    }
    if (eq(s, "dir"))
        copy(s, "ls", sizeof s);
    if (eq(s, "drivers") || eq(s, "lspci")) {
        drivers_list(say);
        return;
    }
    if (eq(s, "vmtools")) {
        say(vm_guest());
        return;
    }
    if (prefix(s, "wifi ") || eq(s, "wifi")) {
        wifi_cmd(s + 4 + (s[4] != 0));
        return;
    }
    if (prefix(s, "bt ") || eq(s, "bt")) {
        bluetooth(s + 2 + (s[2] != 0));
        return;
    }
    if (eq(s, "audio test")) {
        sound();
        say(audio_name());
        return;
    }
    if (eq(s, "pwd")) {
        say("/home");
        return;
    }
    if (prefix(s, "echo ")) {
        say(s + 5);
        return;
    }
    if (prefix(s, "cat ") && !eq(s, "cat note")) {
        char b[1025];
        int n = guest_read(s + 4, b, 1024);
        if (n < 0) {
            say("Cannot read .txt document (1 KiB maximum).");
            return;
        }
        b[n] = 0;
        char *start = b;
        for (int i = 0; i <= n; i++)
            if (b[i] == '\n' || !b[i]) {
                b[i] = 0;
                say(start);
                start = b + i + 1;
            }
        return;
    }
    int opening = prefix(s, "open ");
    if (opening) {
        for (int i = 5, n = len(s); i <= n; i++)
            s[i - 5] = s[i];
    }
    for (int i = 0; i < NAPPS; i++)
        if (eq(s, apps[i].cmd)) {
            launch(i);
            return;
        }
    if (opening) {
        open_file(s);
        return;
    }
    if (prefix(s, "convert ")) {
        convert_file(s + 8);
        return;
    }
    if (prefix(s, "imp ") || prefix(s, "exe ")) {
        open_file(s + 4);
        return;
    }
    if (!prefix(s, "save ") && !prefix(s, "load ") &&
        (suffix(s, ".imp") || suffix(s, ".exe") || suffix(s, ".txt"))) {
        open_file(s);
        return;
    }
    if (eq(s, "start") || eq(s, "apps")) {
        say("START  |  Type an app name, then Enter.");
        say("files   writer   paint   calc   clock   settings   term   mines   system");
        say("calendar   monitor   programs   connections   sticky   todo   charmap");
        say("tips   events   actions   browser   photos   software   /   desktops   osk   /  "
            " more: "
            "utilities");
    } else if (eq(s, "help")) {
        say("exit - return to desktop | doctor, disks, history, uptime, wc, stat, head, "
            "tail, pkg");
        say("help files | help web - file utilities, Aero navigation and JavaScript");
        say("utilities - desktop tools, clipboard, archives and hashes");
        say("help keys | help drivers | lspci | ver | pwd | dir | cat FILE.txt | echo TEXT");
        say("start / apps   open NAME   tasks   resume NAME   clear   gui   cli");
        say("wall sky|lagoon|paint   color mint|ice|sun|white   time   cat note   storage");
    } else if (eq(s, "storage")) {
        say(disk_status());
        say("save NAME / load NAME    savecanvas    prefs    ls    IMP/EXE    install");
    } else if (eq(s, "install"))
        open_setup();
    else if (eq(s, "prefs"))
        say(saveprefs());
    else if (eq(s, "savecanvas"))
        say(home_save("canvas", paint, sizeof paint) ? "Canvas saved."
                                                     : "Canvas not saved. Type storage.");
    else if (eq(s, "ls")) {
        directory_refresh();
        for (int i = 0; i < nfiles; i++)
            say(directory[i]);
        if (!home_ready())
            say("No Fresh Home mounted.");
    } else if (prefix(s, "save ")) {
        write_document(s + 5);
    } else if (prefix(s, "load ")) {
        open_file(s + 5);
    } else if (eq(s, "tasks")) {
        int any = 0;
        for (int i = 0; i < NAPPS; i++)
            if (owner[i] == desk && hidden[i]) {
                say(apps[i].cmd);
                any = 1;
            }
        if (!any)
            say("No minimized apps.");
    } else if (prefix(s, "resume ")) {
        for (int i = 0; i < NAPPS; i++)
            if (owner[i] == desk && hidden[i] && eq(s + 7, apps[i].cmd)) {
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
static void sky(void) {
#ifdef SKY24
    const u8 *p = SKY24;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++, p += 3)
            screen[y][x] = p[0] | p[1] << 8 | p[2] << 16;
#else
    sprite(SKY, SKY_W, SKY_H, 0, 0, W, H, 0);
#endif
}
static void bg(void) {
    if (wall == 0)
        sky();
    else if (wall == 1) {
        for (int y = 0; y < H; y++)
            box(0, y, W, 1, mix(0x055872, 0xa7efd9, y * 256 / H));
    } else
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                screen[y][x] = paint[y * 198 / H][x * 466 / W];
}
static void icon(int x, int y, int i, int r) {
    static const u16 *const glyphs[] = {ICON0, ICON1, ICON2, ICON3,  ICON4,  ICON5,
                                        ICON6, ICON7, ICON8, ICON10, ICON11, ICON9};
    if (i == CONNECTIONS) {
        sprite(GL_NETWORK, GL_NETWORK_W, GL_NETWORK_H, x, y, r, r, 1);
        return;
    }
    if (i >= STICKY) {
        static const u16 *const extra[] = {ICON12, ICON13, ICON14, ICON15, ICON16,
                                           ICON17, ICON18, ICON19, ICON20};
        sprite(extra[i - STICKY], 64, 64, x, y, r, r, 1);
        return;
    }
    sprite(glyphs[i], 64, 64, x, y, r, r, 1);
}
#include "desktop.inc"
#include "workspaces.inc"
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
        fit_label(a + 19, b + 23, 55, apps[i].name, 0);
    }
    text(184, 349, choice / 9 < (NAPPS - 1) / 9 ? "More >" : "< First", 0, INK);
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
    int x = 39, y = 106;
    gel(x, y, 186, 56 + (n > 16 ? 16 : n) * 30, 18, WHITE, 0x94f5d6, 0xb2fbec);
    text(x + 14, y + 8, "Minimized apps", 1, INK);
    if (!n)
        text(x + 14, y + 32, "The tray is empty.", 0, INK);
    int first = choice / 16 * 16;
    for (int j = 0; j < n - first && j < 16; j++) {
        int i = j + first;
        if (choice == i)
            gel(x + 7, y + 31 + j * 30, 170, 27, 10, WHITE, 0x41dfa9, 0xaffcdd);
        text(x + 16, y + 34 + j * 30, apps[nth(i)].name, 1, INK);
    }
}
static void console(int x, int y, int w, int h, int full) {
    gel(x, y, w, h, 12, 0x164556, 0x103e50, 0x123c4a);
    if (full)
        pill(x + w - 185, y + 6, 170, 28, "Desktop / Esc", 0);
    text(x + 16, y + 12, "FRESH SHELL    /    LIVE SESSION", 0, 0x8defff);
    int max = full ? 12 : clamp((h - 107) / 19, 1, 30),
        stop = rows - clamp(scroll, 0, rows > max ? rows - max : 0),
        start = stop > max ? stop - max : 0;
    for (int i = start; i < stop; i++) {
        char b[105];
        copy(b, history[i], sizeof b);
        text(x + 16, y + 36 + (i - start) * 19, b, 0, tones[color]);
    }
    int yy = y + 39 + (stop - start) * 19;
    text(x + 16, yy, "fresh >", 1, tones[color]);
    int cp = clamp(cmdpos, 0, len(line)), off = 0, px = 0;
    for (int i = cp - 1; i >= 0; i--) {
        px += fonts[1].adv[clamp(line[i] - 32, 0, 94)];
        if (px > w - 105) {
            off = i + 1;
            break;
        }
    }
    char view[100];
    copy(view, line + off, sizeof view);
    while (tw(view, 1) > w - 100)
        view[len(view) - 1] = 0;
    int end = x + 78;
    if (secret) {
        for (int i = 0; i < len(pass) && end < x + w - 22; i++)
            end = text(end, yy, "*", 1, WHITE);
    } else {
        text(end, yy, view, 1, WHITE);
        view[clamp(cp - off, 0, 99)] = 0;
        end += tw(view, 1);
    }
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
    if (setpage) {
        const char *labels[] = {"Auto-hide rail", "Framebuffer scaling", "Interface sounds"},
                   *values[] = {autohide ? "On" : "Off", fill ? "Fill screen" : "Keep aspect",
                                quiet_ui ? "Silent" : "Soft chime"};
        for (int i = 0; i < 3; i++) {
            text(x, y + i * 42 + 4, labels[i], 1, INK);
            badge(x + 307, y + i * 42, values[i], 0);
        }
        text(x, y + 146, "Move the pointer to the left edge to reveal a hidden rail.", 1,
             INK);
        pill(x, y + 208, 145, 31, "Back", 0);
        pill(x + 307, y + 208, 114, 31, "Save profile", 1);
        return;
    }
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
    pill(x, y + 208, 145, 31, "More settings", 0);
    pill(x + 307, y + 208, 114, 31, "Save profile", 1);
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
        if (tx + tw(b, 1) > x + width || s[i] == '\n') {
            tx = x;
            y += 22;
        }
        if (s[i] != '\n')
            tx = text(tx, y, b, 1, INK);
    }
}
#include "photos.inc"
#include "software.inc"
#include "editors.inc"
#include "utility_views.inc"
#include "capture.inc"
#include "keyboard.inc"
#include "chrome.inc"
#include "aeroview.inc"
static void window(void) {
    struct Bounds f = frame();
    int x = f.x, y = f.y, w = f.w, h = f.h, a = active;
    gel(x + 3, y + 6, w, h, 15, 0x769ca6, 0x637f8b, 0xa4bcc1);
    gel(x, y, w, h, 15, WHITE, 0x35efbf, 0xeafffb);
    gel(x + 2, y + 30, w - 4, h - 32, 12, 0xf9ffff, 0xeaf7f9, 0xf7fdfd);
    icon(x + 9, y + 3, a, 23);
    text(x + 38, y + 5, apps[a].name, 1, INK);
    pill(x + w - 165, y + 3, 41, 21, maximized[a] ? "<>" : "+", 0);
    pill(x + w - 111, y + 3, 41, 21, "-", 0);
    pill(x + w - 56, y + 3, 41, 21, "x", 0);
    int xx = x + (w - 506) / 2 + 19, yy = y + (h - 311) / 2 + 45;
    if (a == BROWSER)
        browser_draw(x + 3, y + 32, w - 6, h - 36);
    if (a == PHOTOS)
        photo_draw(xx, yy);
    if (a == SOFTWARE)
        software_draw(xx, yy);
    if (a >= STICKY)
        utility_draw(a, xx, yy);
    if (a == WINDOWS) {
        text(xx, yy,
             last_imp == 2 ? "Linux application"
             : last_imp    ? "IMP application"
                           : "Windows EXE  /  linked as IMP",
             2, INK);
        text(xx, yy + 39,
             pe_result.bits == 64 ? "INTERPRETED    /    x86-64" : "INTERPRETED    /    x86",
             0, 0x347e65);
        wrapped(xx, yy + 70, 445, pe_ok ? pe_result.output : pe_result.detail);
        char code[16];
        num(code, (int)pe_result.exit_code);
        int tx = text(xx, yy + 209,
                      pe_ok ? "Program exited with code "
                            : "Stopped: unsupported or invalid program.",
                      1, INK);
        if (pe_ok)
            text(tx, yy + 209, code, 1, INK);
        else
            pill(xx + 316, yy + 203, 144, 30, "Copy details", 0);
    }
    if (a == CALENDAR)
        calendar(xx, yy);
    if (a == TASKMGR) {
        text(xx, yy, "App sessions", 2, INK);
        text(xx, yy + 38, "Minimized on this desktop", 1, INK);
        int n = 0;
        for (int i = 0; i < NAPPS; i++)
            if (owner[i] == desk && hidden[i]) {
                text(xx + (n / 7) * 151, yy + 62 + n % 7 * 24, apps[i].name, 1, INK);

                n++;
            }
        if (!n)
            text(xx, yy + 73, "Nothing minimized.", 1, INK);
    }
    if (a == TERM)
        console(x + 9, y + 36, w - 18, h - 46, 0);
    if (a == SETTINGS)
        settings(xx, yy);
    if (a == CONNECTIONS) {
        text(xx, yy, "Connections", 2, INK);
        netrow = 0;
        network_cmd(0, netline);
        for (int i = 0; i < netrow; i++)
            text(xx, yy + 40 + 17 * i, netinfo[i], 0, INK);
        pill(xx + 320, yy + 35, 135, 30, "Request DHCP", 0);
        text(xx, yy + 86, "Wi-Fi  /  ESP-AT radio", 1, INK);
        pill(xx + 230, yy + 80, 106, 30, "Scan", 0);
        pill(xx + 350, yy + 80, 105, 30, "Join...", 0);
        text(xx, yy + 130, "Bluetooth  /  H4 radio", 1, INK);
        pill(xx + 230, yy + 125, 106, 30, "Controller", 0);
        pill(xx + 350, yy + 125, 105, 30, "Discover", 0);
        text(xx, yy + 174, "Activity   /   Full results in Terminal", 0, INK);
        for (int i = 0; i < 3; i++) {
            int j = rows - 3 + i;
            if (j >= 0) {
                char t[65];
                copy(t, history[j], sizeof t);
                text(xx, yy + 197 + i * 16, t, 0, INK);
            }
        }
    }
    if (a == SYSTEM && hwpage >= 0) {
        text(xx, yy, "Detected PCI devices", 2, INK);
        for (int i = 0; i < 8; i++)
            text(xx, yy + 44 + i * 19, driver_at(hwpage * 8 + i), 0, INK);
        pill(xx, yy + 210, 90, 30, "Back", 0);
        pill(xx + 267, yy + 210, 90, 30, "Prev", 0);
        pill(xx + 365, yy + 210, 90, 30, "Next", 0);
    }
    if (a == SYSTEM && hwpage < 0) {
        text(xx, yy, "FreshOS Current", 2, INK);
        text(xx, yy + 36, EDITION " edition   /   native live kernel", 1, INK);
        text(xx, yy + 72,
#ifdef EFI
             "64-bit x86 long mode  /  UEFI GOP framebuffer"
#else
             "32-bit x86 protected mode  /  VBE framebuffer"
#endif
             ,
             1, INK);
        text(xx, yy + 99, display_driver(), 1, INK);
        text(xx, yy + 126, disk_status(), 0, INK);
        text(xx, yy + 151, audio_name(), 0, INK);
        pill(xx + 308, yy + 208, 148, 30, "Device inventory", 0);
        text(xx, yy + 172, "F1  Launcher     F2 / Win  Tray     F3  GUI / CLI", 1, INK);
        fit_label(xx + 149, yy + 212, 292, vm_guest(), 0);
    }
    if (a == FILES)
        files_draw(xx, yy);
    if (a == WRITER)
        writer_draw(x + 19, y + 45);
    if (a == PAINT) {
        for (int j = 0; j < 198; j++)
            for (int i = 0; i < 466; i++)
                screen[yy + j][xx + i] = paint[j][i];
        for (int i = 0; i < 6; i++) {
            if (i == ink)
                box(xx + i * 32 - 2, yy + 207, 28, 27, INK);
            box(xx + i * 32, yy + 209, 24, 23, paints[i]);
        }
        pill(xx + 208, yy + 205, 96, 31, "Save canvas", 0);
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
        clock_details(xx, yy + 134);
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
    nhover = 0;
    if (welcome) {
        welcome_view();
    } else {
        bg();
        if (!cli) {
            if (!autohide || railshown || menu || tray || widget)
                rail();
            else
                gel(0, H / 2 - 40, 5, 80, 2, WHITE, MINT, ICE);
        } else
            console((W - 916) / 2, (H - 456) / 2, 916, 456, 1);
        if (active >= 0)
            window();
        if (progress && !cli)
            startmenu();
        if (tray)
            traybox();
        if (widget)
            slider_draw();
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
        text(222, 212, disk_status(), 0, INK);
        text(222, 252, "Source: FreshOS ISO on an IDE optical drive", 1, INK);
        text(222, 280, "Destination: marked IDE, SATA or virtio disk only.", 1, INK);
        text(222, 311, "Never formats or migrates an existing Windows drive.", 0, INK);
        pill(545, 164, 190, 30, "Copy disk report", 0);
        badge(225, 341, "Back / Esc", 0);
        badge(410, 341, "Drivers / D", 0);
        if (!finished)
            badge(611, 341, "Install / I", 1);
    }
    if (pending_doc[0]) {
        glass(318, 181, 326, 167, 27, 0xc6f4e9, 248);
        text(343, 196, "Unsaved changes", 2, INK);
        text(343, 251, "Discard edits and open another file?", 1, INK);
        pill(345, 299, 112, 30, "Keep edits", 1);
        pill(502, 299, 112, 30, "Discard", 0);
    }
    if (savebox) {
        glass(318, 181, 326, 167, 27, 0xc6f4e9, 248);
        text(343, 196, "Save document as", 2, INK);
        box(345, 249, 267, 30, WHITE);
        text(351, 253, savename, 1, INK);
        pill(345, 299, 112, 30, "Cancel", 0);
        pill(502, 299, 112, 30, "Save", 1);
    }
    if (feedback[0] && (int)(feedback_until - ticks) > 0) {
        glass(90, H - 45, W - 180, 36, 18, 0xe0fff3, 248);
        text(110, H - 35, feedback, 0, INK);
    }
    if (dim < 10) {
        int k = 90 + dim * 16;
        for (int j = 0; j < H; j++)
            for (int i = 0; i < W; i++)
                screen[j][i] = mix(0, screen[j][i], k);
    }
    if (modal) {
        int x = W / 2 - 220, y = H / 2 - 90;
        glass(x, y, 440, 183, 22, 0xdefff2, 250);
        text(x + 20, y + 13, "Windows application", 2, INK);
        wrapped(x + 20, y + 52, 397, modaltext);
        if (modal == 2)
            pill(x + 180, y + 139, 110, 30, "Deny", 0);
        pill(x + 304, y + 139, 110, 30, modal == 2 ? "Allow / Y" : "OK", 1);
    }
    if (kb_visible())
        keyboard_draw();
    if (overview)
        workspace_draw();
    if (magnify && !secret && !snipping && !modal)
        lens();
    if (context)
        context_draw();
    if (snipping)
        capture_overlay();
    present();
    cursor_present(1, 1);
}
static void sound(void);
static void setting(int i) {
    switch (i) {
    case 6:
        autohide = !autohide;
        railshown = !autohide;
        break;
    case 7:
        fill = !fill;
        display_fit(fill);
        break;
    case 8:
        quiet_ui = !quiet_ui;
        break;
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
    if (secret)
        return;
    if (snipping) {
        dirty = 1;
        if (snipping == 1 && (buttons & 1) && !(old & 1)) {
            sx = x;
            sy = y;
            snipping = 2;
        } else if (snipping == 2 && !(buttons & 1))
            save_snip(x, y);
        return;
    }
    if (magnify)
        dirty = 1;
    if (!welcome && !setup && !modal && !savebox && !pending_doc[0] && !shutdown &&
        !overview) {
        if ((buttons & 2) && !(old & 2)) {
            cx = clamp(x, 0, W - 204);
            cy = clamp(y, 0, H - 244);
            context = 1;
            menu = tray = widget = progress = velocity = goal = drag = 0;
            dirty = 1;
            return;
        }
        if (context) {
            dirty = 1;
            if ((buttons & 1) && !(old & 1)) {
                context = 0;
                if (inside(x, y, cx + 4, cy + 5, 196, 231))
                    context_action((y - cy - 5) / 33);
                dirty = 1;
            }
            return;
        }
        if (autohide && !cli) {
            int shown = railshown;
            if (x <= 5)
                railshown = 1;
            else if (x > 58 && !menu && !tray && !widget)
                railshown = 0;
            if (shown != railshown)
                dirty = 1;
        }
    }
    static int prevdock = -2;
    int current = (dock_at(x, y) + 1) * 64 + hovered(x, y);
    if (!(buttons & 1) && !(old & 1) && prevdock == current) {
        return;
    }
    prevdock = current;
    dirty = 1;
    int click = (buttons & 1) && !(old & 1);
    if (!(buttons & 1))
        drag = 0;
    if (modal) {
        if (click && inside(x, y, W / 2 + 84, H / 2 + 49, 110, 30)) {
            consent = 1;
            modal = 0;
        } else if (modal == 2 && click && inside(x, y, W / 2 - 40, H / 2 + 49, 110, 30)) {
            consent = 0;
            modal = 0;
        }
        return;
    }
    if (overview) {
        if (click) {
            for (int i = 0; i < 4; i++)
                if (inside(x, y, W / 2 - 278 + 141 * i, H / 2 - 33, 132, 104)) {
                    switch_desk(i);
                    return;
                }
            overview = 0;
        }
        return;
    }
    if (kb_visible() && keyboard_click(x, y, click))
        return;
    if (pending_doc[0]) {
        if (click && inside(x, y, 345, 299, 112, 30))
            discard_choice(0);
        else if (click && inside(x, y, 502, 299, 112, 30))
            discard_choice(1);
        return;
    }
    if (savebox) {
        if (click && inside(x, y, 502, 299, 112, 30))
            write_document(savename);
        else if (click && inside(x, y, 345, 299, 112, 30))
            savebox = 0;
        return;
    }
    if (setup) {
        if (click && inside(x, y, 545, 164, 190, 30)) {
            copy(clip, disk_status(), sizeof clip);
            notify("Disk report copied.");
            return;
        }
        if (click && inside(x, y, 410, 341, 114, 31)) {
            setup = 0;
            welcome = 0;
            hwsetup = 1;
            hwpage = 0;
            launch(SYSTEM);
            return;
        }
        if (click && inside(x, y, 225, 341, 114, 31))
            setup = 0;
        else if (click && !finished && inside(x, y, 611, 341, 114, 31))
            begin_setup();
        return;
    }
    if (welcome) {
        if (click && inside(x, y, START_X + 17, START_Y + 57, 174, 32))
            open_setup();
        else if (click && inside(x, y, START_X, START_Y, 208, 42))
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
        wx = clamp(x - dgx, cli ? 0 : 82, W - 510);
        wy = clamp(y - dgy, 0, H - 320);
        return;
    }
    if (widget) {
        int yy = slider_y();
        if (click && inside(x, y, SL_X - 12, yy - 14, SL_LEN + 24, 44))
            slide_grab = 1;
        if (slide_grab) {
            int level = clamp((x - SL_X + 9) / 18, 0, 10);
            if (widget == 1)
                vol = level;
            else
                dim = clamp(level, 1, 10);
            if (!(buttons & 1)) {
                slide_grab = 0;
                if (widget == 1)
                    sound();
            }
            return;
        }
        if (click)
            widget = 0;
    }
    if (tray && click) {
        int j = (y - 137) / 30 + choice / 16 * 16;
        if (inside(x, y, 45, 137, 174, 16 * 30) && nth(j) >= 0)
            launch(nth(j));
        else
            tray = 0;
        return;
    }
    if (menu && click) {
        if (inside(x, y, 175, 342, 62, 25)) {
            choice = (choice / 9 + 1) * 9;
            if (choice >= NAPPS)
                choice = 0;
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
    if (!cli && click && dock_at(x, y) >= 0) {
        switch (dock_at(x, y)) {
        case DOCK_NET:
            launch(CONNECTIONS);
            break;
        case DOCK_APPS:
            launcher();
            break;
        case DOCK_FILES:
            launch(FILES);
            break;
        case DOCK_WRITER:
            launch(WRITER);
            break;
        case DOCK_SETTINGS:
            launch(SETTINGS);
            break;
        case DOCK_CLOCK:
            launch(CLOCK);
            break;
        case DOCK_TRAY:
            tasks();
            break;
        case DOCK_LIGHT:
            widget = 2;
            break;
        case DOCK_SOUND:
            widget = 1;
            break;
        }
        return;
    }
    if (active < 0) {
        if (cli && click && inside(x, y, (W + 916) / 2 - 185, (H - 456) / 2 + 6, 170, 28))
            cli_set(0);
        return;
    }
    struct Bounds f = frame();
    int rx = x - f.x, ry = y - f.y;
    if (click && inside(rx, ry, 0, 0, f.w, 30)) {
        if (rx >= f.w - 56)
            hide(0);
        else if (rx >= f.w - 111)
            hide(1);
        else if (rx >= f.w - 165)
            toggle_max();
        else if (!maximized[active]) {
            dgx = rx;
            dgy = ry;
            drag = 1;
        }
        return;
    }
    int a = rx - 19 - (f.w - 506) / 2, b = ry - 45 - (f.h - 311) / 2;
    if (active == PAINT && (buttons & 1) && inside(a, b, 0, 0, 466, 198)) {
        for (int j = -3; j < 4; j++)
            for (int i = -3; i < 4; i++)
                if (i * i + j * j <= 9 && inside(a + i, b + j, 0, 0, 466, 198))
                    paint[b + j][a + i] = paints[ink];
        return;
    }
    if (!click)
        return;
    if (active == WINDOWS && !pe_ok && inside(a, b, 316, 203, 144, 30)) {
        context_action(2);
        return;
    }
    if (active == BROWSER) {
        browser_click(rx - 3, ry - 32, f.w - 6, f.h - 36);
        return;
    }
    if (active == SOFTWARE) {
        shelf_click(a, b);
        return;
    }
    if (active == PHOTOS && inside(a, b, 0, 215, 98, 28)) {
        launch(FILES);
        return;
    }
    if (utility_click(active, a, b))
        return;
    if (active == CONNECTIONS) {
        if (inside(a, b, 320, 35, 135, 30))
            network_cmd(1, say);
        else if (inside(a, b, 230, 80, 106, 30))
            wifi_cmd("scan");
        else if (inside(a, b, 350, 80, 105, 30)) {
            launch(TERM);
            copy(line, "wifi join ", 100);
            cmdpos = len(line);
        } else if (inside(a, b, 230, 125, 106, 30))
            bluetooth("info");
        else if (inside(a, b, 350, 125, 105, 30))
            bluetooth("scan");
        return;
    }
    if (active == SYSTEM && b >= 208 && b < 242) {
        if (hwpage < 0 && a >= 308 && a < 456)
            hwpage = 0;
        else if (hwpage >= 0) {
            if (a >= 267 && a < 455)
                hwpage = clamp(hwpage + (a >= 365 ? 1 : -1), 0,
                               driver_count() ? (driver_count() - 1) / 8 : 0);
            if (a >= 0 && a < 90) {
                hwpage = -1;
                if (hwsetup) {
                    hwsetup = 0;
                    setup = 1;
                    hide(0);
                }
            }
        }
        return;
    }
    if (active == CALENDAR && inside(a, b, 326, 0, 132, 38))
        monthoff = clamp(monthoff + (a < 375 ? -1 : 1), -1200, 1200);
    if (active == SETTINGS && inside(a, b, 0, 208, 145, 31)) {
        setpage = !setpage;
        return;
    }
    if (active == SETTINGS && setpage && inside(a, b, 307, 0, 114, 126)) {
        setting(6 + b / 42);
        return;
    }
    if (active == SETTINGS && !setpage && inside(a, b, 307, 0, 114, 204))
        setting(b / 34);
    if (active == FILES) {
        file_click(a, b);
        return;
    }
    if (active == TASKMGR && a >= 0 && a < 453 && b >= 62 && b < 230) {
        int id = nth((a / 151) * 7 + (b - 62) / 24);
        if (id >= 0)
            launch(id);
        return;
    }
    if (active == WRITER) {
        a = rx - 19;
        b = ry - 45;
        if (b >= paper_h() + 37 && b < paper_h() + 68) {
            if (a >= paper_w() - 240 && a < paper_w() - 126)
                write_document(document);
            if (a >= paper_w() - 116 && a < paper_w() - 2) {
                savebox = 1;
                copy(savename, document, 32);
            }
            return;
        }
        writer_place(a, b);
    }
    if (active == SETTINGS && inside(a, b, 307, 208, 114, 31)) {
        notify(saveprefs());
        return;
    }
    if (active == PAINT && b >= 205 && b < 239) {
        if (a >= 0 && a < 192)
            ink = a / 32;
        if (a >= 208 && a < 304)
            notify(home_save("canvas", paint, sizeof paint)
                       ? "Canvas saved."
                       : "Canvas not saved: check writable Home.");
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
    if (k == 29 || k == 285) {
        ctrl = down;
        return;
    }
    if (k == 42 || k == 54) {
        shift = down;
        return;
    }
    if (k == 347 || k == 348) {
        if (modal || secret || welcome || setup || savebox || pending_doc[0] || shutdown ||
            snipping) {
            winheld = 0;
            return;
        }
        if (down) {
            if (!winheld)
                winused = 0;
            winheld = 1;
        } else {
            if (winheld && !winused)
                tasks();
            winheld = 0;
        }
        return;
    }
    if (!down)
        return;
    if (modal) {
        if (modal == 2 && k == 21) {
            consent = 1;
            modal = 0;
        } else if (k == 28 || k == 1 || k == 57 || k == 49) {
            consent = 0;
            modal = 0;
        }
        return;
    }
    if (secret) {
        dirty = 1;
        char ch = k < (int)sizeof plain ? (shift ? upper[k] : plain[k]) : 0;
        if (k == 1) {
            wend();
            report("Cancelled.");
        } else if (k == 28) {
            secret = 0;
            if (safeat(pass, 63))
                wifi_start();
            else {
                wend();
                report("Unsupported credential characters.");
            }
        } else if (ch == '\b') {
            int n = len(pass);
            if (n)
                pass[n - 1] = 0;
        } else if (ch >= 32)
            append(pass, sizeof pass, ch);
        return;
    }
    dirty = 1;
    if (pending_doc[0]) {
        if (k == 1 || k == 28)
            discard_choice(0);
        else if (k == 32)
            discard_choice(1);
        return;
    }
    if (savebox) {
        char c = k < (int)sizeof plain ? plain[k] : 0;
        if (k == 1)
            savebox = 0;
        else if (k == 28)
            write_document(savename);
        else if (c == '\b') {
            int n = len(savename);
            if (n)
                savename[n - 1] = 0;
        } else if (c >= 32)
            append(savename, sizeof savename, c);
        return;
    }
    if (setup) {
        if (k == 32) {
            setup = 0;
            welcome = 0;
            hwsetup = 1;
            hwpage = 0;
            launch(SYSTEM);
            return;
        }
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
    if (snipping) {
        if (k == 1)
            snipping = 0;
        return;
    }
    if (context) {
        if (k == 1)
            context = 0;
        return;
    }
    if (winheld) {
        winused = 1;
        if (k == 18) {
            launch(FILES);
            return;
        }
        if (k == 23) {
            launch(SETTINGS);
            return;
        }
        if (k == 32 || k == 50) {
            cli_set(0);
            return;
        }
        if (k == 48) {
            tasks();
            return;
        }
        if (shift && k == 31) {
            quick_action(5);
            return;
        }
        if (active >= 0 && (k == 328 || k == 336)) {
            maximized[active] = k == 328;
            return;
        }
        if (k == 15) {
            task_view();
            return;
        }
        if (ctrl && (k == 331 || k == 333)) {
            switch_desk((desk + 4 + (k == 331 ? -1 : 1)) % 4);
            return;
        }
        if (k == 30) {
            launch(CENTER);
            return;
        }
    }
    if (overview) {
        if (k == 331 || k == 333 || k == 15)
            focusdesk = (focusdesk + 4 + (k == 331 ? -1 : 1)) % 4;
        if (k == 28)
            switch_desk(focusdesk);
        if (k == 1)
            overview = 0;
        return;
    }
    if (k == 87) {
        osk = !osk;
        return;
    }
    if (k == 66) {
        launch(CENTER);
        return;
    }
    if (k == 67) {
        magnify = !magnify;
        return;
    }
    if (k == 68) {
        quick_action(5);
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
    if (k == 60) {
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
    if (k == 1 && hwsetup && active == SYSTEM) {
        hwsetup = 0;
        hwpage = -1;
        setup = 1;
        hide(0);
        return;
    }
    if (k == 1 && active == BROWSER && editing && !tray && !menu) {
        browser_key(k, 0);
        return;
    }
    if (k == 1) {
        if (tray)
            tray = 0;
        else if (menu)
            launcher();
        else if (active >= 0)
            hide(1);
        else if (cli)
            cli_set(0);
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
    if (active == BROWSER) {
        browser_key(k, c);
        return;
    }
    if (active == SOFTWARE) {
        if (k == 28)
            shelf_action("run ");
        if ((k == 328 || k == 336) && pkgcount) {
            pkgsel = clamp(pkgsel + (k == 328 ? -1 : 1), 0, pkgcount - 1);
            confirm_remove = 0;
        }
        return;
    }
    if (utility_key(active, k, c))
        return;
    if (active == TERM || (active < 0 && cli)) {
        if (ctrl && k == 47) {
            paste(line, sizeof line, &cmdpos, 0);
            return;
        }
        if (ctrl && shift && k == 46) {
            copy(clip, line, sizeof clip);
            return;
        }
        if (ctrl && (k == 38 || k == 46)) {
            if (k == 38)
                rows = scroll = 0;
            else
                say("^C");
            line[0] = 0;
            cmdpos = 0;
            return;
        }
        if (k == 329 || k == 337) {
            scroll = clamp(scroll + (k == 329 ? 6 : -6), 0, rows > 0 ? rows - 1 : 0);
            return;
        }
        if (k == 328 || k == 336) {
            recall = clamp(recall + (k == 328 ? -1 : 1), 0, ncmd);
            copy(line, recall == ncmd ? "" : commands[recall], 100);
            cmdpos = len(line);
            return;
        }
        int n = len(line);
        cmdpos = clamp(cmdpos, 0, n);
        if (k == 327 || k == 335) {
            cmdpos = k == 327 ? 0 : n;
            return;
        }
        if (k == 331 || k == 333) {
            cmdpos = clamp(cmdpos + (k == 331 ? -1 : 1), 0, n);
            return;
        }
        if (k == 15) {
            for (int i = 0; n && i < NAPPS; i++)
                if (prefix(apps[i].cmd, line)) {
                    copy(line, apps[i].cmd, 100);
                    cmdpos = len(line);
                    break;
                }
            return;
        }
        if (c == '\n')
            command();
        else if ((c == '\b' && cmdpos > 0) || (k == 339 && cmdpos < n)) {
            if (c == '\b')
                cmdpos--;
            for (int i = cmdpos; i < n; i++)
                line[i] = line[i + 1];
        } else if (c >= 32 && n < 99) {
            for (int i = n; i >= cmdpos; i--)
                line[i + 1] = line[i];
            line[cmdpos++] = c;
        }
        return;
    }
    if (active == WRITER) {
        writer_key(k, c);
        return;
    }
    if (active == FILES) {
        int n = filecount();
        if (k == 15) {
            filetab = (filetab + 1) % 4;
            file_refresh();
            fileidx = 0;
        } else if (n) {
            if (k == 328 || k == 336)
                fileidx = clamp(fileidx + (k == 328 ? -1 : 1), 0, n - 1);
            if (k == 28)
                open_file(selection());
            if (c == 'i' && suffix(selection(), ".imp"))
                install_selection();
            if (c == 'c' && suffix(selection(), ".exe"))
                convert_file(selection());
        }
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
    if (active == SETTINGS && k == 15) {
        setpage = !setpage;
        return;
    }
    if (active == SETTINGS && setpage) {
        if (c >= '1' && c <= '3')
            setting(6 + c - '1');
        return;
    }
    if (active == SETTINGS && c >= '1' && c <= '6') {
        setting(c - '1');
        return;
    }
    if (active < 0 && k == 28)
        launcher();
}
extern void poll(void), beep(int), silence(void);

static void sound(void) {
    if (!quiet_ui && vol)
        pcm_wav(chime, CHIME_SIZE, clamp((vol + 1) / 2, 1, 5));
}
static void serial_line(const char *s) {
    serial(s);
    serial("\r\n");
}
void main(void) {
    for (int i = 0; i < 4; i++) {
        deskx[i] = wx;
        desky[i] = wy;
    }
    hw_init();
    clockread();
    for (int j = 0; j < 198; j++)
        for (int i = 0; i < 466; i++)
            paint[j][i] = WHITE;
    guest_services(guest_read, guest_box);
    guest_save_service(guest_save);
    drivers_init();
    storage_init();
    storage_list(serial_line);
    organizer_load();
    log_event("Desktop initialized");
    if (home_ready())
        loadhome();
    newgame();
    say("FreshOS Shell  /  Type start for apps or help for commands.");
    serial("welcome ready: " EDITION "\r\n");
    u32 last = 0, clocktick = ticks;
    for (;;) {
        poll();
        radios_poll();
        network_poll();
        web_update();
        u32 now = ticks;
        if (feedback[0] && (int)(now - feedback_until) >= 0) {
            feedback[0] = 0;
            dirty = 1;
        }

        if (now - clocktick >= 60) {
            int before = minute;
            clockread();
            clocktick = now;
            alarm_tick(now);
            if (before != minute || active == CONNECTIONS || active == CLOCK)
                dirty = 1;
        }
        if (now != last) {
            int steps = now - last > 6 ? 6 : (int)(now - last);
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
            /* Integrate elapsed display ticks; do not slow transitions when a blit misses a
             * tick. */
            for (int step = 0; step < steps; step++) {
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
            }
            if (dirty) {
                dirty = 0;
                render();
            } else
                cursor_present(1, 0);
        }
        __asm__ volatile("hlt");
    }
}
