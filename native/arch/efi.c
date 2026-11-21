/* Minimal UEFI x64 application ABI. No boot services after ownership transfer. */
#include "base.h"
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
    void *rest[19];
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
    for (int tries = 0; tries < 3; tries++) {
        ptr sz = sizeof memory_map, key, step;
        u32 ver;
        if (b->map(&sz, memory_map, &key, &step, &ver))
            return 5;
        if (!b->exit(image, key))
            enter64(&display);
    }
    return 1;
}
