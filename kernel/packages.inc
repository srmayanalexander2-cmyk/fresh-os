/* A Home catalog contains the installed IMP bytes, not paths back to their sources.
   Replacing the single catalog record makes membership and payload updates atomic. */
static int pkg_name(const char *s) {
    int n = len(s);
    if (n < 1 || n > 23)
        return 0;
    for (int i = 0; i < n; i++)
        if (!(s[i] >= 'a' && s[i] <= 'z') && !(s[i] >= '0' && s[i] <= '9') && s[i] != '-')
            return 0;
    return 1;
}
static u32 pkg_len(const u8 *p) {
    return p[24] | p[25] << 8 | p[26] << 16 | ((u32)p[27] << 24);
}
#define PKG_COUNT 32
static char pkg_names[PKG_COUNT][24];
static int pkgcount, pkgsel, confirm_remove;
/* Validate the entire record before replacement or execution, even when the requested name
 * matches early. */
static int catalog_read(void) {
    pkgcount = 0;
    int size = home_read("packages.db", packed, sizeof packed);
    if (size == -2)
        return 0;
    if (size < 0)
        return -1;
    for (int p = 0; p < size;) {
        if (size - p < 28 || packed[p + 23] || !pkg_name((char *)packed + p) ||
            pkg_len(packed + p) > (u32)(size - p - 28) || pkgcount == PKG_COUNT) {
            pkgcount = 0;
            return -1;
        }
        copy(pkg_names[pkgcount++], (char *)packed + p, 24);
        p += 28 + pkg_len(packed + p);
    }
    return size;
}
static void shelf_refresh(void) {
    confirm_remove = 0;
    if (catalog_read() < 0)
        notify("Invalid package catalog; no changes made.");
    pkgsel = clamp(pkgsel, 0, pkgcount ? pkgcount - 1 : 0);
}
static void package_cmd(char *s) {
    if (!home_ready()) {
        notify("Packages need a writable Home disk. Use disks for details.");
        return;
    }
    int list = eq(s, "list"), add = prefix(s, "install "), del = prefix(s, "remove "),
        run = prefix(s, "run ");
    if (!list && !add && !del && !run) {
        say("pkg list | pkg install NAME FILE.imp | pkg run NAME | pkg remove NAME");
        return;
    }
    char *name = s + (list ? 0 : add ? 8 : del ? 7 : 4), *file = name;
    while (*file && *file != ' ')
        file++;
    if (*file)
        *file++ = 0;
    if (!list && (!pkg_name(name) || (add ? !suffix(file, ".imp") : *file != 0))) {
        notify("Use a short lowercase package name and one .imp source for install.");
        return;
    }
    int size = catalog_read();
    if (size < 0) {
        notify("Package catalog unreadable. No changes made.");
        return;
    }
    int found = -1;
    for (int p = 0; p < size;) {
        if (list)
            say((char *)packed + p);
        if (eq(name, (char *)packed + p))
            found = p;
        p += 28 + pkg_len(packed + p);
    }
    if (list) {
        if (!size)
            say("No installed packages.");
        return;
    }
    if (add) {
        if (found >= 0) {
            notify("Package name in use. Choose a new name or remove it first.");
            return;
        }
        if (pkgcount == PKG_COUNT) {
            notify("Package list full (32 entries).");
            return;
        }
        const u8 *data;
        int n = get_program(file, &data);
        if (n < 1) {
            notify("IMP file missing or unreadable.");
            return;
        }
        if ((u32)n + 28 > sizeof packed - size) {
            notify("Package catalog full (256 KiB). Nothing installed.");
            return;
        }
        const char *err = imp_check(data, n);
        if (err) {
            notify(err);
            return;
        }
        u8 *p = packed + size;
        for (int i = 0; i < 24; i++)
            p[i] = 0;
        copy((char *)p, name, 24);
        for (int i = 0; i < 4; i++)
            p[24 + i] = (u32)n >> (i * 8);
        for (int i = 0; i < n; i++)
            p[28 + i] = data[i];
        size += 28 + n;
    } else {
        if (found < 0) {
            notify("Package not installed.");
            return;
        }
        int n = pkg_len(packed + found);
        if (run) {
            runprogram(packed + found + 28, n, 1);
            launch(WINDOWS);
            return;
        }
        int end = found + 28 + n;
        for (int i = end; i < size; i++)
            packed[found + i - end] = packed[i];
        size -= end - found;
    }
    notify(home_save("packages.db", packed, size)
               ? (add ? "Package installed." : "Package removed.")
               : "Catalog save failed. Previous catalog retained.");
}

static void install_selection(void) {
    const char *f = selection();
    int n = len(f);
    if (n < 5 || n - 4 > 23) {
        notify("For a long filename use pkg install NAME FILE.imp.");
        return;
    }
    char cmd[80] = "install ";
    copy(cmd + 8, f, n - 4 + 1);
    int at = len(cmd);
    cmd[at++] = ' ';
    copy(cmd + at, f, sizeof cmd - at);
    package_cmd(cmd);
}
