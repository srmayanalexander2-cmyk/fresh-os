#include "aerojs.h"
/* Integer guest, little endian. No native pointers, MMIO, host calls, JIT or disk access.
 * A fresh zeroed byte array and instruction budget belong to each invocation. */
static u32 read_n(const u8 *p, int n) {
    u32 v = 0;
    while (n)
        v = (v << 8) | p[--n];
    return v;
}
static int unpack(u8 *dst, u32 max, const u8 *src, u32 size) {
    if (size < 5)
        return 0;
    u32 limit = read_n(src, 4), i = 4, o = 0;
    if (limit > max)
        return 0;
    while (i < size) {
        u32 tok = src[i++], l = tok >> 4, m = tok & 15;
        if (l == 15) {
            int k;
            do {
                if (i >= size)
                    return 0;
                k = src[i++];
                l += k;
            } while (k == 255);
        }
        if (l > limit - o || l > size - i)
            return 0;
        while (l--)
            dst[o++] = src[i++];
        if (i == size)
            return o == limit;
        if (size - i < 2)
            return 0;
        u32 back = read_n(src + i, 2);
        i += 2;
        if (!back || back > o)
            return 0;
        if (m == 15) {
            int k;
            do {
                if (i >= size)
                    return 0;
                k = src[i++];
                m += k;
            } while (k == 255);
        }
        m += 4;
        if (m > limit - o)
            return 0;
        while (m--) {
            dst[o] = dst[o - back];
            o++;
        }
    }
    return 0;
}
static u32 sx(u32 x, int bits) {
    u32 sign = 1u << (bits - 1);
    return (x ^ sign) - sign;
}
const char *aero_eval(u8 *ram, const u8 *rom, u32 size, const char *s, char *out, u32 cap) {
    if (!cap)
        return "Missing output buffer";
    out[0] = 0;
    u32 n = 0;
    while (s[n] && n < JS_SOURCE_MAX)
        n++;
    if (s[n])
        return "Script exceeds 16 KiB limit";
    for (u32 i = 0; i < JS_RAM; i++)
        ram[i] = 0;
    if (!unpack(ram + 4096, 0x33000, rom, size))
        return "Invalid engine image";
    for (u32 i = 0; i < n; i++)
        ram[0x60000 + i] = s[i];
    u32 r[32] = {0}, pc = 4096;
    for (unsigned fuel = 0; fuel < 6000000; fuel++) {
        if (pc < 4096 || pc > 0x33ffc || (pc & 3))
            return "Worker stopped: invalid instruction address";
        u32 op = read_n(ram + pc, 4), next = pc + 4, rd = (op >> 7) & 31, f = (op >> 12) & 7,
            a = r[(op >> 15) & 31], b = r[(op >> 20) & 31], imm = sx(op >> 20, 12), val = 0,
            addr;
        int wr = 1;
        switch (op & 127) {
        case 0x37:
            val = op & 0xfffff000;
            break;
        case 0x17:
            val = pc + (op & 0xfffff000);
            break;
        case 0x6f:
            val = next;
            next = pc + sx(((op >> 31) << 20) | (op & 0xff000) | ((op >> 9) & 0x800) |
                               ((op >> 20) & 0x7fe),
                           21);
            break;
        case 0x67:
            val = next;
            next = (a + imm) & ~1u;
            break;
        case 0x63: {
            int yes = f == 0   ? a == b
                      : f == 1 ? a != b
                      : f == 4 ? (int)a < (int)b
                      : f == 5 ? (int)a >= (int)b
                      : f == 6 ? a < b
                               : a >= b;
            if (yes)
                next = pc + sx(((op >> 31) << 12) | ((op & 128) << 4) | ((op >> 20) & 0x7e0) |
                                   ((op >> 7) & 30),
                               13);
            wr = 0;
            break;
        }
        case 3: {
            int k = f == 0 || f == 4 ? 1 : f == 1 || f == 5 ? 2 : f == 2 ? 4 : 0;
            addr = a + imm;
            if (!k || addr > JS_RAM - (u32)k)
                return "Worker read out of range";
            val = read_n(ram + addr, k);
            if (f < 2)
                val = sx(val, 8 * k);
            break;
        }
        case 0x23: {
            int k = f == 0 ? 1 : f == 1 ? 2 : f == 2 ? 4 : 0;
            addr = a + sx((op >> 25 << 5) | rd, 12);
            if (!k || addr > JS_RAM - (u32)k)
                return "Worker write out of range";
            for (int j = 0; j < k; j++)
                ram[addr + j] = b >> (8 * j);
            wr = 0;
            break;
        }
        case 0x13:
        case 0x33: {
            int reg = (op & 127) == 0x33;
            if (!reg)
                b = imm;
            if (reg && (op >> 25) == 1) {
                switch (f) {
                case 0:
                    val = a * b;
                    break;
                case 1:
                    val = (unsigned long long)((long long)(int)a * (int)b) >> 32;
                    break;
                case 2:
                    val = (unsigned long long)((long long)(int)a * (long long)b) >> 32;
                    break;
                case 3:
                    val = ((unsigned long long)a * b) >> 32;
                    break;
                case 4:
                    val = !b                              ? ~0u
                          : (a == 0x80000000 && b == ~0u) ? a
                                                          : (u32)((int)a / (int)b);
                    break;
                case 5:
                    val = b ? a / b : ~0u;
                    break;
                case 6:
                    val = !b ? a : (a == 0x80000000 && b == ~0u) ? 0 : (u32)((int)a % (int)b);
                    break;
                default:
                    val = b ? a % b : a;
                }
                break;
            }
            switch (f) {
            case 0:
                val = reg && (op & (1u << 30)) ? a - b : a + b;
                break;
            case 1:
                val = a << (b & 31);
                break;
            case 2:
                val = (int)a < (int)b;
                break;
            case 3:
                val = a < b;
                break;
            case 4:
                val = a ^ b;
                break;
            case 5:
                val = op & (1u << 30) ? (u32)((int)a >> (b & 31)) : a >> (b & 31);
                break;
            case 6:
                val = a | b;
                break;
            default:
                val = a & b;
            }
            break;
        }
        case 15:
            wr = 0;
            break;
        case 0x73:
            if (op == 0x73) {
                u32 i = 0;
                while (i + 1 < cap && i < 4095 && ram[0x70000 + i]) {
                    u8 c = ram[0x70000 + i];
                    out[i++] = c == '\n' || (c >= 32 && c < 127) ? c : ' ';
                }
                out[i] = 0;
                return r[10] ? "JavaScript exception" : 0;
            }
            return "Worker trap";
        default:
            return "Unsupported worker instruction";
        }
        if (wr && rd)
            r[rd] = val;
        pc = next;
    }
    return "Script stopped: instruction budget reached";
}
