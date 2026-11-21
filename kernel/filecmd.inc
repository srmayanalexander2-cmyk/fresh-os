/* Operate on bytes read from Home or the built-in sample library. Never replace
 * a destination; metadata and system-owned names are not valid copy targets. */
static int data_commands(char *s) {
    if (eq(s, "help files")) {
        say("ls | cp SOURCE NEWNAME | cmp A B | grep WORD FILE | xxd FILE");
        say("copy, fc /b, findstr and hexdump are alternate spellings. No overwrite, pipes "
            "or wildcards.");
        return 1;
    }
    if (eq(s, "help web")) {
        say("browser | browse http://HOST/PATH | browse home:FILE.html");
        say("pageinfo | savepage NEW.txt | js EXPRESSION | jsfile FILE.js");
        return 1;
    }
    const char *cmd[] = {"cp ",   "copy ",    "cmp ", "fc /b ",
                         "grep ", "findstr ", "xxd ", "hexdump "};
    int op = 0;
    while (op < 8 && !prefix(s, cmd[op]))
        op++;
    if (op == 8)
        return 0;
    char *arg = s + len(cmd[op]), *b = arg;
    while (*b && *b != ' ')
        b++;
    if (*b) {
        *b++ = 0;
        while (*b == ' ')
            b++;
    }
    op /= 2;
    if (!*arg || (op != 3 && !*b)) {
        say("Missing argument. See help files.");
        return 1;
    }
    const u8 *p;
    int n = tool_file(op == 2 ? b : arg, &p);
    if (n < 0) {
        say("Source missing or unreadable.");
        return 1;
    }
    if (op == 0) {
        u8 dummy;
        if (!home_ready() || home_read(b, &dummy, 0) != -2 || eq(b, "profile") ||
            eq(b, "packages.db") || eq(b, "canvas") || eq(b, "aero.favorite")) {
            say("Use a new destination name on writable Home.");
            return 1;
        }
        notify(home_save(b, p, n) ? "Copy completed." : "Copy failed.");
        return 1;
    }
    if (op == 1) {
        if (n > (int)sizeof packed) {
            say("Comparison limited to 256 KiB per file.");
            return 1;
        }
        for (int i = 0; i < n; i++)
            packed[i] = p[i];
        int m = tool_file(b, &p);
        if (m < 0) {
            say("Second file missing or unreadable.");
            return 1;
        }
        int i = 0;
        while (i < n && i < m && p[i] == packed[i])
            i++;
        if (i == n && i == m)
            say("Identical.");
        else {
            char msg[48] = "First differing byte: ";
            num(msg + len(msg), i);
            say(msg);
        }
        return 1;
    }
    if (op == 2) {
        int matches = 0, k = len(arg);
        for (int i = 0; i < n;) {
            int beg = i;
            while (i < n && p[i] != '\n')
                i++;
            for (int j = beg; j + k <= i; j++) {
                int t = 0;
                while (t < k && p[j + t] == (u8)arg[t])
                    t++;
                if (t == k) {
                    char row[101];
                    int z = 0;
                    while (beg < i && z < 100) {
                        u8 c = p[beg++];
                        row[z++] = c >= 32 && c < 127 ? c : '.';
                    }
                    row[z] = 0;
                    say(row);
                    matches++;
                    break;
                }
            }
            if (matches == 64) {
                say("Stopped at 64 matching lines.");
                break;
            }
            i++;
        }
        if (!matches)
            say("No matching lines.");
        return 1;
    }
    const char *digits = "0123456789abcdef";
    for (int i = 0; i < n && i < 256; i += 16) {
        char row[80];
        int k = 0;
        for (int j = 12; j >= 0; j -= 4)
            row[k++] = digits[(i >> j) & 15];
        row[k++] = ':';
        for (int j = i; j < n && j < i + 16; j++) {
            row[k++] = ' ';
            row[k++] = digits[p[j] >> 4];
            row[k++] = digits[p[j] & 15];
        }
        row[k] = 0;
        say(row);
    }
    if (n > 256)
        say("Showing first 256 bytes.");
    return 1;
}
