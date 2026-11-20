/* Bounded scratch buffer; do not overwrite the user's in-memory linked IMP. */
static u8 packed[256 * 1024];
static int tool_file(const char *s, const u8 **p) {
    int n = get_program(s, p);
    if (n == -2 && eq(s, document)) {
        *p = (const u8 *)note;
        return len(note);
    }
    return n;
}
static int archive_cmd(char *s) {
    int tar = prefix(s, "tar "), gz = prefix(s, "gzip ");
    if (prefix(s, "sha3 ")) {
        const u8 *p;
        int n = tool_file(s + 5, &p);
        if (n < 0)
            say("File not found or too large.");
        else {
            char hex[65];
            sha3_256(p, n, hex);
            say(hex);
        }
        return 1;
    }
    if (!tar && !gz)
        return 0;
    char *dest = s + (tar ? 4 : 5), *args = dest;
    while (*args && *args != ' ')
        args++;
    if (!*args || !home_ready()) {
        say("Needs writable Home: tar OUT.tar FILE... | gzip OUT.gz FILE");
        return 1;
    }
    *args++ = 0;
    if (!suffix(dest, tar ? ".tar" : ".gz")) {
        say("Choose the matching archive extension.");
        return 1;
    }
    u32 pos = 0;
    while (*args) {
        while (*args == ' ')
            args++;
        if (!*args)
            break;
        char *src = args;
        while (*args && *args != ' ')
            args++;
        if (*args)
            *args++ = 0;
        if (eq(dest, src) || (!tar && pos)) {
            say("Invalid source list. Nothing saved.");
            return 1;
        }
        const u8 *p;
        int n = tool_file(src, &p);
        if (n < 0) {
            say("Source missing or unreadable. Nothing saved.");
            return 1;
        }
        u32 sz = tar ? tar_member(packed + pos, sizeof packed - pos - 1024, src, p, n)
                     : gzip_store(packed, sizeof packed, p, n);
        if (!sz) {
            say("Invalid name or 256 KiB archive limit. Nothing saved.");
            return 1;
        }
        pos += sz;
    }
    if (!pos) {
        say("At least one input is required.");
        return 1;
    }
    if (tar) {
        for (int i = 0; i < 1024; i++)
            packed[pos++] = 0;
    }
    notify(home_save(dest, packed, pos) ? "Saved archive to Fresh Home."
                                        : "Archive save failed.");
    return 1;
}
