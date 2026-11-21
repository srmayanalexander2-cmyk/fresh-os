#include "math.h"
#include <stdint.h>
struct Parser {
    const char *s, *error;
    int depth;
};
static void spaces(struct Parser *p) {
    while (*p->s == ' ')
        p->s++;
}
static int sum(struct Parser *);
static int atom(struct Parser *p) {
    spaces(p);
    if (++p->depth > 16) {
        p->error = "Expression nested too deeply";
        return 0;
    }
    int v = 0;
    if (*p->s == '-' || *p->s == '+') {
        int neg = *p->s++ == '-';
        v = atom(p);
        if (neg && __builtin_sub_overflow(0, v, &v))
            p->error = "Integer overflow";
    } else if (*p->s == '(') {
        p->s++;
        v = sum(p);
        spaces(p);
        if (*p->s != ')')
            p->error = "Missing closing parenthesis";
        else
            p->s++;
    } else if (*p->s < '0' || *p->s > '9')
        p->error = "Expected a number";
    else
        while (*p->s >= '0' && *p->s <= '9') {
            int n = *p->s++ - '0';
            if (__builtin_mul_overflow(v, 10, &v) || __builtin_add_overflow(v, n, &v)) {
                p->error = "Integer overflow";
                break;
            }
        }
    p->depth--;
    return v;
}
static int product(struct Parser *p) {
    int v = atom(p);
    for (;;) {
        spaces(p);
        char c = *p->s;
        if (p->error || (c != '*' && c != '/'))
            return v;
        p->s++;
        int b = atom(p);
        if (p->error)
            return 0;
        if (c == '*') {
            if (__builtin_mul_overflow(v, b, &v))
                p->error = "Integer overflow";
        } else if (!b)
            p->error = "Cannot divide by zero";
        else if (v == INT32_MIN && b == -1)
            p->error = "Integer overflow";
        else
            v /= b;
    }
}
static int sum(struct Parser *p) {
    int v = product(p);
    for (;;) {
        spaces(p);
        char c = *p->s;
        if (p->error || (c != '+' && c != '-'))
            return v;
        p->s++;
        int b = product(p);
        if (c == '+' ? __builtin_add_overflow(v, b, &v) : __builtin_sub_overflow(v, b, &v))
            p->error = "Integer overflow";
    }
}
int evaluate(const char *s, int *out, const char **reason) {
    struct Parser p = {s, 0, 0};
    int v = sum(&p);
    if (!p.error && *p.s)
        p.error = "Unexpected character";
    *reason = p.error;
    if (p.error)
        return 0;
    *out = v;
    return 1;
}
