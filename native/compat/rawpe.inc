/* Load a raw PE from the marked FAT disk directly into the inactive/current
 * resident. No guest pointers are followed by the kernel as host pointers. */
#ifdef __x86_64__
static int raw_range(VM *v, u64 at, unsigned n) {
    if (!n || at >= v->size || n > v->size - at)
        return 0;
    for (u32 i = at >> 12; i <= (at + n - 1) >> 12; i++)
        if (!(CURRENT.perm[i] & R))
            return 0;
    return 1;
}
static int raw_name(VM *v, u64 at, char *dst, unsigned cap, int lower) {
    for (unsigned i = 0; i < cap; i++) {
        if (!raw_range(v, at + i, 1))
            return 0;
        unsigned c = CURRENT.image[at + i];
        if (c && (c < 32 || c > 126))
            return 0;
        dst[i] = lower && c >= 'A' && c <= 'Z' ? c + 32 : c;
        if (!c)
            return i != 0;
    }
    return 0;
}
static int raw_read(FIL *f, u32 off, void *buf, u32 n) {
    UINT count;
    return span(f_size(f), off, n) && f_lseek(f, off) == FR_OK &&
           f_read(f, buf, n, &count) == FR_OK && count == n;
}
static const char *raw_imports(VM *v, u32 at, u32 size) {
    if (!size && !at)
        return 0;
    if (!raw_range(v, at, size))
        return "Invalid PE import directory";
    unsigned p = ptrsz(v), total = 0;
    for (unsigned off = 0; off + 20 <= size && off < 20 * 128; off += 20) {
        u8 *desc = CURRENT.image + at + off;
        u64 orig = get(desc, 4), name = get(desc + 12, 4), dest = get(desc + 16, 4);
        if (!orig && !name && !dest && !get(desc + 4, 8))
            return 0;
        if (!orig)
            orig = dest;
        char dll[96], sym[192];
        if (!dest || !raw_name(v, name, dll, sizeof dll, 1))
            return "Invalid PE import DLL";
        for (;; orig += p, dest += p) {
            if (++total > 4096 || !raw_range(v, orig, p) || !raw_range(v, dest, p))
                return "Invalid PE thunk table";
            u64 hint = get(CURRENT.image + orig, p);
            if (!hint)
                break;
            if (hint >= v->size || !raw_name(v, hint + 2, sym, sizeof sym, 0))
                return "Unsupported ordinal or invalid PE import name";
            int i = 0;
            while (i < NIMPORT &&
                   !(equal(dll, imports[i].dll) && equal(sym, imports[i].name)))
                i++;
            if (i == NIMPORT) {
                detail(v->res, "Missing import: ");
                detail(v->res, dll);
                detail(v->res, "!");
                detail(v->res, sym);
                return "Unresolved import";
            }
            put(CURRENT.image + dest, p, ABI + 16 * i);
        }
    }
    return "Unterminated PE import directory";
}
/* One initial thread and one primary module. No dynamic DLL TLS registration. */
static const char *raw_tls(VM *v, u32 at, u32 size) {
    unsigned p = ptrsz(v), n = 4 * p + 8;
    if (!at)
        return 0;
    if (size < n || !raw_range(v, at, n))
        return "Invalid PE TLS directory";
    u8 *t = CURRENT.image + at;
    u64 lo = get(t, p), hi = get(t + p, p), index = get(t + 2 * p, p), cb = get(t + 3 * p, p);
    u32 zero = get(t + 4 * p, 4), flags = get(t + 4 * p + 4, 4);
    if ((flags & ~0xf00000u) || (flags >> 20) > 5 || hi < lo || hi - lo > 65536 ||
        zero > 65536 - (hi - lo) ||
        (lo != hi && (lo < v->base || !raw_range(v, lo - v->base, hi - lo))) ||
        index < v->base || !raw_range(v, index - v->base, 4))
        return "Invalid PE TLS template";
    if (cb) {
        for (unsigned i = 0;; i++) {
            if (cb < v->base || !raw_range(v, cb - v->base, p))
                return "Invalid PE TLS callback array";
            u64 fn = get(CURRENT.image + (cb - v->base), p);
            cb += p;
            if (!fn)
                break;
            if (i >= 16 || fn < v->base || fn - v->base >= v->size ||
                !(CURRENT.perm[(fn - v->base) >> 12] & X))
                return "Invalid PE TLS callback";
            v->callbacks[v->ncallbacks++] = fn;
        }
    }
    v->teb = allocate(v, 4096, 1);
    u64 vector = allocate(v, p, 1), block = allocate(v, hi - lo + zero, 1);
    if (v->teb != HEAP || !block || !vector)
        return "Cannot allocate PE TLS";
    writev(v, index, 4, 0);
    writev(v, v->teb + p, p, STACK + STACK_SIZE);
    writev(v, v->teb + 2 * p, p, STACK);
    writev(v, v->teb + 6 * p, p, v->teb);
    writev(v, v->teb + (v->wide ? 0x58 : 0x2c), p, vector);
    writev(v, v->teb + (v->wide ? 0x40 : 0x20), p, current_pid());
    writev(v, v->teb + (v->wide ? 0x48 : 0x24), p, current_pid());
    writev(v, vector, p, block);
    if (hi != lo)
        bytes(CURRENT.heap + (block - HEAP), CURRENT.image + (lo - v->base), hi - lo);
    return v->err;
}
/* Support the PE32 GuardCF layout used by the archived setup. Unknown loader
 * obligations, including extended CFG flags, are never silently discarded. */
