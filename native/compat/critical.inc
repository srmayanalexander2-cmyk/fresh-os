/* The process model has exactly one thread per address space. Recursive entry
 * and final release still change observable state; different processes never
 * share this table, even when their objects have the same virtual address. */
typedef struct {
    u64 key;
    u32 owner, depth;
} Critical;
static Critical criticals[PROCESS_BANKS][128];
static u64 critical_api(VM *v, int k, const u64 *a) {
    unsigned w = ptrsz(v);
    u8 *p = mem(v, a[0], 4 * w + 8, WRT);
    if (!p)
        return 0;
    Critical *s = 0, *free_slot = 0;
    for (unsigned i = 0; i < 128; i++) {
        Critical *c = &criticals[process_bank][i];
        if (c->key == a[0]) {
            s = c;
            break;
        }
        if (!c->key && !free_slot)
            free_slot = c;
    }
    if (k <= CS_INIT_EX) {
        if (k == CS_INIT_EX && ((u32)a[2] & ~0x01000000u))
            return security_error(v, 87);
        if (s || !free_slot) {
            if (k == CS_INIT)
                fail(v, "Critical section initialization failed");
            return security_error(v, s ? 87 : 8);
        }
        s = free_slot;
        *s = (Critical){a[0], 0, 0};
        clear(p, 4 * w + 8);
        put(p, w, ~0ull); /* no debug information */
    } else if (!s) {
        fail(v, "Uninitialized critical section");
        return 0;
    } else if (k == CS_DELETE) {
        if (s->depth) {
            fail(v, "Deleting an owned critical section");
            return 0;
        }
        s->key = 0;
        clear(p, 4 * w + 8);
        return 0;
    } else if (k == CS_LEAVE) {
        if (!s->depth || s->owner != current_pid()) {
            fail(v, "Critical section not owned");
            return 0;
        }
        if (!--s->depth)
            s->owner = 0;
    } else {
        if ((s->owner && s->owner != current_pid()) || s->depth == 0x7fffffffu) {
            if (k == CS_ENTER)
                fail(v, "Critical section cannot be acquired");
            return 0;
        }
        s->owner = current_pid();
        s->depth++;
    }
    put(p + w, 4, s->depth - 1);
    put(p + w + 4, 4, s->depth);
    put(p + w + 8, w, s->owner);
    /* One CPU: spin count is always zero. */
    return 1;
}
