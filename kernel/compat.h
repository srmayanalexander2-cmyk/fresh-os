#ifndef FRESH_COMPAT_H
#define FRESH_COMPAT_H
#include "platform.h"
#define FILE_LIMIT (2u << 20)
#define DEX_LIMIT (1u << 20)
typedef unsigned long long u64;
/* Synchronous, bounded guest runtimes. They never call an address from a file. */
typedef struct {
    char output[768], detail[128];
    u32 exit_code, steps;
    int bits;
} PeResult;
int pe_run(const u8 *file, u32 size, PeResult *result);
const char *android_activity(const u8 *, u32);
const char *apk_activity(const u8 *, u32, const char **error);
#endif
