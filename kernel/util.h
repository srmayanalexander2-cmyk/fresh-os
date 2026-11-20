static int len(const char *s) {
    int n = 0;
    while (s[n])
        n++;
    return n;
}
static int eq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
static int prefix(const char *a, const char *b) {
    while (*b && *a == *b) {
        a++;
        b++;
    }
    return !*b;
}
static void copy(char *d, const char *s, int cap) {
    int i = 0;
    for (; s[i] && i + 1 < cap; i++)
        d[i] = s[i];
    d[i] = 0;
}
static char *num(char *s, int v) {
    unsigned a = v < 0 ? 0u - (unsigned)v : (unsigned)v;
    if (v < 0)
        *s++ = '-';
    char t[12];
    int n = 0;
    do {
        t[n++] = '0' + a % 10;
        a /= 10;
    } while (a);
    while (n)
        *s++ = t[--n];
    *s = 0;
    return s;
}
static void append(char *s, int cap, char c) {
    int n = len(s);
    if (n + 1 < cap) {
        s[n] = c;
        s[n + 1] = 0;
    }
}
static int two(char *s, int n) {
    s[0] = '0' + (n / 10) % 10;
    s[1] = '0' + n % 10;
    s[2] = 0;
    return 2;
}
