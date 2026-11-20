/* AMD PCnet PCI, software style 2. Ethernet/IPv4 DHCP client and gateway
 * neighbour discovery. No sockets, no background traffic until `net dhcp`. */
static u16 nic;
static u8 mac[6];
static u32 ip, gateway, offer, server, xid, deadline, lease_end;
static int phase, neighbour;
static struct {
    u16 mode, rings;
    u8 mac[6], pad[2], filter[8];
    u32 rx, tx;
} init __attribute__((aligned(16)));
struct ring {
    u32 addr;
    short len;
    volatile u16 status;
    u32 msg, res;
};
static struct ring rx[4] __attribute__((aligned(16))), tx[2] __attribute__((aligned(16)));
static u8 rxb[4][1536] __attribute__((aligned(16))),
    txb[2][1536] __attribute__((aligned(16)));
static unsigned ri, ti;
static void csr(u16 r, u16 v) {
    o16(nic + 18, r);
    o16(nic + 16, v);
}
static u16 csread(u16 r) {
    o16(nic + 18, r);
    return i16(nic + 16);
}
static void pcnet(u32 d) {
    u16 p = port(d, 16, 32);
    if (!p || !dma32(&init, sizeof init) || !dma32(rx, sizeof rx) || !dma32(tx, sizeof tx) ||
        !dma32(rxb, sizeof rxb) || !dma32(txb, sizeof txb))
        return;
    enable(d);
    nic = p;
    i16(nic + 20);
    csr(0, 4);
    o16(nic + 18, 20);
    o16(nic + 22, 2);
    for (int j = 0; j < 6; j++)
        init.mac[j] = mac[j] = i8(nic + j);
    init.rings = 0x1020;
    init.rx = (unsigned long)rx;
    init.tx = (unsigned long)tx;
    for (int i = 0; i < 4; i++) {
        rx[i].addr = (unsigned long)rxb[i];
        rx[i].len = -1536;
        rx[i].status = 0x8000;
    }
    for (int i = 0; i < 2; i++)
        tx[i].addr = (unsigned long)txb[i];
    u32 a = (unsigned long)&init;
    csr(1, a);
    csr(2, a >> 16);
    csr(3, 0x5f00);
    __sync_synchronize();
    csr(0, 1);
    u32 start = ticks;
    while (!(csread(0) & 256)) {
        if (ticks - start > 60) {
            csr(0, 4);
            nic = 0;
            return;
        }
    }
    csr(0, 258);
    serial("PCnet bound\r\n");
}
static void be(u8 *p, int n, u32 v) {
    while (n) {
        p[--n] = v;
        v >>= 8;
    }
}
static u32 netint(const u8 *p, int n) {
    u32 v = 0;
    while (n--)
        v = (v << 8) | *p++;
    return v;
}
static u16 checksum(const u8 *p, int n) {
    unsigned v = 0;
    for (int i = 0; i < n; i += 2)
        v += p[i] * 256 + (i + 1 < n ? p[i + 1] : 0);
    while (v >> 16)
        v = (v & 65535) + (v >> 16);
    return ~v;
}
static u8 *frame(int type, const u8 *dest) {
    if (tx[ti].status & 0x8000)
        return 0;
    u8 *p = txb[ti];
    for (int i = 0; i < 600; i++)
        p[i] = 0;
    for (int i = 0; i < 6; i++) {
        p[i] = dest ? dest[i] : 255;
        p[6 + i] = mac[i];
    }
    be(p + 12, 2, type);
    return p + 14;
}
static void transmit(int n) {
    tx[ti].len = -(n < 60 ? 60 : n);
    tx[ti].msg = 0;
    __sync_synchronize();
    tx[ti].status = 0x8300;
    __sync_synchronize();
    csr(0, 8);
    ti ^= 1;
}
static void dhcp(int request) {
    u8 *p = frame(0x800, 0);
    if (!p)
        return;
    u8 *u = p + 20, *b = u + 8;
    p[0] = 0x45;
    p[8] = 64;
    p[9] = 17;
    be(p + 16, 4, ~0u);
    be(u, 2, 68);
    be(u + 2, 2, 67);
    b[0] = 1;
    b[1] = 1;
    b[2] = 6;
    be(b + 4, 4, xid);
    b[10] = 128;
    for (int i = 0; i < 6; i++)
        b[28 + i] = mac[i];
    be(b + 236, 4, 0x63825363);
    u8 *o = b + 240;
    *o++ = 53;
    *o++ = 1;
    *o++ = request ? 3 : 1;
    if (request) {
        *o++ = 50;
        *o++ = 4;
        be(o, 4, offer);
        o += 4;
        *o++ = 54;
        *o++ = 4;
        be(o, 4, server);
        o += 4;
    }
    *o++ = 55;
    *o++ = 3;
    *o++ = 1;
    *o++ = 3;
    *o++ = 51;
    *o = 255;
    be(u + 4, 2, 308);
    be(p + 2, 2, 328);
    be(p + 10, 2, checksum(p, 20));
    transmit(342);
}
static void arp(int reply, u32 target, const u8 *dest) {
    u8 *a = frame(0x806, dest);
    if (!a)
        return;
    be(a, 2, 1);
    be(a + 2, 2, 0x800);
    a[4] = 6;
    a[5] = 4;
    be(a + 6, 2, reply ? 2 : 1);
    for (int i = 0; i < 6; i++) {
        a[8 + i] = mac[i];
        a[18 + i] = dest ? dest[i] : 0;
    }
    be(a + 14, 4, ip);
    be(a + 24, 4, target);
    transmit(42);
}
static void received(const u8 *p, int n) {
    if (n < 42)
        return;
    unsigned type = netint(p + 12, 2);
    if (type == 0x806) {
        const u8 *a = p + 14;
        if (!ip || netint(a, 4) != 0x00010800 || a[4] != 6 || a[5] != 4 ||
            netint(a + 24, 4) != ip)
            return;
        u32 from = netint(a + 14, 4);
        if (netint(a + 6, 2) == 1)
            arp(1, from, a + 8);
        else if (netint(a + 6, 2) == 2 && from == gateway) {
            neighbour = 1;
            serial("Gateway ARP resolved\r\n");
        }
        return;
    }
    if (type != 0x800 || phase < 1 || phase > 2)
        return;
    p += 14;
    n -= 14;
    if (n < 268 || p[0] != 0x45 || p[9] != 17 || checksum(p, 20) || netint(p + 6, 2) & 0x3fff)
        return;
    int total = netint(p + 2, 2);
    if (total > n || total < 268 || netint(p + 20, 4) != 0x00430044)
        return;
    int udp = netint(p + 24, 2);
    if (udp < 248 || udp > total - 20)
        return;
    const u8 *b = p + 28;
    if (b[0] != 2 || b[1] != 1 || b[2] != 6 || netint(b + 4, 4) != xid ||
        netint(b + 236, 4) != 0x63825363)
        return;
    for (int i = 0; i < 6; i++)
        if (b[28 + i] != mac[i])
            return;
    u32 srv = 0, g = 0, lease = 300;
    int kind = 0;
    for (int i = 240; i < udp - 8;) {
        int code = b[i++];
        if (code == 255)
            break;
        if (!code)
            continue;
        if (i >= udp - 8)
            return;
        int sz = b[i++];
        if (sz > udp - 8 - i)
            return;
        if (code == 53 && sz == 1)
            kind = b[i];
        if (code == 54 && sz == 4)
            srv = netint(b + i, 4);
        if (code == 3 && sz >= 4)
            g = netint(b + i, 4);
        if (code == 51 && sz == 4)
            lease = netint(b + i, 4);
        i += sz;
    }
    if (kind == 2 && phase == 1 && srv) {
        offer = netint(b + 16, 4);
        if (!offer)
            return;
        server = srv;
        phase = 2;
        dhcp(1);
    } else if (kind == 5 && phase == 2 && server == srv && netint(b + 16, 4) == offer) {
        phase = 3;
        ip = offer;
        gateway = g;
        lease_end = ticks + 60 * (lease > 86400 ? 86400 : lease);
        serial("DHCP lease acquired\r\n");
        if (g)
            arp(0, g, 0);
    } else if (kind == 6 && server == srv)
        phase = 0;
}
void network_poll(void) {
    if (!nic)
        return;
    for (int i = 0; i < 4; i++) {
        u16 s = rx[ri].status;
        if (s & 0x8000)
            break;
        __sync_synchronize();
        int n = (rx[ri].msg & 4095) - 4;
        if ((s & 0x4300) == 0x300 && n >= 0 && n <= 1532)
            received(rxb[ri], n);
        rx[ri].msg = 0;
        __sync_synchronize();
        rx[ri].status = 0x8000;
        ri = (ri + 1) & 3;
    }
    if (phase > 0 && phase < 3 && (int)(ticks - deadline) >= 0)
        phase = 0;
    if (phase == 3 && (int)(ticks - lease_end) >= 0) {
        phase = 0;
        ip = gateway = 0;
        neighbour = 0;
    }
}
static char *iptext(char *p, u32 a) {
    for (int i = 3; i >= 0; i--) {
        int v = (a >> (8 * i)) & 255;
        if (v >= 100)
            *p++ = '0' + v / 100;
        if (v >= 10)
            *p++ = '0' + v / 10 % 10;
        *p++ = '0' + v % 10;
        if (i)
            *p++ = '.';
    }
    *p = 0;
    return p;
}
void network_cmd(int acquire, void (*line)(const char *)) {
    if (!nic) {
        line("No supported AMD PCnet adapter bound.");
        return;
    }
    if (acquire) {
        ip = gateway = neighbour = 0;
        phase = 1;
        xid = 0x46524553 ^ ticks ^ netint(mac + 2, 4);
        deadline = ticks + 600;
        dhcp(0);
    }
    if (phase != 3) {
        line(phase ? "DHCP in progress; type net to check."
                   : "PCnet ready / no lease. Use net dhcp.");
        return;
    }
    char buf[64];
    char *p = cat(buf, "IPv4  ");
    iptext(p, ip);
    line(buf);
    p = cat(buf, "Gateway  ");
    p = iptext(p, gateway);
    cat(p, neighbour ? " / reachable" : " / not confirmed");
    line(buf);
}
