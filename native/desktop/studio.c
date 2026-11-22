#include "draw.h"
#include "io.h"
#include "home.h"
#include "math.h"
#include "pci.h"
#include "calendar.h"
#include "package.h"
#include "mediafmt.h"
enum {
    FILES,
    WRITER,
    CALC,
    TERM,
    SETTINGS,
    DEVICE,
    SKETCH,
    CLOCK,
    PACKAGES,
    MEDIA,
    PHOTOS,
    SNAKE,
    APPS
};
struct App {
    const char *name, *hint, *cmd;
    u32 tint;
};
static const struct App app[APPS] = {
    {"Files", "Browse your Home documents", "files", 0xf4b960},
    {"Writer", "A quiet place for your words", "writer", 0x74c7b0},
    {"Calculator", "Evaluate integer expressions", "calc", 0x86b4db},
    {"Terminal", "Run native shell commands", "term", 0x7293af},
    {"Settings", "Tune your workspace", "settings", 0x93cbb8},
    {"System", "Inspect the running kernel", "system", 0xc3a3d9},
    {"Sketch", "Draw on a scalable canvas", "sketch", 0xeaa096},
    {"Clock", "Local time and countdown", "clock", 0x9dbbe4},
    {"IMP", "Installed packages", "imp", 0x11cdff},
    {"Media", "Music on the PC speaker", "media", 0xc96a9e},
    {"Photos", "View Netpbm pictures", "photos", 0x5aa469},
    {"Snake", "A classic arcade game", "snake", 0xe07a4f}};
static const u32 accents[] = {0x117b78, 0x3866a0, 0x895b95};
static const char *const inks[] = {"Ocean", "Coral", "Leaf", "Erase"};
static const u32 colors[] = {0x246298, 0xce746b, 0x398468, WHITE};
static int choice, pop;
static int settings_tab, clock_h, clock_m;
static int confirm_install, year, month, today, month_shift;
static const char *months[] = {"January",   "February", "March",    "April",
                               "May",       "June",     "July",     "August",
                               "September", "October",  "November", "December"};
static char results[8][128];
static int result_count;
static int wx[APPS], wy[APPS], placed[APPS], prior[APPS], dragging, gx, gy;
static Rect editor_page;
static struct TextPoint positions[NOTE_CAP];

static int active = -1, opened[APPS], mode[APPS], start = 0, dirty = 1, mx, my, button, shift,
           ctrl, caps, railshow;
static Rect frame, area, paint_area, thumb_light, thumb_sound;
static int adjusting, welcome = 1;
static char message[120];
static u32 expiry;
static int sel, editing = -1, caret, scroll, changed;
static char draft[NOTE_CAP], clipboard[NOTE_CAP];
static char expr[100], answer[80] = "0";
static char command[160], lines[56][168];
static char history[8][160];
static int hist_n, hist_pos;
static int nlines, termoff;
static u8 pixels[180][320];
static int brush, brush_size, px = -1, py = -1;
static u32 alarm;
static int timer_minutes = 5;
static char timetext[12];
static int clocklast = -1;
/* Writer: selection, undo, find. */
static int sel_anchor = -1;
static char undo_buf[16][NOTE_CAP];
static int undo_next, undo_count;
static char find_buf[64];
static int find_mode;
/* Files: sort, filter, rename, pending delete. */
static int files_sort, pending_delete = -1;
static char files_filter[32];
static int files_search_mode;
static char rename_buf[28];
static int rename_mode, rename_id = -1;
static int file_list[16], file_list_n;
/* Calculator memory. */
static int calc_mem;
/* Clock stopwatch. */
static int clock_mode;
static u32 sw_start, sw_elapsed;
static int sw_running;
/* Media player (PC speaker). */
static int media_freqs[MEDIA_MAX_NOTES], media_durs[MEDIA_MAX_NOTES], media_nnotes;
static char media_name[32];
static int media_kind, media_idx = -1, media_note, media_playing, media_paused;
static u32 media_end, media_remain;
static int media_vol = 70, media_loop = 1;
static Rect media_thumb;
/* Photos. */
static u8 photo_buf[PHOTO_MAX_W * PHOTO_MAX_H * 3];
static int photo_w, photo_h, photo_kind, photo_idx = -1, photo_zoom;
static char photo_name[32];
/* Snake. */
static int snake_x[256], snake_y[256], snake_len, snake_dir = 3, snake_fx, snake_fy;
static int snake_score, snake_high, snake_running, snake_paused, snake_dead;
static u32 snake_next;
static int snake_seed = 12345;
/* Right-click context menu. */
static int autoroll;
static int ctx_kind, ctx_x, ctx_y, ctx_arg;
static Rect ctx_rect;
struct CtxItem {
    const char *label;
    int action, arg;
};
static struct CtxItem ctx_items[10];
static int ctx_count;
enum {
    OPEN = 1,
    START,
    RESTORE,
    FULL,
    HIDE,
    CHOOSE,
    READ,
    NEW,
    SAVE,
    DOCUP,
    DOCDOWN,
    EXEC,
    CLEAR,
    CHAR,
    ACCENT,
    LIGHT,
    SOUND,
    AUTO,
    PREFSAVE,
    WALL,
    WIDE,
    PREFPAGE,
    BRUSH,
    WIPE,
    ALARM,
    MINUTES,
    MONTH,
    INSTALL,
    POP,
    RUNPKG,
    WELCOME,
    USE_OS,
    DISMISS,
    TERM_CLEAR,
    TERM_HELP,
    WRITER_UNDO,
    WRITER_CUT,
    WRITER_COPY,
    WRITER_PASTE,
    WRITER_SELECTALL,
    WRITER_FIND,
    WRITER_COUNT,
    FILES_RENAME,
    FILES_DUP,
    FILES_DEL,
    FILES_PROPS,
    FILES_SORT,
    FILES_SEARCH,
    SKETCH_SAVE,
    SKETCH_SIZE,
    MEDIA_PLAY,
    MEDIA_NEXT,
    MEDIA_PREV,
    MEDIA_STOP,
    MEDIA_VOL,
    PHOTOS_NEXT,
    PHOTOS_PREV,
    PHOTOS_ZOOM,
    SNAKE_NEW,
    SNAKE_PAUSE,
    SNAKE_DIR,
    CALC_COPY,
    CALC_MEM,
    WALL_CYCLE,
    ABOUT,
    SW_TOGGLE,
    SW_START,
    SW_RESET,
    GFX_TOGGLE,
    SOUND_TEST,
    CLOCK_MODE
};
struct Target {
    Rect r;
    int action, arg;
};
static struct Target targets[180];
static int count;
static void target(Rect r, int a, int arg) {
    if (count < 180)
        targets[count++] = (struct Target){r, a, arg};
}
static void button_at(Rect r, const char *s, int a, int arg, int on) {
    rounded(r, 9, on ? accents[home.accent] : 0xd8e9ef);
    shorten(r.x + high(6, (r.w - width(s, 0)) / 2), r.y + (r.h - 18) / 2, s, 0,
            on ? WHITE : INK, r.w - 10);
    target(r, a, arg);
}
static void status(const char *s) {
    strset(message, sizeof message, s);
    expiry = ticks + 200;
    dirty = 1;
    logline(s);
}
static void note_load(int i) {
    if (i < 0 || (unsigned)i >= home.count)
        return;
    editing = i;
    sel = i;
    strset(draft, sizeof draft, home.notes[i].body);
    caret = length(draft);
    scroll = changed = 0;
    sel_anchor = -1;
    undo_count = 0;
    find_mode = 0;
}
static void launch(int id) {
    confirm_install = 0;
    if (id < 0 || id >= APPS)
        return;
    pop = 0;
    active = id;
    if (!opened[id] && home.wide_apps)
        mode[id] = 1;
    opened[id] = 1;
    start = 0;
    if (id == WRITER && editing < 0)
        note_load(sel);
    dirty = 1;
    uart("open: ");
    logline(app[id].cmd);
}
static void save(void) {
    if (editing < 0)
        return;
    strset(home.notes[editing].body, NOTE_CAP, draft);
    changed = 0;
    status(home_commit() ? "Document saved to Home" : "Document kept in session only");
}
static void open_note(void) {
    if (changed && editing != sel) {
        status("Save or revert the Writer draft before opening a different note");
        return;
    }
    if (editing != sel)
        note_load(sel);
    launch(WRITER);
}
static void new_note(const char *name) {
    if (changed) {
        status("Save or revert the current draft first");
        return;
    }
    int n = home_new(name);
    if (n < 0) {
        status("Use a unique lowercase name (up to 27 characters; 16 documents maximum)");
        return;
    }
    note_load(n);
    launch(WRITER);
    status("New document / Ctrl+S to save");
}
static void new_auto(void) {
    for (int n = 1; n < 100; n++) {
        char s[28] = "note-";
        decimal(s + 5, n);
        strset(s + length(s), 5, ".txt");
        if (home_find(s) < 0) {
            new_note(s);
            return;
        }
    }
}
static void print(const char *s) {
    if (nlines == 56) {
        for (int i = 1; i < 56; i++)
            strset(lines[i - 1], sizeof *lines, lines[i]);
        nlines--;
    }
    strset(lines[nlines++], sizeof *lines, s);
    termoff = 0;
}
/* ---- File kinds, media, photos, snake, writer and files helpers ---- */
static const char *ext_of(const char *name) {
    const char *dot = 0;
    for (const char *p = name; *p; p++)
        if (*p == '.')
            dot = p;
    return dot ? dot + 1 : 0;
}
static int file_kind(int id) {
    if (id < 0 || (unsigned)id >= home.count)
        return 0;
    const char *e = ext_of(home.notes[id].name);
    if (e && equal(e, "mus"))
        return 1;
    if (e && (equal(e, "pbm") || equal(e, "pgm") || equal(e, "ppm")))
        return 2;
    return 0;
}
static void media_load_builtin(int i) {
    if (i < 0 || i >= builtin_track_count)
        return;
    int n = media_parse(builtin_tracks[i].text, media_freqs, media_durs, MEDIA_MAX_NOTES);
    if (n < 0)
        return;
    media_nnotes = n;
    media_kind = 0;
    media_idx = i;
    strset(media_name, sizeof media_name, builtin_tracks[i].name);
    media_note = 0;
    media_playing = media_paused = 0;
    tone_freq(0, 0);
}
static void media_load_file(int id) {
    if (id < 0 || (unsigned)id >= home.count)
        return;
    int n = media_parse(home.notes[id].body, media_freqs, media_durs, MEDIA_MAX_NOTES);
    if (n < 0) {
        status("Not a valid .mus file (try: C4:1 E4:1 REST:1)");
        return;
    }
    media_nnotes = n;
    media_kind = 1;
    media_idx = id;
    strset(media_name, sizeof media_name, home.notes[id].name);
    media_note = 0;
    media_playing = media_paused = 0;
    tone_freq(0, 0);
    status("Loaded music file");
}
static void media_start(void) {
    if (!media_nnotes) {
        media_load_builtin(0);
    }
    media_note = 0;
    media_playing = 1;
    media_paused = 0;
    media_end = ticks;
    dirty = 1;
}
static void media_stop(void) {
    media_playing = media_paused = 0;
    media_note = 0;
    tone_freq(0, 0);
    dirty = 1;
}
static void media_tick(void) {
    if (!media_playing || media_paused || !media_nnotes)
        return;
    if ((int)(ticks - media_end) >= 0) {
        if (media_note >= media_nnotes) {
            if (media_loop)
                media_note = 0;
            else {
                media_playing = 0;
                tone_freq(0, 0);
                status("Finished playing");
                return;
            }
        }
        int f = media_freqs[media_note];
        int d = media_durs[media_note];
        media_end = ticks + d * 12;
        tone_freq(f, media_vol);
        media_note++;
        dirty = 1;
    } else if (ticks % 6 == 0)
        dirty = 1;
}
static void media_step(int dir) {
    int total = builtin_track_count;
    for (unsigned i = 0; i < home.count; i++)
        if (file_kind(i) == 1)
            total++;
    if (!total)
        return;
    int cur = 0;
    if (media_kind == 0)
        cur = media_idx;
    else {
        cur = builtin_track_count;
        for (unsigned i = 0; i < home.count; i++)
            if (file_kind(i) == 1) {
                if ((int)i == media_idx)
                    break;
                cur++;
            }
    }
    cur = (cur + dir + total) % total;
    if (cur < builtin_track_count)
        media_load_builtin(cur);
    else {
        int k = cur - builtin_track_count;
        for (unsigned i = 0; i < home.count; i++)
            if (file_kind(i) == 1) {
                if (!k) {
                    media_load_file(i);
                    break;
                }
                k--;
            }
    }
    media_start();
}
static void photos_load_builtin(int i) {
    photo_kind = 0;
    photo_idx = i;
    photo_w = photo_h = 0;
    strset(photo_name, sizeof photo_name, (const char *[]){"Sunset", "Mountains", "Ocean", "Pattern"}[i]);
}
static void photos_load_file(int id) {
    if (id < 0 || (unsigned)id >= home.count)
        return;
    int w, h;
    if (media_parse_image(home.notes[id].body, photo_buf, &w, &h)) {
        status("Not a valid P1/P2/P3 picture file");
        return;
    }
    photo_w = w;
    photo_h = h;
    photo_kind = 1;
    photo_idx = id;
    strset(photo_name, sizeof photo_name, home.notes[id].name);
    status("Loaded picture");
}
static void photos_step(int dir) {
    int total = 4;
    for (unsigned i = 0; i < home.count; i++)
        if (file_kind(i) == 2)
            total++;
    int cur = photo_kind == 0 ? photo_idx : 4;
    if (photo_kind == 1) {
        for (unsigned i = 0; i < home.count; i++)
            if (file_kind(i) == 2) {
                if ((int)i == photo_idx)
                    break;
                cur++;
            }
    }
    cur = (cur + dir + total) % total;
    if (cur < 4)
        photos_load_builtin(cur);
    else {
        int k = cur - 4;
        for (unsigned i = 0; i < home.count; i++)
            if (file_kind(i) == 2) {
                if (!k) {
                    photos_load_file(i);
                    break;
                }
                k--;
            }
    }
    dirty = 1;
}
static int snake_rand(void) {
    snake_seed = snake_seed * 1103515245 + 12345;
    return (snake_seed >> 16) & 32767;
}
static void snake_food(void) {
    for (int t = 0; t < 200; t++) {
        int x = snake_rand() % 20, y = snake_rand() % 15, on = 0;
        for (int i = 0; i < snake_len; i++)
            if (snake_x[i] == x && snake_y[i] == y)
                on = 1;
        if (!on) {
            snake_fx = x;
            snake_fy = y;
            return;
        }
    }
}
static void snake_reset(void) {
    snake_len = 3;
    snake_x[0] = 10;
    snake_y[0] = 7;
    snake_x[1] = 9;
    snake_y[1] = 7;
    snake_x[2] = 8;
    snake_y[2] = 7;
    snake_dir = 3;
    snake_score = 0;
    snake_running = 1;
    snake_paused = snake_dead = 0;
    snake_next = ticks + 6;
    snake_food();
    dirty = 1;
}
static void snake_step(void) {
    int nx = snake_x[0], ny = snake_y[0];
    if (snake_dir == 0)
        ny--;
    else if (snake_dir == 1)
        ny++;
    else if (snake_dir == 2)
        nx--;
    else
        nx++;
    if (nx < 0 || ny < 0 || nx >= 20 || ny >= 15) {
        snake_dead = 1;
        snake_running = 0;
        if (snake_score > snake_high)
            snake_high = snake_score;
        status("Snake hit the wall / press Enter to restart");
        return;
    }
    for (int i = 0; i < snake_len; i++)
        if (snake_x[i] == nx && snake_y[i] == ny) {
            snake_dead = 1;
            snake_running = 0;
            if (snake_score > snake_high)
                snake_high = snake_score;
            status("Snake bit itself / press Enter to restart");
            return;
        }
    int grow = nx == snake_fx && ny == snake_fy;
    int last = grow ? snake_len : snake_len - 1;
    for (int i = last; i > 0; i--) {
        snake_x[i] = snake_x[i - 1];
        snake_y[i] = snake_y[i - 1];
    }
    snake_x[0] = nx;
    snake_y[0] = ny;
    if (grow) {
        if (snake_len < 256)
            snake_len++;
        snake_score++;
        snake_food();
    }
    dirty = 1;
}
static void snake_tick(void) {
    if (active != SNAKE || !snake_running || snake_paused || snake_dead)
        return;
    if ((int)(ticks - snake_next) >= 0) {
        snake_step();
        snake_next = ticks + 6;
    }
}
static void undo_push(void) {
    memcpy(undo_buf[undo_next], draft, NOTE_CAP);
    undo_next = (undo_next + 1) % 16;
    if (undo_count < 16)
        undo_count++;
}
static void undo_pop(void) {
    if (!undo_count) {
        status("Nothing to undo");
        return;
    }
    undo_next = (undo_next + 15) % 16;
    memcpy(draft, undo_buf[undo_next], NOTE_CAP);
    undo_count--;
    caret = length(draft);
    sel_anchor = -1;
    changed = autoroll = 1;
    status("Undid last edit");
}
static void sel_range(int *lo, int *hi) {
    if (sel_anchor < 0) {
        *lo = *hi = -1;
        return;
    }
    *lo = low(sel_anchor, caret);
    *hi = high(sel_anchor, caret);
}
static void writer_copy_sel(void) {
    int lo, hi;
    sel_range(&lo, &hi);
    if (lo < 0) {
        strset(clipboard, sizeof clipboard, draft);
        status("Document copied");
        return;
    }
    int n = hi - lo;
    if (n >= NOTE_CAP)
        n = NOTE_CAP - 1;
    memcpy(clipboard, draft + lo, n);
    clipboard[n] = 0;
    status("Selection copied");
}
static void writer_delete_sel(void) {
    int lo, hi;
    sel_range(&lo, &hi);
    if (lo < 0)
        return;
    undo_push();
    int n = length(draft);
    for (int i = lo; i < n - (hi - lo); i++)
        draft[i] = draft[i + (hi - lo)];
    draft[n - (hi - lo)] = 0;
    caret = lo;
    sel_anchor = -1;
    changed = autoroll = 1;
}
static void writer_paste(void) {
    if (!clipboard[0]) {
        status("Clipboard is empty");
        return;
    }
    writer_delete_sel();
    undo_push();
    for (int i = 0; clipboard[i] && length(draft) < NOTE_CAP - 1; i++) {
        int n = length(draft);
        for (int j = n; j >= caret; j--)
            draft[j + 1] = draft[j];
        draft[caret++] = clipboard[i];
    }
    changed = autoroll = 1;
    status("Pasted");
}
static void find_next(void) {
    if (!find_buf[0]) {
        status("Type a word to find, Enter searches");
        return;
    }
    int n = length(draft), q = length(find_buf);
    for (int pass = 0; pass < 2; pass++) {
        int from = pass ? 0 : caret + 1;
        int to = pass ? caret + 1 : n - q + 1;
        for (int i = from; i < to && i + q <= n; i++) {
            int ok = 1;
            for (int j = 0; j < q; j++)
                if (draft[i + j] != find_buf[j]) {
                    ok = 0;
                    break;
                }
            if (ok) {
                caret = i;
                sel_anchor = -1;
                autoroll = 1;
                status("Found / Enter finds next");
                return;
            }
        }
    }
    status("Not found");
}
static int lower_match(const char *name, const char *filter) {
    if (!filter[0])
        return 1;
    for (const char *p = name; *p; p++) {
        int ok = 1;
        for (int j = 0; filter[j]; j++) {
            char c = p[j];
            if (!c)
                ok = 0;
            else {
                if (c >= 'A' && c <= 'Z')
                    c += 32;
                char f = filter[j];
                if (f >= 'A' && f <= 'Z')
                    f += 32;
                if (c != f)
                    ok = 0;
            }
            if (!ok)
                break;
        }
        if (ok)
            return 1;
    }
    return 0;
}
static void files_rebuild(void) {
    file_list_n = 0;
    for (unsigned i = 0; i < home.count; i++)
        if (lower_match(home.notes[i].name, files_filter))
            file_list[file_list_n++] = i;
    for (int i = 0; i < file_list_n; i++)
        for (int j = i + 1; j < file_list_n; j++) {
            int a = file_list[i], b = file_list[j], swap = 0;
            if (files_sort == 1)
                swap = length(home.notes[a].body) < length(home.notes[b].body);
            else {
                const char *x = home.notes[a].name, *y = home.notes[b].name;
                int k = 0;
                while (x[k] && y[k] && x[k] == y[k])
                    k++;
                swap = files_sort == 2 ? (u8)x[k] < (u8)y[k] : (u8)x[k] > (u8)y[k];
            }
            if (swap) {
                file_list[i] = b;
                file_list[j] = a;
            }
        }
}
static void files_delete(int id) {
    if (id < 0 || (unsigned)id >= home.count)
        return;
    if (pending_delete != id) {
        pending_delete = id;
        status("Click Delete again to confirm deleting this file");
        return;
    }
    pending_delete = -1;
    for (unsigned i = id; i + 1 < home.count; i++)
        memcpy(&home.notes[i], &home.notes[i + 1], sizeof home.notes[i]);
    home.count--;
    if (editing == (int)id)
        editing = -1;
    else if (editing > (int)id)
        editing--;
    if (sel >= (int)home.count)
        sel = high(0, (int)home.count - 1);
    home_commit();
    status("File deleted");
    dirty = 1;
}
static void files_dup(int id) {
    if (id < 0 || (unsigned)id >= home.count)
        return;
    char name[28];
    for (int n = 1; n < 100; n++) {
        strset(name, sizeof name, home.notes[id].name);
        int len = length(name);
        char suffix[8] = "-copy";
        if (n > 1) {
            decimal(suffix + 5, n);
        }
        int sl = length(suffix);
        if (len + sl >= 28) {
            name[28 - sl - 1] = 0;
            len = length(name);
        }
        strset(name + len, sizeof name - len, suffix);
        if (home_find(name) < 0) {
            int nid = home_new(name);
            if (nid < 0) {
                status("No room for a copy (16 files maximum)");
                return;
            }
            strset(home.notes[nid].body, NOTE_CAP, home.notes[id].body);
            home_commit();
            status("File duplicated");
            return;
        }
    }
    status("Could not find a free copy name");
}
static void sketch_save(void) {
    static char buf[NOTE_CAP];
    int pos = 0;
    const char *head = "P1\n64 36\n";
    for (int i = 0; head[i] && pos < NOTE_CAP - 1; i++)
        buf[pos++] = head[i];
    for (int y = 0; y < 36; y++) {
        for (int x = 0; x < 64; x++) {
            if (pos >= NOTE_CAP - 1)
                break;
            int on = pixels[y * 180 / 36][x * 320 / 64] ? 1 : 0;
            buf[pos++] = on ? '1' : '0';
            if (x < 63 && pos < NOTE_CAP - 1)
                buf[pos++] = ' ';
        }
        if (pos < NOTE_CAP - 1)
            buf[pos++] = '\n';
    }
    buf[pos] = 0;
    for (int n = 0; n < 100; n++) {
        char name[28] = "sketch";
        if (n) {
            strset(name + 6, sizeof name - 6, "-");
            decimal(name + 7, n);
        }
        strset(name + length(name), sizeof name - length(name), ".pbm");
        if (home_find(name) < 0) {
            int id = home_new(name);
            if (id < 0) {
                status("No room to save drawing (16 files maximum)");
                return;
            }
            strset(home.notes[id].body, NOTE_CAP, buf);
            home_commit();
            status("Drawing saved / open it in Photos");
            return;
        }
    }
    status("Could not find a free sketch name");
}
static void open_smart(int id) {
    if (id < 0 || (unsigned)id >= home.count)
        return;
    sel = id;
    int k = file_kind(id);
    if (k == 1) {
        media_load_file(id);
        launch(MEDIA);
    } else if (k == 2) {
        photos_load_file(id);
        launch(PHOTOS);
    } else
        open_note();
}
static void compute(void) {
    int v;
    const char *err;
    if (evaluate(expr, &v, &err)) {
        decimal(answer, v);
        for (int i = low(result_count, 7); i > 0; i--)
            strset(results[i], 128, results[i - 1]);
        result_count = low(8, result_count + 1);
        strset(results[0], 128, expr);
        int n = length(results[0]);
        strset(results[0] + n, 128 - n, " = ");
        n = length(results[0]);
        strset(results[0] + n, 128 - n, answer);
    } else
        strset(answer, sizeof answer, err);
}
static PeResult result;
/* A plain MessageBox is synchronous in Win32. Keep the interpreter suspended
 * until acknowledgment; never dispatch other programs on its call stack. */
