#include "platform.h"
#include "storage.h"
#include "drivers.h"
static int transport;
#define ATA 0x1f0
#define CD 0x170
#define HOME 32768u
#define CAP_SECTORS 15625000u
#define FIRST (HOME + 2)
#define LIMIT_RECORDS 65536u
static inline u8 rd(int p) {
    u8 v;
    __asm__ volatile("inb %w1,%0" : "=a"(v) : "d"(p));
    return v;
}
static inline void wr(int p, u8 v) { __asm__ volatile("outb %0,%w1" ::"a"(v), "d"(p)); }
static inline u16 word(int p) {
    u16 v;
    __asm__ volatile("inw %w1,%0" : "=a"(v) : "d"(p));
    return v;
}
static inline void put(int p, u16 v) { __asm__ volatile("outw %0,%w1" ::"a"(v), "d"(p)); }
static void delay(int p) {
    for (int i = 0; i < 4; i++)
        rd(p + 0x206);
}
static int wait(int p, int drq) {
    /* A fixed spin count expires prematurely when the host is doing I/O.
       PIT stays enabled; allow five real guest seconds for the required phase. */
    u32 start = ticks;
    do {
        u8 s = rd(p + 7);
        if (!s || s == 255)
            return 0;
        if (s & 128)
            continue;
        if (s & 0x21)
            return 0;
        if (!!(s & 8) == drq)
            return 1;
    } while ((u32)(ticks - start) < 300);
    return 0;
}
static int ident(u32 *capacity) {
    wr(ATA + 6, 0xa0);
    delay(ATA);
    wr(ATA + 7, 0xec);
    if (!wait(ATA, 1))
        return 0;
    u16 id[256];
    for (int i = 0; i < 256; i++)
        id[i] = word(ATA);
    if (!(id[49] & 0x200))
        return 0;
    *capacity = id[60] | ((u32)id[61] << 16);
    return 1;
}
static int sector(u32 lba, void *buf, int write) {
    if (lba >= CAP_SECTORS)
        return 0;
    if (transport == 2)
        return sata_io(lba, buf, write);
    if (transport)
        return vblk_io(lba, buf, write);
    u8 *d = buf;
    wr(ATA + 6, 0xe0 | ((lba >> 24) & 15));
    delay(ATA);
    wr(ATA + 2, 1);
    wr(ATA + 3, lba);
    wr(ATA + 4, lba >> 8);
    wr(ATA + 5, lba >> 16);
    wr(ATA + 7, write ? 0x30 : 0x20);
    if (!wait(ATA, 1))
        return 0;
    for (int i = 0; i < 512; i += 2) {
        if (write)
            put(ATA, d[i] | (d[i + 1] << 8));
        else {
            u16 v = word(ATA);
            d[i] = v;
            d[i + 1] = v >> 8;
        }
    }
    delay(ATA);
    return wait(ATA, 0);
}
static int flush(void) {
    if (transport == 2)
        return sata_io(0, 0, 4);
    if (transport)
        return vblk_io(0, 0, 4);
    wr(ATA + 7, 0xe7);
    delay(ATA);
    return wait(ATA, 0);
}
static int cdread(u32 lba, u8 *buf) {
    wr(CD + 6, 0xa0);
    delay(CD);
    wr(CD + 1, 0);
    wr(CD + 4, 0);
    wr(CD + 5, 8);
    wr(CD + 7, 0xa0);
    if (!wait(CD, 1))
        return 0;
    u8 packet[12] = {0x28, 0, lba >> 24, lba >> 16, lba >> 8, lba, 0, 0, 1};
    for (int j = 0; j < 12; j += 2)
        put(CD, packet[j] | packet[j + 1] << 8);
    delay(CD);
    int done = 0;
    while (done < 2048) {
        if (!wait(CD, 1))
            return 0;
        int n = rd(CD + 4) | (rd(CD + 5) << 8);
        if (!n || (n & 1) || n > 2048 - done)
            return 0;
        for (int i = 0; i < n; i += 2) {
            u16 v = word(CD);
            buf[done++] = v;
            buf[done++] = v >> 8;
        }
        delay(CD);
    }
    return wait(CD, 0);
}
static int same(const void *a, const void *b, int n) {
    const u8 *p = a, *q = b;
    while (n--)
        if (*p++ != *q++)
            return 0;
    return 1;
}
static void zero(void *a, int n) {
    u8 *p = a;
    while (n--)
        *p++ = 0;
}
static u32 sum(u32 h, const void *p, u32 n) {
    const u8 *b = p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}
