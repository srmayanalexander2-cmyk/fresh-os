/* Private Win32 heaps have separate reservation ownership. An allocation in
 * one cannot be freed/resized by supplying another heap handle. */
#ifdef __x86_64__
struct AppHeap {
    u64 base;
    u32 handle, limit, committed, count;
    Allocation blocks[64]; /* ordered by address */
};
static struct AppHeap app_heaps[PROCESS_BANKS][4];
static u32 heap_serial = 0x50000000u;
static int inside_heap(u64 at, u64 bytes) {
    if (!at)
        return 0;
    for (unsigned i = 0; i < 4; i++) {
        struct AppHeap *h = &app_heaps[process_bank][i];
        if (h->handle && at < h->base + h->limit && (at >= h->base || bytes > h->base - at))
            return 1;
    }
    return 0;
}
static int extend_heap(VM *v, struct AppHeap *h, unsigned end) {
    unsigned n = (end + 4095) & ~4095u;
    if (n <= h->committed)
        return 1;
    unsigned delta = n - h->committed;
    if (!charge(v, 1 + delta / 64) || !region_commit(h->base + h->committed, delta, 3))
        return 0;
    h->committed = n;
    return 1;
}
static void discard_block(struct AppHeap *h, unsigned at) {
    h->count--;
    for (unsigned i = at; i < h->count; i++)
        h->blocks[i] = h->blocks[i + 1];
}
static u64 private_allocate(VM *v, struct AppHeap *h, u32 size, int zero) {
    if (h->count == 64 || size > h->limit)
        return 0;
    u32 n = size ? (size + 15) & ~15u : 16, start = 0, i = 0;
    while (i < h->count && n > h->blocks[i].start - start) {
        start = h->blocks[i].start + h->blocks[i].size;
        i++;
    }
    if (n > h->limit - start || !extend_heap(v, h, start + n))
        return 0;
    u64 ptr = h->base + start;
    if (zero) {
        u8 *dst = mem(v, ptr, n, WRT);
        if (!dst || !charge(v, 1 + n / 16))
            return 0;
        clear(dst, n);
    }
    for (unsigned j = h->count; j > i; j--)
        h->blocks[j] = h->blocks[j - 1];
    h->blocks[i] = (Allocation){start, n, size};
    h->count++;
    return ptr;
}
static u64 private_api(VM *v, int k, const u64 *a) {
    if (!v->native)
        return security_error(v, 50);
    if (k == HEAP_CREATE) {
        if (((u32)a[0] & ~1u) || a[1] > REGION_BYTES || a[2] > REGION_BYTES ||
            (a[2] && a[1] > a[2]))
            return security_error(v, 87);
        u32 cap = a[2] ? ((u32)a[2] + 4095) & ~4095u : REGION_BYTES / 4;
        u32 initial = a[1] ? ((u32)a[1] + 4095) & ~4095u : 4096;
        if (!a[2] && cap < initial)
            cap = initial;
        for (unsigned i = 0; i < 4; i++) {
            struct AppHeap *h = &app_heaps[process_bank][i];
            if (h->handle)
                continue;
            u64 base = region_alloc(0, cap, 3, 1, 1);
            if (!base)
                break;
            h->base = base;
            h->limit = cap;
            h->committed = h->count = 0;
            if (!extend_heap(v, h, initial)) {
                region_free(base, 0, 1);
                break;
            }
            h->handle = ++heap_serial;
            return h->handle;
        }
        return security_error(v, 8);
    }
    struct AppHeap *h = 0;
    for (unsigned i = 0; i < 4; i++)
        if (a[0] && app_heaps[process_bank][i].handle == a[0])
            h = &app_heaps[process_bank][i];
    u64 invalid = k == HEAP_QUERY ? addr(v, ~0ull) : 0;
    if (!h) {
        v->last_error = 6;
        return invalid;
    }
    if (k == HEAP_DESTROY) {
        if (!region_free(h->base, 0, 1))
            return security_error(v, 87);
        h->handle = 0;
        return 1;
    }
    u32 flags = (u32)a[1], allowed = k == HEAP_ALLOC ? 9 : k == HEAP_REALLOC ? 25 : 1;
    if (flags & ~allowed) {
        v->last_error = 87;
        return invalid;
    }
    if (k == HEAP_ALLOC)
        return a[2] <= h->limit ? private_allocate(v, h, a[2], flags & 8) : 0;
    unsigned i = 0;
    while (i < h->count && h->base + h->blocks[i].start != a[2])
        i++;
    if (i == h->count) {
        v->last_error = 87;
        return invalid;
    }
    Allocation b = h->blocks[i];
    if (k == HEAP_QUERY)
        return b.requested;
    if (k == HEAP_FREE) {
        discard_block(h, i);
        return 1;
    }
    if (a[3] > h->limit)
        return 0;
    u32 want = a[3], n = want ? (want + 15) & ~15u : 16;
    u32 end = i + 1 == h->count ? h->limit : h->blocks[i + 1].start;
    if (n <= end - b.start) {
        if (!extend_heap(v, h, b.start + n))
            return 0;
        if ((flags & 8) && want > b.requested) {
            u32 diff = want - b.requested;
            u8 *p = mem(v, a[2] + b.requested, diff, WRT);
            if (!p || !charge(v, 1 + diff / 16))
                return 0;
            clear(p, diff);
        }
        h->blocks[i].size = n;
        h->blocks[i].requested = want;
        return a[2];
    }
    if (flags & 16)
        return 0;
    if (!charge(v, 1 + b.requested / 16))
        return 0;
    u64 ptr = private_allocate(v, h, want, flags & 8);
    if (!ptr)
        return 0;
    if (b.requested) {
        u8 *dst = mem(v, ptr, b.requested, WRT), *src = mem(v, a[2], b.requested, R);
        if (!dst || !src)
            return 0;
        bytes(dst, src, b.requested);
    }
    if (ptr < a[2])
        i++; /* allocation inserted before the old block */
    discard_block(h, i);
    return ptr;
}
#endif
