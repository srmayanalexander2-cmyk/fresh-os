/* Minimal x64 UEFI entry. Firmware protocols end at ExitBootServices;
 * the desktop then runs on the same native drivers as the BIOS edition. */
#include "platform.h"
typedef unsigned long long N;
#define FW __attribute__((ms_abi))
typedef struct {
    u32 a;
    u16 b, c;
    u8 d[8];
} Guid;
typedef struct {
    u32 version, width, height, format, mask[4], stride;
} ModeInfo;
typedef struct {
    u32 max, mode;
    ModeInfo *info;
    N info_size, base, fb_size;
} Mode;
typedef struct GOP {
    N(FW *query)(struct GOP *, u32, N *, ModeInfo **);
    N(FW *set)(struct GOP *, u32);
    void *blt;
    Mode *mode;
} GOP;
typedef struct {
    u8 header[24];
    void *raise_tpl, *restore_tpl, *alloc_pages, *free_pages;
    N(FW *map)(N *, void *, N *, N *, u32 *);
    N(FW *alloc)(u32, N, void **);
    N(FW *free)(void *);
    void *through_unload[19];
    N(FW *exit_boot)(void *, N);
    void *through_locate_buffer[10];
    N(FW *locate)(Guid *, void *, void **);
} Boot;
/* Guard the ABI offsets against accidental structure edits. */
_Static_assert(__builtin_offsetof(Boot, exit_boot) == 232, "ExitBootServices offset");
_Static_assert(__builtin_offsetof(Boot, locate) == 320, "LocateProtocol offset");
typedef struct {
    void *reset;
    N(FW *print)(void *, const unsigned short *);
} Console;
typedef struct {
    u8 hdr[24];
    void *vendor;
    u32 rev, pad;
    void *in_handle, *input, *out_handle;
    Console *out;
    void *err_handle, *err, *runtime;
    Boot *boot;
} System;
extern int sw, sh, pitch, red_first;
extern volatile u32 *fb;
extern void native64(void);
static Guid gop_guid = {
    0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
N FW efi_main(void *image, System *st) {
    Boot *b = st->boot;
    GOP *g = 0;
    N bad = 0x8000000000000003ULL;
    if (b->locate(&gop_guid, 0, (void **)&g))
        goto fail;
    int best = -1, score = -100000;
    for (u32 i = 0; i < g->mode->max; i++) {
        N n = 0;
        ModeInfo *m = 0;
        if (g->query(g, i, &n, &m))
            continue;
        if (m->width >= 640 && m->width <= 1920 && m->height >= 480 && m->height <= 1200 &&
            m->format <= 1) {
            int d = (int)m->width * 9 - (int)m->height * 16;
            if (d < 0)
                d = -d;
            int s = (int)m->width - 4 * d;
            if (m->width > 1280)
                s -= 2000;
            if (s > score) {
                score = s;
                best = i;
            }
        }
        b->free(m);
    }
    if (best < 0 || ((u32)best != g->mode->mode && g->set(g, best)))
        goto fail;
    ModeInfo *m = g->mode->info;
    sw = m->width;
    sh = m->height;
    pitch = m->stride;
    red_first = m->format == 0;
    fb = (volatile u32 *)g->mode->base;
    N size = 0, key = 0, desc = 0;
    u32 version = 0;
    void *map = 0;
    b->map(&size, 0, &key, &desc, &version);
    if (!desc || !size)
        goto fail;
    N cap = size + 64 * desc;
    if (b->alloc(2, cap, &map))
        goto fail;
    for (int attempt = 0; attempt < 3; attempt++) {
        size = cap;
        if (b->map(&size, map, &key, &desc, &version))
            break;
        if (!b->exit_boot(image, key)) {
            native64();
            __builtin_unreachable();
        }
    }
    /* No firmware console calls following an unsuccessful ExitBootServices. */
    return bad;
fail:
    st->out->print(st->out, u"FreshOS needs a 640x480 to 1920x1200 RGB/BGR GOP mode.\r\n");
    return bad;
}
