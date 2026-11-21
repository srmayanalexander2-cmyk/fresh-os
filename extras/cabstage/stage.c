/* Test client of libmspack's published system interface, built as a Win64 EXE.
 * Fixed destinations: never treat an archive member name as a writable path. */
#include <windows.h>
#include <mspack.h>
static int io_error, open_error;
static struct mspack_file *open_f(struct mspack_system *s, const char *name, int mode) {
    (void)s;
    if (mode != MSPACK_SYS_OPEN_READ && mode != MSPACK_SYS_OPEN_WRITE)
        return 0;
    HANDLE h = CreateFileA(name, mode == MSPACK_SYS_OPEN_READ ? GENERIC_READ : GENERIC_WRITE,
                           FILE_SHARE_READ, 0,
                           mode == MSPACK_SYS_OPEN_READ ? OPEN_EXISTING : CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, 0);
    if (h == INVALID_HANDLE_VALUE)
        open_error = 1000 * (mode + 1) + GetLastError();
    return h == INVALID_HANDLE_VALUE ? 0 : (struct mspack_file *)h;
}
static void close_f(struct mspack_file *f) {
    if (!CloseHandle(f))
        io_error = 1;
}
static int read_f(struct mspack_file *f, void *p, int n) {
    DWORD z;
    return n >= 0 && ReadFile(f, p, n, &z, 0) ? (int)z : -1;
}
static int write_f(struct mspack_file *f, void *p, int n) {
    DWORD z;
    return n >= 0 && WriteFile(f, p, n, &z, 0) ? (int)z : -1;
}
static int seek_f(struct mspack_file *f, off_t off, int how) {
    LARGE_INTEGER v;
    v.QuadPart = off;
    return !SetFilePointerEx(f, v, 0, how);
}
static off_t tell_f(struct mspack_file *f) {
    LARGE_INTEGER v, z;
    v.QuadPart = 0;
    return SetFilePointerEx(f, v, &z, FILE_CURRENT) ? (off_t)z.QuadPart : -1;
}
static void message_f(struct mspack_file *f, const char *msg, ...) {
    (void)f;
    (void)msg;
}
static void *alloc_f(struct mspack_system *s, size_t n) {
    (void)s;
    return VirtualAlloc(0, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
static void free_f(void *p) {
    if (p)
        VirtualFree(p, 0, MEM_RELEASE);
}
static void copy_f(void *src, void *dst, size_t n) {
    for (size_t i = 0; i < n; i++)
        ((unsigned char *)dst)[i] = ((unsigned char *)src)[i];
}
static struct mspack_system os = {open_f,    close_f, read_f, write_f, seek_f, tell_f,
                                  message_f, alloc_f, free_f, copy_f,  0};
static int match(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
int mainCRTStartup(void) {
    int err;
    MSPACK_SYS_SELFTEST(err);
    if (err)
        return 2;
    struct mscab_decompressor *d = mspack_create_cab_decompressor(&os);
    if (!d)
        return 3;
    struct mscabd_cabinet *cab = d->search(d, "F:\\INSTALL.EXE");
    int status = 4;
    if (cab) {
        for (struct mscabd_cabinet *c = cab; c; c = c->next) {
            for (struct mscabd_file *f = c->files; f; f = f->next) {
                if (!match(f->filename, "setup.exe"))
                    continue;
                if (!CreateDirectoryW(L"F:\\STAGE", 0) &&
                    GetLastError() != ERROR_ALREADY_EXISTS) {
                    status = 5;
                    goto done;
                }
                status = d->extract(d, f, "F:\\STAGE\\SETUP.EXE");
                if (status)
                    status += 10;
                goto done;
            }
        }
    }
done:
    if (cab)
        d->close(d, cab);
    mspack_destroy_cab_decompressor(d);
    return open_error ? open_error : io_error ? 6 : status;
}
