/* FreshOS Release 1: a PE loader and a small integer x86 machine. The entire
 * address space is private byte arrays, including an interpreted guest stack.
 * Preferred-base mappings avoid relocations. IAT addresses identify ABI calls,
 * not host functions. Unknown instructions, imports and permissions fail closed. */
#include "compat.h"
#define IMAGE_MAX (2u << 20)
#define STACK_SIZE 65536u
#define STACK 0x7ff00000u
#define HEAP 0x71000000u
#define HEAP_SIZE (256u << 10)
#define ABI 0x7fe00000u
#define RETURN (ABI - 16)
#define R 1
#define WRT 2
#define X 4
#define STEPS 80000u
static u8 image[IMAGE_MAX], stackmem[STACK_SIZE], perm[IMAGE_MAX / 4096];
static u8 heap[HEAP_SIZE];
typedef struct {
    u32 start, size;
} Allocation;
static Allocation allocs[128];
typedef struct {
    u64 r[16], ip, base;
    u32 size, out, budget, allocated, last_error;
    int wide, cf, zf, sf, of, pf, done;
    const char *err;
    PeResult *res;
} VM;
static u64 get(const u8 *p, int n) {
    u64 x = 0;
    while (n)
        x = (x << 8) | p[--n];
    return x;
}
static void put(u8 *p, int n, u64 v) {
    while (n--) {
        *p++ = (u8)v;
        v >>= 8;
    }
}
static int span(u32 n, u32 p, u32 len) { return p <= n && len <= n - p; }
static void bytes(u8 *d, const u8 *s, u32 n) {
    for (u32 i = 0; i < n; i++)
        d[i] = s[i];
}
static void clear(u8 *d, u32 n) {
    while (n--)
        *d++ = 0;
}
static void fail(VM *v, const char *s) {
    if (!v->err)
        v->err = s;
}
static void detail(PeResult *r, const char *s) {
    u32 i = 0;
    while (i + 1 < sizeof r->detail && r->detail[i])
        i++;
    while (*s && i + 1 < sizeof r->detail) {
        char c = *s++;
        r->detail[i++] = c >= 32 && c < 127 ? c : '?';
    }
    r->detail[i] = 0;
}
static u8 *mem(VM *v, u64 a, u32 n, int access) {
    if (!n)
        return 0;
    if (a >= STACK && a - STACK <= STACK_SIZE && n <= STACK_SIZE - (u32)(a - STACK) &&
        access != X)
        return stackmem + (u32)(a - STACK);
    if (a >= HEAP && a - HEAP < HEAP_SIZE && access != X) {
        u32 pos = (u32)(a - HEAP);
        for (u32 i = 0; i < v->allocated; i++) {
            Allocation al = allocs[i];
            if (pos >= al.start && span(al.size, pos - al.start, n))
                return heap + pos;
        }
        goto bad;
    }
    if (a >= v->base && a - v->base < v->size) {
        u32 p = (u32)(a - v->base);
        if (span(v->size, p, n)) {
            u32 end = (p + n - 1) >> 12;
            for (u32 i = p >> 12; i <= end; i++)
                if (!(perm[i] & access))
                    goto bad;
            return image + p;
        }
    }
bad:
    fail(v, "Guest address or section permission denied");
    return 0;
}
static u64 readv(VM *v, u64 a, int n, int access) {
    const u8 *p = mem(v, a, n, access);
    return p ? get(p, n) : 0;
}
static void writev(VM *v, u64 a, int n, u64 x) {
    u8 *p = mem(v, a, n, WRT);
    if (p)
        put(p, n, x);
}
static u64 fetch(VM *v, int n) {
    u64 x = readv(v, v->ip, n, X);
    v->ip += n;
    return x;
}
static u64 mask(int n) { return n == 8 ? ~(u64)0 : (((u64)1 << (n * 8)) - 1); }
static u64 sx(u64 x, int n) {
    u64 sign = (u64)1 << (8 * n - 1);
    return ((x & mask(n)) ^ sign) - sign;
}
static int ptrsz(VM *v) { return v->wide ? 8 : 4; }
static u64 addr(VM *v, u64 a) { return v->wide ? a : (u32)a; }
static void push(VM *v, u64 x) {
    int n = ptrsz(v);
    v->r[4] = addr(v, v->r[4] - n);
    writev(v, v->r[4], n, x);
}
static u64 pop(VM *v) {
    int n = ptrsz(v);
    u64 x = readv(v, v->r[4], n, R);
    v->r[4] = addr(v, v->r[4] + n);
    return x;
}
static int eqi(const char *a, const char *b, int fold) {
    while (*a && *b) {
        int c = *a++, d = *b++;
        if (fold) {
            if (c >= 'A' && c <= 'Z')
                c += 32;
            if (d >= 'A' && d <= 'Z')
                d += 32;
        }
        if (c != d)
            return 0;
    }
    return !*a && !*b;
}
static const char *name(VM *v, u32 rva) {
    if (rva >= v->size)
        return 0;
    for (u32 i = 0; i < 128 && i < v->size - rva; i++)
        if (!image[rva + i])
            return (const char *)image + rva;
    return 0;
}
enum Api {
    EXIT_PROCESS,
    STD_HANDLE,
    WRITE_FILE,
    WRITE_A,
    WRITE_W,
    BOX_A,
    BOX_W,
    LENGTH_A,
    LENGTH_W,
    PID,
    PROCESS_HEAP,
    HEAP_ALLOC,
    HEAP_FREE,
    LAST_ERROR,
    SET_ERROR,
    CRT_START,
    PUTS = CRT_START,
    PUTCHAR,
    STRLEN,
    MALLOC,
    CALLOC,
    FREE,
    MEMCPY,
    MEMSET,
    MEMCMP,
    CRT_EXIT
};
#define K(n, a) {"kernel32.dll", n, a}
#define C(n, a) {"msvcrt.dll", n, a}
static const struct {
    const char *dll, *name;
    int argc;
} imports[] = {K("ExitProcess", 1),
               K("GetStdHandle", 1),
               K("WriteFile", 5),
               K("WriteConsoleA", 5),
               K("WriteConsoleW", 5),
               {"user32.dll", "MessageBoxA", 4},
               {"user32.dll", "MessageBoxW", 4},
               K("lstrlenA", 1),
               K("lstrlenW", 1),
               K("GetCurrentProcessId", 0),
               K("GetProcessHeap", 0),
               K("HeapAlloc", 3),
               K("HeapFree", 3),
               K("GetLastError", 0),
               K("SetLastError", 1),
               C("puts", 1),
               C("putchar", 1),
               C("strlen", 1),
               C("malloc", 1),
               C("calloc", 2),
               C("free", 1),
               C("memcpy", 3),
               C("memset", 3),
               C("memcmp", 3),
               C("exit", 1)};