static int raw_va_perm(VM *v, u32 va, u32 n, unsigned perm) {
    if (va < v->base || !raw_range(v, va - v->base, n))
        return 0;
    u32 rva = va - v->base;
    for (unsigned i = rva / 4096; i <= (rva + n - 1) / 4096; i++)
        if (CURRENT.perm[i] != perm)
            return 0;
    return 1;
}
static const char *guard_config(VM *v, u32 at, u32 size) {
    if (!at)
        return 0;
    if (v->wide || size < 92 || size > 184 || !raw_va_perm(v, v->base + at, size, R))
        return "Unsupported PE load-config layout";
    const u8 *h = CURRENT.image + at;
    if (get(h, 4) != size || get(h + 88, 4) != 0x500)
        return "Unsupported PE load-config guard flags";
    for (unsigned i = 4; i < size; i++) {
        if (i >= 60 && i < 92)
            continue;
        if (h[i])
            return "Unsupported PE load-config field";
    }
    u32 cookie = get(h + 60, 4), slot = get(h + 72, 4), tab = get(h + 80, 4),
        count = get(h + 84, 4);
    if (get(h + 76, 4) || !count || count > 4096 || !raw_va_perm(v, tab, count * 4, R) ||
        !raw_va_perm(v, slot, 4, R) || (cookie && !raw_va_perm(v, cookie, 4, R | WRT)))
        return "Invalid PE GuardCF metadata";
    for (unsigned i = 0; i < count; i++) {
        u32 rva = get(CURRENT.image + (tab - v->base) + 4 * i, 4);
        if (rva >= v->size || CURRENT.perm[rva / 4096] != (R | X) ||
            (i && rva <= CURRENT.guard_targets[i - 1]))
            return "Invalid PE GuardCF target table";
        CURRENT.guard_targets[i] = rva;
    }
    /* No SEH dispatcher: faults terminate the task. Validate the SafeSEH
     * table nevertheless; it does not grant permission to run handlers. */
    u32 seh = get(h + 64, 4), ns = get(h + 68, 4), prev = 0;
    if ((!seh) != (!ns) || ns > 4096 || (ns && !raw_va_perm(v, seh, 4 * ns, R)))
        return "Invalid PE SafeSEH metadata";
    for (unsigned i = 0; i < ns; i++) {
        u32 rva = get(CURRENT.image + (seh - v->base) + 4 * i, 4);
        if (rva >= v->size || CURRENT.perm[rva / 4096] != (R | X) || (i && rva <= prev))
            return "Invalid PE SafeSEH handler table";
        prev = rva;
    }
    v->nguard = count;
    v->guard_slot = slot - v->base;
    v->cookie_slot = cookie ? cookie - v->base : 0;
    return 0;
}
static const char *install_guard(VM *v) {
    if (!v->nguard)
        return 0;
    if (v->cookie_slot) {
        unsigned a, b, c, d;
        __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1), "c"(0));
        unsigned cookie = 0;
        if (c & (1u << 30))
            for (unsigned i = 0; i < 10; i++) {
                unsigned char ok;
                __asm__ volatile("rdrand %0; setc %1" : "=r"(cookie), "=qm"(ok)::"cc");
                if (ok && (cookie >> 16) && cookie != 0xbb40e64e)
                    break;
                cookie = 0;
            }
        if (!cookie)
            return "Hardware entropy unavailable for PE security cookie";
        put(CURRENT.image + v->cookie_slot, 4, cookie);
    }
    put(CURRENT.image + v->guard_slot, 4, GUARD_CHECK);
    return 0;
}
static const char *raw_load(VM *v, FIL *f) {
    u8 h[4096];
    unsigned hn = f_size(f) > sizeof h ? sizeof h : f_size(f);
    if (hn < 64 || !raw_read(f, 0, h, hn) || get(h, 2) != 0x5a4d)
        return "Not a DOS/PE executable";
    u32 pe = get(h + 60, 4);
    if (!span(hn, pe, 24) || get(h + pe, 4) != 0x4550)
        return "Invalid PE header";
    u8 *co = h + pe + 4;
    u32 arch = get(co, 2), ns = get(co + 2, 2), opt = get(co + 16, 2),
        flags = get(co + 18, 2);
    unsigned wide = arch == 0x8664, dirs = wide ? 112 : 96;
    u32 o = pe + 24;
    if ((!wide && arch != 0x14c) || (flags & 0x2002) != 2 || !ns || ns > 96 || opt < dirs ||
        !span(hn, o, opt) || !span(hn, o + opt, ns * 40))
        return "Invalid PE COFF layout";
    u8 *a = h + o;
    if (get(a, 2) != (wide ? 0x20b : 0x10b) || (get(a + 68, 2) != 2 && get(a + 68, 2) != 3) ||
        get(a + 32, 4) != 4096)
        return "Unsupported PE architecture or alignment";
    u32 size = get(a + 56, 4), headers = get(a + 60, 4), entry = get(a + 16, 4),
        nd = get(a + dirs - 4, 4);
    u64 base = get(a + (wide ? 24 : 28), wide ? 8 : 4);
    if (!size || size > IMAGE_MAX || size % 4096)
        return "Raw PE exceeds mapped image limit";
    if (!base || base % 65536 || base > (1ull << (wide ? 47 : 32)) - size ||
        !(base + size <= HEAP || base >= HEAP + HEAP_SIZE) ||
        !(base + size <= ABI - 4096 || base >= STACK + STACK_SIZE) ||
        headers < o + opt + ns * 40 || headers > hn || headers > size || entry >= size ||
        nd > 16 || dirs + 8 * nd > opt)
        return "Invalid PE address layout";
    u32 imports_rva = 0, imports_size = 0, tls = 0, ts = 0, cfg = 0, cs = 0, delay = 0;
    for (unsigned i = 0; i < nd; i++) {
        u32 at = get(a + dirs + i * 8, 4), len = get(a + dirs + i * 8 + 4, 4);
        if (!at && !len)
            continue;
        if (!at || !len || !span(i == 4 ? f_size(f) : size, at, len))
            return "Invalid PE directory bounds";
        if (i == 9) {
            tls = at;
            ts = len;
            continue;
        }
        if (i == 10) {
            cfg = at;
            cs = len;
            continue;
        }
        if (i == 13) {
            delay = at;
            continue;
        }
        if (i != 0 && i != 1 && i != 2 && !(wide && i == 3) && i != 4 && i != 5 && i != 6 &&
            i != 12)
            return "Unsupported PE data directory";
        if (i == 1) {
            imports_rva = at;
            imports_size = len;
        }
    }
    v->wide = wide;
    v->res->bits = wide ? 64 : 32;
    v->size = size;
    v->base = base;
    v->ip = base + entry;
    clear(CURRENT.perm, sizeof CURRENT.perm);
    clear(CURRENT.image, size);
    bytes(CURRENT.image, h, headers);
    CURRENT.perm[0] = R;
    for (unsigned i = 0; i < ns; i++) {
        u8 *s = a + opt + 40 * i;
        u32 virt = get(s + 8, 4), at = get(s + 12, 4), raw = get(s + 16, 4),
            off = get(s + 20, 4), fl = get(s + 36, 4), n = raw > virt ? raw : virt;
        if (!n)
            continue;
        unsigned prot = (fl >> 30 & 1) * R + (fl >> 31 & 1) * WRT + (fl >> 29 & 1) * X;
        if (!at || at % 4096 || !span(size, at, n) || (prot && !(prot & R)) ||
            (prot & (X | WRT)) == (X | WRT))
            return "Unsupported PE section layout";
        for (unsigned j = at / 4096; j <= (at + n - 1) / 4096; j++) {
            if (CURRENT.perm[j])
                return "Overlapping PE sections";
            CURRENT.perm[j] = prot ? prot : 8; /* reserved, inaccessible section */
        }
        if (raw && !raw_read(f, off, CURRENT.image + at, raw))
            return "Cannot read PE section";
    }
    if (!(CURRENT.perm[entry / 4096] & X))
        return "Invalid PE entry point";
    clear(CURRENT.stackmem, sizeof CURRENT.stackmem);
    clear(CURRENT.heap, sizeof CURRENT.heap);
    const char *err = guard_config(v, cfg, cs);
    if (!err)
        err = raw_imports(v, imports_rva, imports_size);
    if (!err && delay)
        err = "PE delay imports require a DLL loader";
    if (!err)
        err = raw_tls(v, tls, ts);
    if (!err)
        err = install_guard(v);
    return err;
}
#endif
/* 'path' excludes the F: prefix. FatFs sees only absolute paths composed of
 * validated SFN components, never another drive specification. */
int raw_run(const char *path, PeResult *r, void (*box)(const char *)) {
    clear((u8 *)r, sizeof *r);
#ifdef __x86_64__
    VM v = {.res = r, .budget = STEPS};
    if (!task64_available()) {
        detail(r, "Native user mode unavailable");
        return 0;
    }
    char normalized[128];
    if (!normalize_path(path, normalized, sizeof normalized)) {
        detail(r, "Invalid app disk path");
        return 0;
    }
    if (!mount_files(&v)) {
        detail(r, "Attach a marked app data disk");
        return 0;
    }
    FIL f;
    if (f_open(&f, normalized, FA_READ) != FR_OK) {
        detail(r, "Cannot open app disk executable");
        return 0;
    }
    const char *error = raw_load(&v, &f);
    if (f_close(&f) != FR_OK && !error)
        error = "App disk read failed";
    if (error) {
        if (!r->detail[0])
            detail(r, error);
        return 0;
    }
    guest_services(0, box);
    int ok = execute(&v);
    guest_services(0, 0);
    return ok;
#else
    (void)path;
    (void)box;
    detail(r, "Raw PE requires x64 UEFI");
    return 0;
#endif
}
