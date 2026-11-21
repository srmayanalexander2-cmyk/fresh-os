/* Both Linux ABIs use private mapped segments. PT_INTERP, dynamic and TLS
 * images need a real loader and are not treated as static programs. */
static int elf_prepare(const u8 *f, u32 n, VM *v) {
    PeResult *r = v->res;
    if (n < 52 || n > FILE_LIMIT || get(f, 4) != 0x464c457fu || f[5] != 1 || f[6] != 1 ||
        get(f + 20, 4) != 1)
        goto invalid;
    int wide = f[4] == 2;
    if ((!wide && f[4] != 1) || get(f + 18, 2) != (wide ? 62 : 3)) {
        detail(r, "Requires x86 or x86-64 ELF");
        return 0;
    }
    unsigned eh = wide ? 64 : 52, phsize = wide ? 56 : 32, p = wide ? 8 : 4;
    if (n < eh || get(f + (wide ? 52 : 40), 2) != eh ||
        get(f + (wide ? 54 : 42), 2) != phsize)
        goto invalid;
    u64 ph = get(f + (wide ? 32 : 28), p), count = get(f + (wide ? 56 : 44), 2), lo = ~0ull,
        hi = 0;
    if (!count || count > 64 || ph > n || count * phsize > n - ph)
        goto invalid;
    for (int pass = 0; pass < 2; pass++) {
        for (u32 i = 0; i < count; i++) {
            const u8 *h = f + (u32)ph + i * phsize;
            u32 type = get(h, 4);
            if (type == 2 || type == 3 || type == 7) {
                detail(r, "Requires dynamic Linux loader, shared libraries or TLS");
                return 0;
            }
            if (type != 1)
                continue;
            u64 base = get(h + (wide ? 16 : 8), p), ofs = get(h + (wide ? 8 : 4), p),
                file = get(h + (wide ? 32 : 16), p), size = get(h + (wide ? 40 : 20), p);
            if (base < 4096 || base >= HEAP || size > HEAP - base || file > size || ofs > n ||
                file > n - ofs)
                goto invalid;
            if (!size)
                continue;
            if (!pass && ph >= ofs && ph - ofs <= file &&
                count * phsize <= file - (ph - ofs)) {
                v->phdr = base + ph - ofs;
                v->phnum = count;
                v->phent = phsize;
            }
            if (!pass) {
                if ((base & ~4095ull) < lo)
                    lo = base & ~4095ull;
                if (base + size > hi)
                    hi = base + size;
            } else {
                u32 off = base - lo, flags = get(h + (wide ? 4 : 24), 4);
                if (flags & ~7u)
                    goto invalid;
                for (u32 j = off / 4096; j <= (off + (u32)size - 1) / 4096; j++) {
                    if (CURRENT.perm[j])
                        goto invalid;
                    CURRENT.perm[j] =
                        8 | (flags & 1 ? X : 0) | (flags & 2 ? WRT : 0) | (flags & 4 ? R : 0);
                }
                bytes(CURRENT.image + off, f + (u32)ofs, file);
            }
        }
        if (!pass) {
            if (get(f + 16, 2) != 2 || hi <= lo || hi - lo > IMAGE_MAX)
                goto invalid;
            v->size = hi - lo;
            v->base = lo;
            v->wide = wide;
            clear(CURRENT.image, (v->size + 4095) & ~4095u);
            clear(CURRENT.perm, sizeof CURRENT.perm);
        }
    }
    v->ip = get(f + 24, p);
    if (v->ip < lo || v->ip >= hi || !(CURRENT.perm[(v->ip - lo) / 4096] & X))
        goto invalid;
    r->bits = wide ? 64 : 32;
    clear(CURRENT.stackmem, sizeof CURRENT.stackmem);
    return 1;
invalid:
    detail(r, "Invalid or unsupported static ELF image");
    return 0;
}
int elf_check(const u8 *f, u32 n, PeResult *r) {
    clear((u8 *)r, sizeof *r);
    VM v = {.res = r, .linuxabi = 1, .budget = STEPS};
    return elf_prepare(f, n, &v);
}
int elf_run(const u8 *f, u32 n, PeResult *r) {
    clear((u8 *)r, sizeof *r);
    VM v = {.res = r, .linuxabi = 1, .budget = STEPS};
    return elf_prepare(f, n, &v) && execute(&v);
}
