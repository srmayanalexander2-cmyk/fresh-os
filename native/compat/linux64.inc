/* Additional syscall dispatch for a single static x86-64 Linux process. */
static void linux_native(VM *v) {
    u64 nr = v->r[0], a = v->r[7], b = v->r[6], c = v->r[2], d = v->r[10];
    u64 ret = (u64)-22;
    switch (nr) {
    case 5:
        ret = linux_fstat(v, (u32)a, b, 0);
        break;
    case 9:
        if (!a && d == 0x22 && (int)v->r[8] == -1 && !v->r[9] &&
            (c == 0 || c == 1 || c == 3 || c == 5)) {
            ret = region_alloc(0, b, c, 0, 0);
            if (!ret)
                ret = (u64)-12;
        }
        break;
    case 10: {
        int old;
        if (!(a & 4095) && (c == 0 || c == 1 || c == 3 || c == 5) &&
            region_protect(a, b, c, &old))
            ret = 0;
        break;
    }
    case 11:
        if (!(b && a < CURRENT.brk_limit &&
              (a >= CURRENT.brk_base || b > CURRENT.brk_base - a)) &&
            !(a & 4095) && region_free(a, b, 0))
            ret = 0;
        break;
    case 12:
        ret = linux_brk(v, a);
        break;
    case 96:
        ret = linux_gettimeofday(v, a, b, 1);
        break;
    case 201:
        ret = linux_time(v, a, 1);
        break;
    case 228:
        ret = linux_clock_gettime(v, a, b, 1);
        break;
    case 229:
        ret = linux_clock_getres(v, a, b, 1);
        break;
    case 262:
        ret = linux_fstatat(v, (int)a, b, c, (u32)d, 0);
        break;
    case 158:
        if (a == 0x1002 && b < (1ull << 47)) {
            user_fs = b;
            ret = 0;
        } else if (a == 0x1003) {
            writev(v, b, 8, user_fs);
            ret = 0;
        }
        break;
    case 218:
        ret = 1;
        break; /* single thread; address space is discarded on exit */
    case 102:
    case 104:
    case 107:
    case 108:
        ret = 1000;
        break;
    case 16:
        ret = (u64)-25;
        break; /* regular resources / captured output are not ttys */
    case 20:
    case 19: {
        if (c > 32)
            break;
        u64 total = 0;
        for (u32 i = 0; i < c; i++) {
            u64 data = readv(v, b + 16 * i, 8, R), n = readv(v, b + 16 * i + 8, 8, R);
            if (v->err)
                return;
            if (!n)
                continue;
            linux_call(v, nr == 20 ? 1 : 0, a, data, n);
            if (v->err)
                return;
            if ((long long)v->r[0] < 0) {
                ret = total ? total : v->r[0];
                goto end;
            }
            total += v->r[0];
            if (v->r[0] < n)
                break;
        }
        ret = total;
        break;
    }
    case 0:
        if (!a) {
            ret = 0;
            break;
        }
        linux_call(v, nr, a, b, c);
        return; /* no interactive stdin */
    case 8:
        if (a == 3 && CURRENT.doclen >= 0 && c <= 2) {
            u64 base = c == 0 ? 0 : c == 1 ? CURRENT.docpos : CURRENT.doclen;
            u64 p = base + b;
            if (p <= (u32)CURRENT.doclen) {
                CURRENT.docpos = p;
                ret = p;
            }
        } else
            ret = (u64)-9;
        break;
    case 63: {
        u8 *p = mem(v, a, 390, WRT);
        if (!p)
            return;
        memset(p, 0, 390);
        const char *fields[] = {"FreshOS", "stratus", "2", "IMP native", "x86_64", ""};
        for (int i = 0; i < 6; i++)
            memcpy(p + i * 65, fields[i], length(fields[i]));
        ret = 0;
        break;
    }
    default:
        linux_call(v, nr, a, b, c);
        return;
    }
end:
    v->r[0] = ret;
}
