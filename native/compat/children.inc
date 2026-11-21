/* Bounded native process creation. The caller yields until the child exits.
 * Slugs resolve only to published PE packages; never fall back to a host path. */
#ifdef __x86_64__
static u32 pid_counter = 1, child_pid;
static u32 current_pid(void) { return process_bank ? child_pid : 1; }
static int wide_ascii(VM *v, u64 address, char *out, unsigned cap) {
    for (unsigned i = 0; i < cap; i++) {
        unsigned c = readv(v, address + 2 * i, 2, R);
        if (v->err || (c && (c < 32 || c > 126)))
            return 0;
        out[i] = c;
        if (!c)
            return 1;
    }
    return 0;
}
static u64 create_child(VM *v, const u64 *a) {
    unsigned p = ptrsz(v), sisize = p == 8 ? 104 : 68;
    v->last_error = 87;
    if (!v->native || process_bank || v->handles || v->launches >= 4) {
        v->last_error = 8;
        return 0;
    }
    if (a[2] || a[3] || a[4] || a[5] || a[6] || a[7] || !a[8] || !a[9])
        return 0;
    u8 *si = mem(v, a[8], sisize, R), *pi = mem(v, a[9], 2 * p + 8, WRT);
    if (!si || !pi || get(si, 4) != sisize)
        return 0;
    /* No custom desktop, inherited handles, flags, or window parameters. */
    for (unsigned i = p; i < sisize; i++)
        if (si[i])
            return 0;
    char app[128] = {0}, cmd[160] = {0};
    if (a[0] && !wide_ascii(v, a[0], app, sizeof app))
        return 0;
    if (a[1] && !wide_ascii(v, a[1], cmd, sizeof cmd))
        return 0;
    char *args = cmd;
    while (*args && *args != ' ')
        args++;
    if (*args)
        *args++ = 0;
    while (*args == ' ')
        args++;
    const char *name = a[0] ? app : cmd;
    int disk = (*name == 'F' || *name == 'f') && name[1] == ':';
    if ((!disk && !text_name(name)) || (a[0] && *cmd && !equal(name, cmd)))
        return 0;
    int slot = 0;
    while (!disk && slot < PKG_SLOTS && !equal(name, package_name(slot)))
        slot++;
    if (!disk && (slot == PKG_SLOTS || package_pending(slot))) {
        v->last_error = 2;
        return 0;
    }
    PeResult child;
    void (*box)(const char *) = CURRENT.show_box;
    v->launches++;
    child_pid = ++pid_counter;
    if (child_pid < 2)
        child_pid = pid_counter = 2;
    task64_push_child();
    compat_args(name, args);
    int ok = disk ? raw_run(name + 2, &child, box) : package_run(slot, &child, box);
    task64_pop_child();
    if (!child.entered) {
        uart("child load: ");
        logline(child.detail);
        v->last_error = 193;
        return 0;
    }
    v->child_exit = ok ? child.exit_code : 0xc0000001u;
    v->process_handle = 16 + 2 * v->launches;
    v->thread_handle = v->process_handle + 1;
    v->handles = 3;
    put(pi, p, v->process_handle);
    put(pi + p, p, v->thread_handle);
    put(pi + 2 * p, 4, child_pid);
    put(pi + 2 * p + 4, 4, child_pid);
    uart("child: ");
    uart(name);
    logline(ok ? " exited" : " aborted");
    for (const char *s = child.output; *s; s++)
        emit(v, *s);
    return 1;
}
#else
static u32 current_pid(void) { return 1; }
static u64 create_child(VM *v, const u64 *a) {
    (void)a;
    v->last_error = 50;
    return 0;
}
#endif
