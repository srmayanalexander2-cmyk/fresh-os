#pragma once
#include "task64.h"
#ifdef __x86_64__
extern u64 region_bases[2];
#define region_base region_bases[process_bank]
#define REGION_BASE region_base
#define REGION_BYTES (2u << 20)
void region_reset(int wide);
u64 region_alloc(u64 addr, u64 bytes, int prot, int reserve_only, int windows);
int region_commit(u64 addr, u64 bytes, int prot);
int region_protect(u64 addr, u64 bytes, int prot, int *old);
int region_free(u64 addr, u64 bytes, int windows_mode);
void region_query(u64 addr, u64 *base, u64 *bytes, int *current, int *original);
u8 *region_ptr(u64 addr, u32 bytes, int access);
#endif
