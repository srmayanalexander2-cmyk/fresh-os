typedef __SIZE_TYPE__ size_t;
void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}
void *memset(void *dst, int value, size_t n) {
    unsigned char *d = dst;
    while (n--)
        *d++ = value;
    return dst;
}
void *memmove(void *d, const void *s, size_t n) {
    unsigned char *p = d;
    const unsigned char *q = s;
    if ((__UINTPTR_TYPE__)d <= (__UINTPTR_TYPE__)s)
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
size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}
int strncmp(const char *a, const char *b, size_t n) {
    while (n--) {
        int k = (unsigned char)*a - (unsigned char)*b;
        if (k || !*a)
            return k;
        a++;
        b++;
    }
    return 0;
}
int strcmp(const char *a, const char *b) { return strncmp(a, b, (size_t)-1); }
char *strncpy(char *d, const char *s, size_t n) {
    size_t i = 0;
    while (i < n && s[i]) {
        d[i] = s[i];
        i++;
    }
    while (i < n)
        d[i++] = 0;
    return d;
}
int atoi(const char *s) {
    unsigned v = 0;
    while (*s >= '0' && *s <= '9')
        v = v * 10 + *s++ - '0';
    return (int)v;
}
