/* Adapted from FreshOS 1.5 (af3326c): a PE loader and a small integer x86 machine. The entire
 * address space is private byte arrays, including an interpreted guest stack.
 * Local DLLs are relocated by the host packer. IAT addresses identify ABI calls,
 * not host functions. Unknown instructions, imports and permissions fail closed. */
#include "compat.h"
#include "region64.h"
#include "package.h"
#ifdef __x86_64__
#define IMAGE_MAX (4u << 20) /* raw PE; packaged IMP2 retains FILE_LIMIT */
#else
#define IMAGE_MAX FILE_LIMIT
#endif
#define STACK_SIZE 65536u
#define STACK 0x7ff00000u
#define HEAP 0x71000000u
#define HEAP_SIZE (256u << 10)
#define ABI 0x7fe00000u
#define RETURN (ABI - 16)
#define GUARD_CHECK (ABI - 32)
#define R 1
#define WRT 2
#define X 4
#define STEPS 80000u
typedef struct {
    u32 start, size, requested;
} Allocation;
static struct {
    u8 image[IMAGE_MAX] __attribute__((aligned(4096)));
    u8 heap[HEAP_SIZE] __attribute__((aligned(4096)));
    u8 stackmem[STACK_SIZE] __attribute__((aligned(4096)));
    u8 perm[IMAGE_MAX / 4096], docbuf[16384];
#ifdef __x86_64__
    u32 guard_targets[4096]; /* Not mapped into guest page tables. */
#endif
    Allocation allocs[128];
    char docname[32], arg0[128], argline[160];
    u64 brk_base, brk_current, brk_limit;
    int doclen, docpos, writing, dialogs, opens;
    int (*document_read)(const char *, void *, u32);
    void (*show_box)(const char *);
} residents[PROCESS_BANKS];
#define CURRENT residents[process_bank]
typedef struct {
    u64 r[16], ip, base;
    u32 size, out, budget, allocated, last_error;
    u64 phdr, cmdline;
#ifdef __x86_64__
    u64 teb, callbacks[16];
    unsigned ncallbacks, nguard, guard_slot, cookie_slot, registry_writes;
#endif
    u32 phnum, phent;
    int native;
    unsigned launches, handles;
    u32 child_exit, process_handle, thread_handle;
    int linuxabi, wide, cf, zf, sf, of, pf, done;
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
#ifdef __x86_64__
    if (v->native && a >= REGION_BASE && a - REGION_BASE < REGION_BYTES) {
        u8 *p = region_ptr(a, n, access);
        if (!p)
            goto bad;
        return p;
    }
#endif
    if (a >= STACK && a - STACK <= STACK_SIZE && n <= STACK_SIZE - (u32)(a - STACK) &&
        access != X)
        return CURRENT.stackmem + (u32)(a - STACK);
    if (v->linuxabi && access != X && a >= CURRENT.brk_base && a <= CURRENT.brk_current &&
        n <= CURRENT.brk_current - a && a - CURRENT.brk_base <= HEAP_SIZE &&
        n <= HEAP_SIZE - (u32)(a - CURRENT.brk_base))
        return CURRENT.heap + (u32)(a - CURRENT.brk_base);
    if (a >= HEAP && a - HEAP < HEAP_SIZE && access != X) {
        u32 pos = (u32)(a - HEAP);
        for (u32 i = 0; i < v->allocated; i++) {
            Allocation al = CURRENT.allocs[i];
            if (pos >= al.start && span(al.size, pos - al.start, n))
                return CURRENT.heap + pos;
        }
        goto bad;
    }
    if (a >= v->base && a - v->base < v->size) {
        u32 p = (u32)(a - v->base);
        if (span(v->size, p, n)) {
            u32 end = (p + n - 1) >> 12;
            for (u32 i = p >> 12; i <= end; i++)
                if (!(CURRENT.perm[i] & access))
                    goto bad;
            return CURRENT.image + p;
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
    CRT_EXIT,
    VER_A,
    VER_W,
    TID,
    GETVERSION,
    METRICS,
    OPEN_A,
    READ_FILE,
    CLOSE_HANDLE,
    PRODUCT,
    VALLOC,
    VFREE,
    VPROTECT,
    CMDLINE_W,
    CONMODE,
    LOCALE_W,
    SYSTIME,
    FILE_TYPE,
    OPEN_W,
    ARGV_W,
    LOCAL_FREE,
    MEMMOVE,
    VQUERY,
    HEAP_QUERY,
    HEAP_REALLOC,
    CRT_REALLOC,
    CREATE_PROCESS_W,
    WAIT_SINGLE,
    EXIT_CODE_PROCESS,
    FILE_SIZE,
    FILE_SYNC,
    FILE_SEEK,
    CREATE_DIR,
    DELETE_FILE,
    REG_OPEN,
    REG_CREATE,
    REG_QUERY,
    REG_SET,
    REG_DELETE_VALUE,
    REG_CLOSE,
    ACL_INIT,
    ACL_ADD,
    ACL_GET,
    ACL_INFO,
    ACL_DELETE,
    SID_INIT,
    SID_VALID,
    SID_REQUIRED,
    SID_LENGTH,
    SID_SUB,
    SID_COUNT,
    SID_COPY,
    SID_EQUAL,
    HEAP_CREATE,
    HEAP_DESTROY,
    CS_INIT,
    CS_INIT_SPIN,
    CS_INIT_EX,
    CS_ENTER,
    CS_TRY,
    CS_LEAVE,
    CS_DELETE
};
static const struct {
    const char *dll, *name;
    int argc;
} imports[] = {
#define API(d, n, c) {d, n, c},
#include "imports.def"
#undef API
};
#define NIMPORT ((int)(sizeof imports / sizeof *imports))
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
        if (CURRENT.allocs[i].start - start >= n)
            break;
        start = CURRENT.allocs[i].start + CURRENT.allocs[i].size;
    }
    if (n > HEAP_SIZE - start)
        return 0;
    if (zeroed && !charge(v, (n + 15) / 16))
        return 0;
    for (u32 j = v->allocated; j > i; j--)
        CURRENT.allocs[j] = CURRENT.allocs[j - 1];
    CURRENT.allocs[i] = (Allocation){start, n, (u32)size};
    v->allocated++;
    if (zeroed)
        clear(CURRENT.heap + start, n);
    return HEAP + start;
}
static int release(VM *v, u64 ptr) {
    for (u32 i = 0; i < v->allocated; i++)
        if (ptr == HEAP + CURRENT.allocs[i].start) {
            v->allocated--;
            for (; i < v->allocated; i++)
                CURRENT.allocs[i] = CURRENT.allocs[i + 1];
            return 1;
        }
    return 0;
}
static int allocation_index(VM *v, u64 ptr) {
    for (u32 i = 0; i < v->allocated; i++)
        if (ptr == HEAP + CURRENT.allocs[i].start)
            return (int)i;
    return -1;
}
/* On failure leave the original block intact. Keep padding separate from the
 * requested length so HEAP_ZERO_MEMORY also covers growth within a size class. */
