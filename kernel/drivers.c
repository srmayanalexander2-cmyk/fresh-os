/* In-box PCI drivers. No untrusted kernel modules, physical disk autoformat,
 * framebuffer probes at arbitrary addresses, or invented fallback bindings. */
#include "drivers.h"
#define IO(bits, T, s)                                                                       \
    static inline T i##bits(u16 p) {                                                         \
        T v;                                                                                 \
        __asm__ volatile("in" s " %w1,%0" : "=a"(v) : "d"(p));                               \
        return v;                                                                            \
    }                                                                                        \
    static inline void o##bits(u16 p, T v) {                                                 \
        __asm__ volatile("out" s " %0,%w1" ::"a"(v), "d"(p));                                \
    }
IO(8, u8, "b") IO(16, u16, "w") IO(32, u32, "l") static u32 cfg(u32 d, int off) {
    o32(0xcf8, d + (off & 252));
    return i32(0xcfc);
}
static void enable(u32 d) {
    u16 cmd = cfg(d, 4);
    o16(0xcfc, cmd | 5);
}
static u16 port(u32 d, int off, int span) {
    u32 v = cfg(d, off);
    return (v & 1) && v > 3 && v < 65536u - (u32)span ? v & ~3 : 0;
}
static int dma32(const void *p, unsigned size) {
    return (unsigned long)p <= 0xffffffffu - size;
}
static u16 mixer, bus, blk, qn, submitted;
static u32 sectors;
static struct {
    u32 addr;
    u16 len, flags;
} bd[32] __attribute__((aligned(16)));
static short pcm[48000 * 2 * 3] __attribute__((aligned(16)));
static void ac97(u32 d) {
    u16 m = port(d, 16, 256), b = port(d, 20, 64);
    if (!m || !b || !dma32(bd, sizeof bd) || !dma32(pcm, sizeof pcm))
        return;
    enable(d);
    o32(b + 0x2c, i32(b + 0x2c) | 2);
    u32 begin = ticks;
    while (!(i32(b + 0x30) & 256))
        if (ticks - begin > 60)
            return;
    mixer = m;
    bus = b;
    o16(m + 2, 0);
    o16(m + 0x18, 0);
}
const char *audio_name(void) {
    return bus ? "Intel AC97 / 48 kHz stereo PCM" : "PIT / PC speaker";
}
static int pcm_reset(void) {
    if (!bus)
        return 0;
    o8(bus + 0x1b, 0);
    o8(bus + 0x1b, 2);
    u32 start = ticks;
    while (i8(bus + 0x1b) & 2)
        if (ticks - start > 6)
            return 0;
    return 1;
}
static void pcm_play(u32 samples) {
    unsigned ix = 0, offset = 0;
    while (offset < samples) {
        u32 len = samples - offset;
        if (len > 65534)
            len = 65534;
        bd[ix].addr = (unsigned long)(pcm + offset);
        bd[ix].len = len;
        bd[ix++].flags = 0;
        offset += len;
    }
    __sync_synchronize();
    o32(bus + 0x10, (unsigned long)bd);
    o8(bus + 0x15, ix - 1);
    o8(bus + 0x1b, 1);
}
static u32 little(const u8 *p, int n) {
    u32 v = 0;
    while (n)
        v = (v << 8) | p[--n];
    return v;
}
const char *pcm_wav(const u8 *f, u32 n, int level) {
    if (!bus)
        return "WAV playback requires an Intel AC97 output.";
    if (n < 44 || little(f, 4) != 0x46464952 || little(f + 4, 4) != n - 8 ||
        little(f + 8, 4) != 0x45564157)
        return "Not a RIFF WAVE file.";
    u32 chans = 0, rate = 0;
    for (u32 ix = 12; ix <= n - 8;) {
        u32 id = little(f + ix, 4), sz = little(f + ix + 4, 4);
        ix += 8;
        if (sz > n - ix)
            break;
        const u8 *p = f + ix;
        if (id == 0x20746d66 && sz >= 16) {
            chans = little(p + 2, 2);
            rate = little(p + 4, 4);
            if (little(p, 2) != 1 || little(p + 14, 2) != 16 || (chans != 1 && chans != 2) ||
                rate < 8000 || rate > 48000 || little(p + 12, 2) != chans * 2)
                return "Use PCM16 mono or stereo, 8 to 48 kHz.";
        }
        if (id == 0x61746164 && chans) {
            u32 frames = sz / (2 * chans);
            if (!frames || frames > rate * 3 || sz % (2 * chans))
                return "Sound clips are limited to three seconds.";
            if (!pcm_reset())
                return "Audio controller not ready.";
            u32 count = 0, index = 0, phase = 0;
            while (index < frames) {
                const u8 *q = p + index * 2 * chans;
                pcm[count++] = (short)little(q, 2) * level / 10;
                pcm[count++] = (short)little(q + 2 * (chans - 1), 2) * level / 10;
                phase += rate;
                if (phase >= 48000) {
                    phase -= 48000;
                    index++;
                }
            }
            pcm_play(count);
            serial("WAV playing\r\n");
            return "Playing sound clip on AC97.";
        }
        ix += sz + (sz & 1);
    }
    return "Unsupported or truncated WAV chunks.";
}
int pcm_tone(int hz, int level) {
    if (!pcm_reset())
        return 0;
    if (!level)
        return 1;
    unsigned phase = 0;
    for (int i = 0; i < 4800; i++) {
        phase = (phase + hz) % 48000;
        int tri = (phase < 24000 ? (int)phase : 48000 - (int)phase) - 12000;
        int env = i < 200 ? i : i > 4400 ? (4800 - i) / 2 : 200;
        pcm[2 * i] = pcm[2 * i + 1] = tri * level / 10 * env / 200;
    }
    pcm_play(9600);
    return 1;
}
/* Legacy virtio-blk (PCI transport 0.9), one synchronous chain in flight.
 * Device sees only a fixed bounce buffer. Timeout disables the transport. */
