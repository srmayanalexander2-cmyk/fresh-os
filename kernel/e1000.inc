/* Intel 82540EM, legacy descriptors, memory-mapped registers. One active
 * Ethernet interface; receive buffers and two transmit buffers are shared with
 * the mutually exclusive PCnet driver. No interrupts are installed. */
static volatile u32 *eport;
struct e_rx {
    u32 addr, hi;
    u16 len, csum;
    volatile u8 status;
    u8 errors;
    u16 vlan;
};
struct e_tx {
    u32 addr, hi;
    u16 len;
    u8 cso, cmd;
    volatile u8 status;
    u8 css;
    u16 vlan;
};
static struct e_rx er[8] __attribute__((aligned(128)));
static struct e_tx et[8] __attribute__((aligned(128)));
static void ew(u32 reg, u32 val) { eport[reg / 4] = val; }
static u32 eg(u32 reg) { return eport[reg / 4]; }
static void e1000(u32 d) {
    u32 bar = cfg(d, 16);
    if ((bar & 7) || bar < 0x1000000 || bar > 0xfffe0000 || !dma32(er, sizeof er) ||
        !dma32(et, sizeof et) || !dma32(rxb, sizeof rxb) || !dma32(txb, sizeof txb))
        return;
    u16 cmd = cfg(d, 4);
    o16(0xcfc, cmd | 6);
    eport = (void *)(unsigned long)(bar & ~15u);
    ew(0xd8, ~0u);
    ew(0x100, 0);
    ew(0x400, 0);
    ew(0, eg(0) | (1u << 26));
    u32 start = ticks;
    while (eg(0) & (1u << 26))
        if (ticks - start > 60) {
            eport = 0;
            return;
        }
    ew(0xd8, ~0u);
    (void)eg(0xc0);
    u32 lo = eg(0x5400), hi = eg(0x5404);
    if (!(hi >> 31) || (!lo && !(hi & 65535))) {
        eport = 0;
        return;
    }
    for (int i = 0; i < 6; i++)
        mac[i] = (i < 4 ? lo : hi) >> ((i & 3) * 8);
    for (int i = 0; i < 8; i++) {
        er[i].addr = (unsigned long)rxb[i];
        et[i].addr = (unsigned long)txb[i & 1];
        et[i].status = 1;
    }
    ew(0, eg(0) | 64);
    ew(0x2800, (unsigned long)er);
    ew(0x2804, 0);
    ew(0x2808, sizeof er);
    ew(0x2810, 0);
    ew(0x2818, 7);
    ew(0x3800, (unsigned long)et);
    ew(0x3804, 0);
    ew(0x3808, sizeof et);
    ew(0x3810, 0);
    ew(0x3818, 0);
    __sync_synchronize();
    ew(0x410, 0x60200a);
    ew(0x400, 2 | 8 | (15 << 4) | (64 << 12));
    ew(0x100, 2 | (1 << 15) | (1 << 26));
    serial("82540EM Ethernet bound\r\n");
}
static int txbusy(void) {
    return eport ? !(et[(ti + 6) & 7].status & 1) : !!(tx[ti].status & 0x8000);
}
static void received(const u8 *, int);
static void epoll(void) {
    for (int j = 0; j < 8; j++) {
        if (!(er[ri].status & 1))
            break;
        __sync_synchronize();
        if ((er[ri].status & 2) && !er[ri].errors && er[ri].len >= 14 && er[ri].len <= 1518)
            received(rxb[ri], er[ri].len);
        er[ri].status = 0;
        __sync_synchronize();
        ew(0x2818, ri);
        ri = (ri + 1) & 7;
    }
}
