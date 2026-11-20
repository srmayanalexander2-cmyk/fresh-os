#ifndef FRESH_FILETOOLS_H
#define FRESH_FILETOOLS_H
#include "platform.h"
/* Hex digest including final NUL; 65-byte destination. */
void sha3_256(const u8 *data, u32 size, char *hex);
/* ustar member, zero-padded to 512; caller appends two terminal zero blocks. */
u32 tar_member(u8 *out, u32 cap, const char *name, const u8 *data, u32 size);
u32 gzip_store(u8 *out, u32 cap, const u8 *data, u32 size);
#endif
