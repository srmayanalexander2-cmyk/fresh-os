#pragma once
#include "compat.h"
#ifdef __x86_64__
struct UserRegs {
    u64 r[16], ip, flags;
};
_Static_assert(offsetof(struct UserRegs, ip) == 128 &&
                   offsetof(struct UserRegs, flags) == 136,
               "usermode.S register layout");
extern struct UserRegs user_regs __attribute__((visibility("hidden")));
void task64_init(ptr image_base, u64 image_size);
void task64_push_child(void);
void task64_pop_child(void);
int task64_available(void);
int task64_begin(void);
int task64_page(u64 va, void *backing, int perms);
int task64_edit(u64, void *, int);
extern u64 user_fs, user_cs, user_teb;
unsigned user_resume(void);
#endif
