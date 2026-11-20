/* AHCI 1.x, first responding SATA disk only. Poll slot 0; fixed DMA buffers
 * below 4 GiB. Do not reset the controller, issue TRIM or touch an active port. */
static volatile u32 *sata;
static u32 satacap;
static u32 clist[256] __attribute__((aligned(1024)));
static u8 fis[256] __attribute__((aligned(256)));
static u32 table[36] __attribute__((aligned(128)));
static u8 sectorbuf[512] __attribute__((aligned(512)));
static int regwait(volatile u32 *r, u32 mask, int timeout) {
    u32 t = ticks;
    while (*r & mask)
        if (ticks - t > (u32)timeout)
            return 0;
    return 1;
}
static int issue(int opcode, u32 lba, int writing) {
    volatile u32 *p = sata;
    if (!p || p[14] || p[13] || !regwait(p + 8, 0x88, 120))
        return 0;
    for (int i = 0; i < 36; i++)
        table[i] = 0;
    int data = opcode != 0xe7;
    u8 *c = (void *)table;
    c[0] = 0x27;
    c[1] = 0x80;
    c[2] = opcode;
    c[4] = lba;
    c[5] = lba >> 8;
    c[6] = lba >> 16;
    c[7] = 0x40 | (lba >> 24 & 15);
    c[12] = data;
    table[32] = (unsigned long)sectorbuf;
    table[35] = 511;
    clist[0] = 5 | (writing ? 64 : 0) | (data << 16);
    clist[1] = 0;
    clist[2] = (unsigned long)table;
    clist[3] = 0;
    p[4] = ~0u;
    __sync_synchronize();
    p[14] = 1;
    if (!regwait(p + 14, 1, 300)) {
        sata = 0;
        satacap = 0;
        return 0;
    }
    __sync_synchronize();
    return !(p[4] & 0x7d000000) && !(p[8] & 1);
}
static void ahci(u32 d) {
    u32 bar = cfg(d, 36);
    if ((bar & 7) || bar < 0x1000000u || bar > 0xffffe000u || !dma32(clist, sizeof clist) ||
        !dma32(table, sizeof table) || !dma32(sectorbuf, sizeof sectorbuf) ||
        !dma32(fis, sizeof fis))
        return;
    u16 cmd = cfg(d, 4);
    o16(0xcfc, cmd | 6);
    volatile u32 *h = (void *)(unsigned long)(bar & ~15u);
    if (h[0] == ~0u)
        return;
    if (h[9] & 1) {
        h[10] |= 2;
        if (!regwait(h + 10, 17, 150))
            return;
    }
    h[1] |= 0x80000000u;
    u32 ports = h[3];
    for (int i = 0; i < 32; i++)
        if (ports & (1u << i)) {
            volatile u32 *p = h + 64 + 32 * i;
            if ((p[10] & 0xf0f) != 0x103 || p[14] || p[13] || p[9] != 0x101)
                continue;
            p[6] &= ~1u;
            if (!regwait(p + 6, 1 << 15, 60))
                return;
            p[6] &= ~16u;
            if (!regwait(p + 6, 1 << 14, 60))
                return;
            p[5] = 0;
            p[0] = (unsigned long)clist;
            p[1] = 0;
            p[2] = (unsigned long)fis;
            p[3] = 0;
            p[12] = ~0u;
            p[6] |= 16;
            p[6] |= 1;
            sata = p;
            if (issue(0xec, 0, 0)) {
                const u16 *id = (const void *)sectorbuf;
                int longsector = (id[106] & 0xd000) == 0x5000;
                if ((id[49] & 0x300) == 0x300 &&
                    (!longsector || (id[117] == 256 && !id[118]))) {
                    satacap = id[60] | (u32)id[61] << 16;
                    if (satacap) {
                        serial("AHCI SATA ready\r\n");
                        return;
                    }
                }
            }
            sata = 0;
            return;
        }
}
u32 sata_size(void) { return satacap; }
int sata_io(u32 lba, void *buf, int op) {
    if (!sata || lba >= satacap || lba >= 0x10000000)
        return 0;
    u8 *b = buf;
    if (op == 1)
        for (int i = 0; i < 512; i++)
            sectorbuf[i] = b[i];
    if (!issue(op == 4 ? 0xe7 : op == 1 ? 0xca : 0xc8, lba, op == 1))
        return 0;
    if (!op)
        for (int i = 0; i < 512; i++)
            b[i] = sectorbuf[i];
    return 1;
}
