#include "pci.h"
/* Read-only configuration inventory. Only try the VMware I/O protocol if a
 * VMware display device identifies the host. */
static char inventory[48][44];
static int count, absolute;
static u32 config(u32 dev, u32 reg) {
    u32 v;
    __asm__ volatile("outl %0,%w1" ::"a"(dev | reg), "d"((u16)0xcf8));
    __asm__ volatile("inl %w1,%0" : "=a"(v) : "d"((u16)0xcfc));
    return v;
}
struct Reply {
    u32 a, b, c, d;
};
static struct Reply backdoor(u32 cmd, u32 value) {
    struct Reply r = {0x564d5868, value, cmd, 0x5658};
    __asm__ volatile("inl %%dx,%%eax" : "+a"(r.a), "+b"(r.b), "+c"(r.c), "+d"(r.d));
    return r;
}
static void record(u32 id, u32 cls) {
    if (count >= 48)
        return;
    char *dst = inventory[count++];
    char tmp[9];
    hex32(tmp, id & 65535);
    strset(dst, 44, tmp + 4);
    add(dst, 44, ':');
    hex32(tmp, id >> 16);
    strset(dst + 5, 39, tmp + 4);
    const char *kind = cls >> 16 == 1    ? "Storage"
                       : cls >> 16 == 2  ? "Network"
                       : cls >> 16 == 3  ? "Display"
                       : cls >> 16 == 4  ? "Multimedia"
                       : cls >> 16 == 6  ? "Bridge"
                       : cls >> 16 == 12 ? "Serial bus"
                                         : "PCI device";
    strset(dst + 9, 35, "  /  ");
    strset(dst + 14, 30, kind);
}
void pci_init(void) {
    int vm = 0;
    u8 buses[256] = {1};
    for (int b = 0; b < 256; b++)
        if (buses[b])
            for (int s = 0; s < 32; s++) {
                int funcs = 1;
                for (int f = 0; f < funcs; f++) {
                    u32 addr = 0x80000000u | (b << 16) | (s << 11) | (f << 8);
                    u32 id = config(addr, 0);
                    if ((id & 65535) == 65535)
                        continue;
                    if (f == 0 && (config(addr, 12) & 0x800000))
                        funcs = 8;
                    u32 cls = config(addr, 8) >> 8;
                    record(id, cls);
                    if ((cls >> 8) == 0x0604)
                        buses[config(addr, 24) >> 8 & 255] = 1;
                    if ((id & 65535) == 0x15ad && cls >> 16 == 3)
                        vm = 1;
                }
            }
    if (vm) {
        backdoor(41, 0x45414552);
        struct Reply s = backdoor(40, 0);
        if ((s.a >> 16) != 65535 && (s.a & 65535) && backdoor(39, 1).a == 0x3442554a) {
            backdoor(41, 0x53424152);
            absolute = 1;
            logline("VMware absolute pointer enabled");
        }
    }
}
int pci_devices(void) { return count; }
const char *pci_device(int i) { return i >= 0 && i < count ? inventory[i] : ""; }
const char *pointer_name(void) { return absolute ? "VMware absolute" : "PS/2 relative"; }
int vm_pointer(int *x, int *y, int *b) {
    if (!absolute)
        return 0;
    u32 n = backdoor(40, 0).a;
    if ((n >> 16) == 65535) {
        absolute = 0;
        return 0;
    }
    if ((n & 65535) < 4)
        return 0;
    struct Reply r = backdoor(39, 4);
    *x = r.b & 65535;
    *y = r.c & 65535;
    *b = ((r.a & 32) ? 1 : 0) | ((r.a & 16) ? 2 : 0) | ((r.a & 8) ? 4 : 0);
    return 1;
}

int vm_absolute(void) { return absolute; }
