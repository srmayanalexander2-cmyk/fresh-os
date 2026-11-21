#ifndef FRESH_COMPAT_H
#define FRESH_COMPAT_H
#include "platform.h"
#define FILE_LIMIT (2u << 20)
#define IMP_LIMIT (FILE_LIMIT + 4 * (FILE_LIMIT / 4096) + 32)
typedef unsigned long long u64;
/* Synchronous, bounded guest runtimes. They never call an address from a file. */
typedef struct {
    char output[768], detail[128];
    u32 exit_code, steps;
    int bits;
} PeResult;
void guest_save_service(int (*fn)(const char *, const void *, u32));
void guest_services(int (*read)(const char *, void *, u32), void (*box)(const char *));
int elf_run(const u8 *, u32, PeResult *);
int pe_run(const u8 *file, u32 size, PeResult *result);
u32 imp_convert(const u8 *, u32, u8 *, u32, PeResult *);
const char *imp_check(const u8 *, u32);
int imp_run(const u8 *, u32, PeResult *);
#endif
