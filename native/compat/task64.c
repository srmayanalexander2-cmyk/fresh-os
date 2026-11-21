#include "task64.h"
#ifdef __x86_64__
#define PAGE 4096
#define NX (1ull << 63)
#define EFER 0xc0000080u
/* Firmware hands us an identity-mapped PE image. We do not import any
 * firmware PTEs into the process. Only this kernel image is supervisor-mapped. */
unsigned process_bank;
static u64 bank_tables[2][48][512] __attribute__((aligned(PAGE)));
#define tables bank_tables[process_bank]
static unsigned bank_used[2];
#define used bank_used[process_bank]
static ptr kernel_start, kernel_end;
u64 user_root, user_fs, user_cs, user_teb, host_root;
/* Fatal asynchronous entries must not rely on a user RSP, including the
 * short SYSCALL window before the software stack switch. */
static u8 fatal_stacks[3][8192] __attribute__((aligned(16)));
struct UserRegs user_regs;
u8 user_fp[512] __attribute__((aligned(16)));
extern char user_irq_top[] __attribute__((visibility("hidden")));
extern void user_syscall(void) __attribute__((visibility("hidden")));
static u64 gdt[9]
    __attribute__((aligned(16))) = {0,
                                    0x00af9a000000ffffull,
                                    0x00cf92000000ffffull,
                                    0x00cff2000000ffffull,
                                    0x00affa000000ffffull,
                                    0,
                                    0,
                                    0x00cffa000000ffffull,
                                    0x71cff2000000ffffull}; /* ring 3 FS32, base 0x71000000 */
struct Tss {
    u32 zero;
    u64 rsp[3], reserved, ist[7], unused;
    u16 padding, io;
} __attribute__((packed));
static struct Tss tss;
_Static_assert(sizeof tss == 104, "TSS layout");
static void msr(u32 n, u64 v) {
    __asm__ volatile("wrmsr" ::"c"(n), "a"((u32)v), "d"((u32)(v >> 32)));
}
static u64 getmsr(u32 n) {
    u32 a, d;
    __asm__ volatile("rdmsr" : "=a"(a), "=d"(d) : "c"(n));
    return a | ((u64)d << 32);
}
static u32 features(u32 leaf) {
    u32 a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(0));
    return d;
}
void task64_init(ptr base, u64 size) {
    if (!base || !size || (base & 4095) || size > (64u << 20) ||
        !(features(0x80000001) & (1 << 20)) || !(features(0x80000001) & (1 << 11)))
        return;
    ptr cr0, cr4;
    __asm__ volatile("mov %%cr4,%0" : "=r"(cr4));
    if (cr4 & (1 << 12))
        return; /* only four-level paging */
    /* FreshOS uses legacy x87/SSE state, no global pages or user FSGSBASE. */
    cr4 = (cr4 & ~((1ul << 7) | (1ul << 16) | (1ul << 18))) | (3ul << 9);
    __asm__ volatile("mov %0,%%cr4" ::"r"(cr4) : "memory");
    __asm__ volatile("mov %%cr0,%0" : "=r"(cr0));
    cr0 = (cr0 & ~12ul) | 2 | (1ul << 16);
    __asm__ volatile("mov %0,%%cr0" ::"r"(cr0) : "memory");
    ptr a = (ptr)&tss;
    for (int i = 0; i < 3; i++)
        tss.ist[i] = (ptr)(fatal_stacks[i] + sizeof fatal_stacks[i]);
    tss.rsp[0] = (ptr)user_irq_top;
    tss.io = sizeof tss;
    gdt[5] = 103 | ((a & 0xffffff) << 16) | (0x89ull << 40) | ((a & 0xff000000) << 32);
    gdt[6] = a >> 32;
    struct {
        u16 limit;
        ptr addr;
    } __attribute__((packed)) gd = {sizeof gdt - 1, (ptr)gdt};
    __asm__ volatile("lgdt %0; ltr %w1" ::"m"(gd), "r"(40) : "memory");
    msr(EFER, getmsr(EFER) | 0x801);
    msr(EFER + 1, 8ull << 32);
    msr(EFER + 2, (ptr)user_syscall);
    u32 vendor_b, vendor_d, vendor_c, leaf;
    __asm__ volatile("cpuid"
                     : "=a"(leaf), "=b"(vendor_b), "=c"(vendor_c), "=d"(vendor_d)
                     : "a"(0));
    if (vendor_b != 0x756e6547u || vendor_d != 0x49656e69u || vendor_c != 0x6c65746eu)
        msr(EFER + 3,
            (ptr)user_syscall); /* CSTAR: compatibility SYSCALL, not supported on Intel. */
    msr(EFER + 4, ~0u);
    msr(0xc0000100, 0);
    msr(0xc0000101, 0);
    if (features(1) & (1 << 11))
        msr(0x174, 0);
    kernel_start = base;
    kernel_end = base + size;
}
static int page(u64 va, ptr pa, u64 flags, int edit) {
    if ((va | pa) & 4095 || va >= 1ull << 47)
        return 0;
    u64 *t = tables[0];
    for (int shift = 39; shift >= 21; shift -= 9) {
        u64 *e = &t[(va >> shift) & 511];
        if (!(*e & 1)) {
            if (used >= 48)
                return 0;
            *e = (ptr)tables[used++] | 7;
        }
        t = (u64 *)(*e & ~4095ull);
    }
    u64 *e = &t[(va >> 12) & 511];
    if (*e && (!edit || !(*e & 4) || (*e & 0x000ffffffffff000ull) != pa))
        return 0;
    *e = (edit && !flags) ? 0 : pa | flags | 1;
    return 1;
}
/* Switch only after user_resume returned to the kernel CR3 and stack. The VM
 * loop owns integer registers and flags; save the assembly bridge state here. */
static u8 parent_fp[512];
static u64 parent_cs, parent_fs, parent_teb;
void task64_push_child(void) {
    parent_cs = user_cs;
    parent_fs = user_fs;
    parent_teb = user_teb;
    memcpy(parent_fp, user_fp, sizeof user_fp);
    process_bank = 1;
}
void task64_pop_child(void) {
    process_bank = 0;
    user_root = (ptr)tables;
    user_cs = parent_cs;
    user_fs = parent_fs;
    user_teb = parent_teb;
    memcpy(user_fp, parent_fp, sizeof user_fp);
}
int task64_available(void) { return kernel_start != 0; }
int task64_begin(void) {
    if (!kernel_start)
        return 0;
    memset(tables, 0, sizeof tables);
    used = 1;
    user_fs = user_teb = 0;
    user_root = (ptr)tables;
    memset(&user_regs, 0, sizeof user_regs);
    memset(user_fp, 0, sizeof user_fp);
    user_fp[0] = 0x7f;
    user_fp[1] = 3;
    user_fp[24] = 0x80;
    user_fp[25] = 0x1f;
    for (ptr a = kernel_start; a < kernel_end; a += PAGE)
        if (!page(a, a, 2, 0))
            return 0;
    return 1;
}
int task64_page(u64 va, void *pa, int perms) {
    if (!(perms & 1) || (perms & 6) == 6)
        return 0;
    return page(va, (ptr)pa, 4 | (perms & 2) | ((perms & 4) ? 0 : NX), 0);
}
int task64_edit(u64 va, void *pa, int p) {
    if ((p && !(p & 1)) || (p & 6) == 6)
        return 0;
    return page(va, (ptr)pa, p ? (4 | (p & 2) | ((p & 4) ? 0 : NX)) : 0, 1);
}
#endif
