/* Home-2 is deliberately not a general disk writer. LBA 0 must match the v2
 * image marker. Write data to the inactive slot, flush, publish header, flush.
 * A failed newest checksum falls back to the previous committed generation. */
#include "io.h"
#include "home.h"
struct Home home;
static struct Home staging;
#define HOME_BASE 40960u
static u16 port;
/* Optional data disk: sector zero is a private marker, never a foreign MBR.
 * Expose only the following fixed-size filesystem window. */
#define DATA_START 2048u
#define DATA_SECTORS 786432u
static u16 app_port;
static u8 app_unit;
static int installed;
static u8 unit;
static int disks, slot = -1;
static u32 generation;
static const char *state = "Session memory";
static const char marker[] = "FRESHOS2/HOME/01";
static const char stamp[] = "F2SNAP01";
static int same(const u8 *a, const char *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i] != (u8)b[i])
            return 0;
    return 1;
}
static int wait_disk(int drq) {
    u32 start = ticks;
    for (unsigned n = 0; ticks - start < 120; n++) {
        int s = in8(port + 7);
        if (s == 255 || !s)
            return 0;
        if (!(s & 128)) {
            if (s & 0x21)
                return 0;
            if (!drq || (s & 8))
                return 1;
        }
        if ((n & 1023) == 1023)
            __asm__ volatile("hlt");
    }
    logline("ATA timeout");
    return 0;
}
static void select_drive(void) {
    out8(port + 6, 0xe0 | (unit << 4));
    for (int i = 0; i < 4; i++)
        in8(port + 0x206);
}
static int sector(unsigned lba, void *buf, int write) {
    u8 *p = buf;
    select_drive();
    if (!wait_disk(0))
        return 0;
    out8(port + 2, 1);
    out8(port + 3, lba);
    out8(port + 4, lba >> 8);
    out8(port + 5, lba >> 16);
    out8(port + 7, write ? 0x30 : 0x20);
    if (!wait_disk(1))
        return 0;
    unsigned words = 256;
    if (write)
        __asm__ volatile("cld; rep outsw" : "+S"(p), "+c"(words) : "d"(port) : "memory");
    else
        __asm__ volatile("cld; rep insw" : "+D"(p), "+c"(words) : "d"(port) : "memory");
    return wait_disk(0);
}
static int flush_disk(void) {
    select_drive();
    if (!wait_disk(0))
        return 0;
    out8(port + 7, 0xe7);
    int ok = wait_disk(0);
    if (!ok)
        logline("flush failed");
    return ok;
}
static u32 get32(const u8 *p) {
    return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}
