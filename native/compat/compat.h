#ifndef FRESH_COMPAT_H
#define FRESH_COMPAT_H
#include "base.h"
/* Two independent residents under UEFI; BIOS retains its smaller footprint. */
#ifdef __x86_64__
#define PROCESS_BANKS 2
extern unsigned process_bank;
#else
#define PROCESS_BANKS 1
#define process_bank 0u
#endif
#define FILE_LIMIT (2u << 20)
#define IMP_LIMIT (FILE_LIMIT + 4 * (FILE_LIMIT / 4096) + 32)
typedef unsigned long long u64;
/* Synchronous, bounded guest runtimes. They never call an address from a file. */
typedef struct {
    char output[4096], detail[128];
    u32 exit_code, steps;
    int bits, entered;
} PeResult;
int raw_run(const char *, PeResult *, void (*)(const char *));
void guest_save_service(int (*fn)(const char *, const void *, u32));
void guest_services(int (*read)(const char *, void *, u32), void (*box)(const char *));
void compat_args(const char *, const char *);
void compat_clock(u16, u16, u16, u16, u16, u16, u16);
void compat_display(u32, u32);
int elf_check(const u8 *, u32, PeResult *);
int elf_run(const u8 *, u32, PeResult *);
const char *imp_check(const u8 *, u32);
int imp_run(const u8 *, u32, PeResult *);
#endif
