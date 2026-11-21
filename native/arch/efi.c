/* Minimal UEFI x64 application ABI. No boot services after ownership transfer. */
#include "base.h"
#include "task64.h"
#define FW __attribute__((ms_abi))
typedef unsigned long status;
typedef struct {
    u32 a;
    u16 b, c;
    u8 d[8];
} Guid;
struct Info {
    u32 version, w, h, format, mask[4], stride;
};
struct Mode {
    u32 count, id;
    struct Info *info;
    ptr info_size;
    uint64_t base;
    ptr bytes;
};
struct Gop {
    status(FW *query)(struct Gop *, u32, ptr *, struct Info **);
    status(FW *set)(struct Gop *, u32);
    void *blt;
    struct Mode *mode;
};
struct Boot {
    u8 hdr[24];
    void *tpl[2];
    void *pages[2];
    status(FW *map)(ptr *, void *, ptr *, ptr *, u32 *);
    void *alloc;
    status(FW *free)(void *);
    void *rest1[9];
    status(FW *handle)(void *, Guid *, void **);
    void *rest2[9];
    status(FW *exit)(void *, ptr);
    void *more[10];
    status(FW *locate)(Guid *, void *, void **);
};
_Static_assert(offsetof(struct Boot, exit) == 24 + 26 * 8, "exit ABI");
_Static_assert(offsetof(struct Boot, locate) == 24 + 37 * 8, "GOP ABI");
struct System {
    u8 pre[96];
    struct Boot *bs;
};
static u8 memory_map[96 * 1024] __attribute__((aligned(16)));
static struct Display display;
static ptr task_base;
static uint64_t task_bytes;
void task64_boot(void) { task64_init(task_base, task_bytes); }
extern void enter64(struct Display *) __attribute__((noreturn));
status FW efi_main(void *image, struct System *system) {
    struct Boot *b = system->bs;
    struct Gop *g = 0;
    Guid id = {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
    if (b->locate(&id, 0, (void **)&g) || !g)
        return 3;
    u32 best = 0, chosen = g->mode->id;
    for (u32 i = 0; i < g->mode->count; i++) {
        struct Info *m = 0;
        ptr size = 0;
        if (g->query(g, i, &size, &m) || !m)
            continue;
        if (m->format < 2 && m->w >= 640 && m->h >= 480 && m->w <= TARGET_W &&
            m->h <= TARGET_H && m->w * m->h > best) {
            best = m->w * m->h;
            chosen = i;
        }
        b->free(m);
    }
    if (!best || (g->mode->id != chosen && g->set(g, chosen)))
        return 3;
    struct Info *m = g->mode->info;
    if (g->mode->bytes < (uint64_t)m->stride * m->h * 4)
        return 3;
    display = (struct Display){
        g->mode->base, m->w, m->h, m->stride, m->format ? 16 : 0, 8, m->format ? 0 : 16};
    Guid loaded_id = {
        0x5b1b31a1, 0x9562, 0x11d2, {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};
    struct Loaded {
        u32 revision, pad;
        void *parent, *system, *device, *path, *reserved;
        u32 opt_size, pad2;
        void *options, *base;
        uint64_t size;
    } *loaded = 0;
    _Static_assert(offsetof(struct Loaded, base) == 64, "loaded image ABI");
    ptr base = 0;
    uint64_t bytes = 0;
    if (!b->handle(image, &loaded_id, (void **)&loaded) && loaded) {
        base = (ptr)loaded->base;
        bytes = loaded->size;
    }
    for (int tries = 0; tries < 3; tries++) {
        ptr sz = sizeof memory_map, key, step;
        u32 ver;
        if (b->map(&sz, memory_map, &key, &step, &ver))
            return 5;
        if (!b->exit(image, key)) {
            task_base = base;
            task_bytes = bytes;
            enter64(&display);
        }
    }
    return 1;
}
