#include "filetools.h"
typedef unsigned long long Lane;
static Lane rot(Lane a, unsigned n) { return n ? (a << n) | (a >> (64 - n)) : a; }
/* FIPS 202 Keccak-f[1600]: lane indexing x+5*y, bytes little endian. */
static void permute(Lane a[25]) {
    static const unsigned char shifts[] = {0,  1,  62, 28, 27, 36, 44, 6,  55, 20, 3,  10, 43,
                                           25, 39, 41, 45, 15, 21, 8,  18, 2,  61, 56, 14};
    unsigned lfsr = 1;
    for (int round = 0; round < 24; round++) {
        Lane c[5], b[25];
        for (int x = 0; x < 5; x++)
            c[x] = a[x] ^ a[5 + x] ^ a[10 + x] ^ a[15 + x] ^ a[20 + x];
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 5; x++) {
                int i = x + 5 * y;
                b[y + 5 * ((2 * x + 3 * y) % 5)] =
                    rot(a[i] ^ c[(x + 4) % 5] ^ rot(c[(x + 1) % 5], 1), shifts[i]);
            }
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 5; x++)
                a[x + 5 * y] =
                    b[x + 5 * y] ^ ((~b[(x + 1) % 5 + 5 * y]) & b[(x + 2) % 5 + 5 * y]);
        for (unsigned j = 0; j < 7; j++) {
            if (lfsr & 1)
                a[0] ^= (Lane)1 << ((1u << j) - 1);
            lfsr = ((lfsr << 1) ^ ((lfsr & 128) ? 0x71 : 0)) & 255;
        }
    }
}
void sha3_256(const u8 *p, u32 size, char *out) {
    Lane a[25] = {0};
    unsigned j = 0;
    for (u32 i = 0; i < size; i++) {
        a[j / 8] ^= (Lane)p[i] << (8 * (j % 8));
        if (++j == 136) {
            permute(a);
            j = 0;
        }
    }
    a[j / 8] ^= (Lane)6 << (8 * (j % 8));
    a[16] ^= (Lane)128 << 56;
    permute(a);
    for (j = 0; j < 32; j++) {
        u8 b = a[j / 8] >> (8 * (j % 8));
        out[2 * j] = "0123456789abcdef"[b >> 4];
        out[2 * j + 1] = "0123456789abcdef"[b & 15];
    }
    out[64] = 0;
}
static void oct(u8 *p, int digits, u32 n) {
    while (digits) {
        p[--digits] = '0' + (n & 7);
        n >>= 3;
    }
}
u32 tar_member(u8 *out, u32 cap, const char *name, const u8 *p, u32 n) {
    unsigned l = 0;
    for (; name[l]; l++) {
        char c = name[l];
        if (l >= 99 || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                         c == '-' || c == '.'))
            return 0;
    }
    if (!l || (l <= 2 && name[0] == '.' && (l == 1 || name[1] == '.')) || n > 0xfffff000)
        return 0;
    u32 size = 512 + ((n + 511) & ~511u);
    if (size > cap)
        return 0;
    for (u32 i = 0; i < size; i++)
        out[i] = 0;
    for (unsigned i = 0; i < l; i++)
        out[i] = name[i];
    oct(out + 100, 7, 0644);
    oct(out + 108, 7, 0);
    oct(out + 116, 7, 0);
    oct(out + 124, 11, n);
    oct(out + 136, 11, 0);
    for (int i = 148; i < 156; i++)
        out[i] = ' ';
    out[156] = '0';
    for (int i = 0; i < 5; i++)
        out[257 + i] = "ustar"[i];
    out[263] = out[264] = '0';
    u32 sum = 0;
    for (int i = 0; i < 512; i++)
        sum += out[i];
    oct(out + 148, 6, sum);
    out[154] = 0;
    for (u32 i = 0; i < n; i++)
        out[512 + i] = p[i];
    return size;
}
static void le32(u8 *p, u32 n) {
    for (int i = 0; i < 4; i++)
        p[i] = n >> (8 * i);
}
/* RFC 1952 with RFC 1951 stored blocks; interoperable, not size reducing. */
u32 gzip_store(u8 *out, u32 cap, const u8 *p, u32 n) {
    u32 blocks = n ? 1 + (n - 1) / 65535 : 1;
    if (cap < 18 || n > cap - 18 || blocks > (cap - 18 - n) / 5)
        return 0;
    const u8 header[] = {31, 139, 8, 0, 0, 0, 0, 0, 0, 255};
    for (int i = 0; i < 10; i++)
        out[i] = header[i];
    u32 pos = 10, j = 0, crc = ~0u;
    do {
        u32 sz = n - j > 65535 ? 65535 : n - j;
        out[pos++] = (j + sz == n);
        out[pos++] = sz;
        out[pos++] = sz >> 8;
        out[pos++] = ~sz;
        out[pos++] = (~sz) >> 8;
        for (u32 i = 0; i < sz; i++) {
            u8 b = p[j++];
            out[pos++] = b;
            crc ^= b;
            for (int k = 0; k < 8; k++)
                crc = (crc >> 1) ^ ((0u - (crc & 1)) & 0xedb88320u);
        }
    } while (j < n);
    le32(out + pos, ~crc);
    le32(out + pos + 4, n);
    return pos + 8;
}