static void put32(u8 *p, u32 n) {
    for (int i = 0; i < 4; i++)
        p[i] = n >> (8 * i);
}
static u32 checksum(const struct Home *h) {
    u32 a = 2166136261u;
    const u8 *p = (const u8 *)h;
    for (unsigned i = 0; i < sizeof *h; i++)
        a = (a ^ p[i]) * 16777619u;
    return a;
}
static int mount_disk(void) {
    u16 words[256], found = 0;
    u8 chosen = 0;
    for (int channel = 0; channel < 2; channel++)
        for (int drive = 0; drive < 2; drive++) {
            port = channel ? 0x170 : 0x1f0;
            unit = drive;
            select_drive();
            out8(port + 2, 0);
            out8(port + 3, 0);
            out8(port + 4, 0);
            out8(port + 5, 0);
            out8(port + 7, 0xec);
            if (!wait_disk(1))
                continue;
            for (int i = 0; i < 256; i++)
                words[i] = in16(port);
            disks++;
            if (!(words[49] & 512) || ((u32)words[60] | (u32)words[61] << 16) < 65536)
                continue;
            u8 s[512];
            if (!sector(0, s, 0))
                continue;
            if (!app_port && same(s, "STRATUS/APPDATA1", 16) && get32(s + 16) == DATA_START &&
                get32(s + 20) == DATA_SECTORS &&
                ((u32)words[60] | (u32)words[61] << 16) >= DATA_START + DATA_SECTORS) {
                app_port = port;
                app_unit = unit;
                logline("App data disk detected");
                continue;
            }
            if (found)
                continue;
            int boot = same(s + 400, "STRATUS/BOOT/002", 16) && s[510] == 85 && s[511] == 170;
            if (!boot && (!same(s, marker, 16) || get32(s + 16) != 257))
                continue;
            if (!sector(HOME_BASE, s, 0) || !same(s, marker, 16) || get32(s + 16) != 257)
                continue;
            found = port;
            chosen = unit;
            installed = boot;
        }
    port = found;
    unit = chosen;
    return !!port;
}
int goodname(const char *s) {
    int i = 0;
    while (s[i]) {
        char c = s[i++];
        if (i >= 28 || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
                         c == '-' || c == '_'))
            return 0;
    }
    return i > 0;
}
static int valid(void) {
    if (!staging.count || staging.count > NOTES || staging.light < 35 ||
        staging.light > 100 || staging.sound > 100 || staging.accent > 2 ||
        staging.autohide > 1 || staging.wallpaper > 2 || staging.wide_apps > 1)
        return 0;
    for (unsigned i = 0; i < staging.count; i++) {
        struct Note *n = staging.notes + i;
        if (n->name[27] || !goodname(n->name) || n->body[NOTE_CAP - 1])
            return 0;
        for (int j = 0; n->body[j]; j++) {
            u8 c = n->body[j];
            if (c != '\n' && (c < 32 || c > 126))
                return 0;
        }
    }
    return 1;
}
static int transfer(int index, struct Home *h, int writing) {
    u8 tmp[512];
    u8 *p = (u8 *)h;
    for (unsigned off = 0; off < sizeof *h; off += 512) {
        unsigned n = low(sizeof *h - off, 512);
        if (writing) {
            memset(tmp, 0, 512);
            memcpy(tmp, p + off, n);
        }
        if (!sector(HOME_BASE + 2 + index * 128 + off / 512, tmp, writing)) {
            char b[12];
            decimal(b, off / 512);
            uart("Home sector I/O failed at index ");
            logline(b);
            return 0;
        }
        if (!writing)
            memcpy(p + off, tmp, n);
    }
    return 1;
}
_Static_assert(sizeof(struct Home) <= 127 * 512, "snapshot slot");
void home_init(void) {
    home.light = 100;
    /* A blank Home disk should feel like a usable workspace, not an empty shell. */
    home.count = 4;
    strset(home.notes[0].name, 28, "welcome.txt");
    strset(home.notes[0].body, NOTE_CAP,
           "Welcome to Stratus.\n\nF1 opens All Apps. F3 opens Terminal; type help for commands. "
           "Right-click for actions. Esc returns to this desktop.\n\nTry example.mus in Media "
           "and pattern.pbm in Photos, or edit ideas.txt in Writer.\n\nCtrl+S saves. The "
           "supplied Home disk keeps changes after shutdown; without it, changes are session-only.");
    strset(home.notes[1].name, 28, "example.mus");
    strset(home.notes[1].body, NOTE_CAP,
           "C4:1 C4:1 G4:1 G4:1 A4:1 A4:1 G4:2 F4:1 F4:1 E4:1 E4:1 D4:1 D4:1 C4:2");
    strset(home.notes[2].name, 28, "pattern.pbm");
    strset(home.notes[2].body, NOTE_CAP,
           "P1\n16 8\n0 0 0 0 0 1 1 1 1 1 0 0 0 0 0 0\n"
           "0 0 0 0 1 1 1 1 1 1 1 0 0 0 0 0\n"
           "0 0 0 1 1 1 1 1 1 1 1 1 0 0 0 0\n"
           "0 0 1 1 1 1 1 1 1 1 1 1 1 0 0 0\n"
           "0 1 1 1 1 1 1 1 1 1 1 1 1 1 0 0\n"
           "1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 0\n"
           "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
           "0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0");
    strset(home.notes[3].name, 28, "ideas.txt");
    strset(home.notes[3].body, NOTE_CAP,
           "A few things to try\n\n1. Write a note and save it.\n2. Play example.mus.\n"
           "3. View pattern.pbm.\n4. Draw in Sketch and save a PBM.\n5. Open Terminal and type help.");
    if (!mount_disk())
        return;
    state = "Home disk / writable";
    u8 head[512];
    int damaged = 0;
    for (int i = 0; i < 2; i++) {
        if (!sector(HOME_BASE + 1 + i * 128, head, 0)) {
            damaged = 1;
            continue;
        }
        if (!same(head, stamp, 8))
            continue;
        u32 gen = get32(head + 8);
        if (get32(head + 12) != sizeof home || !transfer(i, &staging, 0) ||
            get32(head + 16) != checksum(&staging) || !valid()) {
            damaged = 1;
            continue;
        }
        if (slot < 0 || (int)(gen - generation) > 0) {
            memcpy(&home, &staging, sizeof home);
            generation = gen;
            slot = i;
        }
    }
    if (damaged && slot < 0) {
        port = 0;
        state = "Damaged Home / session only";
    } else if (damaged)
        state = "Recovered Home / writable";
    logline(state);
}
int home_commit(void) {
    if (!port)
        return 0;
    int next = slot == 0 ? 1 : 0;
    u8 hdr[512] = {0};
    memcpy(hdr, stamp, 8);
    put32(hdr + 8, generation + 1);
    put32(hdr + 12, sizeof home);
    put32(hdr + 16, checksum(&home));
    if (!transfer(next, &home, 1) || !flush_disk() ||
        !sector(HOME_BASE + 1 + next * 128, hdr, 1) || !flush_disk()) {
        char b[10];
        hex32(b, in8(port + 1));
        uart("ATA error ");
        logline(b);
        state = "Home write failed";
        return 0;
    }
    slot = next;
    generation++;
    state = "Home disk / writable";
    logline("Home snapshot committed");
    return 1;
}
int home_find(const char *s) {
    for (unsigned i = 0; i < home.count; i++)
        if (equal(s, home.notes[i].name))
            return i;
    return -1;
}
int home_new(const char *name) {
    if (!goodname(name) || home.count >= NOTES || home_find(name) >= 0)
        return -1;
    int i = home.count++;
    memset(&home.notes[i], 0, sizeof home.notes[i]);
    strset(home.notes[i].name, 28, name);
    return i;
}
unsigned disk_count(void) { return disks; }
const char *home_state(void) { return state; }
/* ATAPI packet reads use a different port/unit from the Home destination. */
static u16 cdport;
static u8 cdunit;
static u32 next_cd, end_cd;
static u8 first_block[2048];
static const char *install_message = "Install to the supplied blank Home-2 disk";
static int cdwait(int drq) {
    u32 start = ticks;
    for (int i = 0; ticks - start < 120; i++) {
        int s = in8(cdport + 7);
        if (!s || s == 255)
            return 0;
        if (!(s & 128)) {
            if (s & 0x21)
                return 0;
            if (!drq || (s & 8))
                return 1;
        }
        if ((i & 1023) == 1023)
            __asm__ volatile("hlt");
    }
    return 0;
}
static int cdread(u32 lba, u8 *dest) {
    out8(cdport + 6, 0xa0 | (cdunit << 4));
    for (int i = 0; i < 4; i++)
        in8(cdport +
            0x206); /* A prior IDENTIFY on ATAPI can leave ERR set. PACKET resets it. */
    int s = in8(cdport + 7);
    if (!s || s == 255 || (s & 128))
        return 0;
    out8(cdport + 1, 0);
    out8(cdport + 4, 0);
    out8(cdport + 5, 8);
    out8(cdport + 7, 0xa0);
    if (!cdwait(1))
        return 0;
    u8 command[12] = {0xa8, 0, lba >> 24, lba >> 16, lba >> 8, lba, 0, 0, 0, 1, 0, 0};
    for (int j = 0; j < 12; j += 2)
        out16(cdport, command[j] | command[j + 1] << 8);
    int pos = 0;
    while (pos < 2048) {
        if (!cdwait(1))
            return 0;
        int n = in8(cdport + 4) | (in8(cdport + 5) << 8);
        if (n <= 0 || n > 2048 - pos || (n & 1))
            return 0;
        for (int j = 0; j < n; j += 2) {
            u16 w = in16(cdport);
            dest[pos++] = w;
            dest[pos++] = w >> 8;
        }
    }
    return cdwait(0);
}
const char *install_status(void) {
    return installed ? "Installed / Home-2 system disk" : install_message;
}
int install_progress(void) { return end_cd ? (int)(next_cd * 100 / end_cd) : -1; }
int install_start(void) {
    if (installed || end_cd)
        return 0;
    u8 m[512];
    if (!port || !sector(0, m, 0) || !same(m, marker, 16) || get32(m + 16) != 257) {
        install_message = "No blank marked Home-2 disk. Foreign disks are protected.";
        return 0;
    }
    for (int c = 0; c < 2; c++)
        for (int u = 0; u < 2; u++) {
            cdport = c ? 0x170 : 0x1f0;
            cdunit = u;
            if (cdport == port && u == unit)
                continue; /* Check ATAPI signature using IDENTIFY, not a packet sent to a hard
                             disk. */
            out8(cdport + 6, 0xa0 | (u << 4));
            for (int i = 0; i < 4; i++)
                in8(cdport + 0x206);
            out8(cdport + 7, 0xec);
            u32 t = ticks;
            int s;
            while (((s = in8(cdport + 7)) & 128) && s != 255 && ticks - t < 60)
                ;
            if (s == 255 || !s || in8(cdport + 4) != 0x14 || in8(cdport + 5) != 0xeb) {
                if ((s & 0x89) == 8)
                    for (int i = 0; i < 256; i++)
                        in16(cdport);
                continue;
            }
            if (!cdread(0, first_block) && !cdread(0, first_block))
                continue;
            u32 n = get32(first_block + 416);
            if (!same(first_block + 400, "STRATUS/BOOT/002", 16) || first_block[510] != 85 ||
                first_block[511] != 170 || n < 64 || n % 4 || n >= HOME_BASE ||
                get32(first_block + 462 + 8) != HOME_BASE)
                continue;
            next_cd = 0;
            end_cd = n / 4;
            install_message = "Copying and verifying system sectors";
            logline("install begin");
            return 1;
        }
    install_message = "No compatible Stratus installation CD found";
    return 0;
}
void install_cancel(void) {
    if (end_cd) {
        end_cd = 0;
        install_message = "Cancelled / boot record not published";
    }
}
int install_step(void) {
    if (!end_cd)
        return 0;
    u8 block[2048], check[512];
    for (int j = 0; j < 8 && next_cd < end_cd; j++, next_cd++) {
        if (!cdread(next_cd, block))
            goto fail;
        for (int i = 0; i < 4; i++) {
            u32 lba = next_cd * 4 + i;
            if (!lba)
                continue;
            if (!sector(lba, block + i * 512, 1) || !sector(lba, check, 0))
                goto fail;
            for (int k = 0; k < 512; k++)
                if (check[k] != block[i * 512 + k])
                    goto fail;
        }
    }
    if (next_cd == end_cd) {
        if (!flush_disk() || !sector(0, first_block, 1) || !flush_disk())
            goto fail;
        installed = 1;
        end_cd = 0;
        logline("install verified");
        return 2;
    }
    return 1;
fail:
    end_cd = 0;
    install_message = "Install I/O error / check target before reboot";
    logline(install_message);
    return -1;
}
/* Read-only package library beyond both Home snapshot slots. */
int home_package_sector(unsigned s, unsigned index, void *buf) {
    return port && s < 8 && index <= 2048 && sector(41472 + s * 2049 + index, buf, 0);
}

int home_package_publish(unsigned s) {
    u8 h[512];
    if (!home_package_sector(s, 0, h) || !same(h, "F2INB001", 8))
        return 0;
    memcpy(h, "F2PKG001", 8);
    return sector(41472 + s * 2049, h, 1) && flush_disk();
}

/* Runs on kernel CR3, never from IRQ code. Restore Home's selection even on failure. */
int app_disk_present(void) { return app_port != 0; }
int app_disk_io(u32 lba, u8 *p, unsigned count, int write) {
    if (!app_port || !count || lba >= DATA_SECTORS || count > DATA_SECTORS - lba)
        return 0;
    u16 oldport = port;
    u8 oldunit = unit;
    port = app_port;
    unit = app_unit;
    int ok = 1;
    for (unsigned i = 0; ok && i < count; i++)
        ok = sector(DATA_START + lba + i, p + 512 * i, write);
    port = oldport;
    unit = oldunit;
    return ok;
}
int app_disk_sync(void) {
    if (!app_port)
        return 0;
    u16 oldport = port;
    u8 oldunit = unit;
    port = app_port;
    unit = app_unit;
    select_drive();
    int ok = wait_disk(0) && flush_disk();
    port = oldport;
    unit = oldunit;
    return ok;
}
