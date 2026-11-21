#ifndef FRESH_WEBNET_H
#define FRESH_WEBNET_H
#include "platform.h"
/* Ethernet adapter hooks. Host-order IPv4 addresses. */
int lan_send(const u8 *frame, int size);
int lan_acquire(void);
void web_config(u32 ip, u32 mask, u32 gw, u32 dns, const u8 mac[6]);
void web_frame(const u8 *, int);
void web_tick(void);
void web_cancel(void);
const char *web_get(const char *host, int port, const char *path);
/* done: 0 busy/idle, +1 body, -1 error; consume once. */
int web_result(const char **text, int *plain);
#endif