static int modal_slot;
static void message_box(const char *text) {
    clear_clip();
    int w = low(520, sw - 64);
    Rect p = rect((sw - w) / 2, (sh - 218) / 2, w, 218);
    glass(rect(0, 0, sw, sh), 0, 0x123746, 80);
    rounded(p, 16, WHITE);
    rounded(rect(p.x + 3, p.y + 3, p.w - 6, 36), 12, 0xa6f4ee);
    const char *caption = modal_slot < 0 ? "Windows application" : package_name(modal_slot);
    label(p.x + 16, p.y + 10, caption, 1, INK);
    flow(rect(p.x + 20, p.y + 56, p.w - 40, p.h - 115), text, 1, INK, 0, -1, 0);
    Rect ok = rect(p.x + p.w - 102, p.y + p.h - 48, 80, 32);
    rounded(ok, 9, 0x177e78);
    label(ok.x + 30, ok.y + 6, "OK", 1, WHITE);
    present(home.light);
    cursor(mx, my);
    uart("Win32 dialog: ");
    logline(caption);
    for (;;) {
        input_poll();
        int k;
        while ((k = key_event()) >= 0) {
            if (k == 28 || k == 1 || k == 57)
                goto done;
        }
        int x, y, b;
        int moved = pointer_event(&x, &y, &b) && !vm_absolute();
        if (moved) {
            mx = bound(mx + x, 0, sw - 1);
            my = bound(my + y, 0, sh - 1);
        }
        if (vm_pointer(&x, &y, &b)) {
            mx = x * (sw - 1) / 65535;
            my = y * (sh - 1) / 65535;
            moved = 1;
        }
        if (moved) {
            int pressed = (b & 1) && !(button & 1);
            button = b;
            cursor(mx, my);
            if (pressed && hit(ok, mx, my))
                break;
        }
        idle();
    }
done:
    dirty = 1;
    shift = ctrl = 0;
}
static void report_process(const char *name, int ok) {
    char n[16];
    decimal(n, result.exit_code);
    uart("IMP: ");
    uart(name);
    uart(ok ? " exit " : " failed ");
    logline(ok ? n : result.detail);
    logline(result.output);
    print(result.detail);
    char line[160];
    int nchar = 0;
    for (const char *p = result.output; *p; p++) {
        if (*p == '\n' || nchar == 159) {
            line[nchar] = 0;
            print(line);
            nchar = 0;
        }
        if (*p != '\n')
            line[nchar++] = *p;
    }
    if (nchar) {
        line[nchar] = 0;
        print(line);
    }
}
static void runpkg(int slot, const char *args) {
    compat_args(package_name(slot), args);
    modal_slot = slot;
    int ok = package_run(slot, &result, message_box);
    report_process(package_name(slot), ok);
}
static void installpkg(int s) {
    package_install(s, &result);
    print(result.detail);
    status(result.detail);
}
static void term_complete(void) {
    static const char *const cmds[] = {
        "packages", "runfile",  "run",     "install", "help",    "apps",   "ls",
        "dir",      "cat",      "type",    "new",     "edit",    "save",   "lspci",
        "drivers",  "echo",     "calc",    "uptime",  "ver",     "uname",  "disks",
        "clear",    "cls",      "exit",    "open",    "rm",      "mv",     "cp",
        "wc",       "find",     "play",    "view"};
    char *sp = command;
    while (*sp && *sp != ' ')
        sp++;
    if (*sp) {
        char cmd[32];
        int clen = (int)(sp - command);
        if (clen >= 32)
            return;
        memcpy(cmd, command, clen);
        cmd[clen] = 0;
        char *prefix = sp + 1;
        int need_file = equal(cmd, "open") || equal(cmd, "cat") || equal(cmd, "type") ||
                        equal(cmd, "edit") || equal(cmd, "new") || equal(cmd, "rm") ||
                        equal(cmd, "mv") || equal(cmd, "cp") || equal(cmd, "wc") ||
                        equal(cmd, "play") || equal(cmd, "view");
        if (!need_file)
            return;
        int plen = length(prefix);
        for (unsigned i = 0; i < home.count; i++) {
            const char *name = home.notes[i].name;
            int ok = 1;
            for (int j = 0; j < plen; j++)
                if (name[j] != prefix[j])
                    ok = 0;
            if (ok && length(name) > plen) {
                strset(prefix, sizeof command - (prefix - command), name + plen);
                return;
            }
        }
        return;
    }
    int plen = length(command);
    for (unsigned i = 0; i < sizeof cmds / sizeof *cmds; i++) {
        int ok = 1;
        for (int j = 0; j < plen; j++)
            if (cmds[i][j] != command[j])
                ok = 0;
        if (ok && length(cmds[i]) > plen) {
            strset(command + plen, sizeof command - plen, cmds[i] + plen);
            return;
        }
    }
}
static void execute(void) {
    char buf[160];
    strset(buf, sizeof buf, command);
    if (command[0]) {
        if (hist_n < 8 || !equal(history[hist_n - 1], command)) {
            if (hist_n < 8)
                strset(history[hist_n++], sizeof *history, command);
            else {
                for (int i = 1; i < 8; i++)
                    strset(history[i - 1], sizeof *history, history[i]);
                strset(history[7], sizeof *history, command);
            }
        }
        hist_pos = hist_n;
    }
    print(command);
    command[0] = 0;
    char *args = buf;
    while (*args && *args != ' ')
        args++;
    if (*args)
        *args++ = 0;
    while (*args == ' ')
        args++;
    if (equal(buf, "packages")) {
        for (int i = 0; i < PKG_SLOTS; i++)
            if (*package_name(i))
                print(package_name(i));
    } else if (equal(buf, "runfile") && *args) {
        char path[128] = "/";
        char *tail = args;
        while (*tail && *tail != ' ')
            tail++;
        if (*tail)
            *tail++ = 0;
        if (length(args) > 125) {
            print("App disk path too long");
            return;
        }
        strset(path + 1, sizeof path - 1, args);
        compat_args(args, tail);
        modal_slot = -1;
        report_process(args, raw_run(path, &result, message_box));
    } else if (equal(buf, "run") || (equal(buf, "install") && *args)) {
        char *tail = args;
        while (*tail && *tail != ' ')
            tail++;
        if (*tail)
            *tail++ = 0;
        int i = 0;
        while (i < PKG_SLOTS && (!*args || !equal(package_name(i), args)))
            i++;
        if (i < PKG_SLOTS) {
            if (equal(buf, "install"))
                installpkg(i);
            else
                runpkg(i, tail);
        } else
            print("No installed IMP by that name");
    } else if (equal(buf, "install")) {
        launch(DEVICE);
        confirm_install = 1;
        status("Confirm Install on the System page to continue");
    } else if (equal(buf, "help")) {
        print("packages | install NAME | run NAME | runfile PATH | open imp");
        print("apps | open NAME | ls | cat FILE | edit FILE | new FILE | save");
        print("rm FILE | mv OLD NEW | cp SRC DST | wc FILE | find TEXT");
        print("play FILE | view FILE | calc EXPR | echo TEXT | uptime | disks");
        print("install | ver | clear | exit / Tab completes / Up recalls");
    } else if (equal(buf, "apps"))
        for (int i = 0; i < APPS; i++)
            print(app[i].cmd);
    else if (equal(buf, "ls") || equal(buf, "dir"))
        for (unsigned i = 0; i < home.count; i++) {
            print(home.notes[i].name);
            logline(home.notes[i].name);
        }
    else if (equal(buf, "cat") || equal(buf, "type")) {
        int i = home_find(args);
        if (i < 0)
            print("Document not found");
        else {
            char tmp[156];
            int j = 0;
            const char *s = home.notes[i].body;
            for (; *s; s++) {
                if (*s == '\n' || j == 155) {
                    tmp[j] = 0;
                    print(tmp);
                    j = 0;
                }
                if (*s != '\n')
                    tmp[j++] = *s;
            }
            tmp[j] = 0;
            print(tmp);
        }
    } else if (equal(buf, "new"))
        new_note(args);
    else if (equal(buf, "edit")) {
        int i = home_find(args);
        if (i < 0)
            print("Document not found");
        else {
            sel = i;
            open_note();
        }
    } else if (equal(buf, "save"))
        save();
    else if (equal(buf, "rm")) {
        int i = home_find(args);
        if (i < 0)
            print("Document not found");
        else {
            for (unsigned j = i; j + 1 < home.count; j++)
                memcpy(&home.notes[j], &home.notes[j + 1], sizeof home.notes[j]);
            home.count--;
            if (editing == i)
                editing = -1;
            else if (editing > i)
                editing--;
            home_commit();
            print("Deleted");
            logline("Deleted");
        }
    } else if (equal(buf, "mv") && *args) {
        char *sp2 = args;
        while (*sp2 && *sp2 != ' ')
            sp2++;
        if (!*sp2)
            print("Usage: mv OLD NEW");
        else {
            *sp2++ = 0;
            while (*sp2 == ' ')
                sp2++;
            int i = home_find(args);
            if (i < 0)
                print("Document not found");
            else if (!goodname(sp2) || home_find(sp2) >= 0)
                print("Invalid or taken name");
            else {
                strset(home.notes[i].name, 28, sp2);
                home_commit();
                print("Renamed");
                logline("Renamed");
            }
        }
    } else if (equal(buf, "cp") && *args) {
        char *sp2 = args;
        while (*sp2 && *sp2 != ' ')
            sp2++;
        if (!*sp2)
            print("Usage: cp SRC DST");
        else {
            *sp2++ = 0;
            while (*sp2 == ' ')
                sp2++;
            int i = home_find(args);
            if (i < 0)
                print("Document not found");
            else if (!goodname(sp2) || home_find(sp2) >= 0)
                print("Invalid or taken name");
            else {
                int nid = home_new(sp2);
                if (nid < 0)
                    print("No room (16 files maximum)");
                else {
                    strset(home.notes[nid].body, NOTE_CAP, home.notes[i].body);
                    home_commit();
                    print("Copied");
                    logline("Copied");
                }
            }
        }
    } else if (equal(buf, "wc")) {
        int i = home_find(args);
        if (i < 0)
            print("Document not found");
        else {
            const char *s = home.notes[i].body;
            int words = 0, lines = 1, inword = 0;
            for (int j = 0; s[j]; j++) {
                if (s[j] == '\n')
                    lines++;
                if (s[j] == ' ' || s[j] == '\n' || s[j] == '\t')
                    inword = 0;
                else if (!inword) {
                    words++;
                    inword = 1;
                }
            }
            char b[64];
            decimal(b, words);
            add(b, sizeof b, ' ');
            decimal(b + length(b), length(s));
            add(b, sizeof b, ' ');
            decimal(b + length(b), lines);
            print(b);
            logline(b);
        }
    } else if (equal(buf, "find") && *args) {
        int found = 0;
        for (unsigned i = 0; i < home.count; i++)
            if (lower_match(home.notes[i].body, args)) {
                print(home.notes[i].name);
                logline(home.notes[i].name);
                found = 1;
            }
        if (!found)
            print("No matches");
    } else if (equal(buf, "play")) {
        int i = home_find(args);
        if (i < 0)
            print("Document not found");
        else {
            media_load_file(i);
            launch(MEDIA);
        }
    } else if (equal(buf, "view")) {
        int i = home_find(args);
        if (i < 0)
            print("Document not found");
        else
            open_smart(i);
    } else if (equal(buf, "lspci") || equal(buf, "drivers")) {
        for (int i = 0; i < pci_devices(); i++)
            print(pci_device(i));
        print(pointer_name());
    } else if (equal(buf, "echo"))
        print(args);
    else if (equal(buf, "calc") && *args) {
        int v;
        const char *err;
        if (evaluate(args, &v, &err)) {
            char b[16];
            decimal(b, v);
            print(b);
        } else
            print(err);
    } else if (equal(buf, "uptime")) {
        char b[30];
        decimal(b, ticks / 60);
        add(b, sizeof b, 's');
        print(b);
    } else if (equal(buf, "ver") || equal(buf, "uname"))
        print("FreshOS 2 / Stratus kernel / " EDITION);
    else if (equal(buf, "disks"))
        print(home_state());
    else if (equal(buf, "clear") || equal(buf, "cls"))
        nlines = 0;
    else if (equal(buf, "exit")) {
        active = -1;
        start = 0;
        logline("desktop");
    } else {
        const char *s = equal(buf, "open") ? args : buf;
        int i = 0;
        while (i < APPS && !equal(s, app[i].cmd))
            i++;
        if (i < APPS)
            launch(i);
        else if (*s)
            print("Unknown command. Type help.");
    }
}
static int docked(void) {
    return !(active >= 0 && mode[active] == 2) &&
           (!home.autohide || railshow || start || active < 0);
}
static void geometry(void) {
    int h = sh;
    if (active >= 0 && mode[active]) {
        int left = docked() ? sw / 26 : 0;
        frame = rect(left, 0, sw - left, h);
    } else {
        int w = bound(sw * 72 / 100, 580, 980), fh = bound(h * 78 / 100, 360, 690);
        if (!placed[active]) {
            wx[active] = (sw - w) / 2;
            wy[active] = (h - fh) / 2;
            placed[active] = 1;
        }
        frame = rect(bound(wx[active], 0, sw - w), bound(wy[active], 0, h - fh), w, fh);
    }
    area = rect(frame.x + 14, frame.y + 57, frame.w - 28, frame.h - 72);
}
static void viewmode(int which) {
    if (active < 0)
        return;
    if (which == 2) {
        if (mode[active] == 2)
            mode[active] = prior[active];
        else {
            prior[active] = mode[active];
            mode[active] = 2;
        }
    } else
        mode[active] = mode[active] == 1 ? 0 : 1;
    geometry();
    char s[30];
    uart("layout: ");
    uart(app[active].cmd);
    uart(mode[active] == 2 ? " fullscreen " : mode[active] ? " maximized " : " window ");
    decimal(s, area.w);
    uart(s);
    uart("x");
    decimal(s, area.h);
    logline(s);
    dirty = 1;
}
static void heading(Rect a, const char *k, const char *name) {
    label(a.x, a.y, k, 0, accents[home.accent]);
    label(a.x, a.y + 19, name, 2, INK);
}
static void files_view(Rect a) {
    files_rebuild();
    int wide = a.w >= 840, side = wide ? a.w / 3 : 0;
    heading(a, "HOME  /  COLLECTION", "Documents");
    button_at(rect(a.x + a.w - 126, a.y + 9, 126, 32), "+ New note", NEW, 0, 1);
    int top = a.y + 66, h = a.h - 120, rows = high(1, h / 49),
        cols = a.w - side >= 720 ? 2 : 1;
    int per = rows * cols;
    int selpos = 0;
    for (int i = 0; i < file_list_n; i++)
        if (file_list[i] == sel)
            selpos = i;
    int first = selpos / per * per;
    int cw = (a.w - side) / cols;
    if (rename_mode) {
        Rect box = rect(a.x, top, a.w - side - 8, 36);
        rounded(box, 9, 0xfff3bd);
        char line[64] = "Rename: ";
        strset(line + 8, sizeof line - 8, rename_buf);
        label(box.x + 12, box.y + 9, line, 1, INK);
        top += 44;
        h -= 44;
    } else if (files_search_mode || files_filter[0]) {
        Rect box = rect(a.x, top, a.w - side - 8, 36);
        rounded(box, 9, WHITE);
        char line[64] = "Filter: ";
        strset(line + 8, sizeof line - 8, files_filter);
        label(box.x + 12, box.y + 9, line, 1, INK);
        top += 44;
        h -= 44;
    }
    for (int j = 0; j < per && first + j < file_list_n; j++) {
        int id = file_list[first + j];
        Rect r = rect(a.x + j % cols * cw, top + j / cols * 49, cw - 8, 43);
        rounded(r, 10, sel == id ? 0xc0e4de : WHITE);
        int k = file_kind(id);
        glyph(k == 1 ? MEDIA : k == 2 ? PHOTOS : WRITER, r.x + 10, r.y + 8, 27);
        shorten(r.x + 49, r.y + 6, home.notes[id].name, 1, INK, r.w - 58);
        char info[48];
        strset(info, sizeof info, k == 1 ? "Music / .mus, " : k == 2 ? "Picture, " : "Text, ");
        decimal(info + length(info), length(home.notes[id].body));
        strset(info + length(info), sizeof info - length(info), " bytes");
        label(r.x + 49, r.y + 24, info, 0, DIM);
        target(r, CHOOSE, id);
    }
    if (!file_list_n)
        label(a.x + 8, top + 10, "No files match / right-click for options", 0, DIM);
    if (wide && sel >= 0 && (unsigned)sel < home.count) {
        Rect p = rect(a.x + a.w - side + 7, top, side - 7, h);
        rounded(p, 12, 0xe2eef2);
        label(p.x + 15, p.y + 14, "PREVIEW", 0, DIM);
        shorten(p.x + 15, p.y + 42, home.notes[sel].name, 1, INK, p.w - 30);
        int k = file_kind(sel);
        label(p.x + 15, p.y + 62, k == 1 ? "Opens in Media" : k == 2 ? "Opens in Photos" : "Opens in Writer", 0, DIM);
        flow(rect(p.x + 15, p.y + 85, p.w - 30, p.h - 98), home.notes[sel].body, 0, DIM, 0,
             -1, 0);
    }
    button_at(rect(a.x, a.y + a.h - 36, 114, 32), "Open", READ, 0, 1);
    button_at(rect(a.x + 122, a.y + a.h - 36, 46, 32), "<", CHOOSE,
              file_list_n ? file_list[high(0, first - per)] : 0, 0);
    button_at(rect(a.x + 176, a.y + a.h - 36, 46, 32), ">", CHOOSE,
              file_list_n ? file_list[low(file_list_n - 1, first + per)] : 0, 0);
    button_at(rect(a.x + 230, a.y + a.h - 36, 92, 32), "Search", FILES_SEARCH, 0, 0);
    button_at(rect(a.x + 330, a.y + a.h - 36, 92, 32), "Sort", FILES_SORT, 0, 0);
    shorten(a.x + 436, a.y + a.h - 30, home_state(), 0, DIM, a.w - 440);
}
static int pick_char(int row, int x) {
    int n = length(draft);
    row = bound(row, 0, positions[n].row);
    int best = 0, dist = 0x7fffffff;
    for (int i = 0; i <= n; i++)
        if (positions[i].row == row) {
            int delta = positions[i].x - x;
            if (delta < 0)
                delta = -delta;
            if (delta < dist) {
                dist = delta;
                best = i;
            }
        }
    return best;
}
static int writer_rows;
static void writer_view(Rect a) {
    int side = a.w >= 1000 ? bound(a.w / 5, 180, 260) : 0;
    if (side) {
        rounded(rect(a.x, a.y, side - 14, a.h), 12, 0xe3eff1);
        label(a.x + 16, a.y + 16, "IN THIS HOME", 0, DIM);
        for (unsigned i = 0; i < home.count && 42 + (int)i * 35 < a.h - 32; i++) {
            Rect r = rect(a.x + 8, a.y + 42 + i * 35, side - 30, 31);
            button_at(r, home.notes[i].name, READ, i + 1, (int)i == editing);
        }
    }
    a.x += side;
    a.w -= side;
    heading(a, changed ? "WRITER  /  UNSAVED" : "WRITER  /  DOCUMENT",
            home.notes[editing].name);
    button_at(rect(a.x + a.w - 76, a.y + 9, 76, 32), "Save", SAVE, 0, 1);
    Rect paper = rect(a.x, a.y + 63, a.w, a.h - 101);
    rounded(paper, 12, WHITE);
    int pad = bound(paper.w / 30, 16, 45);
    Rect tr = rect(paper.x + pad, paper.y + 16, paper.w - 2 * pad, paper.h - 32);
    int last = 0;
    editor_page = tr;
    int slo, shi;
    sel_range(&slo, &shi);
    text_sel_lo = slo;
    text_sel_hi = shi;
    int total = flow_map(tr, draft, 1, INK, scroll, caret, &last, positions);
    text_sel_lo = text_sel_hi = -1;
    writer_rows = high(1, tr.h / 24);
    if (scroll >= total)
        scroll = high(0, total - 1);
    {
        int words = 0, lines = 1, inword = 0;
        for (int i = 0; draft[i]; i++) {
            if (draft[i] == '\n')
                lines++;
            if (draft[i] == ' ' || draft[i] == '\n' || draft[i] == '\t')
                inword = 0;
            else if (!inword) {
                words++;
                inword = 1;
            }
        }
        char wc[64];
        decimal(wc, words);
        strset(wc + length(wc), sizeof wc - length(wc), " words / ");
        decimal(wc + length(wc), length(draft));
        strset(wc + length(wc), sizeof wc - length(wc), " chars / ");
        decimal(wc + length(wc), lines);
        strset(wc + length(wc), sizeof wc - length(wc), " lines");
        label(a.x, a.y + a.h - 23, wc, 0, DIM);
    }
    if (find_mode) {
        char line[80] = "Find: ";
        strset(line + 6, sizeof line - 6, find_buf);
        label(a.x + 220, a.y + a.h - 23, line, 0, INK);
    }
    button_at(rect(a.x + a.w - 101, a.y + a.h - 32, 45, 29), "Up", DOCUP, 0, 0);
    button_at(rect(a.x + a.w - 51, a.y + a.h - 32, 51, 29), "Down", DOCDOWN, total, 0);
}
static void calculator(Rect a) {
    int left = a.w >= 800 ? a.w * 2 / 5 : 0;
    Rect display = rect(a.x, a.y, left ? left - 14 : a.w, left ? a.h : 82);
    rounded(display, 12, 0xd8ebe9);
    shorten(display.x + 16, display.y + 12, expr[0] ? expr : "EXPRESSION", 0, DIM,
            display.w - 32);
    shorten(display.x + 16, display.y + 36, answer, 2, INK, display.w - 32);
    if (left) {
        label(display.x + 16, display.y + 108, "RECENT CALCULATIONS", 0, DIM);
        if (!result_count)
            label(display.x + 16, display.y + 145, "Results appear here after Enter or =.", 0,
                  DIM);
        for (int j = 0; j < result_count && 145 + j * 32 < display.h - 22; j++)
            shorten(display.x + 16, display.y + 145 + j * 32, results[j], 1, INK,
                    display.w - 32);
    }
    int x = a.x + left, y = a.y + (left ? 0 : 93), w = a.w - left, h = a.h - (left ? 0 : 93);
    {
        const char *mems[] = {"MC", "MR", "M+", "M-"};
        for (int j = 0; j < 4; j++)
            button_at(rect(x + j * w / 4, y, w / 4 - 6, 30), mems[j], CALC_MEM, j, 0);
        char memline[32] = "Memory: ";
        decimal(memline + 8, calc_mem);
        label(x + 8, y + 36, memline, 0, DIM);
        y += 58;
        h -= 58;
    }
    const char *keys = "789/456*123-0C=+";
    for (int j = 0; j < 16; j++) {
        char s[] = {keys[j], 0};
        Rect r = rect(x + (j % 4) * w / 4, y + (j / 4) * h / 4, w / 4 - 6, h / 4 - 5);
        button_at(r, s,
                  keys[j] == '='   ? EXEC
                  : keys[j] == 'C' ? CLEAR
                                   : CHAR,
                  keys[j], keys[j] == '=');
    }
}
static void terminal(Rect a) {
    rounded(a, 12, 0x17364b);
    int rows = high(1, (a.h - 73) / 21), end = nlines - termoff;
    label(a.x + 16, a.y + 12, "NATIVE CONSOLE    /    type help", 0, 0x91dccc);
    int used = 0;
    for (int i = end - 1; i >= 0 && used < rows; i--) {
        int h = flow(rect(0, 0, a.w - 32, 0), lines[i], 0, WHITE, 0, -1, 0);
        int taken = low(rows - used, h);
        flow(rect(a.x + 16, a.y + 38 + (rows - used - taken) * 21, a.w - 32, taken * 21),
             lines[i], 0, WHITE, h - taken, -1, 0);
        used += taken;
    }
    fill(rect(a.x + 16, a.y + a.h - 36, a.w - 32, 1), 0x375a6b);
    label(a.x + 16, a.y + a.h - 28, ">", 1, 0x84dfc8);
    const char *s = command;
    while (*s && width(s, 1) > a.w - 70)
        s++;
    label(a.x + 38, a.y + a.h - 28, s, 1, WHITE);
}
static void slider(Rect r, int v, int lo, const char *name, int action) {
    label(r.x, r.y, name, 1, INK);
    char b[12];
    decimal(b, v);
    label(r.x + r.w - 36, r.y, b, 1, DIM);
    Rect line = rect(r.x + 3, r.y + 36, r.w - 6, 7);
    rounded(line, 4, 0xc3d8df);
    int k = (line.w - 1) * (v - lo) / (100 - lo);
    rounded(rect(line.x, line.y, k, 7), 4, accents[home.accent]);
    glyph(8, line.x + k - 12, line.y - 10, 26);
    Rect touch = rect(line.x, line.y - 12, line.w, 30);
    target(touch, action, 0);
    if (action == LIGHT)
        thumb_light = touch;
    else
        thumb_sound = touch;
}
static void controls(Rect p) {
    int pad = bound(p.w / 22, 15, 36);
    rounded(p, 12, WHITE);
    label(p.x + pad, p.y + 12, "DISPLAY & SOUND", 0, DIM);
    int tall = p.h > 360;
    int row = tall ? 100 : 57;
    slider(rect(p.x + pad, p.y + 40, p.w - 2 * pad, 46), home.light, 35, "Brightness", LIGHT);
    slider(rect(p.x + pad, p.y + 40 + row, p.w - 2 * pad, 46), home.sound, 0,
           "Feedback pitch", SOUND);
    if (tall) {
        label(p.x + pad, p.y + 98, "Dimming is applied to the guest framebuffer.", 0, DIM);
        label(p.x + pad, p.y + 198, "Choose zero for silence. Drag to audition the tone.", 0,
              DIM);
    }
    int top = p.y + (tall ? 245 : 155);
    label(p.x + pad, top, "WALLPAPER", 0, DIM);
    int cw = (p.w - 2 * pad) / 3, h = bound(p.h - (top - p.y) - 62, 0, 150);
    for (int i = 0; i < 3; i++) {
        Rect q = rect(p.x + pad + i * cw, top + 22, cw - 6, h);
        if (h) {
            wallpaper(q, i);
            target(q, WALL, i);
        }
        button_at(rect(q.x, q.y + h + 2, q.w, 27),
                  (const char *[]){"Sky", "Lagoon", "Dusk"}[i], WALL, i,
                  home.wallpaper == (unsigned)i);
    }
}
static void appearance(Rect p) {
    int pad = bound(p.w / 22, 15, 36);
    rounded(p, 12, WHITE);
    label(p.x + pad, p.y + 12, "WORKSPACE", 0, DIM);
    int w = (p.w - 2 * pad) / 3;
    for (int i = 0; i < 3; i++)
        button_at(rect(p.x + pad + i * w, p.y + 41, w - 5, 29),
                  (const char *[]){"Lagoon", "Sky", "Orchid"}[i], ACCENT, i,
                  home.accent == (unsigned)i);
    int y = p.y + 82;
    if (p.h > 370) {
        int h = low(238, p.h - 234);
        Rect sample = rect(p.x + pad, y, p.w - 2 * pad, h);
        wallpaper(sample, home.wallpaper);
        Rect win =
            rect(sample.x + sample.w / 8, sample.y + h / 6, sample.w * 3 / 4, h * 2 / 3);
        rounded(win, 10, PAPER);
        rounded(rect(win.x + 12, win.y + 12, 9, 9), 4, accents[home.accent]);
        label(win.x + 30, win.y + 8, "Window preview", 0, INK);
        fill(rect(win.x + 10, win.y + 32, win.w - 20, 1), 0xd4e7ea);
        shorten(win.x + 14, win.y + 42, "FreshOS   /   Personalization", 1, INK, win.w - 24);
        y += h + 18;
    }
    button_at(rect(p.x + pad, y, p.w - 2 * pad, 29),
              home.autohide ? "Dock: reveal at screen edge" : "Dock: always visible", AUTO, 0,
              0);
    button_at(rect(p.x + pad, y + 36, p.w - 2 * pad, 29),
              home.wide_apps ? "New apps: maximized" : "New apps: windowed", WIDE, 0, 0);
    button_at(rect(p.x + pad, y + 72, (p.w - 2 * pad - 8) / 2, 29),
              gfx_fast ? "Graphics: Fast" : "Graphics: Fancy", GFX_TOGGLE, 0, gfx_fast);
    button_at(rect(p.x + pad + (p.w - 2 * pad - 8) / 2 + 8, y + 72, (p.w - 2 * pad - 8) / 2, 29),
              "Test sound", SOUND_TEST, 0, 0);
    button_at(rect(p.x + pad, p.y + p.h - 42, p.w - 2 * pad, 32), "Save preferences to Home",
              PREFSAVE, 0, 1);
}
static void settings(Rect a) {
    heading(a, "PREFERENCES  /  WORKSPACE", "Make it yours");
    int two = a.w >= 840, cw = two ? (a.w - 14) / 2 : a.w;
    Rect p = rect(a.x, a.y + 63, cw, a.h - 66);
    if (two) {
        controls(p);
        appearance(rect(p.x + cw + 14, p.y, cw, p.h));
    } else {
        button_at(rect(a.x + a.w - 181, a.y + 14, 82, 29), "Controls", PREFPAGE, 0,
                  !settings_tab);
        button_at(rect(a.x + a.w - 94, a.y + 14, 92, 29), "Workspace", PREFPAGE, 1,
                  settings_tab);
        if (settings_tab)
            appearance(p);
        else
            controls(p);
    }
}
static void info(Rect a) {
    heading(a, "SYSTEM  /  NATIVE HARDWARE", "Kernel overview");
    if (a.w >= 1000) {
        int side = bound(a.w / 3, 270, 480);
        Rect panel = rect(a.x + a.w - side, a.y + 64, side, a.h - 68);
        rounded(panel, 12, 0xe2eef2);
        label(panel.x + 18, panel.y + 16, "PCI INVENTORY", 0, DIM);
        label(panel.x + 18, panel.y + 41, pointer_name(), 1, INK);
        for (int i = 0; i < pci_devices() && 82 + 24 * i < panel.h - 20; i++)
            shorten(panel.x + 18, panel.y + 82 + 24 * i, pci_device(i), 0, INK, panel.w - 36);
        a.w -= side + 12;
    }
    const char *keys[] = {"Kernel",      "Firmware",     "Edition",
                          "Framebuffer", "Clock source", "Documents"};
    char dims[32], num[12];
    decimal(dims, sw);
    add(dims, sizeof dims, 'x');
    decimal(num, sh);
    strset(dims + length(dims), sizeof dims - length(dims), num);
    const char *vals[] = {"Stratus / x86",
#ifdef __x86_64__
                          "x64 UEFI / GOP",
#else
                          "BIOS / VBE",
#endif
                          EDITION,          dims, "8254 PIT / 60 Hz", home_state()};
    int cols = a.w >= 810 ? 3 : 2, rows = (6 + cols - 1) / cols;
    for (int i = 0; i < 6; i++) {
        int w = a.w / cols, h = (a.h - 124) / rows;
        Rect p = rect(a.x + i % cols * w, a.y + 64 + i / cols * h, w - 8, h - 8);
        rounded(p, 12, WHITE);
        label(p.x + 14, p.y + 12, keys[i], 0, DIM);
        shorten(p.x + 14, p.y + p.h / 2, vals[i], 1, INK, p.w - 28);
    }
    {
        int used = 0;
        for (unsigned i = 0; i < home.count; i++)
            used += length(home.notes[i].body) + 28;
        int cap = NOTES * NOTE_CAP;
        int bar_y = a.y + a.h - 82;
        label(a.x, bar_y, "Home storage", 0, DIM);
        Rect bar = rect(a.x + 110, bar_y, low(300, a.w - 340), 12);
        rounded(bar, 6, 0xc3d8df);
        rounded(rect(bar.x, bar.y, bar.w * used / cap, 12), 6, accents[home.accent]);
        char stor[64];
        decimal(stor, used);
        strset(stor + length(stor), sizeof stor - length(stor), " / ");
        decimal(stor + length(stor), cap);
        strset(stor + length(stor), sizeof stor - length(stor), " bytes");
        label(bar.x + bar.w + 12, bar_y - 2, stor, 0, DIM);
        char up[32] = "Uptime ";
        decimal(up + 7, ticks / 60);
        strset(up + length(up), sizeof up - length(up), "s");
        label(a.x, bar_y + 18, up, 0, DIM);
    }
    int y = a.y + a.h - 47;
    shorten(a.x, y, install_status(), 0, DIM, a.w - 230);
    char progress[16];
    int pc = install_progress();
    if (pc >= 0) {
        decimal(progress, pc);
        add(progress, sizeof progress, '%');
        label(a.x, y + 19, progress, 0, INK);
    }
    button_at(rect(a.x + a.w - 204, y, 198, 35),
              pc >= 0           ? "Cancel install"
              : confirm_install ? "Confirm Install (I)"
                                : "Install to Home disk",
              INSTALL, 0, confirm_install);
}
static void sketch(Rect a) {
    int left = a.w >= 900 ? 164 : 0;
    paint_area = rect(a.x + left, a.y + (left ? 0 : 46), a.w - left, a.h - (left ? 0 : 46));
    for (int j = 0; j < 4; j++)
        button_at(
            rect(a.x + (left ? 0 : 90 * j), a.y + (left ? 45 * j : 0), left ? 145 : 83, 32),
            inks[j], BRUSH, j, j == brush);
    button_at(rect(a.x + (left ? 0 : 364), a.y + (left ? 184 : 0), 80, 32), "Clear", WIPE, 0,
              0);
    if (left) {
        label(a.x + 8, a.y + 222, "BRUSH SIZE", 0, DIM);
        const char *sizes[] = {"Small", "Medium", "Large"};
        for (int j = 0; j < 3; j++)
            button_at(rect(a.x, a.y + 244 + 38 * j, 145, 32), sizes[j], SKETCH_SIZE, j,
                      j == brush_size);
        button_at(rect(a.x, a.y + 362, 145, 32), "Save .pbm", SKETCH_SAVE, 0, 1);
        label(a.x + 8, a.y + 404, "Saved drawings open", 0, DIM);
        label(a.x + 8, a.y + 420, "in Photos.", 0, DIM);
    } else {
        button_at(rect(a.x + 454, a.y, 110, 32), "Save .pbm", SKETCH_SAVE, 0, 1);
    }
    int sx = paint_area.w, sy = paint_area.h;
    fill(paint_area, WHITE);
    /* Direct buffer writes: fill() per pixel was the main repaint bottleneck. */
    for (int y = 0; y < 180; y++) {
        int y0 = paint_area.y + y * sy / 180, y1 = paint_area.y + (y + 1) * sy / 180;
        if (y1 <= y0)
            y1 = y0 + 1;
        for (int x = 0; x < 320; x++) {
            u8 v = pixels[y][x];
            if (!v)
                continue;
            int x0 = paint_area.x + x * sx / 320, x1 = paint_area.x + (x + 1) * sx / 320;
            if (x1 <= x0)
                x1 = x0 + 1;
            rect_direct(rect(x0, y0, x1 - x0, y1 - y0), colors[v - 1]);
        }
    }
}
static void clock_view(Rect a) {
    heading(a, "TIME  /  FOCUS", "Clock");
    Rect p = rect(a.x, a.y + 64, a.w, a.h - 68);
    if (a.w >= 740) {
        int ww = a.w * 53 / 100;
        p.w = a.w - ww - 14;
        Rect q = rect(p.x + p.w + 14, p.y, ww, p.h);
        rounded(q, 15, 0xe5f2ef);
        int abs = bound(year * 12 + month - 1 + month_shift, 24000, 25199), yy = abs / 12,
            mm = abs % 12 + 1;
        char date[40];
        strset(date, sizeof date, months[mm - 1]);
        add(date, sizeof date, ' ');
        decimal(date + length(date), yy);
        label(q.x + 15, q.y + 16, date, 1, INK);
        button_at(rect(q.x + q.w - 86, q.y + 9, 34, 30), "<", MONTH, -1, 0);
        button_at(rect(q.x + q.w - 44, q.y + 9, 34, 30), ">", MONTH, 1, 0);
        int cw = (q.w - 24) / 7, ch = (q.h - 78) / 6, offset = first_weekday(yy, mm);
        const char *names[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
        for (int i = 0; i < 7; i++)
            label(q.x + 15 + i * cw, q.y + 49, names[i], 0, DIM);
        for (int i = 1; i <= days_in_month(yy, mm); i++) {
            int k = offset + i - 1, x = q.x + 10 + k % 7 * cw, y = q.y + 72 + k / 7 * ch;
            int now = yy == year && mm == month && i == today;
            rounded(rect(x, y, cw - 4, ch - 3), 8, now ? accents[home.accent] : 0xf6fcfa);
            char day[4];
            decimal(day, i);
            label(x + 6, y + 3, day, 1, now ? WHITE : INK);
        }
    }
    rounded(p, 15, WHITE);
    int x = p.x + bound(p.w / 15, 18, 75);
    label(x, p.y + 16, "LOCAL HARDWARE CLOCK", 0, DIM);
    label(x, p.y + 42, timetext, 2, INK);
    char timer[44];
    if (alarm) {
        strset(timer, sizeof timer, "Seconds left: ");
        decimal(timer + 14, (alarm - ticks + 59) / 60);
    } else {
        decimal(timer, timer_minutes);
        strset(timer + length(timer), 10, " minutes");
    }
    if (p.h > 355) {
        static const int sint[] = {0,   107, 213, 316, 416, 512,  602,  685,
                                   761, 828, 887, 935, 974, 1002, 1018, 1024};
        int rad = low((p.h - 217) / 2, (p.w - 80) / 2), cx = p.x + p.w / 2,
            cy = p.y + 141 + rad;
        rounded(rect(cx - rad, cy - rad, 2 * rad, 2 * rad), rad, 0xe1f1ee);
        int hour = (clock_h % 12) * 5 + clock_m / 12;
        for (int t = 0; t < 60; t++) {
            int q = t / 15, z = t % 15,
                cs = q == 0   ? sint[15 - z]
                     : q == 1 ? -sint[z]
                     : q == 2 ? -sint[15 - z]
                              : sint[z],
                sn = q == 0   ? sint[z]
                     : q == 1 ? sint[15 - z]
                     : q == 2 ? -sint[z]
                              : -sint[15 - z];
            int outer = rad - 9, inner = outer - (t % 5 ? 3 : 9);
            stroke(cx + sn * inner / 1024, cy - cs * inner / 1024, cx + sn * outer / 1024,
                   cy - cs * outer / 1024, t % 5 ? 1 : 3, DIM);
            if (t == hour || t == clock_m) {
                int len = t == clock_m ? rad * 7 / 10 : rad / 2;
                stroke(cx, cy, cx + sn * len / 1024, cy - cs * len / 1024,
                       t == clock_m ? 3 : 6, accents[home.accent]);
            }
        }
        rounded(rect(cx - 5, cy - 5, 10, 10), 5, INK);
    }
    if (clock_mode) {
        u32 elapsed = sw_elapsed + (sw_running ? ticks - sw_start : 0);
        char swt[32];
        decimal(swt, elapsed / 3600);
        strset(swt + length(swt), sizeof swt - length(swt), ":");
        int mm = (elapsed / 60) % 60, ss = (elapsed % 60);
        char part[8];
        decimal(part, mm);
        if (mm < 10) {
            swt[length(swt)] = '0';
            swt[length(swt) + 1] = 0;
        }
        strset(swt + length(swt), sizeof swt - length(swt), part);
        strset(swt + length(swt), sizeof swt - length(swt), ":");
        decimal(part, ss);
        if (ss < 10) {
            swt[length(swt)] = '0';
            swt[length(swt) + 1] = 0;
        }
        strset(swt + length(swt), sizeof swt - length(swt), part);
        label(x, p.y + 102, swt, 1, INK);
        int y = p.y + high(133, p.h - 57);
        button_at(rect(x, y, 120, 32), sw_running ? "Pause" : "Start", SW_START, 0, 1);
        button_at(rect(x + 128, y, 90, 32), "Reset", SW_RESET, 0, 0);
        button_at(rect(x + 226, y, 120, 32), "Timer", CLOCK_MODE, 0, 0);
    } else {
        label(x, p.y + 102, timer, 1, INK);
        int y = p.y + high(133, p.h - 57);
        button_at(rect(x, y, 54, 32), "-", MINUTES, -1, 0);
        button_at(rect(x + 61, y, 54, 32), "+", MINUTES, 1, 0);
        button_at(rect(x + 124, y, 120, 32), alarm ? "Cancel timer" : "Start timer", ALARM, 0, 1);
        button_at(rect(x + 252, y, 130, 32), "Stopwatch", CLOCK_MODE, 0, 0);
    }
}
static void media_view(Rect a) {
    heading(a, "MEDIA  /  PC SPEAKER", "Music player");
    int side = a.w >= 760 ? 240 : 0;
    int y = a.y + 64;
    label(a.x, y, "TRACKS", 0, DIM);
    y += 24;
    for (int i = 0; i < builtin_track_count; i++) {
        Rect r = rect(a.x, y, side ? side - 14 : a.w, 30);
        button_at(r, builtin_tracks[i].name, MEDIA_PREV, i, media_kind == 0 && media_idx == i);
        y += 34;
    }
    for (unsigned i = 0; i < home.count; i++)
        if (file_kind(i) == 1) {
            Rect r = rect(a.x, y, side ? side - 14 : a.w, 30);
            button_at(r, home.notes[i].name, MEDIA_PREV, 100 + (int)i,
                      media_kind == 1 && media_idx == (int)i);
            y += 34;
        }
    if (!side)
        y = a.y + 64;
    Rect p = rect(a.x + side, y, a.w - side, a.h - (y - a.y));
    rounded(p, 12, WHITE);
    int px = p.x + 16;
    label(px, p.y + 14, "NOW PLAYING", 0, DIM);
    shorten(px, p.y + 38, media_nnotes ? media_name : "Choose a track", 2, INK, p.w - 32);
    char prog[48];
    if (media_nnotes) {
        strset(prog, sizeof prog, "Note ");
        decimal(prog + 5, low(media_note, media_nnotes));
        strset(prog + length(prog), sizeof prog - length(prog), " / ");
        decimal(prog + length(prog), media_nnotes);
        if (media_playing)
            strset(prog + length(prog), sizeof prog - length(prog), media_paused ? " / Paused" : " / Playing");
    } else
        strset(prog, sizeof prog, "Built-in tunes + your .mus files");
    label(px, p.y + 78, prog, 1, DIM);
    Rect bar = rect(px, p.y + 104, p.w - 32, 14);
    rounded(bar, 7, 0xc3d8df);
    if (media_nnotes)
        rounded(rect(bar.x, bar.y, bar.w * low(media_note, media_nnotes) / media_nnotes, 14), 7,
                accents[home.accent]);
    int vy = p.y + 132, vh = low(90, p.h - 220);
    if (vh > 20) {
        for (int i = 0; i < 16; i++) {
            int f = media_playing && media_note > 0 && media_note <= media_nnotes
                        ? media_freqs[media_note - 1]
                        : 0;
            int level = f ? bound(f / 8, 8, 100) : 8;
            int wobble = media_playing ? (ticks / 6 + i * 7) % 30 : 0;
            int hh = vh * bound(level + wobble - 15, 4, 100) / 100;
            int bw = (p.w - 32) / 16;
            rounded(rect(px + i * bw, vy + vh - hh, bw - 4, hh), 4, accents[home.accent]);
        }
    }
    int by = p.y + p.h - 44;
    button_at(rect(px, by, 110, 32), media_playing && !media_paused ? "Pause" : "Play", MEDIA_PLAY,
              0, 1);
    button_at(rect(px + 118, by, 80, 32), "Stop", MEDIA_STOP, 0, 0);
    button_at(rect(px + 206, by, 80, 32), "Next", MEDIA_NEXT, 0, 0);
    button_at(rect(px + 294, by, 90, 32), "Prev", MEDIA_PREV, -1, 0);
    label(px, by - 26, "Volume (speaker)", 0, DIM);
    Rect vol = rect(px, by - 18, low(240, p.w - 32), 12);
    rounded(vol, 6, 0xc3d8df);
    rounded(rect(vol.x, vol.y, vol.w * media_vol / 100, 12), 6, accents[home.accent]);
    media_thumb = rect(vol.x, vol.y - 8, vol.w, 28);
    target(media_thumb, MEDIA_VOL, 0);
}
static void photos_view(Rect a) {
    heading(a, "PHOTOS  /  NETPBM", "Picture viewer");
    int side = a.w >= 760 ? 220 : 0;
    int y = a.y + 64;
    label(a.x, y, "PICTURES", 0, DIM);
    y += 24;
    const char *builtins[] = {"Sunset", "Mountains", "Ocean", "Pattern"};
    for (int i = 0; i < 4; i++) {
        Rect r = rect(a.x, y, side ? side - 14 : a.w, 30);
        button_at(r, builtins[i], PHOTOS_PREV, i, photo_kind == 0 && photo_idx == i);
        y += 34;
    }
    for (unsigned i = 0; i < home.count; i++)
        if (file_kind(i) == 2) {
            Rect r = rect(a.x, y, side ? side - 14 : a.w, 30);
            button_at(r, home.notes[i].name, PHOTOS_PREV, 100 + (int)i,
                      photo_kind == 1 && photo_idx == (int)i);
            y += 34;
        }
    if (!side)
        y = a.y + 64;
    Rect p = rect(a.x + side, y, a.w - side, a.h - (y - a.y));
    rounded(p, 12, 0x183b50);
    Rect img = rect(p.x + 12, p.y + 12, p.w - 24, p.h - 76);
    clip(img);
    if (photo_kind == 0) {
        if (photo_idx == 0) {
            for (int j = 0; j < img.h; j++)
                fill(rect(img.x, img.y + j, img.w, 1),
                     ((0xf0a060 * (img.h - j) + 0x3c456f * j) / img.h) & 0xffffff);
            rounded(rect(img.x + img.w / 2 - 40, img.y + img.h / 3 - 40, 80, 80), 40, 0xffd94d);
            for (int j = 0; j < img.h / 3; j++)
                fill(rect(img.x, img.y + img.h - j * 4, img.w, 3), 0x2c5f4f);
        } else if (photo_idx == 1) {
            fill(img, 0x9fd4e8);
            rounded(rect(img.x + img.w / 5, img.y + img.h / 5, img.w / 5, img.w / 5), 20, 0xffd94d);
            for (int j = 0; j < img.h; j++) {
                int peak = img.h / 2 - (j < img.w / 2 ? j : img.w - j) / 3;
                if (img.y + j > img.y + peak)
                    fill(rect(img.x, img.y + j, img.w, 1), 0x4a7a5f);
            }
            fill(rect(img.x, img.y + img.h - 30, img.w, 30), 0x3a6a4f);
        } else if (photo_idx == 2) {
            fill(img, 0x2a7ab8);
            fill(rect(img.x, img.y, img.w, img.h / 3), 0x9fd4e8);
            rounded(rect(img.x + img.w / 4, img.y + img.h / 6, 60, 60), 30, 0xffd94d);
            for (int j = 0; j < 6; j++)
                fill(rect(img.x, img.y + img.h / 3 + j * 14, img.w, 4), 0xffffff);
        } else {
            for (int j = 0; j < img.h; j += 24)
                for (int i = 0; i < img.w; i += 24)
                    fill(rect(img.x + i, img.y + j, 12, 12),
                         ((i / 24 + j / 24) % 2) ? 0x117b78 : 0xf4fbff);
        }
    } else if (photo_w && photo_h) {
        int dw, dh, ox, oy;
        if (photo_zoom) {
            dw = photo_w;
            dh = photo_h;
            ox = img.x + (img.w - dw) / 2;
            oy = img.y + (img.h - dh) / 2;
        } else {
            int sx = img.w / photo_w, sy = img.h / photo_h;
            int s = low(sx, sy);
            if (s < 1)
                s = 1;
            dw = photo_w * s;
            dh = photo_h * s;
            ox = img.x + (img.w - dw) / 2;
            oy = img.y + (img.h - dh) / 2;
        }
        for (int j = 0; j < dh; j++) {
            int sy = photo_zoom ? j : j / high(1, dw / photo_w);
            if (sy < 0 || sy >= photo_h)
                continue;
            for (int i = 0; i < dw; i++) {
                int sx = photo_zoom ? i : i / high(1, dh / photo_h);
                if (sx < 0 || sx >= photo_w)
                    continue;
                int src = (sy * photo_w + sx) * 3;
                u32 c = (photo_buf[src] << 16) | (photo_buf[src + 1] << 8) | photo_buf[src + 2];
                rect_direct(rect(ox + i, oy + j, 1, 1), c);
            }
        }
    } else {
        label(img.x + 16, img.y + 16, "Choose a picture", 1, WHITE);
    }
    clear_clip();
    shorten(p.x + 12, p.y + p.h - 56, photo_name, 1, WHITE, p.w - 24);
    if (photo_kind == 1 && photo_w) {
        char dims[32];
        decimal(dims, photo_w);
        add(dims, sizeof dims, 'x');
        decimal(dims + length(dims), photo_h);
        label(p.x + 12, p.y + p.h - 36, dims, 0, 0x91dccc);
    }
    button_at(rect(p.x + p.w - 240, p.y + p.h - 52, 70, 32), "Prev", PHOTOS_PREV, -1, 0);
    button_at(rect(p.x + p.w - 162, p.y + p.h - 52, 70, 32), "Next", PHOTOS_NEXT, 0, 0);
    button_at(rect(p.x + p.w - 84, p.y + p.h - 52, 76, 32), photo_zoom ? "Fit" : "Actual", PHOTOS_ZOOM, 0, 0);
}
static void snake_view(Rect a) {
    heading(a, "GAMES  /  ARCADE", "Snake");
    int bw = a.w >= 760 ? 200 : 0;
    Rect board_area = bw ? rect(a.x + bw, a.y + 64, a.w - bw, a.h - 108)
                         : rect(a.x, a.y + 104, a.w, a.h - 212);
    int cell = low(board_area.w / 20, board_area.h / 15);
    if (cell < 4)
        cell = 4;
    int ox = board_area.x + (board_area.w - cell * 20) / 2;
    int oy = board_area.y + (board_area.h - cell * 15) / 2;
    rounded(rect(ox - 6, oy - 6, cell * 20 + 12, cell * 15 + 12), 10, 0x183b50);
    for (int j = 0; j < 15; j++)
        for (int i = 0; i < 20; i++)
            fill(rect(ox + i * cell, oy + j * cell, cell, cell),
                 (i + j) % 2 ? 0x1e4a3a : 0x265a48);
    if (!snake_running && !snake_dead && !snake_len)
        snake_reset();
    for (int i = snake_len - 1; i >= 0; i--)
        fill(rect(ox + snake_x[i] * cell, oy + snake_y[i] * cell, cell, cell),
             i == 0 ? 0x7ee787 : 0x3fb950);
    if (snake_running || snake_dead)
        fill(rect(ox + snake_fx * cell, oy + snake_fy * cell, cell, cell), 0xff6b6b);
    if (bw) {
        label(a.x, a.y + 64, "SCORE", 0, DIM);
        char sc[32];
        decimal(sc, snake_score);
        label(a.x, a.y + 88, sc, 2, INK);
        label(a.x, a.y + 130, "BEST", 0, DIM);
        decimal(sc, snake_high);
        label(a.x, a.y + 154, sc, 2, INK);
        button_at(rect(a.x, a.y + 200, 170, 32), snake_dead ? "Restart" : "New game", SNAKE_NEW, 0, 1);
        button_at(rect(a.x, a.y + 240, 170, 32), snake_paused ? "Resume" : "Pause", SNAKE_PAUSE, 0, 0);
        label(a.x, a.y + 290, "Arrow keys steer.", 0, DIM);
        label(a.x, a.y + 308, "Space pauses.", 0, DIM);
        if (snake_dead)
            label(a.x, a.y + 340, "Game over!", 1, INK);
    } else {
        button_at(rect(a.x, a.y + 64, 110, 32), snake_dead ? "Restart" : "New", SNAKE_NEW, 0, 1);
        button_at(rect(a.x + 118, a.y + 64, 110, 32), snake_paused ? "Resume" : "Pause", SNAKE_PAUSE, 0, 0);
        char sc[32] = "Score ";
        decimal(sc + 6, snake_score);
        label(a.x + 240, a.y + 72, sc, 1, INK);
    }
    int dy = bw ? a.y + 380 : a.y + a.h - 108;
    int dx = bw ? a.x + 60 : ox + cell * 9;
    button_at(rect(dx, dy, 44, 32), "^", SNAKE_DIR, 0, 0);
    button_at(rect(dx - 48, dy + 36, 44, 32), "<", SNAKE_DIR, 2, 0);
    button_at(rect(dx + 48, dy + 36, 44, 32), ">", SNAKE_DIR, 3, 0);
    button_at(rect(dx, dy + 72, 44, 32), "v", SNAKE_DIR, 1, 0);
}
/* Reference artwork uses 960 x 540 design coordinates; maintain square controls
 * even on 4:3 framebuffers. Anchor the assistant at the lower middle. */
static int scale(int v) { return v * low(sw * 1000 / 960, sh * 1000 / 540) / 1000; }
static Rect tile(int x, int y, int w, int h) {
    return rect(scale(x), scale(y), scale(w), scale(h));
}
static void center(Rect r, int y, const char *s, int f, u32 c) {
    label(r.x + (r.w - width(s, f)) / 2, y, s, f, c);
}
static void desk_date(char b[16]) {
    decimal(b, month);
    add(b, 16, '/');
    decimal(b + length(b), today);
}
static void dock(void) {
    Rect rail = tile(0, 0, 34, 334);
    shell_art(start ? 1 : 0, rail);
    Rect head = tile(0, 0, 31, 39);
    glyph(8, scale(5), scale(3), scale(21));
    center(head, scale(24), "IMP", 0, INK);
    target(head, OPEN, PACKAGES);
    Rect clk = tile(0, 143, 31, 32);
    char date[16];
    desk_date(date);
    if (!start && pop != 1) {
        clip(clk);
        center(clk, clk.y, timetext[0] == '0' ? timetext + 1 : timetext, 0, DIM);
        center(clk, clk.y + 14, date, 0, DIM);
        clear_clip();
    }
    target(clk, OPEN, CLOCK);
    target(tile(0, 175, 30, 21), POP, 1);
    target(tile(0, 197, 30, 20), POP, 2);
    target(tile(0, 218, 30, 21), POP, 3);
    target(tile(0, 242, 34, 88), START, 0);
}
static void launcher(void) {
    Rect disc = tile(54, 157, 238, 238), clock = tile(239, 54, 184, 184);
    shell_art(2, clock);
    center(clock, clock.y + clock.h / 2 - scale(30),
           timetext[0] == '0' ? timetext + 1 : timetext, sw >= 1280 ? 3 : 2, DIM);
    char date[16];
    desk_date(date);
    center(clock, clock.y + clock.h / 2 + scale(17), date, sw >= 1280 ? 2 : 1, DIM);
    target(clock, OPEN, CLOCK);
    shell_art(2, disc);
    center(disc, disc.y + scale(18), "All Apps", 1, DIM);
    for (int i = 0; i < APPS; i++) {
        Rect r = tile(87 + i % 3 * 57, 201 + i / 3 * 47, 56, 45);
        int size = scale(22);
        if (i == choice)
            glass(rect(r.x + 1, r.y, r.w - 2, r.h), 8, WHITE, 110);
        glyph(i, r.x + (r.w - size) / 2, r.y, size);
        center(r, r.y + size, app[i].name, 0, INK);
        target(r, OPEN, i);
    }
    Rect intro = tile(158, 354, 30, 27);
    glyph(8, intro.x, intro.y, scale(19));
    target(intro, WELCOME, 0);
}
static void bubbles(void) {
    if (start || pop == 1) {
        Rect r = tile(0, 99, 96, 85);
        shell_art(3, r);
        int last = -1, n = 0;
        for (int i = 0; i < APPS; i++)
            if (opened[i]) {
                last = i;
                n++;
            }
        center(r, r.y + scale(13), n ? app[last].name : "No apps", 0, DIM);
        center(r, r.y + scale(32), "in Tray", 0, DIM);
        if (n)
            target(r, OPEN, last);
    }
    if (pop < 2)
        return;
    int x = sw / 2, y = sh - scale(54);
    glyph(8, x, y, scale(54));
    target(rect(x, y, scale(54), scale(54)), POP, 0);
    Rect r = rect(x + scale(27), sh - scale(124), scale(132), scale(102));
    shell_art(pop == 3 ? 4 : 5, r);
    if (pop == 2) {
        center(r, r.y + scale(16), "Network", 1, INK);
        center(r, r.y + scale(38), "Offline", 0, DIM);
        return;
    }
    int light = pop == 4;
    center(r, r.y + scale(8), light ? "Brightness" : "Volume", 1, INK);
    Rect t = rect(r.x + scale(14), r.y + scale(37), r.w - scale(28), scale(23));
    rounded(t, 7, 0x09cbff);
    int pct = light ? (int)(home.light - 35) * 100 / 65 : (int)home.sound;
    rounded(rect(t.x, t.y, t.w * pct / 100, t.h), 7, 0x60f628);
    if (light)
        thumb_light = t;
    else
        thumb_sound = t;
    target(t, light ? LIGHT : SOUND, 0);
    target(rect(r.x, r.y, r.w, scale(32)), POP, light ? 3 : 4);
}
static void paint_window(void) {
    geometry();
    if (mode[active] == 0)
        glass(rect(frame.x + 3, frame.y + 6, frame.w, frame.h), 16, 0x204f63, 135);
    rounded(frame, mode[active] ? 0 : 15, WHITE);
    for (int y = 3; y < 43; y++) {
        int a = y <= 24 ? y * 10 : (42 - y) * 13;
        fill(rect(frame.x + 4, frame.y + y, frame.w - 8, 1),
             ((255 - a) << 16) | (255 << 8) | (255 - a / 3));
    }
    glyph(active, frame.x + 15, frame.y + 9, 29);
    label(frame.x + 56, frame.y + 13, app[active].name, 1, INK);
    int x = frame.x + frame.w;
    button_at(rect(x - 211, frame.y + 9, 75, 28), mode[active] == 2 ? "Restore" : "Full F11",
              FULL, 0, mode[active] == 2);
    button_at(rect(x - 130, frame.y + 9, 66, 28), mode[active] == 1 ? "Restore" : "Max",
              RESTORE, 0, mode[active] == 1);
    button_at(rect(x - 57, frame.y + 9, 45, 28), "Hide", HIDE, 0, 0);
    fill(rect(frame.x + 10, frame.y + 46, frame.w - 20, 1), 0xd4e4e9);
    clip(area);
    switch (active) {
    case PACKAGES: {
        heading(area, "HOME DISK  /  EXECUTABLES", "IMP Library");
        int y = area.y + 64;
        for (int i = 0; i < PKG_SLOTS; i++)
            if (*package_name(i)) {
                char caption[44];
                strset(caption, sizeof caption, package_pending(i) ? "Install: " : "Run: ");
                strset(caption + length(caption), sizeof caption - length(caption),
                       package_name(i));
                button_at(rect(area.x + 8, y, 160, 26), caption, RUNPKG, i,
                          package_pending(i));
                y += 30;
            }
        if (y == area.y + 64)
            label(area.x + 8, y, "No packages on Home", 0, DIM);
        Rect report = rect(area.x + 188, area.y + 68, area.w - 208, area.h - 88);
        label(report.x, report.y,
              result.detail[0] ? result.detail : "Choose an installed package to run.", 1,
              INK);
        report.y += 36;
        report.h -= 36;
        flow(report, result.output, 1, DIM, 0, -1, 0);
        break;
    }
    case FILES:
        files_view(area);
        break;
    case WRITER: {
        if (autoroll) {
            int side = area.w >= 1000 ? bound(area.w / 5, 180, 260) : 0, w = area.w - side,
                pad = bound(w / 30, 16, 45);
            int line = 0;
            flow(rect(0, 0, w - 2 * pad, 0), draft, 1, INK, 0, caret, &line);
            int rows = high(1, (area.h - 133) / 24);
            if (line < scroll)
                scroll = line;
            if (line >= scroll + rows)
                scroll = line - rows + 1;
            autoroll = 0;
        }
        writer_view(area);
        break;
    }
    case CALC:
        calculator(area);
        break;
    case TERM:
        terminal(area);
        break;
    case SETTINGS:
        settings(area);
        break;
    case DEVICE:
        info(area);
        break;
    case SKETCH:
        sketch(area);
        break;
    case CLOCK:
        clock_view(area);
        break;
    case MEDIA:
        media_view(area);
        break;
    case PHOTOS:
        photos_view(area);
        break;
    case SNAKE:
        snake_view(area);
        break;
    }
    clear_clip();
}
static void repaint(void) {
    clear_clip();
    count = 0;
    if (welcome) {
        wallpaper(rect(0, 0, sw, sh), 3);
        Rect use = rect(sw * 472 / 960, sh * 377 / 540, sw * 78 / 960, sh * 163 / 540);
        center(use, sh * 398 / 540, "Use OS", sw >= 1280 ? 2 : 1, 0xfaff79);
        int x = use.x + use.w / 2, y = sh * 455 / 540;
        rounded(rect(x - scale(15), y - scale(4), scale(30), scale(8)), 1, 0x06ff96);
        stroke(x + scale(14), y, x + scale(4), y - scale(9), scale(4), 0x06ff96);
        stroke(x + scale(14), y, x + scale(4), y + scale(9), scale(4), 0x06ff96);
        target(use, USE_OS, 0);
        present(home.light);
        cursor(mx, my);
        dirty = 0;
        return;
    }
    if (active < 0 || mode[active] != 2 || start)
        wallpaper(rect(0, 0, sw, sh), home.wallpaper);
    if (active >= 0)
        paint_window();
    if (docked() && !start)
        dock();
    if (start) {
        count = 0;
        dock();
        launcher();
    }
    if (docked() || start)
        bubbles();
    if (message[0]) {
        int w = low(sw - 20, width(message, 0) + 36);
        Rect p = rect((sw - w) / 2, sh - 38, w, 32);
        rounded(p, 13, 0x20495c);
        shorten(p.x + 16, p.y + 7, message, 0, WHITE, w - 32);
    }
    if (ctx_kind) {
        glass(rect(ctx_rect.x + 3, ctx_rect.y + 5, ctx_rect.w, ctx_rect.h), 10, 0x204f63, 120);
        rounded(ctx_rect, 10, WHITE);
        for (int i = 0; i < ctx_count; i++) {
            Rect r = rect(ctx_rect.x + 4, ctx_rect.y + 4 + i * 26, ctx_rect.w - 8, 24);
            rounded(r, 7, 0xe8f4f8);
            label(r.x + 10, r.y + 4, ctx_items[i].label, 0, INK);
            target(r, ctx_items[i].action, ctx_items[i].arg);
        }
    }
    present(home.light);
    cursor(mx, my);
    dirty = 0;
}
static void slider_set(int a, int x) {
    Rect r = a == LIGHT ? thumb_light : a == MEDIA_VOL ? media_thumb : thumb_sound;
    int min = a == LIGHT ? 35 : 0;
    int v = min + bound(x - r.x, 0, r.w - 1) * (100 - min) / high(1, r.w - 1);
    if (a == LIGHT)
        home.light = v;
    else if (a == MEDIA_VOL)
        media_vol = v;
    else
        home.sound = v;
    dirty = 1;
}
static void action(int a, int arg) {
    switch (a) {
    case WELCOME:
        welcome = 1;
        start = 0;
        break;
    case USE_OS:
        welcome = 0;
        break;
    case RUNPKG:
        if (package_pending(arg))
            installpkg(arg);
        else
            runpkg(arg, "");
        break;
    case POP:
        pop = pop == arg ? 0 : arg;
        break;
    case MONTH:
        month_shift = bound(month_shift + arg, -120, 120);
        break;
    case INSTALL:
        if (install_progress() >= 0) {
            install_cancel();
            status(install_status());
        } else if (confirm_install) {
            confirm_install = 0;
            install_start();
            status(install_status());
        } else {
            confirm_install = 1;
            status("Write system area of the marked Home disk? Click Confirm Install.");
        }
        break;
    case OPEN:
        pop = 0;
        confirm_install = 0;
        launch(arg);
        break;
    case START:
        pop = 0;
        start = !start;
        choice = 0;
        break;
    case HIDE:
        confirm_install = 0;
        active = -1;
        logline("desktop");
        break;
    case FULL:
        viewmode(2);
        break;
    case RESTORE:
        viewmode(1);
        break;
    case NEW:
        new_auto();
        break;
    case SAVE:
        save();
        break;
    case CHOOSE:
        sel = arg;
        break;
    case READ:
        if (arg)
            sel = arg - 1;
        if (active == FILES)
            open_smart(sel);
        else
            open_note();
        break;
    case DOCUP:
        scroll = high(0, scroll - writer_rows);
        break;
    case DOCDOWN:
        scroll = low(high(0, arg - writer_rows), scroll + writer_rows);
        break;
    case EXEC:
        compute();
        break;
    case CLEAR:
        expr[0] = 0;
        strset(answer, sizeof answer, "0");
        break;
    case CHAR:
        add(expr, sizeof expr, arg);
        break;
    case WALL:
        home.wallpaper = arg;
        break;
    case WIDE:
        home.wide_apps ^= 1;
        break;
    case PREFPAGE:
        settings_tab = arg;
        break;
    case ACCENT:
        home.accent = arg;
        break;
    case AUTO:
        home.autohide = !home.autohide;
        break;
    case PREFSAVE:
        status(home_commit() ? "Settings saved to Home" : "Settings applied / session only");
        break;
    case LIGHT:
    case SOUND:
        adjusting = a;
        slider_set(a, mx);
        break;
    case BRUSH:
        brush = arg;
        break;
    case WIPE:
        memset(pixels, 0, sizeof pixels);
        break;
    case ALARM:
        alarm = alarm ? 0 : ticks + timer_minutes * 3600;
        break;
    case MINUTES:
        if (!alarm)
            timer_minutes = bound(timer_minutes + arg, 1, 60);
        break;
    case TERM_CLEAR:
        nlines = 0;
        break;
    case TERM_HELP:
        print("Type help for commands. Tab completes. Right-click for menu.");
        break;
    case WRITER_UNDO:
        undo_pop();
        break;
    case WRITER_CUT:
        if (sel_anchor < 0)
            status("Select text first (Ctrl+A selects all)");
        else {
            writer_copy_sel();
            writer_delete_sel();
        }
        break;
    case WRITER_COPY:
        writer_copy_sel();
        break;
    case WRITER_PASTE:
        writer_paste();
        break;
    case WRITER_SELECTALL:
        sel_anchor = 0;
        caret = length(draft);
        status("Selected all / Ctrl+C copies");
        break;
    case WRITER_FIND:
        find_mode = 1;
        find_buf[0] = 0;
        status("Find: type, Enter searches, Esc cancels");
        break;
    case WRITER_COUNT:
        status("See word count in the footer");
        break;
    case FILES_RENAME:
        if (arg >= 0 && (unsigned)arg < home.count) {
            rename_mode = 1;
            rename_id = arg;
            strset(rename_buf, sizeof rename_buf, home.notes[arg].name);
            status("Type new name, Enter confirms, Esc cancels");
        }
        break;
    case FILES_DUP:
        files_dup(arg);
        break;
    case FILES_DEL:
        files_delete(arg);
        break;
    case FILES_PROPS: {
        if (arg >= 0 && (unsigned)arg < home.count) {
            sel = arg;
            char info[96];
            strset(info, sizeof info, home.notes[arg].name);
            strset(info + length(info), sizeof info - length(info), " / ");
            decimal(info + length(info), length(home.notes[arg].body));
            strset(info + length(info), sizeof info - length(info), " bytes / ");
            int k = file_kind(arg);
            strset(info + length(info), sizeof info - length(info),
                   k == 1 ? "Music" : k == 2 ? "Picture" : "Text");
            status(info);
        }
        break;
    }
    case FILES_SORT:
        files_sort = (files_sort + 1) % 3;
        status(files_sort == 1 ? "Sorted by size" : files_sort == 2 ? "Sorted Z to A" : "Sorted A to Z");
        break;
    case FILES_SEARCH:
        files_search_mode = !files_search_mode;
        if (!files_search_mode)
            files_filter[0] = 0;
        status(files_search_mode ? "Type to filter, Enter keeps, Esc clears" : "Filter cleared");
        break;
    case SKETCH_SAVE:
        sketch_save();
        break;
    case SKETCH_SIZE:
        brush_size = arg;
        break;
    case MEDIA_PLAY:
        if (media_playing && !media_paused) {
            media_paused = 1;
            media_remain = media_end - ticks;
            tone_freq(0, 0);
        } else if (media_paused) {
            media_paused = 0;
            media_end = ticks + media_remain;
        } else
            media_start();
        break;
    case MEDIA_NEXT:
        media_step(1);
        break;
    case MEDIA_PREV:
        if (arg >= 100)
            media_load_file(arg - 100);
        else if (arg >= 0)
            media_load_builtin(arg);
        else
            media_step(-1);
        break;
    case MEDIA_STOP:
        media_stop();
        break;
    case MEDIA_VOL:
        adjusting = MEDIA_VOL;
        slider_set(MEDIA_VOL, mx);
        break;
    case PHOTOS_NEXT:
        photos_step(1);
        break;
    case PHOTOS_PREV:
        if (arg >= 100)
            photos_load_file(arg - 100);
        else if (arg >= 0)
            photos_load_builtin(arg);
        else
            photos_step(-1);
        break;
    case PHOTOS_ZOOM:
        photo_zoom ^= 1;
        break;
    case SNAKE_NEW:
        snake_reset();
        break;
    case SNAKE_PAUSE:
        snake_paused = !snake_paused;
        break;
    case SNAKE_DIR: {
        int want = arg;
        if (!((snake_dir == 0 && want == 1) || (snake_dir == 1 && want == 0) ||
              (snake_dir == 2 && want == 3) || (snake_dir == 3 && want == 2)))
            snake_dir = want;
        break;
    }
    case CALC_COPY:
        strset(clipboard, sizeof clipboard, answer);
        status("Result copied / paste in Writer with Ctrl+V");
        break;
    case CALC_MEM:
        if (arg == 0)
            calc_mem = 0;
        else if (arg == 1) {
            decimal(expr, calc_mem);
        } else {
            int v;
            const char *err;
            if (evaluate(expr, &v, &err))
                calc_mem = arg == 2 ? calc_mem + v : calc_mem - v;
            else
                status(err);
        }
        break;
    case WALL_CYCLE:
        home.wallpaper = (home.wallpaper + 1) % 3;
        break;
    case ABOUT:
        launch(DEVICE);
        break;
    case SW_TOGGLE:
    case CLOCK_MODE:
        clock_mode ^= 1;
        break;
    case SW_START:
        if (sw_running) {
            sw_elapsed += ticks - sw_start;
            sw_running = 0;
        } else {
            sw_start = ticks;
            sw_running = 1;
        }
        break;
    case SW_RESET:
        sw_elapsed = 0;
        sw_start = ticks;
        break;
    case GFX_TOGGLE:
        gfx_fast ^= 1;
        status(gfx_fast ? "Fast graphics on (smoother)" : "Fancy graphics on");
        break;
    case SOUND_TEST:
        tone(home.sound);
        status("Test tone played");
        break;
    }
    dirty = 1;
}
static void dab(int x, int y) {
    int r = brush_size == 0 ? 2 : brush_size == 1 ? 4 : 7;
    for (int j = -r; j <= r; j++)
        for (int i = -r; i <= r; i++)
            if (i * i + j * j <= r * r && (unsigned)(x + i) < 320 && (unsigned)(y + j) < 180)
                pixels[y + j][x + i] = brush == 3 ? 0 : brush + 1;
}
static u32 last_click;
static int last_id = -1;
static void ctx_add(const char *label, int action, int arg) {
    if (ctx_count < 10)
        ctx_items[ctx_count++] = (struct CtxItem){label, action, arg};
}
static void close_ctx(void) {
    ctx_kind = 0;
    dirty = 1;
}
static void open_ctx(int x, int y) {
    if (welcome)
        return;
    if (start) {
        start = 0;
        dirty = 1;
        return;
    }
    ctx_count = 0;
    ctx_arg = -1;
    if (active >= 0 && hit(rect(frame.x, frame.y, frame.w, 45), x, y)) {
        ctx_add(mode[active] == 2 ? "Restore window" : "Fullscreen", FULL, 0);
        ctx_add(mode[active] == 1 ? "Restore" : "Maximize", RESTORE, 0);
        ctx_add("Hide", HIDE, 0);
        ctx_add("Close", HIDE, 0);
    } else if (active == FILES) {
        for (int i = count - 1; i >= 0; i--)
            if (targets[i].action == CHOOSE && hit(targets[i].r, x, y)) {
                ctx_arg = targets[i].arg;
                break;
            }
        if (ctx_arg >= 0) {
            ctx_add("Open", READ, 0);
            ctx_add("Rename", FILES_RENAME, ctx_arg);
            ctx_add("Duplicate", FILES_DUP, ctx_arg);
            ctx_add("Delete", FILES_DEL, ctx_arg);
            ctx_add("Properties", FILES_PROPS, ctx_arg);
        }
        ctx_add("+ New note", NEW, 0);
        ctx_add("Search", FILES_SEARCH, 0);
        ctx_add("Sort", FILES_SORT, 0);
    } else if (active == WRITER) {
        ctx_add("Save", SAVE, 0);
        ctx_add("Undo", WRITER_UNDO, 0);
        ctx_add("Cut", WRITER_CUT, 0);
        ctx_add("Copy", WRITER_COPY, 0);
        ctx_add("Paste", WRITER_PASTE, 0);
        ctx_add("Select All", WRITER_SELECTALL, 0);
        ctx_add("Find", WRITER_FIND, 0);
        ctx_add("Word count", WRITER_COUNT, 0);
        ctx_add("Close", HIDE, 0);
    } else if (active == SKETCH) {
        ctx_add("Save drawing (.pbm)", SKETCH_SAVE, 0);
        ctx_add("Clear", WIPE, 0);
        ctx_add("Brush: Small", SKETCH_SIZE, 0);
        ctx_add("Brush: Medium", SKETCH_SIZE, 1);
        ctx_add("Brush: Large", SKETCH_SIZE, 2);
        ctx_add("Close", HIDE, 0);
    } else if (active == TERM) {
        ctx_add("Clear", TERM_CLEAR, 0);
        ctx_add("Help", TERM_HELP, 0);
        ctx_add("Close", HIDE, 0);
    } else if (active == MEDIA) {
        ctx_add(media_playing && !media_paused ? "Pause" : "Play", MEDIA_PLAY, 0);
        ctx_add("Next track", MEDIA_NEXT, 0);
        ctx_add("Stop", MEDIA_STOP, 0);
        ctx_add("Close", HIDE, 0);
    } else if (active == PHOTOS) {
        ctx_add("Next picture", PHOTOS_NEXT, 0);
        ctx_add("Previous picture", PHOTOS_PREV, -1);
        ctx_add(photo_zoom ? "Fit to window" : "Actual size", PHOTOS_ZOOM, 0);
        ctx_add("Close", HIDE, 0);
    } else if (active == SNAKE) {
        ctx_add("New game", SNAKE_NEW, 0);
        ctx_add(snake_paused ? "Resume" : "Pause", SNAKE_PAUSE, 0);
        ctx_add("Close", HIDE, 0);
    } else if (active == CALC) {
        ctx_add("Copy result", CALC_COPY, 0);
        ctx_add("Clear", CLEAR, 0);
        ctx_add("Close", HIDE, 0);
    } else if (active >= 0) {
        ctx_add(mode[active] == 2 ? "Restore window" : "Fullscreen", FULL, 0);
        ctx_add(mode[active] == 1 ? "Restore" : "Maximize", RESTORE, 0);
        ctx_add("Hide", HIDE, 0);
    } else {
        ctx_add("Terminal", OPEN, TERM);
        ctx_add("All Apps", START, 0);
        ctx_add("Files", OPEN, FILES);
        ctx_add("Media", OPEN, MEDIA);
        ctx_add("Photos", OPEN, PHOTOS);
        ctx_add("Snake", OPEN, SNAKE);
        ctx_add("Settings", OPEN, SETTINGS);
        ctx_add("Change wallpaper", WALL_CYCLE, 0);
        ctx_add("About", ABOUT, 0);
    }
    int w = 210, h = ctx_count * 26 + 8;
    ctx_x = low(x, sw - w - 4);
    ctx_y = low(y, sh - h - 4);
    if (ctx_x < 2)
        ctx_x = 2;
    if (ctx_y < 2)
        ctx_y = 2;
    ctx_rect = rect(ctx_x, ctx_y, w, h);
    ctx_kind = 1;
    dirty = 1;
    uart("ctx: ");
    logline(active >= 0 ? app[active].cmd : "desktop");
}
static void pointer(int dx, int dy, int b) {
    mx = bound(mx + dx, 0, sw - 1);
    my = bound(my + dy, 0, sh - 1);
    int old = button;
    button = b;
    if ((b & 2) && !(old & 2)) {
        if (ctx_kind)
            close_ctx();
        else
            open_ctx(mx, my);
        return;
    }
    if (ctx_kind && (b & 1) && !(old & 1) && !hit(ctx_rect, mx, my)) {
        close_ctx();
        return;
    }
    int reveal = mx < 8 || (railshow && mx <= scale(34));
    if (reveal != railshow) {
        railshow = reveal;
        if (home.autohide)
            dirty = 1;
    }
    if (!(b & 1)) {
        if (adjusting == SOUND)
            tone(home.sound);
        dragging = 0;
        adjusting = 0;
        px = -1;
    }
    if ((b & 1) && dragging && active >= 0) {
        wx[active] = mx - gx;
        wy[active] = my - gy;
        dirty = 1;
        return;
    }
    if ((b & 1) && adjusting)
        slider_set(adjusting, mx);
    if ((b & 1) && !(old & 1)) {
        if (welcome && !count)
            return;
        for (int i = count - 1; i >= 0; i--)
            if (hit(targets[i].r, mx, my)) {
                int a = targets[i].action, id = targets[i].arg;
                int was_ctx = ctx_kind && hit(ctx_rect, mx, my);
                action(a, id);
                if (was_ctx)
                    close_ctx();
                if (a == CHOOSE && last_id == id && ticks - last_click < 24) {
                    if (active == FILES)
                        open_smart(id);
                    else
                        open_note();
                }
                last_id = a == CHOOSE ? id : -1;
                last_click = ticks;
                return;
            }
        if (welcome)
            return;
        if (!start && active >= 0 && !mode[active] &&
            hit(rect(frame.x, frame.y, frame.w - 215, 45), mx, my)) {
            dragging = 1;
            gx = mx - frame.x;
            gy = my - frame.y;
            return;
        }
        if (!start && active == WRITER && hit(editor_page, mx, my)) {
            caret = pick_char(scroll + (my - editor_page.y) / 24, mx - editor_page.x);
            dirty = 1;
            return;
        }
        if (start) {
            start = 0;
            dirty = 1;
            return;
        }
    }
    if ((b & 1) && active == SKETCH && !start && hit(paint_area, mx, my)) {
        int x = (mx - paint_area.x) * 320 / paint_area.w,
            y = (my - paint_area.y) * 180 / paint_area.h;
        if (px < 0) {
            px = x;
            py = y;
        }
        int steps = high(high(x - px, px - x), high(y - py, py - y));
        for (int i = 0; i <= steps; i++)
            dab(px + (x - px) * i / high(1, steps), py + (y - py) * i / high(1, steps));
        px = x;
        py = y;
        dirty = 1;
    }
    if (!dirty)
        cursor(mx, my);
}
static void edit_insert(char c) {
    int n = length(draft);
    if (n + 1 >= NOTE_CAP)
        return;
    for (int i = n; i >= caret; i--)
        draft[i + 1] = draft[i];
    draft[caret++] = c;
    changed = autoroll = 1;
}
static void erase(int forward) {
    int n = length(draft);
    if (!(forward ? caret < n : caret > 0))
        return;
    if (!forward)
        caret--;
    for (int i = caret; i < n; i++)
        draft[i] = draft[i + 1];
    changed = autoroll = 1;
}
static char keychar(int k) {
    static const char plain[] =
        "\0\0"
        "1234567890-=\0\0qwertyuiop[]\0\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
    static const char upper[] =
        "\0\0!@#$%^&*()_+\0\0QWERTYUIOP{}\0\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";
    if ((unsigned)k >= sizeof plain)
        return 0;
    char c = (shift ? upper : plain)[k];
    if (caps && c >= 'a' && c <= 'z')
        c -= 32;
    else if (caps && c >= 'A' && c <= 'Z')
        c += 32;
    return c;
}
static void keyboard(int raw) {
    int down = !(raw & 128), k = raw & ~128;
    if (k == 42 || k == 54) {
        shift = down;
        return;
    }
    if (k == 29 || k == 285) {
        ctrl = down;
        return;
    }
    if (!down)
        return;
    dirty = 1;
    if (welcome) {
        welcome = 0;
        if (k == 28 || k == 1)
            return;
    }
    if (k == 58) {
        caps = !caps;
        return;
    }
    if (k == 59 || k == 347) {
        start = !start;
        choice = 0;
        return;
    }
    if (k == 61) {
        launch(TERM);
        return;
    }
    if (k == 87) {
        viewmode(2);
        return;
    }
    if (k == 1) {
        if (ctx_kind) {
            close_ctx();
            return;
        }
        if (rename_mode) {
            rename_mode = 0;
            status("Rename cancelled");
            return;
        }
        if (files_search_mode) {
            files_search_mode = 0;
            files_filter[0] = 0;
            status("Filter cleared");
            return;
        }
        if (find_mode) {
            find_mode = 0;
            status("Find cancelled");
            return;
        }
        if (pop) {
            pop = 0;
            return;
        }
        confirm_install = 0;
        if (start)
            start = 0;
        else {
            active = -1;
            logline("desktop");
        }
        return;
    }
    if (start) {
        if (k == 28)
            launch(choice);
        else if (k == 15 || k == 333 || k == 336)
            choice = (choice + 1) % APPS;
        else if (k == 331 || k == 328)
            choice = (choice + APPS - 1) % APPS;
        return;
    }
    if (active < 0) {
        if (k == 28)
            start = 1;
        return;
    }
    char c = keychar(k);
    if (rename_mode) {
        if (k == 28) {
            rename_mode = 0;
            if (goodname(rename_buf) && (home_find(rename_buf) < 0 || home_find(rename_buf) == rename_id)) {
                strset(home.notes[rename_id].name, 28, rename_buf);
                home_commit();
                status("File renamed");
            } else
                status("Invalid or taken name (lowercase, 27 chars max)");
        } else if (k == 14 && rename_buf[0])
            rename_buf[length(rename_buf) - 1] = 0;
        else if (c >= 32)
            add(rename_buf, sizeof rename_buf, c);
        return;
    }
    if (files_search_mode) {
        if (k == 28) {
            files_search_mode = 0;
            status("Filter applied / Esc clears");
        } else if (k == 14 && files_filter[0])
            files_filter[length(files_filter) - 1] = 0;
        else if (c >= 32)
            add(files_filter, sizeof files_filter, c);
        return;
    }
    if (find_mode) {
        if (k == 28)
            find_next();
        else if (k == 14 && find_buf[0])
            find_buf[length(find_buf) - 1] = 0;
        else if (c >= 32)
            add(find_buf, sizeof find_buf, c);
        return;
    }
    if (active == WRITER) {
        if (ctrl && k == 31)
            save();
        else if (ctrl && k == 30) {
            sel_anchor = 0;
            caret = length(draft);
            status("Selected all / Ctrl+C copies");
        } else if (ctrl && k == 46)
            writer_copy_sel();
        else if (ctrl && k == 45) {
            if (sel_anchor < 0)
                status("Select text first (Ctrl+A selects all)");
            else {
                writer_copy_sel();
                writer_delete_sel();
            }
        } else if (ctrl && k == 47)
            writer_paste();
        else if (ctrl && k == 44)
            undo_pop();
        else if (ctrl && k == 33)
            action(WRITER_FIND, 0);
        else if (ctrl && k == 19) {
            note_load(editing);
            status("Draft reverted to session copy");
        } else if (k == 331) {
            if (sel_anchor >= 0) {
                int lo, hi;
                sel_range(&lo, &hi);
                caret = lo;
                sel_anchor = -1;
            } else
                caret = high(0, caret - 1);
            autoroll = 1;
        } else if (k == 333) {
            if (sel_anchor >= 0) {
                int lo, hi;
                sel_range(&lo, &hi);
                caret = hi;
                sel_anchor = -1;
            } else
                caret = low(length(draft), caret + 1);
            autoroll = 1;
        } else if (k == 328 || k == 336) {
            sel_anchor = -1;
            caret = pick_char(positions[caret].row + (k == 328 ? -1 : 1), positions[caret].x);
            autoroll = 1;
        } else if (k == 327 || k == 335) {
            sel_anchor = -1;
            caret = k == 327 ? 0 : length(draft);
            autoroll = 1;
        } else if (k == 329 || k == 337) {
            scroll = high(0, scroll + (k == 329 ? -1 : 1) * writer_rows);
        } else if (k == 14 || k == 339) {
            if (sel_anchor >= 0)
                writer_delete_sel();
            else {
                undo_push();
                erase(k == 339);
            }
        } else if (k == 28 || c >= 32) {
            if (sel_anchor >= 0)
                writer_delete_sel();
            undo_push();
            edit_insert(k == 28 ? '\n' : c);
        }
    } else if (active == DEVICE && k == 23 && confirm_install) {
        action(INSTALL, 0);
    } else if (active == FILES) {
        files_rebuild();
        int pos = 0;
        for (int i = 0; i < file_list_n; i++)
            if (file_list[i] == sel)
                pos = i;
        if (k == 328 || k == 331)
            sel = file_list_n ? file_list[high(0, pos - 1)] : sel;
        if (k == 336 || k == 333)
            sel = file_list_n ? file_list[low(file_list_n - 1, pos + 1)] : sel;
        if (k == 28)
            open_smart(sel);
        if (k == 339)
            files_delete(sel);
        if (ctrl && k == 49)
            new_auto();
    } else if (active == TERM) {
        if (k == 28)
            execute();
        else if (k == 15)
            term_complete();
        else if (k == 14 && command[0])
            command[length(command) - 1] = 0;
        else if (k == 328) {
            if (hist_pos > 0) {
                hist_pos--;
                strset(command, sizeof command, history[hist_pos]);
            }
        } else if (k == 336) {
            if (hist_pos < hist_n - 1) {
                hist_pos++;
                strset(command, sizeof command, history[hist_pos]);
            } else
                command[0] = 0;
        } else if (k == 329 || k == 337)
            termoff = bound(termoff + (k == 329 ? 6 : -6), 0, high(0, nlines - 1));
        else if (c >= 32)
            add(command, sizeof command, c);
    } else if (active == CALC) {
        if (ctrl && k == 46)
            action(CALC_COPY, 0);
        else if (k == 28 || c == '=')
            compute();
        else if (k == 14 && expr[0])
            expr[length(expr) - 1] = 0;
        else if (c == 'c' || c == 'C')
            action(CLEAR, 0);
        else if (c >= 32)
            add(expr, sizeof expr, c);
    } else if (active == SNAKE) {
        if (k == 328)
            action(SNAKE_DIR, 0);
        else if (k == 336)
            action(SNAKE_DIR, 1);
        else if (k == 331)
            action(SNAKE_DIR, 2);
        else if (k == 333)
            action(SNAKE_DIR, 3);
        else if (k == 57)
            action(SNAKE_PAUSE, 0);
        else if (k == 28 && (snake_dead || !snake_running))
            snake_reset();
    } else if (active == MEDIA) {
        if (k == 57)
            action(MEDIA_PLAY, 0);
        else if (k == 333)
            action(MEDIA_NEXT, 0);
        else if (k == 331)
            action(MEDIA_PREV, -1);
    } else if (active == PHOTOS) {
        if (k == 333)
            action(PHOTOS_NEXT, 0);
        else if (k == 331)
            action(PHOTOS_PREV, -1);
        else if (c == 'z' || c == 'Z')
            action(PHOTOS_ZOOM, 0);
    }
}
static u8 rtc(int reg) {
    out8(0x70, reg);
    return in8(0x71);
}
static int bcd(int n) { return (n >> 4) * 10 + (n & 15); }
static void update_clock(void) {
    if ((int)(ticks / 60) == clocklast)
        return;
    clocklast = ticks / 60;
    int b = rtc(11), mm = rtc(2), hh = rtc(4), pm = hh & 128;
    int d = rtc(7), m = rtc(8), y = rtc(9);
    if (!(b & 4)) {
        d = bcd(d);
        m = bcd(m);
        y = bcd(y);
    }
    year = 2000 + bound(y, 0, 99);
    month = bound(m, 1, 12);
    today = bound(d, 1, 31);
    hh &= 127;
    if (!(b & 4)) {
        mm = bcd(mm);
        hh = bcd(hh);
    }
    if (!(b & 2))
        hh = hh % 12 + (pm ? 12 : 0);
    clock_h = hh;
    clock_m = mm;
    int ss = rtc(0);
    if (!(b & 4))
        ss = bcd(ss);
    compat_clock(year, month, (first_weekday(year, month) + today - 1) % 7, today, hh, mm,
                 bound(ss, 0, 59));
    char t[] = {'0' + hh / 10, '0' + hh % 10, ':', '0' + mm / 10, '0' + mm % 10, 0};
    if (!equal(t, timetext)) {
        strset(timetext, sizeof timetext, t);
        if (active < 0 || active == CLOCK || docked() || start)
            dirty = 1;
    }
    if (active == CLOCK && alarm)
        dirty = 1;
}
void desktop(struct Display *d) {
    uart_init();
    logline("Stratus kernel / " EDITION);
    if (!canvas(d)) {
        logline("Unsupported framebuffer");
        for (;;)
            __asm__ volatile("cli; hlt");
    }
    irq_init();
    ps2_init();
    pci_init();
    home_init();
    packages_init();
    compat_display(sw, sh);
    mx = sw / 2;
    my = sh / 2;
    print("FreshOS native console. Type help to see commands.");
    update_clock();
    repaint();
    logline("workspace ready");
    for (;;) {
        input_poll();
        int k;
        while ((k = key_event()) >= 0)
            keyboard(k);
        int x, y, b;
        if (pointer_event(&x, &y, &b) && !vm_absolute())
            pointer(x, y, b);
        if (vm_pointer(&x, &y, &b))
            pointer(x * (sw - 1) / 65535 - mx, y * (sh - 1) / 65535 - my, b);
        update_clock();
        media_tick();
        snake_tick();
        if (active == CLOCK && clock_mode && sw_running)
            dirty = 1;
        if (alarm && (int)(ticks - alarm) >= 0) {
            alarm = 0;
            status("Focus timer finished");
            tone(home.sound);
        }
        if (message[0] && (int)(ticks - expiry) >= 0) {
            message[0] = 0;
            dirty = 1;
        }
        int before = install_progress(), work = install_step();
        if (work == 2)
            status("Install verified. Shut down, detach the CD and boot the disk.");
        else if (work < 0)
            status(install_status());
        else if (work && active == DEVICE && before != install_progress())
            dirty = 1;
        if (dirty)
            repaint();
        if (!work)
            idle();
    }
}
void panic_cpu(ptr vector, ptr error, ptr pc) {
    __asm__ volatile("cli");
    char n[12], addr[17], code[9];
    decimal(n, vector);
#ifdef __x86_64__
    hex32(addr, pc >> 32);
    hex32(addr + 8, pc);
#else
    hex32(addr, pc);
#endif
    hex32(code, error);
    static const char *const reason[] = {"Division by zero or quotient overflow",
                                         "Debug exception",
                                         "Non-maskable interrupt",
                                         "Breakpoint",
                                         "Arithmetic overflow",
                                         "Bounds fault",
                                         "Invalid instruction",
                                         "Coprocessor not available",
                                         "Double fault"};
    const char *cause = vector < 9     ? reason[vector]
                        : vector == 13 ? "General protection fault"
                        : vector == 14 ? "Invalid virtual-memory access"
                                       : "Processor exception";
    uart("kernel stop: vector ");
    uart(n);
    uart(" / IP ");
    uart(addr);
    uart(" / error ");
    uart(code);
    uart(" / ");
    logline(cause);
    if (sw && sh) {
        clear_clip();
        fill(rect(0, 0, sw, sh), 0x19394e);
        int x = sw / 9, y = sh / 4;
        label(x, y, "FreshOS stopped", 2, WHITE);
        label(x, y + 62, "CPU exception / vector", 1, WHITE);
        label(x + 240, y + 62, n, 1, WHITE);
        label(x, y + 100, "Instruction address", 0, WHITE);
        label(x + 180, y + 100, addr, 1, WHITE);
        label(x, y + 135, "Hardware error code", 0, WHITE);
        label(x + 180, y + 135, code, 1, WHITE);
        label(x, y + 161, cause, 1, WHITE);
        label(x, y + 194, "Restart the VM. Unsaved session changes are not on disk.", 0,
              WHITE);
        present(100);
    }
    for (;;)
        __asm__ volatile("hlt");
}
