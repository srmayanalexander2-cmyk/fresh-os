#include "region64.h"
#ifdef __x86_64__
#define PG 4096
#define N (REGION_BYTES / PG)
static u8 phys[2][REGION_BYTES] __attribute__((aligned(PG)));
/* owner is the base page + 1. prot = -1 is reserved, not committed. */
static u16 owners[2][N];
static signed char prots[2][N], initials[2][N];
u64 region_bases[2];
#define frames phys[process_bank]
#define owner owners[process_bank]
#define prot prots[process_bank]
#define initial initials[process_bank]
void region_reset(int wide) {
    region_base = wide ? 0x4000000000ull : 0x62000000u;
    memset(owner, 0, sizeof owner);
}
static int pages(u64 addr, u64 bytes, unsigned *i, unsigned *n) {
    if (!bytes || addr < REGION_BASE || addr - REGION_BASE >= REGION_BYTES ||
        bytes > REGION_BYTES - (addr - REGION_BASE))
        return 0;
    *i = (addr - REGION_BASE) / PG;
    *n = (((addr - REGION_BASE) % PG) + bytes + PG - 1) / PG;
    return 1;
}
static int change(unsigned i, int p) {
    return task64_edit(REGION_BASE + (u64)i * PG, frames + i * PG, p > 0 ? p : 0);
}
int region_commit(u64 addr, u64 bytes, int p) {
    unsigned i, n;
    if (!pages(addr, bytes, &i, &n) || !owner[i])
        return 0;
    for (unsigned j = i; j < i + n; j++)
        if (owner[j] != owner[i])
            return 0;
    for (unsigned j = i; j < i + n; j++) {
        if (prot[j] < 0)
            memset(frames + j * PG, 0, PG);
        if (!change(j, p))
            return 0;
        prot[j] = p;
    }
    return 1;
}
u64 region_alloc(u64 addr, u64 bytes, int p, int reserve_only, int windows) {
    if (!bytes || bytes > REGION_BYTES)
        return 0;
    unsigned i = 0, n = (bytes + PG - 1) / PG;
    if (addr) {
        if ((addr & (windows ? 65535 : 4095)) || !pages(addr, bytes, &i, &n))
            return 0;
    }
    for (; i + n <= N; i += windows ? 16 : 1) {
        unsigned j = 0;
        while (j < n && !owner[i + j])
            j++;
        if (j < n) {
            if (addr)
                break;
            continue;
        }
        initial[i] = p;
        for (j = 0; j < n; j++) {
            owner[i + j] = i + 1;
            prot[i + j] = -1;
        }
        u64 a = REGION_BASE + (u64)i * PG;
        if (!reserve_only && !region_commit(a, bytes, p)) {
            region_free(a, 0, 1);
            return 0;
        }
        return a;
    }
    return 0;
}
int region_protect(u64 addr, u64 bytes, int p, int *old) {
    unsigned i, n;
    if (!pages(addr, bytes, &i, &n))
        return 0;
    for (unsigned j = i; j < i + n; j++)
        if (!owner[j] || prot[j] < 0)
            return 0;
    *old = prot[i];
    for (unsigned j = i; j < i + n; j++) {
        if (!change(j, p))
            return 0;
        prot[j] = p;
    }
    return 1;
}
int region_free(u64 addr, u64 bytes, int win) {
    unsigned i, n;
    if (win == 1) {
        if (bytes || !pages(addr, 1, &i, &n) || addr % PG || owner[i] != i + 1)
            return 0;
        n = 0;
        while (i + n < N && owner[i + n] == i + 1)
            n++;
    } else if (!pages(addr, bytes, &i, &n))
        return 0;
    if (win == 2) {
        if (!owner[i])
            return 0;
        for (unsigned j = i; j < i + n; j++)
            if (owner[j] != owner[i])
                return 0;
    }
    for (unsigned j = i; j < i + n; j++) {
        if (owner[j] && !change(j, 0))
            return 0;
        prot[j] = -1;
        if (win != 2)
            owner[j] = 0;
    }
    return 1;
}
/* Return the run from this page to the next change within this arena. */
void region_query(u64 addr, u64 *base, u64 *bytes, int *current, int *original) {
    unsigned i = (addr - REGION_BASE) / PG, j = i + 1;
    while (j < N && owner[j] == owner[i] && (!owner[i] || prot[j] == prot[i]))
        j++;
    *bytes = (u64)(j - i) * PG;
    *base = owner[i] ? REGION_BASE + (u64)(owner[i] - 1) * PG : 0;
    *current = owner[i] ? prot[i] : -1;
    *original = owner[i] ? initial[owner[i] - 1] : -1;
}
u8 *region_ptr(u64 addr, u32 bytes, int access) {
    unsigned i, n;
    if (!pages(addr, bytes, &i, &n))
        return 0;
    for (unsigned j = i; j < i + n; j++)
        if (!owner[j] || prot[j] < 0 || !(prot[j] & access))
            return 0;
    return frames + i * PG + (addr & (PG - 1));
}
#endif