#define BASIS 2166136261u
struct Super {
    char magic[8];
    u32 seq, tail, last, count, hash;
    u8 pad[484];
};
struct Record {
    char magic[8], name[32];
    u32 size, prev, hash;
    u8 pad[460];
};
_Static_assert(sizeof(struct Super) == 512 && sizeof(struct Record) == 512,
               "disk structures");
static struct Super fs;
static u32 capacity;
static int mounted, marked;
static const char *status = "No writable Fresh Home disk.";
static u32 le(const u8 *x) { return x[0] | x[1] << 8 | x[2] << 16 | ((u32)x[3] << 24); }
static int valid(const struct Super *s) {
    return same(s->magic, "FRSHOME1", 8) && s->hash == sum(BASIS, s, 24) &&
           s->tail >= FIRST && s->tail <= CAP_SECTORS && s->count <= LIMIT_RECORDS &&
           (!s->count ? (s->last == 0 && s->tail == FIRST)
                      : (s->last >= FIRST && s->last < s->tail));
}
static int token(const u8 *s) {
    return same(s, "FRESHOS:INSTALL-TARGET:v1", 25) && le(s + 32) == CAP_SECTORS &&
           s[510] == 0 && s[511] == 0;
}
static int media(const u8 *s) {
    return same(s + 368, "FreshOS live v1", 15) && s[510] == 85 && s[511] == 170 &&
           le(s + 470) == HOME && le(s + 474) == CAP_SECTORS - HOME;
}
void storage_init(void) {
    u8 sec[512];
    struct Super a, b;
    mounted = marked = 0;
    transport = 0;
    if (!ident(&capacity) || capacity < CAP_SECTORS || !sector(0, sec, 0) ||
        !(token(sec) || media(sec))) {
        for (transport = 1; transport <= 2; transport++) {
            capacity = transport == 1 ? vblk_size() : sata_size();
            if (capacity >= CAP_SECTORS && sector(0, sec, 0) && (media(sec) || token(sec)))
                break;
        }
        if (transport > 2) {
            transport = 0;
            return;
        }
    }
    if (token(sec)) {
        marked = 1;
        status = "Prepared install target found.";
        return;
    }
    if (!media(sec))
        return;
    if (!sector(HOME, &a, 0) || !sector(HOME + 1, &b, 0))
        return;
    int av = valid(&a), bv = valid(&b);
    if (!av && !bv)
        return;
    fs = av && (!bv || a.seq >= b.seq) ? a : b;
    mounted = 1;
    status = transport == 2 ? "Fresh Home mounted (AHCI/SATA)."
             : transport    ? "Fresh Home mounted (virtio-blk)."
                            : "Fresh Home mounted (primary IDE).";
}
int home_ready(void) { return mounted; }
const char *disk_status(void) { return status; }
static int valid_name(const char *s) {
    int i = 0;
    while (s[i]) {
        char c = s[i++];
        if (i > 31 || !(c == '-' || c == '.' || c == '_' || (c >= '0' && c <= '9') ||
                        (c >= 'a' && c <= 'z')))
            return 0;
    }
    return i > 0;
}
static int next(u32 pos, struct Record *r) {
    return pos >= FIRST && pos < fs.tail && sector(pos, r, 0) &&
           same(r->magic, "FRSHREC1", 8) && r->name[31] == 0 && valid_name(r->name) &&
           (!r->prev || (r->prev >= FIRST && r->prev < pos)) && r->size <= 64 * 1024 * 1024 &&
           pos + 1 + (r->size + 511) / 512 <= fs.tail;
}
int home_save(const char *name, const void *data, u32 size) {
    if (!mounted || !valid_name(name) || size > 64 * 1024 * 1024 || fs.count >= LIMIT_RECORDS)
        return 0;
    u32 blocks = (size + 511) / 512, end = fs.tail + 1 + blocks;
    if (end > CAP_SECTORS)
        return 0;
    struct Record r = {
        .magic = "FRSHREC1", .size = size, .prev = fs.last, .hash = sum(BASIS, data, size)};
    for (int i = 0; name[i]; i++)
        r.name[i] = name[i];
    if (!sector(fs.tail, &r, 1))
        return 0;
    const u8 *p = data;
    for (u32 i = 0; i < blocks; i++) {
        u8 last[512];
        u32 n = size - i * 512;
        void *src = (void *)(p + i * 512);
        if (n < 512) {
            zero(last, 512);
            for (u32 j = 0; j < n; j++)
                last[j] = p[i * 512 + j];
            src = last;
        }
        if (!sector(fs.tail + 1 + i, src, 1))
            return 0;
    }
    if (!flush())
        return 0;
    struct Super s = fs;
    s.seq++;
    s.last = s.tail;
    s.tail = end;
    s.count++;
    s.hash = sum(BASIS, &s, 24);
    if (!sector(HOME + (s.seq & 1), &s, 1) || !flush())
        return 0;
    fs = s;
    return 1;
}
int home_read(const char *name, void *dest, u32 cap) {
    if (!mounted)
        return -2;
    if (!valid_name(name))
        return -1;
    u32 pos = fs.last;
    struct Record r;
    u8 *p = dest;
    for (u32 i = 0; pos && i < fs.count; i++) {
        if (!next(pos, &r))
            return -1;
        int j = 0;
        while (name[j] && name[j] == r.name[j])
            j++;
        if (!name[j] && !r.name[j]) {
            if (r.size > cap)
                return -1;
            for (u32 k = 0; k < r.size; k += 512) {
                u8 b[512];
                if (!sector(pos + 1 + k / 512, b, 0))
                    return -1;
                for (u32 t = 0; t < 512 && k + t < r.size; t++)
                    p[k + t] = b[t];
            }
            return sum(BASIS, p, r.size) == r.hash ? (int)r.size : -1;
        }
        pos = r.prev;
    }
    return -2;
}
int home_list(void (*emit)(const char *)) {
    if (!mounted)
        return 0;
    u32 pos = fs.last;
    struct Record r;
    for (u32 i = 0; pos && i < fs.count && i < 10; i++) {
        if (!next(pos, &r))
            return 0;
        emit(r.name);
        pos = r.prev;
    }
    return 1;
}
/* Only a disk explicitly stamped by new-target.py is eligible. Copy verified
 * sectors before publishing the bootable MBR. Power loss leaves the token intact. */
