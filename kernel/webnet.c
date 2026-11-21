#include "webnet.h"
/* Built-ins: cross-unit callback addresses must be direct RIP-relative references
 * on EFI. The PE loader does not provide an ELF global offset table. */
#pragma GCC visibility push(hidden)
#include "lwip/init.h"
#include "lwip/etharp.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"
#pragma GCC visibility pop
#include <string.h>
static struct netif iface;
static struct tcp_pcb *conn;
static u32 generation, until, randstate = 0xac54312;
static int initialized, ready, state, result, used, plaintext;
static char response[24576], host[128], request[512];
static const char *answer;
void lw_fatal(const char *s) {
    serial("Network assertion: ");
    serial(s);
    serial("\r\n");
    halt();
}
u32 sys_now(void) { /* Modular milliseconds; 60 Hz PIT. */
    return ticks / 3 * 50 + (ticks % 3) * 50 / 3;
}
u32 lw_rand(void) {
    u32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    randstate ^= lo ^ hi ^ ticks;
    randstate ^= randstate << 13;
    randstate ^= randstate >> 17;
    return randstate ^= randstate << 5;
}
static err_t linkout(struct netif *n, struct pbuf *p) {
    (void)n;
    u8 buf[1518];
    if (p->tot_len > sizeof buf)
        return ERR_BUF;
    pbuf_copy_partial(p, buf, p->tot_len, 0);
    return lan_send(buf, p->tot_len) ? ERR_OK : ERR_MEM;
}
static err_t bindif(struct netif *n) {
    n->name[0] = 'a';
    n->name[1] = 'e';
    n->mtu = 1500;
    n->hwaddr_len = 6;
    n->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    n->output = etharp_output;
    n->linkoutput = linkout;
    return ERR_OK;
}
void web_cancel(void) {
    generation++;
    if (conn) {
        tcp_arg(conn, 0);
        tcp_err(conn, 0);
        tcp_recv(conn, 0);
        tcp_abort(conn);
        conn = 0;
    }
    state = result = used = 0;
}
void web_config(u32 ip, u32 mask, u32 gw, u32 ns, const u8 mac[6]) {
    if (!initialized && !ip)
        return;
    ip4_addr_t a = {lwip_htonl(ip)}, m = {lwip_htonl(mask)}, g = {lwip_htonl(gw)},
               dns = {lwip_htonl(ns)};
    if (!initialized) {
        lwip_init();
        netif_add(&iface, &a, &m, &g, 0, bindif, ethernet_input);
        netif_set_default(&iface);
        initialized = 1;
    }
    memcpy(iface.hwaddr, mac, 6);
    netif_set_addr(&iface, &a, &m, &g);
    dns_setserver(0, &dns);
    ready = ip != 0;
    if (ready) {
        netif_set_up(&iface);
        netif_set_link_up(&iface);
    } else {
        int interrupted = state != 0;
        web_cancel();
        if (interrupted) {
            answer = "Network configuration changed";
            result = -1;
        }
        netif_set_down(&iface);
        netif_set_link_down(&iface);
    }
}
void web_frame(const u8 *f, int n) {
    if (!ready || n < 14 || n > 1518)
        return;
    struct pbuf *p = pbuf_alloc(PBUF_RAW, n, PBUF_POOL);
    if (!p)
        return;
    if (pbuf_take(p, f, n) != ERR_OK || iface.input(p, &iface) != ERR_OK)
        pbuf_free(p);
}
static void failed(const char *s) {
    web_cancel();
    answer = s;
    result = -1;
}
static int begins(const char *a, const char *b) {
    while (*b) {
        char c = *a++;
        if (c >= 'A' && c <= 'Z')
            c += 32;
        if (c != *b++)
            return 0;
    }
    return 1;
}
static int portnum;
static void finish(void) {
    response[used] = 0;
    if (used < 12 || !begins(response, "http/1.") || response[8] != ' ') {
        failed("Invalid HTTP response");
        return;
    }
    if (response[9] != '2') {
        static char msg[] = "HTTP status 000 (redirects are not followed)";
        memcpy(msg + 12, response + 9, 3);
        failed(msg);
        return;
    }
    int end = 0;
    for (int i = 0; i + 3 < used; i++)
        if (!memcmp(response + i, "\r\n\r\n", 4)) {
            end = i + 4;
            break;
        }
    if (!end) {
        failed("Incomplete HTTP headers");
        return;
    }

    plaintext = 0;
    for (int i = 0; i < end - 2; i++) {
        if (i && response[i - 1] != '\n')
            continue;
        char *h = response + i;
        if (begins(h, "transfer-encoding:")) {
            failed("Unsupported transfer encoding (requires identity)");
            return;
        }
        if (begins(h, "content-encoding:") && !begins(h, "content-encoding: identity\r")) {
            failed("Compressed pages are not supported");
            return;
        }
        if (begins(h, "content-type:")) {
            h += 13;
            while (*h == ' ')
                h++;
            if (begins(h, "text/plain"))
                plaintext = 1;
            else if (!begins(h, "text/html")) {
                failed("Not an HTML or text page");
                return;
            }
        }
        if (begins(h, "content-length:")) {
            h += 15;
            while (*h == ' ')
                h++;
            int k = 0;
            if (*h < '0' || *h > '9') {
                failed("Invalid HTTP length");
                return;
            }
            while (*h >= '0' && *h <= '9' && k <= used)
                k = k * 10 + *h++ - '0';
            if (*h != '\r' || k != used - end) {
                failed("Incomplete or oversized HTTP response");
                return;
            }
        }
    }

    for (int i = end; i < used; i++)
        if (!response[i]) {
            failed("Binary response not supported");
            return;
        }
    state = 0;
    result = 1;
    answer = response + end;
}
static err_t receive(void *arg, struct tcp_pcb *p, struct pbuf *data, err_t err) {
    (void)arg;
    (void)err;
    if (!data) {
        tcp_recv(p, 0);
        tcp_err(p, 0);
        conn = 0;
        err_t e = tcp_close(p);
        if (e != ERR_OK)
            tcp_abort(p);
        finish();
        return e == ERR_OK ? ERR_OK : ERR_ABRT;
    }
    if (data->tot_len >= sizeof response - (unsigned)used) {
        pbuf_free(data);
        failed("Page exceeds 24 KiB response limit");
        return ERR_ABRT;
    }
    pbuf_copy_partial(data, response + used, data->tot_len, 0);
    used += data->tot_len;
    tcp_recved(p, data->tot_len);
    pbuf_free(data);
    return ERR_OK;
}
static void lost(void *arg, err_t e) {
    (void)arg;
    (void)e;
    conn = 0;
    failed("TCP connection closed before the page was received");
}
static err_t connected(void *arg, struct tcp_pcb *p, err_t err) {
    (void)arg;
    if (err || tcp_write(p, request, strlen(request), TCP_WRITE_FLAG_COPY) != ERR_OK) {
        failed("Could not send HTTP request");
        return ERR_ABRT;
    }
    tcp_output(p);
    state = 4;
    return ERR_OK;
}
static void resolved(const char *name, const ip_addr_t *a, void *arg) {
    (void)name;
    if ((u32)(unsigned long)arg != generation || state != 2)
        return;
    if (!a) {
        failed("DNS name not found");
        return;
    }
    conn = tcp_new();
    if (!conn) {
        failed("No free TCP connection");
        return;
    }
    tcp_err(conn, lost);
    tcp_recv(conn, receive);
    state = 3;
    if (tcp_connect(conn, a, portnum, connected) != ERR_OK)
        failed("TCP route or connection unavailable");
}
void web_tick(void) {
    if (initialized)
        sys_check_timeouts();
    if (state && (int)(ticks - until) >= 0) {
        failed("Request timed out (check DHCP, DNS and server)");
        return;
    }
    if (state == 1 && ready) {
        state = 2;
        ip_addr_t a;
        err_t e = dns_gethostbyname(host, &a, resolved, (void *)(unsigned long)generation);
        if (e == ERR_OK)
            resolved(host, &a, (void *)(unsigned long)generation);
        else if (e != ERR_INPROGRESS)
            failed("DNS resolver unavailable");
    }
}
const char *web_get(const char *h, int port, const char *path) {
    web_cancel();
    if (!h[0] || strlen(h) >= sizeof host || strlen(path) > 224 || port < 1 || port > 65535)
        return "Address too long or invalid";
    if (!ready && !lan_acquire())
        return "No supported Ethernet adapter bound";
    memcpy(host, h, strlen(h) + 1);
    portnum = port;
    char *q = request;
#define APPEND(s)                                                                            \
    do {                                                                                     \
        const char *t = (s);                                                                 \
        while (*t)                                                                           \
            *q++ = *t++;                                                                     \
    } while (0)
    APPEND("GET ");
    APPEND(path);
    APPEND(" HTTP/1.0\r\nHost: ");
    APPEND(h);
    if (port != 80) {
        *q++ = ':';
        char tmp[5];
        int k = 0;
        do {
            tmp[k++] = '0' + port % 10;
            port /= 10;
        } while (port);
        while (k)
            *q++ = tmp[--k];
    }
    APPEND("\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n");
    *q = 0;
    state = 1;
    until = ticks + 900;
    return 0;
}
int web_result(const char **text, int *plain) {
    int r = result;
    *text = answer;
    *plain = plaintext;
    result = 0;
    return r;
}
