#include <stddef.h>
#include <stdint.h>
void *memset(void *v, int a, size_t n) {
    unsigned char *p = v;
    while (n--)
        *p++ = a;
    return v;
}
void *memcpy(void *d, const void *s, size_t n) {
    unsigned char *p = d;
    const unsigned char *q = s;
    while (n--)
        *p++ = *q++;
    return d;
}
void *memmove(void *d, const void *s, size_t n) {
    unsigned char *p = d;
    const unsigned char *q = s;
    if ((uintptr_t)d <= (uintptr_t)s)
        return memcpy(d, s, n);
    while (n) {
        n--;
        p[n] = q[n];
    }
    return d;
}
int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p = a, *q = b;
    while (n--) {
        int k = *p++ - *q++;
        if (k)
            return k;
    }
    return 0;
}
int bcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}
void abort(void) { __builtin_trap(); }
int abs(int x) { return x < 0 ? -x : x; }
double copysign(double x, double y) {
    union {
        double d;
        uint64_t u;
    } a = {.d = x}, b = {.d = y};
    a.u = (a.u & 0x7fffffffffffffffULL) | (b.u & 0x8000000000000000ULL);
    return a.d;
}