#define NIMPORT ((int)(sizeof imports / sizeof *imports))
static int bind(VM *v, u32 at, u32 size) {
    if (!at && !size)
        return 1;
    if (!span(v->size, at, size) || size < 20)
        return 0;
    int p = ptrsz(v);
    for (u32 i = 0; i < 128 && 20 * i + 20 <= size; i++) {
        const u8 *d = image + at + 20 * i;
        u32 lookup = get(d, 4), dll = get(d + 12, 4), iat = get(d + 16, 4);
        if (!lookup && !dll && !iat)
            return 1;
        if (!lookup)
            lookup = iat;
        const char *source = name(v, dll);
        char lib[128];
        if (!source || !iat || !lookup)
            return 0;
        for (u32 k = 0; (lib[k] = source[k]); k++)
            ;
        u32 j;
        for (j = 0; j < 512; j++, iat += p, lookup += p) {
            if (!span(v->size, lookup, p) || !span(v->size, iat, p))
                return 0;
            u64 hint = get(image + lookup, p);
            if (!hint)
                break;
            if (hint > v->size || v->size - hint < 3) {
                fail(v, "Ordinal or invalid import is unsupported");
                return 0;
            }
            const char *s = name(v, (u32)hint + 2);
            if (!s)
                return 0;
            int k = 0;
            for (; k < NIMPORT; k++)
                if (eqi(lib, imports[k].dll, 1) && eqi(s, imports[k].name, 0))
                    break;
            if (k == NIMPORT) {
                detail(v->res, "Missing import: ");
                detail(v->res, lib);
                detail(v->res, "!");
                detail(v->res, s);
                fail(v, "Unsupported Windows API");
                return 0;
            }
            put(image + iat, p, ABI + k * 16);
        }
        if (j == 512)
            return 0;
    }
    return 0;
}
static int load(VM *v, const u8 *f, u32 n) {
    if (n < 64 || n > FILE_LIMIT || get(f, 2) != 0x5a4d)
        return 0;
    u32 pe = get(f + 60, 4);
    if (!span(n, pe, 24) || get(f + pe, 4) != 0x4550)
        return 0;
    u32 m = get(f + pe + 4, 2), ns = get(f + pe + 6, 2), opt = get(f + pe + 20, 2),
        flags = get(f + pe + 22, 2);
    u32 start = pe + 24;
    if ((m != 0x14c && m != 0x8664) || !(flags & 2) || (flags & 0x2000)) {
        fail(v, "Requires an x86 or x64 executable, not a DLL or driver");
        return 0;
    }
    v->wide = m == 0x8664;
    v->res->bits = v->wide ? 64 : 32;
    u32 dirs = v->wide ? 112 : 96;
    if (opt < dirs || !span(n, start, opt) || ns == 0 || ns > 96 ||
        !span(n, start + opt, ns * 40))
        return 0;
    const u8 *h = f + start;
    if (get(h, 2) != (v->wide ? 0x20bu : 0x10bu))
        return 0;
    u32 entry = get(h + 16, 4), size = get(h + 56, 4), headers = get(h + 60, 4),
        nd = get(h + dirs - 4, 4);
    v->base = get(h + (v->wide ? 24 : 28), ptrsz(v));
    int sub = get(h + 68, 2);
    if (sub != 2 && sub != 3) {
        fail(v, "Only Windows GUI and console subsystems are supported");
        return 0;
    }
    if (nd > 16 || nd > (opt - dirs) / 8 || !size || size > IMAGE_MAX || !headers ||
        headers > size || headers > n || headers < start + opt + 40 * ns || entry >= size ||
        get(h + 32, 4) < 4096 || !v->base || v->base > 0x7fffffffffffULL - size ||
        (!v->wide && v->base > 0xffffffffu - size) ||
        !(v->base + size <= HEAP || v->base >= HEAP + HEAP_SIZE) ||
        !(v->base + size <= RETURN || v->base >= STACK + STACK_SIZE))
        return 0;
    for (u32 i = 0; i < nd; i++)
        if ((i == 9 || i == 13 || i == 14) && get(h + dirs + 8 * i, 4)) {
            fail(v, "TLS, delay loading and managed .NET images are unsupported");
            return 0;
        }
    v->size = size;
    clear(image, size);
    clear(perm, sizeof perm);
    clear(stackmem, sizeof stackmem);
    clear(heap, sizeof heap);
    bytes(image, f, headers);
    for (u32 i = 0; i < (headers + 4095) / 4096; i++)
        perm[i] = R;
    for (u32 i = 0; i < ns; i++) {
        const u8 *s = h + opt + 40 * i;
        u32 vs = get(s + 8, 4), rva = get(s + 12, 4), raw = get(s + 16, 4),
            off = get(s + 20, 4), ch = get(s + 36, 4);
        if (raw > vs)
            vs = raw;
        if (!vs)
            continue;
        if ((rva & 4095) || !span(size, rva, vs) || (raw && !span(n, off, raw)))
            return 0;
        int bits =
            8 | ((ch >> 30) & 1 ? R : 0) | ((ch >> 31) ? WRT : 0) | ((ch >> 29) & 1 ? X : 0);
        for (u32 j = rva >> 12; j <= (rva + vs - 1) >> 12; j++) {
            if (perm[j])
                return 0;
            perm[j] = bits;
        }
        if (raw)
            bytes(image + rva, f + off, raw);
    }
    v->ip = v->base + entry;
    if (!(perm[entry >> 12] & X))
        return 0;
    return nd < 2 || bind(v, get(h + dirs + 8, 4), get(h + dirs + 12, 4));
}
static void emit(VM *v, u32 c) {
    if (v->out < sizeof v->res->output - 1 && c != '\r') {
        v->res->output[v->out++] = c == '\n' || (c >= 32 && c < 127) ? (char)c : '?';
        v->res->output[v->out] = 0;
    }
}
static u32 string(VM *v, u64 a, int wide, int show) {
    for (u32 i = 0; i < 4096; i++) {
        if (!v->budget) {
            fail(v, "Guest work budget exhausted");
            return 0;
        }
        v->budget--;
        u32 c = readv(v, a + i * (1 + wide), 1 + wide, R);
        if (!c || v->err)
            return i;
        if (show)
            emit(v, c);
    }
    fail(v, "Guest string exceeds limit");
    return 0;
}
static int charge(VM *v, u32 n) {
    if (n > v->budget) {
        fail(v, "Guest work budget exhausted");
        return 0;
    }
    v->budget -= n;
    return 1;
}
static u32 allocate(VM *v, u64 size, int zeroed) {
    if (size > HEAP_SIZE || v->allocated >= 128)
        return 0;
    u32 n = ((u32)size + 15) & ~15u;
    if (!n)
        n = 16;
    u32 start = 0, i = 0;
    for (; i < v->allocated; i++) {
        if (allocs[i].start - start >= n)
            break;
        start = allocs[i].start + allocs[i].size;
    }
    if (n > HEAP_SIZE - start)
        return 0;
    if (zeroed && !charge(v, (n + 15) / 16))
        return 0;
    for (u32 j = v->allocated; j > i; j--)
        allocs[j] = allocs[j - 1];
    allocs[i] = (Allocation){start, n};
    v->allocated++;
    if (zeroed)
        clear(heap + start, n);
    return HEAP + start;
}
static int release(VM *v, u64 ptr) {
    for (u32 i = 0; i < v->allocated; i++)
        if (ptr == HEAP + allocs[i].start) {
            v->allocated--;
            for (; i < v->allocated; i++)
                allocs[i] = allocs[i + 1];
            return 1;
        }
    return 0;
}
static void api(VM *v, int k) {
    int n = imports[k].argc;
    u64 a[5] = {0};
    static const int regs[] = {1, 2, 8, 9};
    for (int i = 0; i < n; i++)
        a[i] = v->wide && i < 4 ? v->r[regs[i]]
                                : readv(v, v->r[4] + ptrsz(v) * (i + 1), ptrsz(v), R);
    if (v->err)
        return;
    u64 result = 0;
    if (k == EXIT_PROCESS || k == CRT_EXIT) {
        v->done = 1;
        v->res->exit_code = (u32)a[0];
        return;
    }
    if (k == STD_HANDLE) {
        result = (u32)a[0] == (u32)-11 ? 1 : (u32)a[0] == (u32)-12 ? 2 : 0;
    }
    if (k >= WRITE_FILE && k <= WRITE_W) {
        u32 count = (u32)a[2], stride = k == WRITE_W ? 2 : 1;
        if (count > 16384 || a[4] || (a[0] != 1 && a[0] != 2)) {
            fail(v, "Only bounded writes to standard output/error are supported");
            return;
        }
        if (count > v->budget) {
            fail(v, "Guest work budget exhausted");
            return;
        }
        v->budget -= count;
        if (count) {
            const u8 *p = mem(v, a[1], count * stride, R);
            if (!p)
                return;
            for (u32 i = 0; i < count; i++)
                emit(v, (u32)get(p + i * stride, stride));
        }
        if (a[3])
            writev(v, a[3], 4, count);
        result = 1;
    }
    if (k == BOX_A || k == BOX_W) {
        if (a[0] || a[3]) {
            fail(v, "Only plain message boxes are supported");
            return;
        }
        if (a[2]) {
            string(v, a[2], k == BOX_W, 1);
            emit(v, '\n');
        }
        if (a[1])
            string(v, a[1], k == BOX_W, 1);
        result = 1;
    }
    if (k == LENGTH_A || k == LENGTH_W || k == STRLEN)
        result = string(v, a[0], k == LENGTH_W, 0);
    if (k == PID)
        result = 1;
    if (k == PROCESS_HEAP)
        result = 3;
    if (k == HEAP_ALLOC) {
        if (a[0] != 3 || (a[1] & ~(u64)8)) {
            fail(v, "Unsupported heap flags or handle");
            return;
        }
        result = allocate(v, a[2], a[1] & 8);
    }
    if (k == HEAP_FREE) {
        result = a[0] == 3 && a[1] == 0 && release(v, a[2]);
        if (!result)
            v->last_error = 87;
    }
    if (k == LAST_ERROR)
        result = v->last_error;
    if (k == SET_ERROR)
        v->last_error = (u32)a[0];
    if (k == PUTS) {
        string(v, a[0], 0, 1);
        emit(v, '\n');
        result = 0;
    }
    if (k == PUTCHAR) {
        emit(v, a[0] & 255);
        result = a[0] & 255;
    }
    if (k == MALLOC)
        result = allocate(v, a[0], 0);
    if (k == CALLOC && a[0] <= HEAP_SIZE && a[1] <= HEAP_SIZE)
        result = allocate(v, a[0] * a[1], 1);
    if (k == FREE && a[0] && !release(v, a[0]))
        fail(v, "Invalid guest allocation");
    if (k == MEMCPY || k == MEMSET || k == MEMCMP) {
        u64 count = a[2];
        if (count > IMAGE_MAX || !charge(v, ((u32)count + 15) / 16)) {
            fail(v, "Guest memory operation exceeds limit");
            return;
        }
        result = k == MEMCMP ? 0 : a[0];
        if (count) {
            u8 *dst = mem(v, a[0], count, k == MEMCMP ? R : WRT);
            const u8 *src = k == MEMSET ? 0 : mem(v, a[1], count, R);
            if (v->err)
                return;
            for (u32 i = 0; i < count; i++) {
                if (k == MEMCMP) {
                    if (dst[i] != src[i]) {
                        result = (u32)((int)dst[i] - src[i]);
                        break;
                    }
                } else
                    dst[i] = k == MEMSET ? (u8)a[1] : src[i];
            }
        }
    }
    v->r[0] = result;
    v->ip = pop(v);
    if (!v->wide && k < CRT_START)
        v->r[4] = addr(v, v->r[4] + 4 * n);
}
/* Decode ModR/M and SIB. RIP-relative operands are resolved only after the
 * complete instruction (including any immediate) has been consumed. */
