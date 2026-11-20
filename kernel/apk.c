/* Read a standalone APK in the guest. Only the manifest and primary DEX are
 * materialized; nothing is extracted to disk, including named ZIP paths. */
#include "compat.h"
static u8 dex[DEX_LIMIT], manifest[128 * 1024];
static u32 u16le(const u8 *p) { return p[0] | ((u32)p[1] << 8); }
static u32 u32le(const u8 *p) { return u16le(p) | (u16le(p + 2) << 16); }
static int range(u32 cap, u32 p, u32 n) { return p <= cap && n <= cap - p; }
static int same(const u8 *p, const char *s, u32 n) {
    for (u32 i = 0; i < n; i++)
        if (!s[i] || p[i] != (u8)s[i])
            return 0;
    return s[n] == 0;
}
typedef struct {
    const u8 *p;
    u32 left, acc;
    int bits, bad;
} Bits;
static u32 take(Bits *b, int n) {
    while (b->bits < n) {
        if (!b->left) {
            b->bad = 1;
            return 0;
        }
        b->acc |= (u32)*b->p++ << b->bits;
        b->bits += 8;
        b->left--;
    }
    u32 v = b->acc & ((1u << n) - 1);
    b->bits -= n;
    b->acc >>= n;
    return v;
}
typedef struct {
    int count[16], symbols[288];
} Tree;
static int tree(Tree *t, const u8 *l, int n) {
    int off[16] = {0}, space = 1;
    for (int i = 0; i < 16; i++)
        t->count[i] = 0;
    for (int i = 0; i < n; i++)
        t->count[l[i]]++;
    for (int i = 1; i < 16; i++) {
        space = space * 2 - t->count[i];
        if (space < 0)
            return 0;
        if (i < 15)
            off[i + 1] = off[i] + t->count[i];
    }
    for (int i = 0; i < n; i++)
        if (l[i])
            t->symbols[off[l[i]]++] = i;
    return 1;
}
static int symbol(Bits *b, const Tree *t) {
    int code = 0, base = 0, offset = 0;
    for (int n = 1; n < 16; n++) {
        code = (code << 1) | (int)take(b, 1);
        int count = t->count[n];
        if (code - base < count)
            return t->symbols[offset + code - base];
        base = (base + count) * 2;
        offset += count;
    }
    b->bad = 1;
    return -1;
}
static int inflate(const u8 *src, u32 size, u8 *out, u32 cap) {
    Bits b = {.p = src, .left = size};
    u32 used = 0;
    int last = 0;
    for (int blocks = 0; !last && !b.bad && blocks < 8192; blocks++) {
        last = take(&b, 1);
        u32 kind = take(&b, 2);
        if (kind == 3)
            return 0;
        if (!kind) {
            take(&b, b.bits & 7);
            u32 n = take(&b, 16), inv = take(&b, 16);
            if (b.bad || (n ^ inv) != 65535 || !range(cap, used, n) || n > b.left)
                return 0;
            for (u32 i = 0; i < n; i++)
                out[used++] = *b.p++;
            b.left -= n;
            continue;
        }
        Tree lit, dist;
        u8 l[320];
        int nl = 288, nd = 32;
        if (kind == 1) {
            for (int i = 0; i < 320; i++)
                l[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : i < 288 ? 8 : 5;
        } else {
            nl = 257 + take(&b, 5);
            nd = 1 + take(&b, 5);
            int nc = 4 + take(&b, 4);
            if (nl > 286)
                return 0;
            static const u8 order[] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                       11, 4,  12, 3, 13, 2, 14, 1, 15};
            u8 lens[19] = {0};
            Tree ctl;
            for (int i = 0; i < nc; i++)
                lens[order[i]] = take(&b, 3);
            if (!tree(&ctl, lens, 19))
                return 0;
            int i = 0;
            while (i < nl + nd) {
                int s = symbol(&b, &ctl);
                if (b.bad)
                    return 0;
                if (s < 16)
                    l[i++] = s;
                else {
                    if (s == 16 && !i)
                        return 0;
                    int run = s == 16   ? 3 + take(&b, 2)
                              : s == 17 ? 3 + take(&b, 3)
                                        : 11 + take(&b, 7);
                    u8 v = s == 16 ? l[i - 1] : 0;
                    if (run > nl + nd - i)
                        return 0;
                    while (run--)
                        l[i++] = v;
                }
            }
        }
        if (!l[256] || !tree(&lit, l, nl) || !tree(&dist, l + nl, nd))
            return 0;
        while (!b.bad) {
            int s = symbol(&b, &lit);
            if (b.bad)
                return 0;
            if (s < 256) {
                if (used == cap)
                    return 0;
                out[used++] = s;
                continue;
            }
            if (s == 256)
                break;
            if (s > 285)
                return 0;
            static const u16 length[] = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                         15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                         67, 83, 99, 115, 131, 163, 195, 227, 258};
            int k = s - 257, ex = k < 8 || k == 28 ? 0 : k / 4 - 1;
            u32 len = length[k] + take(&b, ex);
            s = symbol(&b, &dist);
            if (b.bad || s > 29)
                return 0;
            ex = s < 4 ? 0 : s / 2 - 1;
            u32 d = s < 4 ? (u32)s + 1 : 1u + ((2u + (s & 1)) << ex) + take(&b, ex);
            if (d > used || !range(cap, used, len))
                return 0;
            while (len--) {
                out[used] = out[used - d];
                used++;
            }
        }
    }
    return last && !b.bad && !b.left && used == cap;
}
static u32 crc(const u8 *p, u32 n) {
    u32 v = ~0u;
    while (n--) {
        v ^= *p++;
        for (int i = 0; i < 8; i++)
            v = v & 1 ? (v >> 1) ^ 0xedb88320u : v >> 1;
    }
    return ~v;
}
typedef struct {
    const u8 *p;
    u32 n, raw, crc;
    int method;
} Member;
static int unpack(Member m, u8 *out, u32 cap) {
    if (!m.p || !m.raw || m.raw > cap)
        return 0;
    if (m.method == 0) {
        if (m.n != m.raw)
            return 0;
        for (u32 i = 0; i < m.n; i++)
            out[i] = m.p[i];
    } else if (m.method != 8 || !inflate(m.p, m.n, out, m.raw))
        return 0;
    return crc(out, m.raw) == m.crc;
}
static const char *members(const u8 *f, u32 n, Member *m, Member *d) {
    if (n < 22 || n > FILE_LIMIT)
        return "APK must be a ZIP of at most 2 MiB";
    u32 e = n - 22, limit = e > 65535 ? e - 65535 : 0;
    while (u32le(f + e) != 0x06054b50u || u16le(f + e + 20) != n - e - 22) {
        if (e == limit)
            return "Missing APK ZIP directory";
        e--;
    }
    u32 count = u16le(f + e + 10), size = u32le(f + e + 12), cd = u32le(f + e + 16), p = cd;
    if (u32le(f + e + 4) || u16le(f + e + 8) != count || count > 4096 || cd > e ||
        size != e - cd)
        return "Unsupported ZIP layout";
    for (u32 i = 0; i < count; i++) {
        if (!range(e, p, 46) || u32le(f + p) != 0x02014b50)
            return "Invalid ZIP entry";
        const u8 *c = f + p;
        u32 names = u16le(c + 28), extra = u16le(c + 30), comment = u16le(c + 32);
        if (!range(e, p + 46, names + extra + comment))
            return "Invalid ZIP name";
        const u8 *nm = c + 46;
        p += 46 + names + extra + comment;
        if (names >= 12 && nm[0] == 'c' && nm[1] == 'l' && nm[2] == 'a' && nm[3] == 's' &&
            nm[4] == 's' && nm[5] == 'e' && nm[6] == 's' && nm[7] >= '0' && nm[7] <= '9' &&
            same(nm + names - 4, ".dex", 4))
            return "Multidex APKs are not supported";
        Member *x = same(nm, "AndroidManifest.xml", names) ? m
                    : same(nm, "classes.dex", names)       ? d
                                                           : 0;
        if (!x)
            continue;
        if (x->p)
            return "Duplicate APK member";
        u32 h = u32le(c + 42), packed = u32le(c + 20);
        if (!range(cd, h, 30) || u32le(f + h) != 0x04034b50 || u16le(c + 8) & 1 ||
            u16le(c + 34) || u16le(c + 10) != u16le(f + h + 8) || u16le(f + h + 26) != names)
            return "Invalid or encrypted APK member";
        u32 bytes = u16le(f + h + 28) + names;
        if (!range(cd, h + 30, bytes) || !range(cd, h + 30 + bytes, packed))
            return "Truncated APK member";
        for (u32 j = 0; j < names; j++)
            if (f[h + 30 + j] != nm[j])
                return "Mismatched APK member name";
        *x = (Member){f + h + 30 + bytes, packed, u32le(c + 24), u32le(c + 16),
                      (int)u16le(c + 10)};
    }
    return p == e && m->p && d->p ? 0 : "APK needs a manifest and primary classes.dex";
}
/* Android binary XML: select a MAIN/LAUNCHER filter on a concrete Activity.
 * Resolve the manifest class name, not the first subclass found in the DEX. */
