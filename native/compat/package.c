#include "package.h"
#include "home.h"
#include "task64.h"
static char names[PKG_SLOTS][28];
static int pending[PKG_SLOTS];
int package_pending(int s) { return s >= 0 && s < PKG_SLOTS && pending[s]; }
static u32 sizes[PKG_SLOTS];
static u8 payloads[PROCESS_BANKS][PKG_MAX];
#define payload payloads[process_bank]
static struct Resource {
    const char *name;
    const u8 *data;
    u32 size;
} all_resources[PROCESS_BANKS][16];
static unsigned counts[PROCESS_BANKS];
#define resources all_resources[process_bank]
#define nresources counts[process_bank]
static u32 word(const u8 *p) {
    return p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24;
}
static int magic(const u8 *p, const char *s) {
    while (*s)
        if (*p++ != *s++)
            return 0;
    return 1;
}
void packages_init(void) {
    u8 h[512];
    for (int s = 0; s < PKG_SLOTS; s++) {
        if (!home_package_sector(s, 0, h) ||
            (!magic(h, "F2INB001") && !magic(h, "F2PKG001")) || h[35] ||
            !goodname((char *)h + 8))
            continue;
        u32 n = word(h + 36);
        if (n < 20 || n > PKG_MAX)
            continue;
        strset(names[s], 28, (char *)h + 8);
        sizes[s] = n;
        pending[s] = magic(h, "F2INB001");
    }
}
const char *package_name(int s) { return s >= 0 && s < PKG_SLOTS ? names[s] : ""; }
static int resource_read(const char *name, void *dest, u32 cap) {
    for (unsigned i = 0; i < nresources; i++)
        if (equal(resources[i].name, name) && resources[i].size <= cap) {
            memcpy(dest, resources[i].data, resources[i].size);
            return resources[i].size;
        }
    return -1;
}
static int process(int s, PeResult *r, int installing, void (*dialog)(const char *)) {
    memset(r, 0, sizeof *r);
    if (!*package_name(s))
        goto invalid;
    if (installing != pending[s]) {
        strset(r->detail, sizeof r->detail,
               installing ? "Already installed" : "Install this inbox package first");
        return 0;
    }
    u32 n = sizes[s];
    for (u32 i = 0; i < n; i += 512)
        if (!home_package_sector(s, 1 + i / 512, payload + i))
            goto invalid;
    u32 kind = word(payload + 4), code = word(payload + 8), count = word(payload + 12),
        crc = ~0u;
    if (!magic(payload, "IMP2") || kind > 3 || code > n - 20 || count > 16)
        goto invalid;
    for (u32 i = 20; i < n; i++) {
        crc ^= payload[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    if (~crc != word(payload + 16))
        goto invalid;
    if (process_bank && (kind & 1)) goto invalid; /* Win32 launches PE only. */
    if (kind & 2) {
        kind &= 1;
        if (code < 8 || (kind ? payload[24] != 2 : payload[25] != 64))
            goto invalid;
#ifdef __x86_64__
        if (!task64_available())
#endif
        {
            strset(r->detail, sizeof r->detail, "This package requires x64 UEFI user mode");
            return 0;
        }
    }
    u32 off = 20 + code;
    for (u32 i = 0; i < count; i++) {
        if (n - off < 32 || payload[off + 27] || !goodname((char *)payload + off))
            goto invalid;
        u32 k = word(payload + off + 28);
        if (k > 16384 || k > n - off - 32)
            goto invalid;
        for (u32 j = 0; j < i; j++)
            if (equal(resources[j].name, (char *)payload + off))
                goto invalid;
        resources[i] = (struct Resource){(char *)payload + off, payload + off + 32, k};
        off += 32 + k;
    }
    if (off != n)
        goto invalid;
    if (installing) {
        if (kind) {
            if (!elf_check(payload + 20, code, r))
                return 0;
        } else {
            const char *err = imp_check(payload + 20, code);
            if (err) {
                strset(r->detail, sizeof r->detail, err);
                return 0;
            }
        }
        if (!home_package_publish(s)) {
            strset(r->detail, sizeof r->detail, "Package header write failed");
            return 0;
        }
        pending[s] = 0;
        strset(r->detail, sizeof r->detail, "Package installed");
        return 1;
    }
    nresources = count;
    guest_services(resource_read, dialog);
    int ok = kind ? elf_run(payload + 20, code, r) : imp_run(payload + 20, code, r);
    guest_services(0, 0);
    nresources = 0;
    return ok;
invalid:
    strset(r->detail, sizeof r->detail, "Unreadable or invalid IMP2 package");
    return 0;
}

int package_run(int s, PeResult *r, void (*dialog)(const char *)) {
    return process(s, r, 0, dialog);
}
int package_install(int s, PeResult *r) { return process(s, r, 1, 0); }