static u64 resize_heap(VM *v, u64 ptr, u64 size, unsigned flags) {
    int i = allocation_index(v, ptr);
    if (i < 0 || size > HEAP_SIZE)
        return 0;
    Allocation old = CURRENT.allocs[i];
    u32 n = ((u32)size + 15) & ~15u;
    if (!n)
        n = 16;
    u32 end = (u32)(i + 1) < v->allocated ? CURRENT.allocs[i + 1].start : HEAP_SIZE;
    if (n <= end - old.start) {
        if ((flags & 8) && size > old.requested) {
            if (!charge(v, 1 + (size - old.requested) / 16))
                return 0;
            clear(CURRENT.heap + old.start + old.requested, size - old.requested);
        }
        CURRENT.allocs[i].size = n;
        CURRENT.allocs[i].requested = size;
        return ptr;
    }
    if ((flags & 16) || !charge(v, 1 + old.requested / 16))
        return 0;
    u32 next = allocate(v, size, flags & 8);
    if (next) {
        bytes(CURRENT.heap + (next - HEAP), CURRENT.heap + old.start, old.requested);
        release(v, ptr);
    }
    return next;
}
void guest_services(int (*read)(const char *, void *, u32), void (*box)(const char *)) {
    CURRENT.document_read = read;
    CURRENT.show_box = box;
}
static int (*save_document)(const char *, const void *, u32);
void guest_save_service(int (*fn)(const char *, const void *, u32)) { save_document = fn; }
static int text_name(const char *s) {
    int n = 0;
    for (; s[n]; n++)
        if (!((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= '0' && s[n] <= '9') || s[n] == '.' ||
              s[n] == '_' || s[n] == '-'))
            return 0;
    return n > 0 && s[0] != '.';
}
static u64 linux_brk(VM *v, u64 requested) {
    u64 current = CURRENT.brk_current;
    if (!current || !requested)
        return current;
    if (requested < CURRENT.brk_base || requested > CURRENT.brk_limit ||
        requested >= (1ull << (v->wide ? 47 : 32)))
        return current;
    if (!v->native) {
        u32 old_off = (u32)(current - CURRENT.brk_base);
        u32 new_off = (u32)(requested - CURRENT.brk_base);
        if (new_off > old_off)
            clear(CURRENT.heap + old_off, new_off - old_off);
        else if (new_off < old_off)
            clear(CURRENT.heap + new_off, old_off - new_off);
        CURRENT.brk_current = requested;
        return requested;
    }
#ifdef __x86_64__
    u64 old_end = (current + 4095) & ~4095ull;
    u64 new_end = (requested + 4095) & ~4095ull;
    if (new_end > old_end && !region_commit(old_end, new_end - old_end, R | WRT))
        return current;
    if (new_end < old_end && !region_free(new_end, old_end - new_end, 2))
        return current;
    CURRENT.brk_current = requested;
    return requested;
#else
    return current;
#endif
}

