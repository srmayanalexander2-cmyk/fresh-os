/* Deliberately small DEX 035-039 Activity interpreter, not an ART replacement.
 * No guest address is a host pointer; registers contain checked tagged handles.
 * Accept only exact whitelisted UI calls. No JIT, native libraries or class loading. */
#include "platform.h"
typedef struct {
    const u8 *b;
    u32 n, ns, ss, nt, ts, np, ps, nm, ms, nc, cs;
} Dex;
static u32 d32(const u8 *p) {
    return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}
static u32 d16(const u8 *p) { return p[0] | (u32)p[1] << 8; }
static int span(Dex *d, u32 p, u32 size) { return p <= d->n && size <= d->n - p; }
static int table(Dex *d, u32 p, u32 n, u32 stride) {
    return n <= 65536 && span(d, p, n * stride);
}
static int equals(const char *a, const char *b) {
    if (!a || !b)
        return 0;
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
static int uleb(Dex *d, u32 *pos, u32 *out) {
    u32 n = 0;
    for (int i = 0; i < 5; i++) {
        if (!span(d, *pos, 1))
            return 0;
        u32 x = d->b[(*pos)++];
        if (i == 4 && x > 15)
            return 0;
        n |= (x & 127) << (7 * i);
        if (!(x & 128)) {
            *out = n;
            return 1;
        }
    }
    return 0;
}
static const char *strid(Dex *d, u32 i) {
    if (i >= d->ns)
        return 0;
    u32 p = d32(d->b + d->ss + i * 4), len;
    if (!uleb(d, &p, &len) || len > 4096)
        return 0;
    const char *s = (const char *)d->b + p;
    for (u32 j = 0; j <= 4096 && span(d, p + j, 1); j++)
        if (!s[j])
            return s;
    return 0;
}
static const char *type(Dex *d, u32 i) {
    return i >= d->nt ? 0 : strid(d, d32(d->b + d->ts + i * 4));
}
static int match(Dex *d, u32 i, const char *owner, const char *name, const char *arg) {
    if (i >= d->nm)
        return 0;
    const u8 *m = d->b + d->ms + 8 * i;
    u32 proto = d16(m + 2);
    if (!equals(type(d, d16(m)), owner) || !equals(strid(d, d32(m + 4)), name) ||
        proto >= d->np)
        return 0;
    const u8 *p = d->b + d->ps + 12 * proto;
    if (!equals(type(d, d32(p + 4)), "V"))
        return 0;
    u32 params = d32(p + 8);
    return span(d, params, 6) && d32(d->b + params) == 1 &&
           equals(type(d, d16(d->b + params + 4)), arg);
}
#define ACT "Landroid/app/Activity;"
#define TXT "Landroid/widget/TextView;"
#define BUNDLE "Landroid/os/Bundle;"
static const char *execute(Dex *d, u32 code, const char *owner) {
    if (!span(d, code, 16))
        return 0;
    const u8 *c = d->b + code;
    u32 nr = d16(c), ins = d16(c + 2), size = d32(c + 12);
    if (nr < 2 || nr > 32 || ins != 2 || size > 4096 || !span(d, code + 16, size * 2) ||
        d16(c + 6))
        return 0;
    u32 regs[32] = {0};
    regs[nr - 2] = 1;
    regs[nr - 1] = 2;
    const char *text = 0;
    int attached = 0, constructed = 0;
    const u8 *p = c + 16;
    for (u32 pc = 0; pc < size;) {
        u32 v = d16(p + 2 * pc++), op = v & 255, a = v >> 8;
        if (op == 0x0e)
            return attached ? text : 0;
        if (pc >= size)
            return 0;
        u32 id = d16(p + 2 * pc++);
        if (op == 0x22) {
            if (a >= nr || !equals(type(d, id), TXT))
                return 0;
            regs[a] = 3;
            constructed = attached = 0;
            text = 0;
            continue;
        }
        if (op == 0x1a) {
            if (a >= nr || !strid(d, id))
                return 0;
            regs[a] = 4 + id;
            continue;
        }
        if ((op != 0x6e && op != 0x6f && op != 0x70) || pc >= size || (v >> 12) != 2)
            return 0;
        u32 args = d16(p + 2 * pc++), ra = args & 15, rb = (args >> 4) & 15;
        if (ra >= nr || rb >= nr)
            return 0;
        u32 self = regs[ra], arg = regs[rb];
        if (op == 0x6f && self == 1 && arg == 2 && match(d, id, ACT, "onCreate", BUNDLE))
            continue;
        if (op == 0x70 && self == 3 && arg == 1 &&
            match(d, id, TXT, "<init>", "Landroid/content/Context;")) {
            constructed = 1;
            continue;
        }
        if (op == 0x6e && constructed && self == 3 && arg >= 4 &&
            match(d, id, TXT, "setText", "Ljava/lang/CharSequence;")) {
            text = strid(d, arg - 4);
            continue;
        }
        if (op == 0x6e && constructed && self == 1 && arg == 3 &&
            (match(d, id, ACT, "setContentView", "Landroid/view/View;") ||
             match(d, id, owner, "setContentView", "Landroid/view/View;"))) {
            attached = 1;
            continue;
        }
        return 0;
    }
    return 0;
}
const char *android_named_activity(const u8 *bytes, u32 n, const char *requested) {
    if (n < 112 || n > 1048576 || bytes[0] != 'd' || bytes[1] != 'e' || bytes[2] != 'x' ||
        bytes[3] != 10 || bytes[4] != '0' || bytes[5] != '3' || bytes[6] < '5' ||
        bytes[6] > '9' || bytes[7] || d32(bytes + 32) != n || d32(bytes + 36) != 112 ||
        d32(bytes + 40) != 0x12345678)
        return 0;
    Dex d = {bytes,           n,
             d32(bytes + 56), d32(bytes + 60),
             d32(bytes + 64), d32(bytes + 68),
             d32(bytes + 72), d32(bytes + 76),
             d32(bytes + 88), d32(bytes + 92),
             d32(bytes + 96), d32(bytes + 100)};
    if (!table(&d, d.ss, d.ns, 4) || !table(&d, d.ts, d.nt, 4) ||
        !table(&d, d.ps, d.np, 12) || !table(&d, d.ms, d.nm, 8) ||
        !table(&d, d.cs, d.nc, 32) || d.nc > 256)
        return 0;
    for (u32 i = 0; i < d.nc; i++) {
        const u8 *c = bytes + d.cs + 32 * i;
        if (!equals(type(&d, d32(c + 8)), ACT))
            continue;
        const char *owner = type(&d, d32(c));
        if (!owner)
            return 0;
        if (requested && !equals(owner, requested))
            continue;
        u32 pos = d32(c + 24), count[4];
        for (int j = 0; j < 4; j++)
            if (!uleb(&d, &pos, count + j) || count[j] > 2048)
                return 0;
        u32 v, flags, code;
        for (u32 j = 0; j < count[0] + count[1]; j++)
            if (!uleb(&d, &pos, &v) || !uleb(&d, &pos, &flags))
                return 0;
        for (int k = 2; k < 4; k++) {
            u32 idx = 0;
            for (u32 j = 0; j < count[k]; j++) {
                if (!uleb(&d, &pos, &v) || v > d.nm || idx >= d.nm || v >= d.nm - idx ||
                    !uleb(&d, &pos, &flags) || !uleb(&d, &pos, &code))
                    return 0;
                idx += v;
                if (k == 3 && match(&d, idx, owner, "onCreate", BUNDLE))
                    return execute(&d, code, owner);
            }
        }
    }
    return 0;
}

const char *android_activity(const u8 *bytes, u32 n) {
    return android_named_activity(bytes, n, 0);
}
