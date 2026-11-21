/* Aliases map only to implemented operations. Fresh Home is a flat record store. */
static int shell_tools(char *s) {
    if (eq(s, "disks") || eq(s, "lsblk") || eq(s, "disklist")) {
        storage_list(say);
        return 1;
    }
    if (eq(s, "doctor")) {
        say(EDITION " edition");
        say(display_driver());
        say(vm_guest());
        storage_list(say);
        say(audio_name());
        say("VMware: extract the whole ZIP and open FreshOS.vmx; the ISO alone is "
            "read-only.");
        return 1;
    }
    if (eq(s, "history") || eq(s, "doskey /history")) {
        for (int i = 0; i < ncmd; i++)
            say(commands[i]);
        return 1;
    }
    if (eq(s, "uptime")) {
        char b[20];
        num(b, ticks / 60);
        say(b);
        say("seconds since boot");
        return 1;
    }
    if (eq(s, "pkg") || prefix(s, "pkg ")) {
        package_cmd(s + (s[3] ? 4 : 3));
        return 1;
    }
    if (prefix(s, "wc ") || prefix(s, "stat ") || prefix(s, "head ") || prefix(s, "tail ")) {
        int word = prefix(s, "wc "), stat = prefix(s, "stat "), tail = prefix(s, "tail ");
        const u8 *p;
        int n = tool_file(s + (word ? 3 : 5), &p);
        if (n < 0) {
            notify("File missing, unreadable or larger than the 2 MiB read limit.");
            return 1;
        }
        if (word || stat) {
            char b[16];
            num(b, n);
            say("Bytes:");
            say(b);
            if (word) {
                int words = 0, lines = 0, in = 0;
                for (int i = 0; i < n; i++) {
                    int sep = p[i] <= 32;
                    words += !sep && !in;
                    in = !sep;
                    lines += p[i] == '\n';
                }
                say("Words:");
                num(b, words);
                say(b);
                say("Newlines:");
                num(b, lines);
                say(b);
            }
            return 1;
        }
        int i = 0, end = n;
        if (tail) {
            i = n;
            if (i && p[i - 1] == '\n')
                i--;
            int count = 0;
            while (i > 0) {
                if (p[i - 1] == '\n' && ++count == 10)
                    break;
                i--;
            }
        }
        char b[100];
        int col = 0, rows = 0;
        for (; i < end && (tail || rows < 10); i++) {
            u8 c = p[i];
            if (c == '\n') {
                b[col] = 0;
                say(b);
                col = 0;
                rows++;
            } else {
                b[col++] = c >= 32 && c < 127 ? c : '.';
                if (col == 99) {
                    b[col] = 0;
                    say(b);
                    col = 0;
                }
            }
        }
        if (col) {
            b[col] = 0;
            say(b);
        }
        return 1;
    }
    return 0;
}