static u8 queue[16384] __attribute__((aligned(4096)));
static struct {
    u32 type, res;
    unsigned long long lba;
} req;
static u8 bounce[512];
static volatile u8 result;
struct desc {
    unsigned long long addr;
    u32 len;
    u16 flags, next;
};
static volatile u16 *avail, *used;
static void virtblk(u32 d) {
    u16 p = port(d, 16, 40);
    if (!p || !dma32(queue, sizeof queue) || !dma32(bounce, sizeof bounce) ||
        !dma32(&req, sizeof req) || !dma32((const void *)&result, 1))
        return;
    enable(d);
    o8(p + 18, 0);
    o8(p + 18, 3);
    u32 features = i32(p);
    if ((features & (1 << 5)) || !(features & (1 << 9)))
        goto bad;
    o32(p + 4, 1 << 9);
    o16(p + 14, 0);
    qn = i16(p + 12);
    if (qn < 3 || qn > 256 || (qn & (qn - 1)) || i32(p + 8))
        goto bad;
    avail = (void *)(queue + 16 * qn);
    used = (void *)(queue + ((16 * qn + 6 + 2 * qn + 4095) & ~4095));
    avail[0] = 1;
    o32(p + 8, (unsigned long)queue >> 12);
    sectors = i32(p + 24) ? 0xffffffffu : i32(p + 20);
    o8(p + 18, 7);
    blk = p;
    return;
bad:
    o8(p + 18, 128);
}
u32 vblk_size(void) { return blk ? sectors : 0; }
int vblk_io(u32 lba, void *buf, int op) {
    if (!blk || lba >= sectors)
        return 0;
    u8 *p = buf;
    struct desc *dt = (void *)queue;
    req.type = op;
    req.lba = lba;
    result = 255;
    dt[0] = (struct desc){(unsigned long)&req, 16, 1, op == 4 ? 2 : 1};
    dt[1] = (struct desc){(unsigned long)bounce, 512, op == 0 ? 3 : 1, 2};
    dt[2] = (struct desc){(unsigned long)&result, 1, 2, 0};
    if (op == 1)
        for (int i = 0; i < 512; i++)
            bounce[i] = p[i];
    avail[2 + submitted % qn] = 0;
    __sync_synchronize();
    avail[1] = ++submitted;
    __sync_synchronize();
    o16(blk + 16, 0);
    u32 start = ticks;
    while (used[1] != submitted)
        if (ticks - start > 300) {
            o8(blk + 18, 0);
            blk = 0;
            return 0;
        }
    __sync_synchronize();
    if (result)
        return 0;
    if (op == 0)
        for (int i = 0; i < 512; i++)
            p[i] = bounce[i];
    return 1;
}
static char records[48][64];
static int count;
static int vm_abs, vm_display;
static const char *vm = "No VMware guest interface confirmed";
static char *cat(char *p, const char *s) {
    while (*s)
        *p++ = *s++;
    *p = 0;
    return p;
}
static char *hex(char *p, u32 v, int n) {
    while (n--)
        *p++ = "0123456789abcdef"[(v >> (4 * n)) & 15];
    *p = 0;
    return p;
}
#include "pcnet.inc"
#include "ahci.inc"
static void probe(u32 d, u32 id) {
    u32 cls = cfg(d, 8) >> 16;
    if ((u16)id == 0x15ad && (cls >> 8) == 3)
        vm_display = 1;
    const char *kind = cls == 0x0101   ? "IDE"
                       : cls == 0x0106 ? "SATA/AHCI"
                       : cls == 0x0108 ? "NVMe"
                       : cls == 0x0280 ? "Wireless controller"
                       : cls == 0x0d11 ? "Bluetooth"
                       : cls >> 8 == 3 ? "Display"
                       : cls >> 8 == 2 ? "Network"
                       : cls >> 8 == 4 ? "Multimedia"
                                       : "PCI";
    if ((id == 0x11111234 || id == 0xbeef80ee) && display_flip_bind(cfg(d, 16) & ~15u))
        kind = "DISPI / page flip bound";
    if (cls == 0x0106 && !satacap && ((cfg(d, 8) >> 8) & 255) == 1) {
        ahci(d);
        if (satacap)
            kind = "AHCI / SATA bound";
    }
    if (id == 0x20001022 && !nic) {
        pcnet(d);
        if (nic)
            kind = "PCnet / Ethernet bound";
    }
    if (id == 0x24158086 && !bus) {
        ac97(d);
        if (bus)
            kind = "AC97 / PCM bound";
    }
    if (id == 0x10011af4 && !blk) {
        virtblk(d);
        if (blk)
            kind = "virtio-blk / bound";
    }
    if (count < 48) {
        char *p = records[count++];
        p = hex(p, d >> 16, 2);
        *p++ = ':';
        p = hex(p, d >> 11 & 31, 2);
        *p++ = '.';
        p = hex(p, d >> 8 & 7, 1);
        *p++ = ' ';
        p = hex(p, id, 4);
        *p++ = ':';
        p = hex(p, id >> 16, 4);
        *p++ = ' ';
        cat(p, kind);
    }
}
struct back {
    u32 a, b, c, d;
};
static struct back vmcall(u32 command, u32 arg) {
    struct back r = {0x564d5868, arg, command, 0x5658};
    __asm__ volatile("inl %%dx,%%eax" : "+a"(r.a), "+b"(r.b), "+c"(r.c), "+d"(r.d));
    return r;
}
int guest_pointer(int *x, int *y, int *keys) {
    if (!vm_abs)
        return -1;
    u32 status = vmcall(40, 0).a;
    if (status >> 16 == 0xffff) {
        vmcall(41, 0xf5);
        vm_abs = 0;
        return -1;
    }
    if ((status & 65535) < 4)
        return 0;
    struct back r = vmcall(39, 4);
    *x = r.b & 65535;
    *y = r.c & 65535;
    *keys = ((r.a & 0x20) ? 1 : 0) | ((r.a & 0x10) ? 2 : 0) | ((r.a & 8) ? 4 : 0);
    return 1;
}
const char *vm_guest(void) { return vm; }
void drivers_init(void) {
    for (int b = 0; b < 256; b++)
        for (int s = 0; s < 32; s++) {
            u32 d = 0x80000000u | (b << 16) | (s << 11);
            u32 id = cfg(d, 0);
            if ((u16)id == 0xffff)
                continue;
            probe(d, id);
            if (cfg(d, 12) & 0x800000)
                for (int f = 1; f < 8; f++) {
                    id = cfg(d + (f << 8), 0);
                    if ((u16)id != 0xffff)
                        probe(d + (f << 8), id);
                }
        }
    u32 a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x40000000u));
    if (vm_display || (b == 0x61774d56 && c == 0x4d566572 && d == 0x65726177)) {
        struct back r = vmcall(10, ~0x564d5868u);
        if (r.b == 0x564d5868 && r.a != ~0u) {
            vm = "VMware guest / version handshake";
            vmcall(41, 0x45414552);
            r = vmcall(40, 0);
            if ((r.a >> 16) != 0xffff && (r.a & 65535) && vmcall(39, 1).a == 0x3442554a) {
                vmcall(41, 0x53424152);
                vm_abs = 1;
                vm = "VMware guest / absolute pointer active";
            }
            serial(vm);
            serial("\r\n");
        }
    }
    serial(audio_name());
    serial("\r\n");
    if (blk)
        serial("virtio-blk ready\r\n");
}
void drivers_list(void (*line)(const char *)) {
    for (int i = 0; i < count; i++)
        line(records[i]);
    line(audio_name());
    line(vm);
}

int driver_count(void) { return count; }
const char *driver_at(int i) { return i >= 0 && i < count ? records[i] : ""; }
