/* File associations for the graphical workspace and command desktop. A live
 * session keeps its last linked IMP in memory even without a writable disk. */
static char linked_name[32], document[32] = "note.txt", feedback[124];
static u32 linked_size;
static char recent[12][32];
static int nrecent, filetab, fileidx, editpos, savebox, ctrl;
static char savename[32], pending_doc[32];
static int modified;
static u32 last_click;
static int last_file = -1;
static const char *const library[] = {"note.txt",    "canvas",       "hello32.exe",
                                      "hello64.exe", "hello32.imp",  "hello64.imp",
                                      "linux.elf",   "postcard.exe", "chime.wav"};
static int suffix(const char *s, const char *ext) {
    int a = len(s), b = len(ext);
    return a >= b && eq(s + a - b, ext);
}
static u32 feedback_until;
static void notify(const char *s) {
    copy(feedback, s, sizeof feedback);
    say(s);
    feedback_until = ticks + 210;
    dirty = 1;
}
static void add_recent(const char *s) {
    if (eq(s, "profile") || nrecent >= 12)
        return;
    for (int i = 0; i < nrecent; i++)
        if (eq(recent[i], s))
            return;
    copy(recent[nrecent++], s, 32);
}
static void file_refresh(void) {
    nrecent = 0;
    if (linked_size)
        add_recent(linked_name);
    home_list(add_recent);
    last_file = -1;
}
static int filecount(void) {
    return filetab ? nrecent : (int)(sizeof library / sizeof *library);
}
static const char *selection(void) {
    return fileidx >= 0 && fileidx < filecount()
               ? (filetab ? recent[fileidx] : library[fileidx])
               : "";
}
static int get_program(const char *name, const u8 **data) {
    if (linked_size && eq(name, linked_name)) {
        *data = converted;
        return linked_size;
    }
    int n = home_read(name, imported, sizeof imported);
    *data = imported;
    if (n != -2)
        return n;
    if (eq(name, "chime.wav")) {
        *data = chime;
        return CHIME_SIZE;
    }
    if (eq(name, "postcard.exe")) {
        *data = postcard;
        return POSTCARD_SIZE;
    }
    if (eq(name, "linux.elf")) {
        *data = linuxdemo;
        return LINUXDEMO_SIZE;
    }
    if (eq(name, "hello32.exe")) {
        *data = hello32;
        return HELLO32_SIZE;
    }
    if (eq(name, "hello64.exe")) {
        *data = hello64;
        return HELLO64_SIZE;
    }
    if (eq(name, "hello32.imp")) {
        *data = imp32;
        return IMP32_SIZE;
    }
    if (eq(name, "hello64.imp")) {
        *data = imp64;
        return IMP64_SIZE;
    }
    return -2;
}
static void open_file(const char *name) {
    if (!*name)
        return;
    if (eq(name, "canvas")) {
        launch(PAINT);
        return;
    }
    if (suffix(name, ".wav")) {
        const u8 *p;
        int n = get_program(name, &p);
        notify(n > 0 ? pcm_wav(p, n, vol) : "Sound clip not found.");
        return;
    }
    if (suffix(name, ".elf") || suffix(name, ".imp") || suffix(name, ".exe")) {
        const u8 *p;
        int n = get_program(name, &p);
        if (n <= 0) {
            notify("Could not read program. Check its name and size.");
            return;
        }
        if (suffix(name, ".elf")) {
            last_imp = 2;
            pe_ran = 1;
            pe_ok = elf_run(p, n, &pe_result);
            serial(pe_ok ? "Linux ELF completed\r\n" : "ELF rejected\r\n");
        } else
            runprogram(p, n, suffix(name, ".imp"));
        launch(WINDOWS);
        return;
    }
    if (!eq(document, name)) {
        if (modified) {
            copy(pending_doc, name, 32);
            return;
        }
        char tmp[sizeof note];
        int n = home_read(name, tmp, sizeof tmp - 1);
        if (n == -2 && eq(name, "note.txt"))
            n = 0;
        if (n < 0) {
            notify("Document missing, unreadable or too large for Writer.");
            return;
        }
        for (int i = 0; i < n; i++)
            if ((u8)tmp[i] < 32 && tmp[i] != '\n' && tmp[i] != '\t') {
                notify("Writer opens plain text only.");
                return;
            }
        tmp[n] = 0;
        copy(note, tmp, sizeof note);
        copy(document, name, 32);
        editpos = n;
    }
    launch(WRITER);
}
static void convert_file(const char *name) {
    if (!suffix(name, ".exe") || len(name) > 31) {
        notify("Select an .exe file to link an IMP image.");
        return;
    }
    const u8 *p;
    int n = get_program(name, &p);
    if (n <= 0) {
        notify("EXE missing, unreadable or larger than 2 MiB.");
        return;
    }
    PeResult r;
    u32 size = imp_convert(p, n, imported, sizeof imported, &r);
    if (!size) {
        notify(r.detail);
        return;
    }
    for (u32 i = 0; i < size; i++)
        converted[i] = imported[i];
    linked_size = size;
    copy(linked_name, name, 32);
    copy(linked_name + len(name) - 4, ".imp", 5);
    int saved = home_ready() && home_save(linked_name, converted, size);
    file_refresh();
    filetab = 1;
    fileidx = 0;
    notify(saved          ? "Linked to Fresh Home. Open the IMP to run it."
           : home_ready() ? "Linked in memory. Disk save failed."
                          : "Linked in memory. Open the IMP to run it.");
    serial("IMP linked: ");
    serial(linked_name);
    serial("\r\n");
}
static void write_document(const char *name) {
    if (!*name || eq(name, "canvas") || eq(name, "profile") || suffix(name, ".exe") ||
        suffix(name, ".imp")) {
        notify("Choose a document name, for example journal.txt.");
        return;
    }
    if (!home_ready()) {
        notify("Live session: install onto Fresh Home to save documents.");
        return;
    }
    if (!home_save(name, note, len(note))) {
        notify("Save failed. Use a-z, digits, dot, dash or underscore.");
        return;
    }
    copy(document, name, 32);
    modified = 0;
    notify("Document saved to Fresh Home.");
    savebox = 0;
}

static void discard_choice(int yes) {
    char target[32];
    copy(target, pending_doc, 32);
    pending_doc[0] = 0;
    if (yes) {
        modified = 0;
        open_file(target);
    }
}
