#ifndef FRESH_AEROJS_H
#define FRESH_AEROJS_H
#include "platform.h"
#define JS_RAM (512u * 1024)
#define JS_SOURCE_MAX 16384
/* Private scratch; only NUL-terminated text crosses the boundary. */
const char *aero_eval(u8 *ram, const u8 *rom, u32 size, const char *source, char *output,
                      u32 cap);
#endif
