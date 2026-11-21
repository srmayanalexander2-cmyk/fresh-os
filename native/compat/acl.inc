/* Win32 ACL/SID buffer operations. These manipulate caller-owned bytes; none
 * of them performs an AccessCheck or changes a FreshOS process privilege. */
static u64 security_error(VM *v, u32 error) {
    v->last_error = error;
    return 0;
}
static unsigned sid_bytes(const u8 *s, unsigned available) {
    if (available < 8 || s[0] != 1 || s[1] > 15)
        return 0;
    unsigned n = 8 + 4u * s[1];
    return n <= available ? n : 0;
}
static u8 *guest_sid(VM *v, u64 address, unsigned *size) {
    if (!address)
        return 0;
    u8 *s = mem(v, address, 8, R);
    if (!s || !(*size = sid_bytes(s, 68)))
        return 0;
    return mem(v, address, *size, R);
}
static u64 sid_api(VM *v, int k, const u64 *a) {
    if (k == SID_REQUIRED)
        return 8 + 4 * (u8)a[0];
    if (k == SID_INIT) {
        unsigned count = (u8)a[2];
        if (count > 15 || !a[0] || !a[1])
            return security_error(v, 87);
        u8 *src = mem(v, a[1], 6, R), *dst = mem(v, a[0], 8 + 4 * count, WRT);
        if (!src || !dst)
            return 0;
        u8 authority[6];
        bytes(authority, src, 6);
        dst[0] = 1;
        dst[1] = count;
        bytes(dst + 2, authority, 6);
        return 1;
    }
    unsigned n = 0, m = 0;
    u8 *s = guest_sid(v, k == SID_COPY ? a[2] : a[0], &n);
    if (k == SID_VALID)
        return s != 0;
    if (!s)
        return security_error(v, 1337); /* ERROR_INVALID_SID */
    if (k == SID_LENGTH)
        return n;
    if (k == SID_COUNT)
        return a[0] + 1;
    if (k == SID_SUB)
        return (u32)a[1] < s[1] ? a[0] + 8 + 4 * (u32)a[1] : security_error(v, 87);
    if (k == SID_EQUAL) {
        u8 *other = guest_sid(v, a[1], &m);
        if (!other || n != m)
            return 0;
        for (unsigned i = 0; i < n; i++)
            if (s[i] != other[i])
                return 0;
        return 1;
    }
    if ((u32)a[0] < n)
        return security_error(v, 122);
    u8 *dst = mem(v, a[1], n, WRT);
    if (!dst)
        return 0;
    u8 copy[68];
    bytes(copy, s, n);
    bytes(dst, copy, n);
    return 1;
}
/* Accept standard SID-bearing ACE records. Object ACEs carry zero, one or
 * two 16-byte GUIDs between their flags and SID. Callback payloads are opaque. */
static unsigned ace_bytes(const u8 *p, unsigned left, unsigned revision) {
    if (left < 8)
        return 0;
    unsigned t = p[0], n = get(p + 2, 2), sid = 8;
    if (n < 8 || n > left || (n & 3))
        return 0;
    if ((t >= 5 && t <= 8) || t == 11 || t == 12 || t == 15 || t == 16) {
        if (revision != 4 || n < 12)
            return 0;
        u32 flags = get(p + 8, 4);
        if (flags & ~3u)
            return 0;
        sid = 12 + 16 * !!(flags & 1) + 16 * !!(flags & 2);
    } else if (t == 4 || t > 17)
        return 0;
    if (sid > n || !sid_bytes(p + sid, n - sid))
        return 0;
    return n;
}
static u8 ace_copy[65536]; /* Not mapped into user space; no callbacks during insertion. */
static u64 acl_api(VM *v, int k, const u64 *a) {
    if (!a[0] || (a[0] & 3))
        return security_error(v, 87);
    if (k == ACL_INIT) {
        u32 n = (u32)a[1], rev = (u32)a[2];
        if (n < 8 || n > 65535 || (n & 3) || (rev != 2 && rev != 4))
            return security_error(v, 87);
        u8 *p = mem(v, a[0], n, WRT);
        if (!p)
            return 0;
        clear(p, 8);
        p[0] = rev;
        put(p + 2, 2, n);
        return 1;
    }
    u8 *p = mem(v, a[0], 8, R);
    if (!p)
        return 0;
    unsigned n = get(p + 2, 2), count = get(p + 4, 2), rev = p[0];
    if (n < 8 || (n & 3) || (rev != 2 && rev != 4))
        return security_error(v, 87);
    p = mem(v, a[0], n, R);
    if (!p || !charge(v, 1 + n / 16))
        return 0;
    u32 index = (u32)(k == ACL_ADD ? a[2] : a[1]);
    unsigned used = 8, pos = 0, len = 0;
    for (unsigned i = 0; i < count; i++) {
        unsigned z = ace_bytes(p + used, n - used, rev);
        if (!z)
            return security_error(v, 87);
        if (i == index) {
            pos = used;
            len = z;
        }
        used += z;
    }
    if (k == ACL_GET) {
        if (!pos || !a[2])
            return security_error(v, 87);
        writev(v, a[2], ptrsz(v), a[0] + pos);
        return !v->err;
    }
    if (k == ACL_INFO) {
        u32 cls = (u32)a[3], size = cls == 1 ? 4 : 12;
        if (cls != 1 && cls != 2)
            return security_error(v, 87);
        if ((u32)a[2] < size)
            return security_error(v, 122);
        u8 *out = mem(v, a[1], size, WRT);
        if (!out)
            return 0;
        put(out, 4, cls == 1 ? rev : count);
        if (cls == 2) {
            put(out + 4, 4, used);
            put(out + 8, 4, n - used);
        }
        return 1;
    }
    p = mem(v, a[0], n, WRT);
    if (!p)
        return 0;
    if (k == ACL_DELETE) {
        if (!pos)
            return security_error(v, 87);
        bytes(p + pos, p + pos + len, used - pos - len);
        put(p + 4, 2, count - 1);
        return 1;
    }
    u32 newrev = (u32)a[1], nb = (u32)a[4];
    if (newrev != 2 && newrev != 4)
        return security_error(v, 87);
    if (!a[3] || !nb || (nb & 3))
        return security_error(v, 87);
    if (nb > n - used)
        return security_error(v, 122); /* no partial insertion */
    u8 *src = mem(v, a[3], nb, R);
    if (!src || !charge(v, 1 + nb / 8))
        return 0;
    unsigned added = 0;
    for (unsigned off = 0; off < nb; added++) {
        unsigned z = ace_bytes(src + off, nb - off, newrev);
        if (!z)
            return security_error(v, 87);
        off += z;
    }
    if (!pos)
        pos = used;
    bytes(ace_copy, src, nb);
    for (unsigned i = used; i > pos; i--)
        p[i + nb - 1] = p[i - 1];
    bytes(p + pos, ace_copy, nb);
    put(p + 4, 2, count + added);
    p[0] = rev > newrev ? rev : newrev;
    return 1;
}
