/* Linux x86-64 static ET_EXEC loader, no dynamic linker. Deliberately narrower
 * than ELF: aligned, non-overlapping PT_LOAD pages; reject interpreters/TLS. */
int elf_run(const u8 *f, u32 n, PeResult *r) {
    clear((u8 *)r, sizeof *r);
    VM v = {.res = r, .wide = 1, .linuxabi = 1, .budget = STEPS};
    if (n < 64 || n > FILE_LIMIT || get(f, 4) != 0x464c457fu || f[4] != 2 || f[5] != 1 ||
        f[6] != 1 || get(f + 16, 2) != 2 || get(f + 18, 2) != 62 || get(f + 20, 4) != 1 ||
        get(f + 52, 2) != 64 || get(f + 54, 2) != 56)
        goto invalid;
    u64 ph = get(f + 32, 8), count = get(f + 56, 2), lo = ~(u64)0, hi = 0;
    if (!count || count > 64 || ph > n || count * 56 > n - ph)
        goto invalid;
    for (u32 i = 0; i < count; i++) {
        const u8 *h = f + (u32)ph + 56 * i;
        u32 type = get(h, 4);
        if (type == 3 || type == 2 || type == 7) {
            detail(r, "Requires a dynamic linker or TLS");
            return 0;
        }
        if (type != 1)
            continue;
        u64 base = get(h + 16, 8), file = get(h + 32, 8), sz = get(h + 40, 8),
            ofs = get(h + 8, 8);
        if (base < 4096 || base >= HEAP || sz > HEAP - base || file > sz || ofs > n ||
            file > n - ofs)
            goto invalid;
        if (!sz)
            continue;
        if ((base & ~4095ull) < lo)
            lo = base & ~4095ull;
        if (base + sz > hi)
            hi = base + sz;
    }
    if (hi <= lo || hi - lo > IMAGE_MAX)
        goto invalid;
    v.base = lo;
    v.size = hi - lo;
    clear(image, v.size);
    clear(perm, sizeof perm);
    for (u32 i = 0; i < count; i++) {
        const u8 *h = f + (u32)ph + 56 * i;
        if (get(h, 4) != 1 || !get(h + 40, 8))
            continue;
        u32 off = get(h + 16, 8) - lo, sz = get(h + 40, 8), flags = get(h + 4, 4);
        if (flags & ~7u)
            goto invalid;
        for (u32 j = off / 4096; j <= (off + sz - 1) / 4096; j++) {
            if (perm[j])
                goto invalid;
            perm[j] = 8 | (flags & 1 ? X : 0) | (flags & 2 ? WRT : 0) | (flags & 4 ? R : 0);
        }
        bytes(image + off, f + (u32)get(h + 8, 8), get(h + 32, 8));
    }
    v.ip = get(f + 24, 8);
    if (v.ip < lo || v.ip >= hi || !(perm[(v.ip - lo) / 4096] & X))
        goto invalid;
    r->bits = 64;
    clear(stackmem, sizeof stackmem);
    return execute(&v);
invalid:
    detail(r, "Invalid or unsupported static Linux x86-64 ELF layout");
    return 0;
}
