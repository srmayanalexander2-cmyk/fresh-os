/* Freestanding subset needed by the selected FatFs configuration. */
#include "base.h"
static inline int memcmp(const void *a, const void *b, size_t n) {
    const u8 *x = a, *y = b;
    while (n--) { int d = *x++ - *y++; if (d) return d; }
    return 0;
}
static inline char *strchr(const char *s, int c) {
    do { if (*s == c) return (char *)s; } while (*s++);
    return 0;
}