typedef struct {
    int reg, rip;
    u64 a;
} Operand;
static Operand decode(VM *v, u32 m, int rex) {
    int mod = m >> 6, rm = m & 7;
    Operand o = {-1, 0, 0};
    if (mod == 3) {
        o.reg = rm + ((rex & 1) * 8);
        return o;
    }
    int base = rm;
    if (rm == 4) {
        int sib = fetch(v, 1);
        base = sib & 7;
        int idx = (sib >> 3) & 7;
        if (idx != 4 || (rex & 2))
            o.a = v->r[idx + ((rex & 2) ? 8 : 0)] << (sib >> 6);
    }
    if (mod || base != 5)
        o.a += v->r[base + ((rex & 1) ? 8 : 0)];
    else {
        o.rip = v->wide && rm == 5;
        o.a += sx(fetch(v, 4), 4);
    }
    if (mod == 1)
        o.a += sx(fetch(v, 1), 1);
    if (mod == 2)
        o.a += sx(fetch(v, 4), 4);
    return o;
}
static u64 ea(VM *v, Operand o) { return addr(v, o.a + (o.rip ? v->ip : 0)); }
static u64 operand(VM *v, Operand o, int n) {
    return o.reg >= 0 ? v->r[o.reg] & mask(n) : readv(v, ea(v, o), n, R);
}
static void set(VM *v, Operand o, int n, u64 val) {
    if (o.reg >= 0) {
        if (n == 1 || n == 2)
            v->r[o.reg] = (v->r[o.reg] & ~mask(n)) | (val & mask(n));
        else
            v->r[o.reg] = val & mask(n);
    } else
        writev(v, ea(v, o), n, val);
}
static u64 alu(VM *v, int op, u64 a, u64 b, int n) {
    u64 z;
    u64 sign = (u64)1 << (n * 8 - 1);
    a &= mask(n);
    b &= mask(n);
    v->cf = v->of = 0;
    if (op == 0) {
        z = (a + b) & mask(n);
        v->cf = z < a;
        v->of = !!(~(a ^ b) & (a ^ z) & sign);
    } else if (op == 5 || op == 7) {
        z = (a - b) & mask(n);
        v->cf = a < b;
        v->of = !!((a ^ b) & (a ^ z) & sign);
    } else if (op == 1)
        z = a | b;
    else if (op == 4)
        z = a & b;
    else if (op == 6)
        z = a ^ b;
    else {
        fail(v, "Unsupported arithmetic operation");
        return 0;
    }
    v->zf = !z;
    v->sf = !!(z & sign);
    unsigned t = z & 255;
    t ^= t >> 4;
    v->pf = !(0x6996u >> (t & 15) & 1);
    return z;
}
static int cond(VM *v, int k) {
    int a[] = {v->of, v->cf, v->zf,          v->cf || v->zf,
               v->sf, v->pf, v->sf != v->of, v->zf || v->sf != v->of};
    return a[k >> 1] ^ (k & 1);
}
static void step(VM *v) {
    int op = fetch(v, 1), rex = 0, w16 = 0;
    if (op == 0x66) {
        w16 = 1;
        op = fetch(v, 1);
    }
    if (v->wide && op >= 0x40 && op <= 0x4f) {
        rex = op;
        op = fetch(v, 1);
    }
    int n = rex & 8 ? 8 : w16 ? 2 : 4, p = ptrsz(v);
    if (v->err)
        return;
    if (w16 && (op == 0xe8 || op == 0xe9 || op == 0xff || op == 0x0f))
        goto unsupported;
    if (op >= 0xb8 && op <= 0xbf) {
        set(v, (Operand){(op & 7) + 8 * (rex & 1), 0, 0}, n, fetch(v, n));
        return;
    }
    if (op >= 0x50 && op <= 0x5f && !w16) {
        int r = (op & 7) + 8 * (rex & 1);
        if (op & 8)
            v->r[r] = pop(v);
        else
            push(v, v->r[r]);
        return;
    }
    if (!v->wide && op >= 0x40 && op <= 0x4f) {
        int cf = v->cf;
        Operand r = {op & 7, 0, 0};
        set(v, r, n, alu(v, op & 8 ? 5 : 0, operand(v, r, n), 1, n));
        v->cf = cf;
        return;
    }
    if (op == 0xa1 || op == 0xa3) {
        u64 a = fetch(v, p);
        if (op == 0xa1)
            set(v, (Operand){0, 0, 0}, n, readv(v, a, n, R));
        else
            writev(v, a, n, v->r[0]);
        return;
    }
    if (op == 0x90)
        return;
    if (op == 0xc9 && !w16) {
        v->r[4] = v->r[5];
        v->r[5] = pop(v);
        return;
    }
    if ((op == 0xc3 || op == 0xc2) && !w16) {
        u32 off = op == 0xc2 ? fetch(v, 2) : 0;
        v->ip = pop(v);
        v->r[4] = addr(v, v->r[4] + off);
        return;
    }
    if ((op == 0x68 || op == 0x6a) && !w16) {
        int s = op == 0x68 ? 4 : 1;
        push(v, sx(fetch(v, s), s));
        return;
    }
    if (op == 0xe8 || op == 0xe9 || op == 0xeb || (op >= 0x70 && op < 0x80)) {
        int s = op == 0xe8 || op == 0xe9 ? 4 : 1;
        u64 d = sx(fetch(v, s), s);
        if (op == 0xe8)
            push(v, v->ip);
        if (op >= 0xe8 || cond(v, op & 15))
            v->ip = addr(v, v->ip + d);
        return;
    }
    if (op == 0x0f) {
        op = fetch(v, 1);
        if (op >= 0x80 && op <= 0x8f) {
            u64 d = sx(fetch(v, 4), 4);
            if (cond(v, op & 15))
                v->ip = addr(v, v->ip + d);
            return;
        }
        if (op == 0xb6 || op == 0xb7) {
            u32 m = fetch(v, 1);
            Operand o = decode(v, m, rex);
            if (op == 0xb6 && o.reg >= 4 && o.reg <= 7 && !rex)
                goto unsupported;
            set(v, (Operand){(int)((m >> 3) & 7) + ((rex & 4) ? 8 : 0), 0, 0}, n,
                operand(v, o, op == 0xb6 ? 1 : 2));
            return;
        }
        goto unsupported;
    }
    if ((op & 7) == 5 && op < 0x40) {
        u64 b = sx(fetch(v, n == 2 ? 2 : 4), n == 2 ? 2 : 4);
        u64 z = alu(v, op >> 3, v->r[0], b, n);
        if (op >> 3 != 7)
            set(v, (Operand){0, 0, 0}, n, z);
        return;
    }
    {
        u32 m = fetch(v, 1);
        int group = (m >> 3) & 7;
        Operand o = decode(v, m, rex), r = {group + ((rex & 4) ? 8 : 0), 0, 0};
        if (v->err)
            return;
        if (op == 0x8d && o.reg < 0) {
            set(v, r, n, ea(v, o));
            return;
        }
        if (op == 0x63 && v->wide && n == 8) {
            set(v, r, n, sx(operand(v, o, 4), 4));
            return;
        }
        if (op == 0x88 || op == 0x8a) {
            if (!rex && ((o.reg >= 4 && o.reg <= 7) || (r.reg >= 4 && r.reg <= 7)))
                goto unsupported;
            Operand dst = op == 0x88 ? o : r, src = op == 0x88 ? r : o;
            set(v, dst, 1, operand(v, src, 1));
            return;
        }
        if (op == 0x89 || op == 0x8b) {
            if (op == 0x89)
                set(v, o, n, operand(v, r, n));
            else
                set(v, r, n, operand(v, o, n));
            return;
        }
        if (op == 0xc7 && group == 0) {
            int s = n == 2 ? 2 : 4;
            u64 val = sx(fetch(v, s), s);
            set(v, o, n, val);
            return;
        }
        if (op == 0xff) {
            if (group == 0 || group == 1) {
                int cf = v->cf;
                set(v, o, n, alu(v, group ? 5 : 0, operand(v, o, n), 1, n));
                v->cf = cf;
                return;
            }
            if (group == 2 || group == 4 || group == 6) {
                u64 target = operand(v, o, p);
                if (group == 6)
                    push(v, target);
                else {
                    if (group == 2)
                        push(v, v->ip);
                    v->ip = target;
                }
                return;
            }
        }
        if (op == 0x81 || op == 0x83) {
            int s = op == 0x83 ? 1 : n == 2 ? 2 : 4;
            u64 val = sx(fetch(v, s), s);
            u64 z = alu(v, group, operand(v, o, n), val, n);
            if (group != 7)
                set(v, o, n, z);
            return;
        }
        if (op == 0x85) {
            alu(v, 4, operand(v, o, n), operand(v, r, n), n);
            return;
        }
        if (op < 0x40 && ((op & 7) == 1 || (op & 7) == 3)) {
            Operand dst = op & 2 ? r : o, src = op & 2 ? o : r;
            u64 z = alu(v, op >> 3, operand(v, dst, n), operand(v, src, n), n);
            if ((op >> 3) != 7)
                set(v, dst, n, z);
            return;
        }
    }
unsupported:
    fail(v, "Unsupported x86 instruction");
}
static int execute(VM *state) {
    VM v = *state;
    PeResult *r = v.res;
    if (!v.err) {
        v.r[4] = STACK + STACK_SIZE - 64;
        push(&v, RETURN);
    }
    while (!v.err && !v.done && v.budget--) {
        r->steps++;
        if (v.ip == RETURN) {
            v.done = 1;
            r->exit_code = (u32)v.r[0];
        } else if (v.ip >= ABI && v.ip < ABI + 16 * NIMPORT && !(v.ip & 15))
            api(&v, (int)((v.ip - ABI) / 16));
        else
            step(&v);
    }
    if (!v.done && !v.err)
        fail(&v, "Instruction budget exhausted");
    if (!r->detail[0])
        detail(r, v.err ? v.err : "Program exited");
    return v.done && !v.err;
}

