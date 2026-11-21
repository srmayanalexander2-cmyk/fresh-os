#ifndef FRESH_LW_ARCH_H
#define FRESH_LW_ARCH_H
#define LWIP_NO_INTTYPES_H 1
#define LWIP_NO_LIMITS_H 1
#define LWIP_NO_CTYPE_H 1
#define INT_MAX __INT_MAX__
#define BYTE_ORDER LITTLE_ENDIAN
#define LWIP_PLATFORM_DIAG(x) ((void)0)
void lw_fatal(const char *);
#define LWIP_PLATFORM_ASSERT(x) lw_fatal(x)
unsigned int lw_rand(void);
#define LWIP_RAND() lw_rand()
#endif
