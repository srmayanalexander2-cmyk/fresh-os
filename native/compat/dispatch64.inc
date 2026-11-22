#include "linux64.inc"
/* Machine-code path: the CPU handles ordinary instructions; on a boundary,
 * use the same checked register/memory ABI adapters as the portable engine. */
static u8 gates[8192] __attribute__((aligned(4096)));
static void call_loader(VM *v, u64 target, int reason) {
    memset(v->r, 0, sizeof v->r);
    v->r[4] = STACK + STACK_SIZE - 64;
    if (reason >= 0) {
        if (v->wide) {
            v->r[1] = v->base;
            v->r[2] = reason;
        } else {
            push(v, 0);
            push(v, reason);
            push(v, v->base);
        }
    }
    push(v, RETURN);
    v->ip = target;
}
static int native64(VM *v) {
    if (!task64_available())
        return 0;
    if (!task64_begin())
        goto bad;
    user_cs = v->wide ? 0x23 : 0x3b;
    region_reset(v->wide);
    if (v->linuxabi) {
        u64 bytes = REGION_BYTES / 2;
        CURRENT.brk_base = REGION_BASE + REGION_BYTES - bytes;
        CURRENT.brk_current = CURRENT.brk_base;
        CURRENT.brk_limit = CURRENT.brk_base + bytes;
        if (region_alloc(CURRENT.brk_base, bytes, R | WRT, 1, 0) != CURRENT.brk_base)
            goto bad;
    }
    clear((u8 *)app_heaps[process_bank], sizeof app_heaps[process_bank]);
    if (v->base < REGION_BASE + REGION_BYTES && v->base + v->size > REGION_BASE)
        goto bad;

    v->native = 1;
    for (u32 i = 0; i < v->size; i += 4096) {
        int f = CURRENT.perm[i / 4096] & 7;
        if (f && !task64_page(v->base + i, CURRENT.image + i, f))
            goto bad;
    }
    for (u32 i = 0; i < STACK_SIZE; i += 4096)
        if (!task64_page(STACK + i, CURRENT.stackmem + i, 3))
            goto bad;
    if (!v->linuxabi) {
        for (u32 i = 0; i < HEAP_SIZE; i += 4096)
            if (!task64_page(HEAP + i, CURRENT.heap + i, 3))
                goto bad;
        memset(gates, 0xcc, sizeof gates);
        for (int i = -2; i < NIMPORT; i++) {
            int j = 4096 + 16 * i;
            gates[j] = 0xcd;
            gates[j + 1] = 0x81;
        }
        for (int i = 0; i < 2; i++)
            if (!task64_page(ABI - 4096 + i * 4096, gates + i * 4096, 5))
                goto bad;
    }
    v->res->entered = 1;
    user_teb = v->teb;
    if (!v->wide && v->teb)
        user_fs = v->teb;
    u64 entry = v->ip;
    unsigned callback = 0;
    int loader_phase = v->ncallbacks ? 1 : -1; /* attach=1, program=-1, detach=0 */
    u32 final_code = 0;
    if (loader_phase == 1)
        call_loader(v, v->callbacks[callback++], 1);
    u32 slices = 0;
    u64 flags = 0x202;
    logline(v->linuxabi ? (v->wide ? "process: ELF64 user mode" : "process: ELF32 user mode")
                        : (v->wide ? "process: PE64 user mode" : "process: PE32 user mode"));
    while (!v->err && !v->done) {
        if (!v->budget || slices >= 180) {
            fail(v, "Process CPU limit reached");
            break;
        }
        if (v->ip >= (1ull << (v->wide ? 47 : 32)) ||
            v->r[4] >= (1ull << (v->wide ? 47 : 32))) {
            fail(v, "Invalid user context");
            break;
        }
        memcpy(user_regs.r, v->r, sizeof v->r);
        user_regs.ip = v->ip;
        user_regs.flags = (flags & 0xcd5) | 0x202;
        unsigned event = user_resume();
        memcpy(v->r, user_regs.r, sizeof v->r);
        v->ip = user_regs.ip;
        flags = user_regs.flags;
        if (event == 32) {
            slices++;
            continue;
        }
        v->budget--;
        v->res->steps++;
        if (event != (v->linuxabi ? (v->wide ? 256u : 258u) : 257u)) {
            char n[16];
            decimal(n, event);
            detail(v->res, "Application CPU exception ");
            detail(v->res, n);
            fail(v, "User fault");
            break;
        }
        if (v->linuxabi) {
            if (v->wide)
                linux_native(v);
            else
                linux32_call(v);
        } else if (v->ip == GUARD_CHECK + 2 && v->nguard && !v->wide) {
            /* MSVC x86 guard check: target in ECX, preserve all registers and flags. */
            u64 target = (u32)v->r[1];
            u64 rva = target - v->base;
            unsigned lo = 0, hi = v->nguard;
            while (lo < hi) {
                unsigned mid = lo + (hi - lo) / 2;
                if (CURRENT.guard_targets[mid] < rva)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            if (lo == v->nguard || CURRENT.guard_targets[lo] != rva)
                fail(v, "PE GuardCF rejected indirect target");
            else
                v->ip = pop(v);
        } else if (v->ip == RETURN + 2) {
            if (loader_phase >= 0 && callback < v->ncallbacks)
                call_loader(v, v->callbacks[callback++], loader_phase);
            else if (loader_phase == 1) {
                loader_phase = -1;
                call_loader(v, entry, -1);
            } else {
                v->done = 1;
                v->res->exit_code = loader_phase == 0 ? final_code : (u32)v->r[0];
            }
        } else if (v->ip >= ABI + 2 && v->ip < ABI + NIMPORT * 16 && ((v->ip - 2) & 15) == 0)
            api(v, (int)((v->ip - 2 - ABI) / 16));
        else
            fail(v, "Unsupported Windows system call");
        if (v->done && v->ncallbacks && loader_phase != 0) {
            v->done = 0;
            final_code = v->res->exit_code;
            loader_phase = 0;
            callback = 1;
            call_loader(v, v->callbacks[0], 0);
        }
    }
    return 1;
bad:
    fail(v, "Unsupported native page layout or permissions");
    return 1;
}