static u32 screen_w = 1024, screen_h = 768;
void compat_display(u32 w, u32 h) {
    screen_w = w;
    screen_h = h;
}
void compat_args(const char *name, const char *args) {
    strset(CURRENT.arg0, sizeof CURRENT.arg0, name);
    strset(CURRENT.argline, sizeof CURRENT.argline, args);
}
static u16 systime[8];
void compat_clock(u16 y, u16 m, u16 dow, u16 day, u16 h, u16 min, u16 sec) {
    systime[0] = y;
    systime[1] = m;
    systime[2] = dow;
    systime[3] = day;
    systime[4] = h;
    systime[5] = min;
    systime[6] = sec;
}
/* Shell argv subset: plain words and quoted spans. Reject ambiguous backslash/quote
 * runs rather than silently handing the application a different file name. Return
 * the pointer array and UTF-16 strings in a single LocalFree-able allocation. */
static u64 shell_argv(VM *v, u64 cmd, u64 count) {
    u16 buf[256];
    unsigned offsets[32], used = 0, argc = 0, pos = 0;
    int quote = 0, word = 0;
    if (!count || !cmd)
        goto invalid;
    for (; pos < 254; pos++) {
        u16 c = readv(v, cmd + 2 * pos, 2, R);
        if (v->err)
            return 0;
        if (!c)
            break;
        if (!quote && (c == ' ' || c == '\t')) {
            if (!pos)
                offsets[argc++] = used;
            if (word || !pos)
                buf[used++] = 0;
            word = 0;
        } else {
            if (!word) {
                if (argc == 32)
                    goto invalid;
                offsets[argc++] = used;
                word = 1;
            }
            if (c == '"') {
                if ((used && buf[used - 1] == '\\') ||
                    readv(v, cmd + 2 * (pos + 1), 2, R) == '"')
                    goto invalid;
                quote = !quote;
            } else
                buf[used++] = c;
        }
    }
    if (pos == 254 || quote || !pos)
        goto invalid;
    if (word)
        buf[used++] = 0;
    unsigned p = ptrsz(v), head = (argc + 1) * p;
    u64 block = allocate(v, head + 2 * used, 1);
    if (!block) {
        v->last_error = 8;
        return 0;
    }
    for (unsigned i = 0; i < argc; i++)
        writev(v, block + i * p, p, block + head + 2 * offsets[i]);
    for (unsigned i = 0; i < used; i++)
        writev(v, block + head + 2 * i, 2, buf[i]);
    writev(v, count, 4, argc);
    return block;
invalid:
    v->last_error = 87;
    return 0;
}
static int win_protection(int flags) {
    return flags < 0     ? 0
           : flags & X   ? ((flags & R) ? 0x20 : 0x10)
           : flags & WRT ? 4
           : flags & R   ? 2
                         : 1;
}
/* Describe only guest virtual addresses. Never traverse host/kernel tables. */
static u64 virtual_query(VM *v, u64 address, u64 output, u64 capacity) {
    unsigned p = ptrsz(v), n = v->wide ? 48 : 28, sizeoff = v->wide ? 24 : 12;
    u64 page = address & ~4095ull, limit = 1ull << (v->wide ? 47 : 32);
    if (address >= limit || capacity < n) {
        v->last_error = 87;
        return 0;
    }
    u64 bytes = limit - page, base = 0;
    int orig = -1, cur = -1, image_type = 0;
    struct {
        u64 base, size;
        int flags;
    } ranges[] = {
        {v->base, v->size, R},
        {STACK, STACK_SIZE, R | WRT},
        {HEAP, HEAP_SIZE, R | WRT},
        {ABI - 4096, 8192, R | X},
#ifdef __x86_64__
        {REGION_BASE, v->native ? REGION_BYTES : 0, 0},
#endif
    };
    for (unsigned i = 0; i < sizeof ranges / sizeof *ranges; i++) {
        u64 lo = ranges[i].base, hi = lo + ranges[i].size;
        if (!ranges[i].size)
            continue;
        if (page < lo) {
            if (lo - page < bytes)
                bytes = lo - page;
            continue;
        }
        if (page >= hi)
            continue;
        base = lo;
        bytes = hi - page;
        orig = cur = ranges[i].flags;
        if (!i) {
            unsigned idx = (page - lo) / 4096, end = idx + 1;
            orig = R | X;
            cur = CURRENT.perm[idx];
            image_type = 1;
            while (end * 4096 < v->size && CURRENT.perm[end] == cur)
                end++;
            bytes = (end - idx) * 4096;
        }
#ifdef __x86_64__
        if (i == 4)
            region_query(page, &base, &bytes, &cur, &orig);
#endif
        break;
    }
    u8 *out = mem(v, output, n, WRT);
    if (!out)
        return 0;
    clear(out, n);
    put(out, p, page);
    put(out + p, p, base);
    put(out + 2 * p, 4, win_protection(orig));
    put(out + sizeoff, p, bytes);
    put(out + sizeoff + p, 4, !base ? 0x10000 : cur < 0 ? 0x2000 : 0x1000);
    put(out + sizeoff + p + 4, 4, win_protection(cur));
    put(out + sizeoff + p + 8, 4, !base ? 0 : image_type ? 0x1000000 : 0x20000);
    return n;
}
#include "acl.inc"
#include "privateheaps.inc"
#include "children.inc"
#include "critical.inc"
#include "diskfiles.inc"
#include "registry.inc"
static void api(VM *v, int k) {
    int n = imports[k].argc + (!v->wide && k == FILE_SEEK);
    u64 a[10] = {0};
    static const int regs[] = {1, 2, 8, 9};
    for (int i = 0; i < n; i++)
        a[i] = v->wide && i < 4 ? v->r[regs[i]]
                                : readv(v, v->r[4] + ptrsz(v) * (i + 1), ptrsz(v), R);
    if (v->err)
        return;
    /* Win64 DWORD parameters occupy 8-byte argument slots, but their upper
     * halves are unspecified, especially when the compiler reuses stack slots. */
    if (k == OPEN_A || k == OPEN_W) {
        a[1] = (u32)a[1];
        a[2] = (u32)a[2];
        a[4] = (u32)a[4];
        a[5] = (u32)a[5];
    }
    if (k == CREATE_PROCESS_W) {
        a[4] = (u32)a[4];
        a[5] = (u32)a[5];
    }
    if (k == HEAP_ALLOC || k == HEAP_QUERY || k == HEAP_REALLOC || k == HEAP_FREE)
        a[1] = (u32)a[1]; /* dwFlags, not SIZE_T */
    if (k == READ_FILE || k == WRITE_FILE)
        a[2] = (u32)a[2];
    u64 result = 0;
#ifdef __x86_64__
    if (k == HEAP_CREATE || k == HEAP_DESTROY ||
        (a[0] != 3 &&
         (k == HEAP_ALLOC || k == HEAP_QUERY || k == HEAP_FREE || k == HEAP_REALLOC))) {
        result = private_api(v, k, a);
        goto complete;
    }
    if (k >= REG_OPEN && k <= REG_CLOSE) {
        result = reg_api(v, k, a);
        goto complete;
    }
    if (diskfile_api(v, k, a, &result))
        goto complete;
#endif
    if (k >= REG_OPEN && k <= REG_CLOSE)
        result = 50; /* interpreter has no mounted hive */
    if (k >= CS_INIT && k <= CS_DELETE)
        result = critical_api(v, k, a);
    if (k >= ACL_INIT && k <= ACL_DELETE)
        result = acl_api(v, k, a);
    if (k >= SID_INIT && k <= SID_EQUAL)
        result = sid_api(v, k, a);
    if (k == HEAP_CREATE || k == HEAP_DESTROY)
        v->last_error = 50; /* BIOS interpreter */
    if (k == FILE_SIZE || k == FILE_SEEK || k == FILE_SYNC)
        v->last_error = 6;
    if (k == CREATE_DIR || k == DELETE_FILE)
        v->last_error = 3;
    if (k == CREATE_PROCESS_W)
        result = create_child(v, a);
    if (k == WAIT_SINGLE || k == EXIT_CODE_PROCESS) {
        int ph = (v->handles & 1) && a[0] == v->process_handle;
        int th = (v->handles & 2) && a[0] == v->thread_handle;
        if (k == WAIT_SINGLE && (ph || th))
            result = 0; /* signaled */
        else if (k == EXIT_CODE_PROCESS && ph && a[1]) {
            writev(v, a[1], 4, v->child_exit);
            result = 1;
        } else {
            result = k == WAIT_SINGLE ? 0xffffffffu : 0;
            v->last_error = 6;
        }
    }
    if (k == EXIT_PROCESS || k == CRT_EXIT) {
        v->done = 1;
        v->res->exit_code = (u32)a[0];
        return;
    }
    if (k >= VALLOC && k <= VPROTECT) {
#ifdef __x86_64__
        int p = a[k == VALLOC ? 3 : 2] == 1      ? 0
                : a[k == VALLOC ? 3 : 2] == 2    ? 1
                : a[k == VALLOC ? 3 : 2] == 4    ? 3
                : a[k == VALLOC ? 3 : 2] == 0x20 ? 5
                                                 : -1;
        if (!v->native) {
            fail(v, "Virtual memory requires x64 user mode");
            return;
        }
        if (inside_heap(a[0], a[1])) {
            v->last_error = 87;
            goto complete;
        }
        if (k == VALLOC && p >= 0) {
            if (a[2] == 0x1000 && a[0])
                result = region_commit(a[0], a[1], p) ? a[0] & ~4095ull : 0;
            else if (a[2] == 0x1000 || a[2] == 0x2000 || a[2] == 0x3000)
                result = region_alloc(a[0], a[1], p, a[2] == 0x2000, 1);
        }
        if (k == VFREE && (a[2] == 0x8000 || a[2] == 0x4000))
            result = region_free(a[0], a[1], a[2] == 0x8000 ? 1 : 2);
        if (k == VPROTECT && p >= 0) {
            u8 *dest = mem(v, a[3], 4, WRT);
            if (!dest)
                return;
            int old = 0;
            if (region_protect(a[0], a[1], p, &old)) {
                put(dest, 4, old == 0 ? 1 : old == 1 ? 2 : old == 3 ? 4 : 0x20);
                result = 1;
            }
        }
        if (!result)
            v->last_error = 87;
#else
        fail(v, "Virtual memory requires x64 user mode");
        return;
#endif
    }
    if (k == VQUERY)
        result = virtual_query(v, a[0], a[1], a[2]);
    if (k == SYSTIME) {
        u8 *dst = mem(v, a[0], 16, WRT);
        if (!dst)
            return;
        bytes(dst, (const u8 *)systime, 16);
    }
    if (k == CONMODE) {
        if (a[0] == 1 || a[0] == 2) {
            writev(v, a[1], 4, 3);
            result = 1;
        } else
            v->last_error = 6;
    }
    if (k == CMDLINE_W) {
        if (!v->cmdline) {
            unsigned n = length(CURRENT.arg0), m = length(CURRENT.argline);
            v->cmdline = allocate(v, 2 * (n + m + 2), 1);
            if (v->cmdline) {
                for (unsigned j = 0; j < n; j++)
                    writev(v, v->cmdline + 2 * j, 2, (u8)CURRENT.arg0[j]);
                if (m) {
                    writev(v, v->cmdline + 2 * n++, 2, ' ');
                    for (unsigned j = 0; j < m; j++)
                        writev(v, v->cmdline + 2 * (n + j), 2, (u8)CURRENT.argline[j]);
                }
            }
        }
        result = v->cmdline;
    }
    if (k == LOCALE_W) {
        static const char *const months[] = {"January",   "February", "March",    "April",
                                             "May",       "June",     "July",     "August",
                                             "September", "October",  "November", "December"};
        static const char *const days[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        const char *val = a[1] >= 56 && a[1] <= 67   ? months[a[1] - 56]
                          : a[1] >= 49 && a[1] <= 55 ? days[a[1] - 49]
                                                     : 0;
        if (!val || (a[0] != 0x400 && a[0] != 0x800 && a[0] != 0x409))
            v->last_error = 87;
        else {
            unsigned n = length(val) + 1;
            if (!a[3])
                result = n;
            else if (a[3] < n)
                v->last_error = 122;
            else {
                u8 *dst = mem(v, a[2], 2 * n, WRT);
                if (!dst)
                    return;
                for (unsigned j = 0; j < n; j++)
                    put(dst + 2 * j, 2, val[j]);
                result = n;
            }
        }
    }
    if (k == TID)
        result = current_pid();
    if (k == PRODUCT && a[4]) {
        writev(v, a[4], 4, 48);
        result = 1;
    }
    if (k == GETVERSION)
        result = 10;
    if (k == METRICS)
        result = a[0] == 0 ? screen_w : a[0] == 1 ? screen_h : 0;
    if (k == VER_A || k == VER_W) {
        u32 size = readv(v, a[0], 4, R);
        u32 norm = k == VER_A ? 148 : 276;
        if (size != norm && size != norm + 8)
            v->last_error = 87;
        else {
            u8 *data = mem(v, a[0], size, WRT);
            if (!data)
                return;
            clear(data + 4, size - 4);
            put(data + 4, 4, 10);
            put(data + 12, 4, 22631);
            put(data + 16, 4, 2);
            if (size == norm + 8)
                data[norm + 6] = 1;
            result = 1;
        }
    }
    if (k == ARGV_W)
        result = shell_argv(v, a[0], a[1]);
    if (k == LOCAL_FREE && a[0] && !release(v, a[0])) {
        v->last_error = 6;
        result = a[0];
    }
    if (k == FILE_TYPE) {
        if (a[0] == 1 || a[0] == 2)
            result = 2; /* FILE_TYPE_CHAR */
        else if (a[0] == 4 && CURRENT.doclen >= 0)
            result = 1; /* FILE_TYPE_DISK */
        else
            v->last_error = 6;
    }
    if (k == OPEN_A || k == OPEN_W) {
        result = addr(v, ~(u64)0);
        v->last_error = 5;
        if (CURRENT.doclen < 0 && CURRENT.opens++ < 16 &&
            ((CURRENT.document_read && a[1] == 0x80000000u && a[4] == 3) ||
             (save_document && a[1] == 0x40000000u && a[4] == 2)) &&
            !a[3] && !a[6]) {
            char file[32];
            for (int i = 0; i < 32; i++) {
                int stride = k == OPEN_W ? 2 : 1;
                unsigned c = readv(v, a[0] + i * stride, stride, R);
                if (c > 127)
                    break; /* Home names are ASCII; never truncate UTF-16. */
                file[i] = c;
                if (v->err)
                    return;
                if (!file[i]) {
                    if (!text_name(file))
                        break;
                    CURRENT.writing = a[1] == 0x40000000u;
                    bytes((u8 *)CURRENT.docname, (const u8 *)file, i + 1);
                    CURRENT.doclen = CURRENT.writing
                                         ? 0
                                         : CURRENT.document_read(file, CURRENT.docbuf,
                                                                 sizeof CURRENT.docbuf);
                    CURRENT.docpos = 0;
                    if (CURRENT.doclen >= 0)
                        result = 4;
                    else
                        v->last_error = 2;
                    break;
                }
            }
        }
    }
    if (k == READ_FILE) {
        if (a[3])
            writev(v, a[3], 4, 0);
        if (a[0] != 4 || CURRENT.writing || CURRENT.doclen < 0 || a[4] || !a[3])
            v->last_error = 6;
        else {
            u32 amt = CURRENT.doclen - CURRENT.docpos;
            if (amt > a[2])
                amt = a[2];
            if (amt) {
                u8 *dst = mem(v, a[1], amt, WRT);
                if (!dst || !charge(v, 1 + amt / 16))
                    return;
                bytes(dst, CURRENT.docbuf + CURRENT.docpos, amt);
            }
            writev(v, a[3], 4, amt);
            CURRENT.docpos += amt;
            result = 1;
        }
    }
    if (k == CLOSE_HANDLE) {
        if ((v->handles & 1) && a[0] == v->process_handle) {
            v->handles &= ~1u;
            result = 1;
        } else if ((v->handles & 2) && a[0] == v->thread_handle) {
            v->handles &= ~2u;
            result = 1;
        } else if (a[0] == 4 && CURRENT.doclen >= 0) {
            result = !CURRENT.writing ||
                     (CURRENT.dialogs++ < 8 &&
                      save_document(CURRENT.docname, CURRENT.docbuf, CURRENT.doclen));
            if (!result)
                v->last_error = 5;
            CURRENT.doclen = -1;
            CURRENT.writing = 0;
        } else
            v->last_error = 6;
    }
    if (k == STD_HANDLE) {
        result = (u32)a[0] == (u32)-11 ? 1 : (u32)a[0] == (u32)-12 ? 2 : 0;
    }
    if (k == WRITE_FILE && a[0] == 4) {
        if (CURRENT.doclen < 0 || !CURRENT.writing || !a[3] || a[4] ||
            a[2] > sizeof CURRENT.docbuf - (u32)CURRENT.doclen)
            v->last_error = 5;
        else {
            u32 sz = a[2];
            if (sz) {
                const u8 *src = mem(v, a[1], sz, R);
                if (!src || !charge(v, 1 + sz / 16))
                    return;
                bytes(CURRENT.docbuf + CURRENT.doclen, src, sz);
            }
            CURRENT.doclen += sz;
            writev(v, a[3], 4, sz);
            result = 1;
        }
    } else if (k >= WRITE_FILE && k <= WRITE_W) {
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
        u32 begin = v->out;
        if (a[2]) {
            string(v, a[2], k == BOX_W, 1);
            emit(v, '\n');
        }
        if (a[1])
            string(v, a[1], k == BOX_W, 1);
        if (!v->err && CURRENT.show_box && CURRENT.dialogs++ >= 8) {
            fail(v, "Dialog budget exceeded");
            return;
        }
        if (!v->err && CURRENT.show_box)
            CURRENT.show_box(v->res->output + begin);
        result = 1;
    }
    if (k == LENGTH_A || k == LENGTH_W || k == STRLEN)
        result = string(v, a[0], k == LENGTH_W, 0);
    if (k == PID)
        result = current_pid();
    if (k == PROCESS_HEAP)
        result = 3;
    if (k == HEAP_ALLOC) {
        if (a[0] != 3 || (a[1] & ~(u64)9)) {
            fail(v, "Unsupported heap flags or handle");
            return;
        }
        result = allocate(v, a[2], a[1] & 8);
    }
    if (k == HEAP_QUERY) {
        int i = allocation_index(v, a[2]);
        result = a[0] == 3 && (a[1] & ~1ull) == 0 && i >= 0 ? CURRENT.allocs[i].requested
                                                            : addr(v, ~0ull);
    }
    if (k == HEAP_REALLOC && a[0] == 3 && (a[1] & ~25ull) == 0)
        result = resize_heap(v, a[2], a[3], a[1]);
    if (k == CRT_REALLOC) {
        if (!a[0])
            result = allocate(v, a[1], 0);
        else if (a[1])
            result = resize_heap(v, a[0], a[1], 0);
        else
            release(v, a[0]);
    }
    if (k == HEAP_FREE) {
        result = a[0] == 3 && (a[1] & ~1ull) == 0 && release(v, a[2]);
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
    if (k == MEMCPY || k == MEMSET || k == MEMCMP || k == MEMMOVE) {
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
                if (k == MEMMOVE && a[0] > a[1]) {
                    dst[count - 1 - i] = src[count - 1 - i];
                } else if (k == MEMCMP) {
                    if (dst[i] != src[i]) {
                        result = (u32)((int)dst[i] - src[i]);
                        break;
                    }
                } else
                    dst[i] = k == MEMSET ? (u8)a[1] : src[i];
            }
        }
    }
#ifdef __x86_64__
complete:
#endif
    v->r[0] = result;
    v->ip = pop(v);
    if (!v->wide && k != MEMMOVE && k != CRT_REALLOC && (k < CRT_START || k > CRT_EXIT))
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
static void linux_call(VM *v, u64 call, u64 fd, u64 addr, u64 sz) {
    if (call == 12)
        v->r[0] = linux_brk(v, fd);
    else if (call == 60 || call == 231) {
        v->done = 1;
        v->res->exit_code = fd & 255;
    } else if (call == 39 || call == 186)
        v->r[0] = 1;
    else if (call == 2 || call == 257) {
        u64 path = call == 2 ? fd : addr, flags = call == 2 ? addr : sz;
        v->r[0] = (u64)-13;
        if (!(flags & ~(u64)(0x8000 | 0x80000)) && CURRENT.doclen < 0 &&
            CURRENT.opens++ < 16 && CURRENT.document_read && (call == 2 || (int)fd == -100)) {
            char name[32];
            for (int i = 0; i < 32; i++) {
                name[i] = readv(v, path + i, 1, R);
                if (v->err)
                    return;
                if (!name[i]) {
                    if (text_name(name)) {
                        CURRENT.doclen = CURRENT.document_read(name, CURRENT.docbuf,
                                                               sizeof CURRENT.docbuf);
                        CURRENT.docpos = 0;
                        v->r[0] = CURRENT.doclen < 0 ? (u64)-2 : 3;
                    }
                    break;
                }
            }
        }
    } else if (call == 0) {
        v->r[0] = (u64)-9;
        if (fd == 3 && CURRENT.doclen >= 0) {
            u32 n = CURRENT.doclen - CURRENT.docpos;
            if (sz < n)
                n = sz;
            if (n) {
                u8 *to = mem(v, addr, n, WRT);
                if (!to || !charge(v, 1 + n / 16))
                    return;
                bytes(to, CURRENT.docbuf + CURRENT.docpos, n);
            }
            CURRENT.docpos += n;
            v->r[0] = n;
        }
    } else if (call == 3) {
        v->r[0] = (u64)-9;
        if (fd == 3 && CURRENT.doclen >= 0) {
            CURRENT.doclen = -1;
            v->r[0] = 0;
        }
    } else if (call == 1 && (fd == 1 || fd == 2)) {
        if (sz > 16384 || !charge(v, (u32)sz + 1)) {
            fail(v, "Linux write exceeds limit");
            return;
        }
        const u8 *p = sz ? mem(v, addr, sz, R) : 0;
        if (v->err)
            return;
        for (u32 i = 0; i < sz; i++)
            emit(v, p[i]);
        v->r[0] = sz;
    } else
        v->r[0] = (u64)-38;
}
static void linux32_call(VM *v) {
    u32 id = v->r[0];
    u32 call = id == 1 || id == 252 ? 60
               : id == 4            ? 1
               : id == 3            ? 0
               : id == 5            ? 2
               : id == 6            ? 3
               : id == 20           ? 39
               : id == 45           ? 12
               : id == 224          ? 186
                                    : ~0u;
    linux_call(v, call, (u32)v->r[3], (u32)v->r[1], (u32)v->r[2]);
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
    if (op == 0xcd && v->linuxabi && !v->wide && fetch(v, 1) == 0x80) {
        linux32_call(v);
        return;
    }
    if (op == 0x0f) {
        op = fetch(v, 1);
        if (op == 5 && v->linuxabi && v->wide) {
            u64 call = v->r[0];
            v->r[1] = v->ip;
            v->r[11] = 2;
            linux_call(v, call, v->r[7], v->r[6], v->r[2]);
            return;
        }
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
        if (op == 0xc6 && group == 0) {
            set(v, o, 1, fetch(v, 1));
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
        if (op == 0x80 || op == 0x81 || op == 0x83) {
            int width = op == 0x80 ? 1 : n;
            int s = op == 0x80 || op == 0x83 ? 1 : width == 2 ? 2 : 4;
            u64 val = sx(fetch(v, s), s);
            u64 z = alu(v, group, operand(v, o, width), val, width);
            if (group != 7)
                set(v, o, width, z);
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
#ifdef __x86_64__
#include "dispatch64.inc"
#endif
static void initial_stack(VM *v) {
    u64 pos = STACK + STACK_SIZE - 1024, argv[32],
        aux[] = {3,  v->phdr, 4,  v->phent, 5,  v->phnum, 6,  4096, 9, v->ip,
                 11, 1000,    12, 1000,     13, 1000,     14, 1000, 0, 0};
    unsigned argc = 0;
    char words[160];
    strset(words, sizeof words, CURRENT.argline);
    char *arg = CURRENT.arg0, *next = words;
    int sz = ptrsz(v);
    while (arg) {
        int len = length(arg) + 1;
        argv[argc++] = pos;
        memcpy(CURRENT.stackmem + (pos - STACK), arg, len);
        pos += len;
        while (*next == ' ')
            next++;
        if (!*next)
            break;
        if (argc == 32) {
            fail(v, "Too many program arguments");
            return;
        }
        arg = next;
        while (*next && *next != ' ')
            next++;
        if (*next)
            *next++ = 0;
    }
    u64 start =
            (STACK + STACK_SIZE - 1024 - (argc + 3 + sizeof aux / sizeof *aux) * sz) & ~15ull,
        sp = start;
    writev(v, sp, sz, argc);
    sp += sz;
    for (unsigned i = 0; i < argc; i++, sp += sz)
        writev(v, sp, sz, argv[i]);
    sp += 2 * sz;
    for (unsigned i = 0; i < sizeof aux / sizeof *aux; i++, sp += sz)
        writev(v, sp, sz, aux[i]);
    v->r[4] = start;
}
static int execute(VM *state) {
    VM v = *state;
    clear((u8 *)criticals[process_bank], sizeof criticals[process_bank]);
    CURRENT.doclen = -1;
    CURRENT.docpos = CURRENT.writing = CURRENT.dialogs = CURRENT.opens = 0;
    CURRENT.brk_base = CURRENT.brk_current = HEAP;
    CURRENT.brk_limit = HEAP + HEAP_SIZE;
    clear(CURRENT.heap, sizeof CURRENT.heap);
    PeResult *r = v.res;
    if (!v.err) {
        v.r[4] = STACK + STACK_SIZE - 64;
        if (!v.linuxabi)
            push(&v, RETURN);
        else {
            initial_stack(&v);
        }
    }
#ifdef __x86_64__
    if (!v.err && native64(&v)) {
        close_diskfiles();
        clear((u8 *)key_handles[process_bank], sizeof key_handles[process_bank]);
        if (!r->detail[0])
            detail(r, v.err ? v.err : "Program exited");
        return v.done && !v.err;
    }
#endif
    while (!v.err && !v.done && v.budget--) {
        r->steps++;
        if (!v.linuxabi && v.ip == RETURN) {
            v.done = 1;
            r->exit_code = (u32)v.r[0];
        } else if (!v.linuxabi && v.ip >= ABI && v.ip < ABI + 16 * NIMPORT && !(v.ip & 15))
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

static u32 imp_crc(const u8 *f, u32 n) {
    u32 crc = ~0u;
    for (u32 i = 0; i < n; i++) {
        crc ^= (i >= 28 && i < 32) ? 0 : f[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
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
    if (!size || size > FILE_LIMIT || entry >= size || !v->base ||
        v->base > 0x7fffffffffffULL - size || (!v->wide && v->base > 0xffffffffu - size) ||
        !(v->base + size <= HEAP || v->base >= HEAP + HEAP_SIZE) ||
        !(v->base + size <= RETURN || v->base >= STACK + STACK_SIZE))
        return "Invalid IMP address layout";
    clear(CURRENT.image, (size + 4095) & ~4095u);
    clear(CURRENT.perm, sizeof CURRENT.perm);
    u32 pos = 32;
    for (u32 off = 0; off < size; off += 4096) {
        if (!span(n, pos, 4))
            return "Truncated IMP page";
        int flags = f[pos];
        u32 used = get(f + pos + 2, 2);
        if (flags > 7 || f[pos + 1] || used > 4096 || used > size - off || (!flags && used) ||
            !span(n, pos + 4, used))
            return "Invalid IMP page";
        CURRENT.perm[off >> 12] = flags;
        bytes(CURRENT.image + off, f + pos + 4, used);
        pos += 4 + used;
    }
    if (pos != n || !(CURRENT.perm[entry >> 12] & X))
        return "Invalid IMP entry or trailing data";
    v->ip = v->base + entry;
    clear(CURRENT.stackmem, sizeof CURRENT.stackmem);
    clear(CURRENT.heap, sizeof CURRENT.heap);
    return 0;
}
const char *imp_check(const u8 *f, u32 n) {
    PeResult r = {0};
    VM v = {.res = &r};
    return imp_load(&v, f, n);
}
int imp_run(const u8 *f, u32 n, PeResult *r) {
    clear((u8 *)r, sizeof *r);
    VM v = {.res = r, .budget = STEPS};
    v.err = imp_load(&v, f, n);
    return execute(&v);
}

#include "elf.inc"

#include "rawpe.inc"