int pe_run(const u8 *f, u32 n, PeResult *r) {
    clear((u8 *)r, sizeof *r);
    VM v = {.res = r, .budget = STEPS};
    if (!load(&v, f, n))
        fail(&v, "Invalid or oversized PE image");
    return execute(&v);
}
/* IMP1 is a prelinked page image. Import ordinals are frozen for ABI 1.
 * Preserve the mapped headers for applications which inspect their own image;
 * the IMP loader does not read or trust PE structures. See docs/imp.md. */
static u32 imp_crc(const u8 *f, u32 n) {
    u32 crc = ~0u;
    for (u32 i = 0; i < n; i++) {
        crc ^= (i >= 28 && i < 32) ? 0 : f[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
u32 imp_convert(const u8 *f, u32 n, u8 *out, u32 cap, PeResult *r) {
    clear((u8 *)r, sizeof *r);
    VM v = {.res = r};
    if (!load(&v, f, n)) {
        if (!r->detail[0])
            detail(r, v.err ? v.err : "Invalid or unsupported PE");
        return 0;
    }
    if (cap < 32) {
        detail(r, "Output too small");
        return 0;
    }
    clear(out, 32);
    bytes(out, (const u8 *)"FIMP", 4);
    out[4] = 1;
    out[5] = v.res->bits;
    out[6] = 1;
    out[7] = 32;
    put(out + 12, 4, v.size);
    put(out + 16, 8, v.base);
    put(out + 24, 4, v.ip - v.base);
    u32 pos = 32;
    for (u32 off = 0; off < v.size; off += 4096) {
        u32 size = v.size - off;
        if (size > 4096)
            size = 4096;
        int flags = perm[off >> 12] & 7;
        if (!flags)
            size = 0;
        while (size && image[off + size - 1] == 0)
            size--;
        if (!span(cap, pos, 4 + size)) {
            detail(r, "Output too small");
            return 0;
        }
        out[pos] = flags;
        out[pos + 1] = 0;
        put(out + pos + 2, 2, size);
        bytes(out + pos + 4, image + off, size);
        pos += 4 + size;
    }
    put(out + 8, 4, pos);
    put(out + 28, 4, imp_crc(out, pos));
    return pos;
}
static const char *imp_load(VM *v, const u8 *f, u32 n) {
    if (n < 32 || n > IMP_LIMIT || get(f, 4) != 0x504d4946u)
        return "Not a FreshOS IMP image";
    if (f[4] != 1 || f[6] != 1 || f[7] != 32)
        return "Unsupported IMP format or ABI";
    if (f[5] != 32 && f[5] != 64)
        return "Unsupported IMP architecture";
    if (get(f + 8, 4) != n || imp_crc(f, n) != get(f + 28, 4))
        return "IMP length or checksum mismatch";
    v->res->bits = f[5];
    v->wide = f[5] == 64;
    v->size = get(f + 12, 4);
    v->base = get(f + 16, 8);
    u32 entry = get(f + 24, 4), size = v->size;
    if (!size || size > IMAGE_MAX || entry >= size || !v->base ||
        v->base > 0x7fffffffffffULL - size || (!v->wide && v->base > 0xffffffffu - size) ||
        !(v->base + size <= HEAP || v->base >= HEAP + HEAP_SIZE) ||
        !(v->base + size <= RETURN || v->base >= STACK + STACK_SIZE))
        return "Invalid IMP address layout";
    clear(image, size);
    clear(perm, sizeof perm);
    u32 pos = 32;
    for (u32 off = 0; off < size; off += 4096) {
        if (!span(n, pos, 4))
            return "Truncated IMP page";
        int flags = f[pos];
        u32 used = get(f + pos + 2, 2);
        if (flags > 7 || f[pos + 1] || used > 4096 || used > size - off || (!flags && used) ||
            !span(n, pos + 4, used))
            return "Invalid IMP page";
        perm[off >> 12] = flags;
        bytes(image + off, f + pos + 4, used);
        pos += 4 + used;
    }
    if (pos != n || !(perm[entry >> 12] & X))
        return "Invalid IMP entry or trailing data";
    v->ip = v->base + entry;
    clear(stackmem, sizeof stackmem);
    clear(heap, sizeof heap);
    return 0;
}
int imp_run(const u8 *f, u32 n, PeResult *r) {
    clear((u8 *)r, sizeof *r);
    VM v = {.res = r, .budget = STEPS};
    v.err = imp_load(&v, f, n);
    return execute(&v);
}
