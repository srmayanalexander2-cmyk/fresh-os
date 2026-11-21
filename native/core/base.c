#include "base.h"
void *memcpy(void *d, const void *s, size_t n) {
    u8 *a = d;
    const u8 *b = s;
    while (n--)
        *a++ = *b++;
    return d;
}
void *memset(void *d, int c, size_t n) {
    u8 *a = d;
    while (n--)
        *a++ = c;
    return d;
}
int equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
int length(const char *s) {
    int n = 0;
    while (s[n])
        n++;
    return n;
}
void strset(char *d, unsigned cap, const char *s) {
    if (!cap)
        return;
    unsigned i = 0;
    while (i + 1 < cap && s[i]) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}
int add(char *s, unsigned cap, char c) {
    unsigned n = length(s);
    if (n + 1 >= cap)
        return 0;
    s[n] = c;
    s[n + 1] = 0;
    return 1;
}
void decimal(char *d, int v) {
    unsigned n = v < 0 ? 0u - (unsigned)v : (unsigned)v;
    char t[12];
    int k = 0;
    if (v < 0)
        *d++ = '-';
    do {
        t[k++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (k)
        *d++ = t[--k];
    *d = 0;
}
void hex32(char *d, u32 v) {
    for (int i = 0; i < 8; i++)
        d[i] = "0123456789ABCDEF"[(v >> (28 - 4 * i)) & 15];
    d[8] = 0;
}