typedef struct {
    const u8 *p;
    u32 size, header, count, strings;
    int utf8;
} Pool;
static int str(const Pool *t, u32 index, char *out, u32 cap) {
    if (index >= t->count)
        return 0;
    u32 off = u32le(t->p + t->header + 4 * index);
    if (off >= t->size - t->strings)
        return 0;
    u32 p = t->strings + off, len;
    if (t->utf8) {
        if (t->p[p++] & 128)
            p++;
        if (p >= t->size)
            return 0;
        len = t->p[p++];
        if (len & 128) {
            if (p >= t->size)
                return 0;
            len = ((len & 127) << 8) | t->p[p++];
        }
    } else {
        if (!range(t->size, p, 2))
            return 0;
        len = u16le(t->p + p);
        p += 2;
        if (len & 0x8000)
            return 0;
    }
    u32 step = t->utf8 ? 1 : 2;
    if (len >= cap || !range(t->size, p, (len + 1) * step))
        return 0;
    for (u32 i = 0; i <= len; i++) {
        u32 c = step == 1 ? t->p[p + i] : u16le(t->p + p + 2 * i);
        if (i < len && (c < 32 || c > 126))
            return 0;
        out[i] = (char)c;
        if (i == len && c)
            return 0;
    }
    return 1;
}
static int eq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
static int attr(Pool *t, const u8 *c, u32 n, const char *key, char *out, u32 cap) {
    u32 start = 16 + u16le(c + 24), stride = u16le(c + 26), count = u16le(c + 28);
    if (stride < 20 || !range(n, start, count * stride))
        return 0;
    for (u32 i = 0; i < count; i++) {
        const u8 *a = c + start + i * stride;
        char name[48];
        char ns[80];
        u32 space = u32le(a);
        if (eq(key, "name")) {
            if (!str(t, space, ns, sizeof ns) ||
                !eq(ns, "http://schemas.android.com/apk/res/android"))
                continue;
        } else if (space != 0xffffffffu)
            continue;
        if (str(t, u32le(a + 4), name, sizeof name) && eq(name, key) && a[15] == 3)
            return str(t, u32le(a + 16), out, cap);
    }
    return 0;
}
static const char *launcher(const u8 *f, u32 n, char *out, u32 cap) {
    if (n < 8 || u16le(f) != 3 || u16le(f + 2) != 8 || u32le(f + 4) != n)
        return "Requires a binary AndroidManifest.xml";
    Pool t = {0};
    char pkg[160] = {0}, activity[192] = {0}, selected[192] = {0};
    int depth = 0, act = 0, filter = 0, main = 0, launch = 0;
    for (u32 p = 8; p < n;) {
        if (!range(n, p, 8))
            return "Invalid manifest";
        const u8 *c = f + p;
        u32 tag = u16le(c), h = u16le(c + 2), size = u32le(c + 4);
        if (h < 8 || size < h || !range(n, p, size))
            return "Invalid manifest chunk";
        p += size;
        if (tag == 1) {
            if (h < 28 || t.p)
                return "Invalid manifest string pool";
            t = (Pool){c, size, h, u32le(c + 8), u32le(c + 20), !!(u32le(c + 16) & 256)};
            if (t.count > 32768 || !range(size, h, 4 * t.count) ||
                t.strings < h + 4 * t.count || t.strings >= size)
                return "Invalid manifest strings";
        } else if (tag == 0x102) {
            if (size < 36 || h != 16 || ++depth > 96)
                return "Invalid manifest element";
            char name[64];
            if (!str(&t, u32le(c + 20), name, sizeof name))
                continue;
            if (eq(name, "manifest")) {
                char split[64];
                if (attr(&t, c, size, "split", split, sizeof split))
                    return "Split APKs need an Android runtime";
                attr(&t, c, size, "package", pkg, sizeof pkg);
            } else if (eq(name, "activity")) {
                act = depth;
                activity[0] = 0;
                attr(&t, c, size, "name", activity, sizeof activity);
            } else if (act && depth == act + 1 && eq(name, "intent-filter")) {
                filter = depth;
                main = launch = 0;
            } else if (filter && depth == filter + 1) {
                char value[96];
                if (attr(&t, c, size, "name", value, sizeof value)) {
                    if (eq(name, "action") && eq(value, "android.intent.action.MAIN"))
                        main = 1;
                    if (eq(name, "category") && eq(value, "android.intent.category.LAUNCHER"))
                        launch = 1;
                }
            }
        } else if (tag == 0x103) {
            if (depth <= 0 || size < 24)
                return "Invalid manifest nesting";
            if (depth == filter) {
                if (main && launch && activity[0] && !selected[0]) {
                    for (u32 i = 0; (selected[i] = activity[i]); i++)
                        ;
                }
                filter = 0;
            }
            if (depth == act)
                act = 0;
            depth--;
        }
    }
    if (depth || !selected[0] || !pkg[0])
        return "No supported MAIN/LAUNCHER Activity in manifest";
    int dot = 0;
    for (int i = 0; selected[i]; i++)
        if (selected[i] == '.')
            dot = 1;
    const char *parts[] = {selected[0] == '.' || !dot ? pkg : "", !dot ? "." : "", selected};
    u32 j = 0;
    out[j++] = 'L';
    for (int k = 0; k < 3; k++)
        for (const char *s = parts[k]; *s; s++) {
            if (j + 2 >= cap)
                return "Activity name is too long";
            out[j++] = *s == '.' ? '/' : *s;
        }
    out[j++] = ';';
    out[j] = 0;
    return 0;
}
extern const char *android_named_activity(const u8 *, u32, const char *);
const char *apk_activity(const u8 *f, u32 n, const char **error) {
    Member m = {0}, d = {0};
    char owner[360];
    *error = members(f, n, &m, &d);
    if (*error)
        return 0;
    if (!unpack(m, manifest, sizeof manifest) || !unpack(d, dex, sizeof dex)) {
        *error = "Invalid checksum, compression or member size";
        return 0;
    }
    *error = launcher(manifest, m.raw, owner, sizeof owner);
    if (*error)
        return 0;
    const char *s = android_named_activity(dex, d.raw, owner);
    if (!s)
        *error = "Launcher Activity uses unsupported Android APIs or DEX instructions";
    return s;
}
