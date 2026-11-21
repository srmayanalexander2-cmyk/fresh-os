/* Bounded HKCU hive for native user processes. Only indices, never pointers,
 * are serialized. Keep the previous generation until its successor is synced. */
#ifdef __x86_64__
#define REG_KEYS 64
#define REG_VALUES 96
#define REG_DATA 1024
struct RegValue {
    u32 key, type, size, live;
    char name[80];
    u8 data[REG_DATA];
};
struct Hive {
    u32 keys;
    char paths[REG_KEYS][160];
    struct RegValue values[REG_VALUES];
};
static struct Hive hive, trial;
static u32 generation, next_token;
static int hive_ready;
struct KeyHandle {
    u32 token, key, access;
};
static struct KeyHandle key_handles[PROCESS_BANKS][16];
static const char *const snapshots[] = {"/FREG0.DAT", "/FREG1.DAT"};
static u32 hive_checksum(u32 seq, const struct Hive *h) {
    u32 x = 2166136261u ^ seq;
    const u8 *p = (const u8 *)h;
    for (unsigned i = 0; i < sizeof *h; i++)
        x = (x ^ p[i]) * 16777619u;
    return x;
}
static int hive_valid(const struct Hive *h) {
    if (!h->keys || h->keys > REG_KEYS || h->paths[0][0])
        return 0;
    for (unsigned i = 0; i < h->keys; i++)
        if (h->paths[i][sizeof h->paths[i] - 1])
            return 0;
    for (unsigned i = 0; i < REG_VALUES; i++) {
        const struct RegValue *z = &h->values[i];
        if (z->live &&
            (z->key >= h->keys || z->size > REG_DATA || z->name[sizeof z->name - 1]))
            return 0;
    }
    return 1;
}
static int open_hive(VM *v) {
    if (hive_ready)
        return 0;
    if (!mount_files(v))
        return 21;
    int found = 0, damaged = 0;
    for (unsigned i = 0; i < 2; i++) {
        FIL f;
        u32 header[4];
        UINT n = 0;
        FRESULT e = f_open(&f, snapshots[i], FA_READ);
        if (e == FR_NO_FILE)
            continue;
        if (e != FR_OK)
            return fs_error(e);
        int ok = f_size(&f) == sizeof header + sizeof trial &&
                 f_read(&f, header, sizeof header, &n) == FR_OK && n == sizeof header &&
                 header[0] == 0x31475246 && header[2] == sizeof trial &&
                 (header[1] & 1) == i && f_read(&f, &trial, sizeof trial, &n) == FR_OK &&
                 n == sizeof trial && hive_valid(&trial) &&
                 hive_checksum(header[1], &trial) == header[3];
        if (f_close(&f) != FR_OK)
            return 1117;
        if (!ok) {
            damaged = 1;
            continue;
        }
        if (!found || (int32_t)(header[1] - generation) > 0) {
            memcpy(&hive, &trial, sizeof hive);
            generation = header[1];
            found = 1;
        }
    }
    if (!found) {
        if (damaged)
            return 1015;
        clear((u8 *)&hive, sizeof hive);
        hive.keys = 1;
        generation = 0;
    }
    hive_ready = 1;
    return 0;
}
static int commit_hive(VM *v) {
    if (v->registry_writes >= 48)
        return 1816; /* bounded disk work per launch */
    v->registry_writes++;
    u32 seq = generation + 1;
    u32 header[4] = {0x31475246, seq, sizeof trial, hive_checksum(seq, &trial)};
    FIL f;
    UINT n = 0;
    FRESULT e = f_open(&f, snapshots[seq & 1], FA_WRITE | FA_CREATE_ALWAYS);
    if (e != FR_OK)
        return fs_error(e);
    /* An interrupted write leaves the other file intact; a partially written
     * snapshot cannot satisfy the length and checksum checks on remount. */
    int ok = f_write(&f, header, sizeof header, &n) == FR_OK && n == sizeof header &&
             f_write(&f, &trial, sizeof trial, &n) == FR_OK && n == sizeof trial &&
             f_sync(&f) == FR_OK;
    if (f_close(&f) != FR_OK || !ok)
        return 1117;
    memcpy(&hive, &trial, sizeof hive);
    generation = seq;
    return 0;
}
static int reg_text(VM *v, u64 src, char *out, unsigned cap) {
    if (!src) {
        *out = 0;
        return 0;
    }
    for (unsigned i = 0; i < cap; i++) {
        unsigned c = readv(v, src + 2 * i, 2, R);
        if (v->err)
            return 87;
        if (c && (c < 32 || c > 126))
            return 123;
        out[i] = c >= 'A' && c <= 'Z' ? c + 32 : c;
        if (!c)
            return 0;
    }
    return 206;
}
static int reg_key(const struct Hive *h, const char *path) {
    for (unsigned i = 0; i < h->keys; i++)
        if (equal(h->paths[i], path))
            return i;
    return -1;
}
static int reg_api(VM *v, int k, const u64 *a) {
    if (!v->native)
        return 50;
    struct KeyHandle *slots = key_handles[process_bank], *handle = 0;
    unsigned key = 0, access = 0xf003fu;
    int root = a[0] == 0x80000001u || a[0] == 0xffffffff80000001ull;
    if (!root) {
        for (unsigned i = 0; i < 16; i++)
            if (slots[i].token && slots[i].token == a[0]) {
                handle = slots + i;
                break;
            }
        if (!handle)
            return (u32)a[0] == 0x80000002u ? 5 : 6;
        key = handle->key;
        access = handle->access;
    }
    if (k == REG_CLOSE) {
        if (handle)
            handle->token = 0;
        return 0;
    }
    int err = open_hive(v);
    if (err)
        return err;
    if (k == REG_OPEN || k == REG_CREATE) {
        unsigned create = k == REG_CREATE, desired = (u32)a[create ? 5 : 3];
        if ((u32)a[2] || (create && (a[3] || (u32)a[4] || a[6])))
            return 87;
        if (desired & ~0xf003fu)
            return 50; /* no WOW64 alternate view */
        if ((desired & access) != desired || (create && !(access & 4)))
            return 5;
        u8 *out = mem(v, a[create ? 7 : 4], ptrsz(v), WRT), *disp = 0;
        if (!out)
            return 87;
        if (create && a[8] && !(disp = mem(v, a[8], 4, WRT)))
            return 87;
        char sub[160], path[160];
        err = reg_text(v, a[1], sub, sizeof sub);
        if (err)
            return err;
        unsigned n = length(hive.paths[key]);
        memcpy(path, hive.paths[key], n + 1);
        if (n + !!n + length(sub) >= sizeof path)
            return 206;
        if (n && *sub)
            path[n++] = '\\';
        unsigned ix = 0;
        while (ix < 16 && slots[ix].token)
            ix++;
        if (ix == 16)
            return 4;
        int changed = 0;
        if (create)
            memcpy(&trial, &hive, sizeof hive);
        for (unsigned i = 0; sub[i]; i++) {
            if (sub[i] == '\\' && (!i || sub[i - 1] == '\\' || !sub[i + 1]))
                return 123;
            path[n++] = sub[i];
            path[n] = 0;
            if (!sub[i + 1] || sub[i + 1] == '\\') {
                int id = reg_key(create ? &trial : &hive, path);
                if (id < 0) {
                    if (!create)
                        return 2;
                    if (trial.keys == REG_KEYS)
                        return 8;
                    id = trial.keys++;
                    strset(trial.paths[id], sizeof trial.paths[id], path);
                    changed = 1;
                }
                key = id;
            }
        }
        if (changed && (err = commit_hive(v)) != 0)
            return err;
        if (++next_token < 0x1000 || next_token >= 0x80000000u)
            next_token = 0x1000;
        slots[ix] = (struct KeyHandle){next_token, key, desired};
        put(out, ptrsz(v), next_token);
        if (disp)
            put(disp, 4, changed ? 1 : 2);
        return 0;
    }
    if (!(access & (k == REG_QUERY ? 1 : 2)))
        return 5;
    char name[80];
    err = reg_text(v, a[1], name, sizeof name);
    if (err)
        return err;
    int which = -1, free_slot = -1;
    for (unsigned i = 0; i < REG_VALUES; i++) {
        const struct RegValue *z = &hive.values[i];
        if (!z->live) {
            if (free_slot < 0)
                free_slot = i;
        } else if (z->key == key && equal(z->name, name)) {
            which = i;
            break;
        }
    }
    if (k == REG_QUERY) {
        if (a[2] || (a[4] && !a[5]))
            return 87;
        if (which < 0)
            return 2;
        const struct RegValue *z = &hive.values[which];
        u32 cap = a[4] ? readv(v, a[5], 4, R) : 0;
        if (a[3])
            writev(v, a[3], 4, z->type);
        if (a[5])
            writev(v, a[5], 4, z->size);
        if (a[4] && z->size) {
            if (cap < z->size)
                return 234;
            u8 *dst = mem(v, a[4], z->size, WRT);
            if (!dst)
                return 87;
            memcpy(dst, z->data, z->size);
        }
        return 0;
    }
    memcpy(&trial, &hive, sizeof hive);
    if (k == REG_DELETE_VALUE) {
        if (which < 0)
            return 2;
        clear((u8 *)&trial.values[which], sizeof trial.values[which]);
        return commit_hive(v);
    }
    u32 type = a[3], n = a[5];
    if ((u32)a[2] || n > REG_DATA || (n && !a[4]))
        return 87;
    if (type != 0 && type != 1 && type != 2 && type != 3 && type != 4 && type != 7 &&
        type != 11)
        return 50;
    if ((type == 4 && n != 4) || (type == 11 && n != 8))
        return 87;
    if (which < 0)
        which = free_slot;
    if (which < 0)
        return 8;
    struct RegValue *z = &trial.values[which];
    clear((u8 *)z, sizeof *z);
    z->live = 1;
    z->key = key;
    z->type = type;
    z->size = n;
    strset(z->name, sizeof z->name, name);
    if (n) {
        u8 *src = mem(v, a[4], n, R);
        if (!src)
            return 87;
        memcpy(z->data, src, n);
    }
    return commit_hive(v);
}
#endif
