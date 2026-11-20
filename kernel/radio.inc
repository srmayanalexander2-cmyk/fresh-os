/* Optional external radio transports. No probing random UARTs at boot: the
 * user explicitly selects the wired ESP-AT (COM2) or H4 (COM3) adapter. */
static inline u8 uart_in(u16 p) {
    u8 v;
    __asm__ volatile("inb %1,%0" : "=a"(v) : "d"(p));
    return v;
}
static inline void uart_out(u16 p, u8 v) { __asm__ volatile("outb %0,%1" ::"a"(v), "d"(p)); }
static int uart_start(u16 p) {
    u8 old = uart_in(p + 7);
    uart_out(p + 7, 0x5a);
    int ok = uart_in(p + 7) == 0x5a;
    uart_out(p + 7, old);
    if (!ok)
        return 0;
    uart_out(p + 1, 0);
    uart_out(p + 3, 0x80);
    uart_out(p, 1);
    uart_out(p + 1, 0);
    uart_out(p + 3, 3);
    uart_out(p + 2, 7);
    uart_out(p + 4, 3);
    return 1;
}
static int sendbyte(u16 p, u8 c) {
    u32 t = ticks;
    while (!(uart_in(p + 5) & 32))
        if (ticks - t >= 15)
            return 0;
    uart_out(p, c);
    return 1;
}
#define WIFI 0x2f8
#define BT 0x3e8
static int wstate, bstate, wp, bp, secret;
static u32 wt, bt;
static char wire[180], ssid[33], pass[64], action[12];
static u8 event[258];
static char pingcmd[82];
static void wipe(char *p, int n) {
    volatile char *v = p;
    while (n--)
        *v++ = 0;
}
static void wend(void) {
    wstate = wp = secret = 0;
    wipe(pass, sizeof pass);
    wipe(wire, sizeof wire);
}
static void report(const char *s) {
    say(s);
    dirty = 1;
}
static int wat(const char *s) {
    while (*s)
        if (!sendbyte(WIFI, *s++))
            return 0;
    return sendbyte(WIFI, 13) && sendbyte(WIFI, 10);
}
static int safeat(const char *s, int max) {
    if (len(s) > max)
        return 0;
    for (; *s; s++)
        if (*s < 32 || *s > 126 || *s == '"' || *s == '\\' || *s == ',')
            return 0;
    return 1;
}
static void wifi_start(void) {
    wp = 0;
    if (!uart_start(WIFI) || !wat("ATE0")) {
        report("ESP-AT: COM2 unavailable.");
        wend();
        return;
    }
    wt = ticks;
    wstate = 1;
    report("Contacting ESP-AT station on COM2...");
}
static void wifi_cmd(const char *s) {
    if (wstate) {
        report("Wi-Fi command in progress.");
        return;
    }
    if (prefix(s, "join ") && s[5] && safeat(s + 5, 32)) {
        copy(ssid, s + 5, 33);
        copy(action, "join", 12);
        secret = 1;
        report("Enter Wi-Fi passphrase (hidden), or Esc to cancel.");
        return;
    }
    if (prefix(s, "ping ") && s[5] && len(s + 5) < 64) {
        for (const char *p = s + 5; *p; p++)
            if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '.' ||
                  *p == '-')) {
                report("Use an IPv4 address or lowercase host name.");
                return;
            }
        copy(pingcmd, "AT+PING=\"", sizeof pingcmd);
        copy(pingcmd + len(pingcmd), s + 5, 65);
        append(pingcmd, sizeof pingcmd, '"');
        copy(action, "ping", 12);
        wifi_start();
        return;
    }
    if (eq(s, "scan") || eq(s, "ip") || eq(s, "leave")) {
        copy(action, s, 12);
        wifi_start();
        return;
    }
    report("wifi scan | join SSID | ip | ping HOST | leave   (ESP-AT, COM2)");
}
static void wline(void) {
    if (eq(wire, "ERROR") || eq(wire, "FAIL") || prefix(wire, "+CWJAP:")) {
        report("ESP-AT rejected the request. Check adapter / credentials.");
        wend();
        return;
    }
    if (eq(wire, "OK")) {
        int ok = 1;
        if (wstate == 1) {
            wstate = 2;
            ok = wat("AT+CWMODE=1");
        } else if (wstate == 2) {
            wstate = 3;
            if (eq(action, "join")) {
                char cmd[124] = "AT+CWJAP=\"";
                int i = len(cmd);
                copy(cmd + i, ssid, 33);
                i = len(cmd);
                copy(cmd + i, "\",\"", 4);
                i += 3;
                copy(cmd + i, pass, 64);
                i = len(cmd);
                copy(cmd + i, "\"", 2);
                ok = wat(cmd);
                wipe(cmd, sizeof cmd);
                wipe(pass, sizeof pass);
            } else
                ok = wat(eq(action, "ping")   ? pingcmd
                         : eq(action, "scan") ? "AT+CWLAP"
                         : eq(action, "ip")   ? "AT+CIFSR"
                                              : "AT+CWQAP");
        } else {
            report("ESP-AT request complete.");
            serial("ESP-AT complete\r\n");
            wend();
            return;
        }
        wt = ticks;
        if (!ok) {
            report("Wi-Fi transmit timeout.");
            wend();
        }
    } else if (wstate == 3 && (prefix(wire, "+PING:") || prefix(wire, "+CWLAP:") ||
                               prefix(wire, "+CIFSR:") || prefix(wire, "WIFI ")))
        report(wire);
}
static void hci(int opcode, int scan) {
    u8 cmd[] = {1, opcode & 255, opcode >> 8, scan ? 5 : 0, 0x33, 0x8b, 0x9e, 4, 8};
    for (int i = 0; i < (scan ? 9 : 4); i++)
        if (!sendbyte(BT, cmd[i])) {
            report("HCI transmit timeout.");
            bstate = 0;
            break;
        }
    bt = ticks;
}
static void bluetooth(const char *s) {
    if (bstate) {
        report("HCI busy.");
        return;
    }
    if (!eq(s, "info") && !eq(s, "scan")) {
        report("bt info | scan   (HCI H4, COM3)");
        return;
    }
    if (!uart_start(BT)) {
        report("Bluetooth UART unavailable.");
        return;
    }
    bp = 0;
    bstate = eq(s, "scan") ? 2 : 1;
    hci(0xc03, 0);
}
static void bdaddr(const u8 *p) {
    char s[36] = "Bluetooth  ";
    int n = len(s);
    for (int i = 5; i >= 0; i--) {
        s[n++] = "0123456789ABCDEF"[p[i] >> 4];
        s[n++] = "0123456789ABCDEF"[p[i] & 15];
        if (i)
            s[n++] = ':';
    }
    s[n] = 0;
    report(s);
}
static void hevent(void) {
    int ev = event[1], n = event[2];
    u8 *p = event + 3;
    if (ev == 0xe && n >= 4) {
        int op = p[1] | p[2] << 8;
        if (p[3]) {
            report("HCI command rejected.");
            bstate = 0;
        } else if (op == 0xc03 && bstate <= 2) {
            int scan = bstate == 2;
            bstate = scan ? 4 : 3;
            hci(scan ? 0x401 : 0x1009, scan);
        } else if (op == 0x1009 && bstate == 3 && n >= 10) {
            bdaddr(p + 4);
            serial("Bluetooth controller responded\r\n");
            bstate = 0;
        }
    } else if (ev == 0xf && n >= 4 && p[0]) {
        report("Bluetooth inquiry refused.");
        bstate = 0;
    } else if (bstate == 4 && (ev == 2 || ev == 0x22 || ev == 0x2f) && n >= 7) {
        /* Inquiry results are packed records, not an array of addresses. */
        int stride = ev == 0x2f ? 254 : 14;
        if (ev == 0x22 && p[0] && n == 1 + 15 * p[0])
            stride = 15;
        if (n == 1 + stride * p[0])
            for (int i = 0; i < p[0]; i++)
                bdaddr(p + 1 + stride * i);
    } else if (ev == 1 && bstate == 4) {
        report("Bluetooth inquiry ended.");
        serial("Bluetooth inquiry ended\r\n");
        bstate = 0;
    }
}
static void radios_poll(void) {
    if (wstate && ticks - wt > 1800) {
        report("Wi-Fi timeout: check COM2 adapter (115200 8N1).");
        wend();
    }
    for (int i = 0; i < 256 && wstate && (uart_in(WIFI + 5) & 1); i++) {
        int c = uart_in(WIFI);
        if (c == 10) {
            wire[wp] = 0;
            wline();
            wp = 0;
        } else if (c >= 32 && c < 127 && wp < 179)
            wire[wp++] = c;
    }
    if (bstate && ticks - bt > 720) {
        report("Bluetooth timeout: check COM3 H4 controller.");
        bstate = 0;
    }
    for (int i = 0; i < 256 && bstate && (uart_in(BT + 5) & 1); i++) {
        int c = uart_in(BT);
        if (!bp && c != 4)
            continue;
        event[bp++] = c;
        if (bp >= 3 && bp == 3 + event[2]) {
            hevent();
            bp = 0;
        }
    }
}