static u32 install_lba, total;
static u8 data_cd[2048], mbr[512];
static int copying;
int install_probe(void) {
    if (!marked) {
        status = "Needs a marked IDE, SATA or virtio target.";
        return 0;
    }
    if (!cdread(0, data_cd) || !media(data_cd)) {
        status = "FreshOS CD must be secondary IDE master.";
        return 0;
    }
    for (int i = 0; i < 512; i++)
        mbr[i] = data_cd[i];
    if (!cdread(16, data_cd) || !same(data_cd + 1, "CD001", 5)) {
        status = "Cannot read ISO descriptor.";
        return 0;
    }
    total = le(data_cd + 80);
    if (total < 24 || total > HOME / 4) {
        status = "Invalid installation extent.";
        return 0;
    }
    status = "Ready: prepared disk only. No existing OS is eligible.";
    return 1;
}
int install_begin(void) {
    u8 b[512];
    if (!install_probe() || !sector(0, b, 0) || !token(b))
        return 0;
    install_lba = 0;
    copying = 1;
    return 1;
}
int install_percent(void) { return total ? install_lba * 100 / total : 0; }
int install_step(void) {
    if (!copying)
        return -1;
    u8 check[512];
    for (int n = 0; n < 16 && install_lba < total; n++, install_lba++) {
        if (!cdread(install_lba, data_cd)) {
            serial("install: optical read failed\r\n");
            goto bad;
        }
        for (int j = 0; j < 4; j++) {
            u32 l = install_lba * 4 + j;
            if (!l)
                continue;
            u8 *s = data_cd + 512 * j;
            if (!sector(l, s, 1)) {
                serial("install: target write failed\r\n");
                goto bad;
            }
            if (!sector(l, check, 0)) {
                serial("install: target read failed\r\n");
                goto bad;
            }
            if (!same(check, s, 512)) {
                serial("install: readback mismatch\r\n");
                goto bad;
            }
        }
    }
    if (install_lba < total)
        return 0;
    struct Super a = {.magic = "FRSHOME1", .seq = 0, .tail = FIRST};
    a.hash = sum(BASIS, &a, 24);
    if (!sector(HOME, &a, 1) || !sector(HOME + 1, &a, 1) || !flush() || !sector(0, mbr, 1) ||
        !flush())
        goto bad;
    copying = 0;
    storage_init();
    status = "Installed. Eject the CD and restart to boot the disk.";
    return 1;
bad:
    copying = 0;
    status = "Install I/O error. Installation did not complete.";
    return -1;
}
