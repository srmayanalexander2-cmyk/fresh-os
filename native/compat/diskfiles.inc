/* F:\ is an explicitly attached, separately marked FAT16 data disk. No host
 * paths, Home sectors or executable package headers are reachable here. */
#ifdef __x86_64__
#include "../vendor/fatfs/ff.h"
static FATFS appfs;
static int mounted;
struct OpenFile {
    FIL file;
    unsigned live;
};
static struct OpenFile open_files[2][8];
static unsigned fs_error(FRESULT e) {
    switch (e) {
    case FR_OK:
        return 0;
    case FR_NO_FILE:
        return 2;
    case FR_NO_PATH:
        return 3;
    case FR_INVALID_NAME:
        return 123;
    case FR_EXIST:
        return 183;
    case FR_LOCKED:
        return 32;
    case FR_DENIED:
    case FR_WRITE_PROTECTED:
        return 5;
    case FR_TOO_MANY_OPEN_FILES:
        return 4;
    case FR_INVALID_OBJECT:
        return 6;
    case FR_NOT_READY:
    case FR_NO_FILESYSTEM:
        return 21;
    default:
        return 1117;
    }
}
static int normalize_path(const char *input, char *path, unsigned cap) {
    unsigned used = 0, stem = 0, ext = 0, dot = 0;
    for (unsigned j = 0; j < cap; j++) {
        unsigned c = (u8)input[j];
        if (j == 0 && c != '\\' && c != '/')
            return 0;
        if (!c || c == '\\' || c == '/') {
            if (j != 0 && (!stem || (dot && !ext)))
                return 0;
            if (!c) {
                path[used] = 0;
                return 1;
            }
            stem = ext = dot = 0;
            path[used++] = '/';
            continue;
        }
        if (c == '.') {
            if (!stem || dot++)
                return 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_'))
                return 0;
            if (dot ? ++ext > 3 : ++stem > 8)
                return 0;
        }
        path[used++] = c;
    }
    return 0;
}
static int drivepath(VM *v, u64 src, int stride, char *path, unsigned cap) {
    unsigned c = readv(v, src, stride, R);
    if (!(c == 'f' || c == 'F') || readv(v, src + stride, stride, R) != ':')
        return 0;
    v->last_error = 123;
    char buf[128];
    for (unsigned j = 0; j < sizeof buf; j++) {
        c = readv(v, src + (j + 2) * stride, stride, R);
        if (v->err || c > 127)
            break;
        buf[j] = c;
        if (!c)
            return normalize_path(buf, path, cap) ? 1 : -1;
    }
    return -1;
}
static int mount_files(VM *v) {
    if (mounted)
        return 1;
    FRESULT e = f_mount(&appfs, "", 1);
    if (e != FR_OK || appfs.fs_type != FS_FAT16) {
        v->last_error = e == FR_OK ? 13 : fs_error(e);
        return 0;
    }
    return mounted = 1;
}
static int diskfile_api(VM *v, int k, const u64 *a, u64 *result) {
    if (!v->native)
        return 0;
    if (k == OPEN_A || k == OPEN_W || k == CREATE_DIR || k == DELETE_FILE) {
        char path[128];
        int found = drivepath(v, a[0], k == OPEN_A ? 1 : 2, path, sizeof path);
        if (!found)
            return 0;
        int opening = k == OPEN_A || k == OPEN_W;
        *result = opening ? addr(v, ~0ull) : 0;
        if (found < 0 || !mount_files(v))
            return 1;
        if (!opening) {
            if (k == CREATE_DIR && a[1]) {
                v->last_error = 87;
                return 1;
            }
            FRESULT e;
            if (k == DELETE_FILE) {
                FILINFO fi;
                e = f_stat(path, &fi);
                if (e == FR_OK)
                    e = fi.fattrib & AM_DIR ? FR_DENIED : f_unlink(path);
            } else
                e = f_mkdir(path);
            v->last_error = fs_error(e);
            *result = e == FR_OK;
            return 1;
        }
        BYTE mode = 0;
        if (a[1] & 0x80000000u)
            mode |= FA_READ;
        if (a[1] & 0x40000000u)
            mode |= FA_WRITE;
        if (!mode || (a[1] & ~0xc0000000ull) || a[3] || a[6] || (a[2] & ~7ull) ||
            (a[5] & ~128ull)) {
            v->last_error = 87;
            return 1;
        }
        if (a[4] == 1)
            mode |= FA_CREATE_NEW;
        else if (a[4] == 2)
            mode |= FA_CREATE_ALWAYS;
        else if (a[4] == 4)
            mode |= FA_OPEN_ALWAYS;
        else if (a[4] != 3) {
            v->last_error = 87;
            return 1;
        }
        if (a[4] != 3 && !(mode & FA_WRITE)) {
            v->last_error = 5;
            return 1;
        }
        /* FAT locking is conservative: writers cannot share an open file. */
        for (unsigned i = 0; i < 8; i++) {
            struct OpenFile *o = &open_files[process_bank][i];
            if (o->live)
                continue;
            FILINFO info;
            int existed = (a[4] == 2 || a[4] == 4) && f_stat(path, &info) == FR_OK;
            FRESULT e = f_open(&o->file, path, mode);
            v->last_error = fs_error(e);
            if (e == FR_OK) {
                o->live = 1;
                *result = 256 + i;
                if (existed)
                    v->last_error = 183;
            }
            return 1;
        }
        v->last_error = 4;
        return 1;
    }
    if (a[0] < 256 || a[0] >= 264 ||
        !(k == READ_FILE || k == WRITE_FILE || k == FILE_TYPE || k == CLOSE_HANDLE ||
          k == FILE_SIZE || k == FILE_SYNC || k == FILE_SEEK))
        return 0;
    struct OpenFile *o = &open_files[process_bank][a[0] - 256];
    if (!o->live) {
        v->last_error = 6;
        return 1;
    }
    FIL *f = &o->file;
    FRESULT e = FR_OK;
    if (k == FILE_TYPE) {
        *result = 1;
        return 1;
    }
    if (k == CLOSE_HANDLE) {
        e = f_close(f);
        if (e == FR_OK)
            o->live = 0;
    }
    if (k == FILE_SYNC)
        e = f_sync(f);
    if (k == FILE_SIZE)
        writev(v, a[1], 8, f_size(f));
    if (k == FILE_SEEK) {
        u64 delta = v->wide ? a[1] : (u32)a[1] | (a[2] << 32);
        u64 out = a[v->wide ? 2 : 3], whence = a[v->wide ? 3 : 4];
        u64 start = whence == 0 ? 0 : whence == 1 ? f_tell(f) : f_size(f);
        u64 off = start + delta;
        /* No speculative extension; seeking past EOF is currently unsupported. */
        if (whence > 2 || off > f_size(f)) {
            v->last_error = 87;
            return 1;
        }
        e = f_lseek(f, off);
        if (e == FR_OK && out)
            writev(v, out, 8, f_tell(f));
    }
    if (k == READ_FILE || k == WRITE_FILE) {
        if (!a[3] || a[4] || a[2] > 65536) {
            v->last_error = 87;
            return 1;
        }
        u8 *cnt = mem(v, a[3], 4, WRT);
        if (!cnt)
            return 1;
        put(cnt, 4, 0);
        unsigned n = 0;
        if (a[2]) {
            u8 *buf = mem(v, a[1], a[2], k == READ_FILE ? WRT : R);
            if (!buf || !charge(v, 1 + a[2] / 4096))
                return 1;
            e = k == READ_FILE ? f_read(f, buf, a[2], &n) : f_write(f, buf, a[2], &n);
            put(cnt, 4, n);
            if (k == WRITE_FILE && e == FR_OK && n != a[2]) {
                v->last_error = 112;
                return 1;
            }
        }
    }
    *result = e == FR_OK;
    v->last_error = fs_error(e);
    return 1;
}
static void close_diskfiles(void) {
    for (unsigned i = 0; i < 8; i++) {
        struct OpenFile *o = &open_files[process_bank][i];
        if (o->live) {
            if (f_close(&o->file) != FR_OK)
                logline("app file close failed");
            else
                o->live = 0;
        }
    }
}
#endif
