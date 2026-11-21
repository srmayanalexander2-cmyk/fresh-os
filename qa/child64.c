/* Link as a normal PE64, using only its KERNEL32 imports. */
typedef unsigned u32;
typedef unsigned long long u64;
typedef unsigned short W;
#define API __declspec(dllimport)
struct Info {
    u64 process, thread;
    u32 pid, tid;
};
API int CreateProcessW(const W *, W *, void *, void *, int, u32, void *, void *, void *,
                       struct Info *);
API u32 GetLastError(void);
API u32 GetCurrentProcessId(void);
API u32 WaitForSingleObject(u64, u32);
API int GetExitCodeProcess(u64, u32 *);
API int CloseHandle(u64);
API void *VirtualAlloc(void *, u64, u32, u32);
API int VirtualFree(void *, u64, u32);
static W missing[] = {'m', 'i', 's', 's', 'i', 'n', 'g', 0};
static W good[] = {'c', 'a', 'l', '6', '4', ' ', '2', ' ', '2', '0', '2', '4', 0};
static W bad[] = {'c', 'a', 'l', '6', '4', ' ', '1', '3', ' ', '2', '0', '2', '4', 0};
int mainCRTStartup(void) {
    u64 startup[13] = {104};
    struct Info pi;
    volatile u64 *page = VirtualAlloc(0, 4096, 0x3000, 4);
    if (!page)
        return 2;
    *page = 0x40ea17a0bc55ed64ULL;
    if (CreateProcessW(0, missing, 0, 0, 0, 0, 0, 0, startup, &pi) || GetLastError() != 2)
        return 3;
    u32 last = 0;
    for (unsigned i = 0; i < 2; i++) {
        if (!CreateProcessW(0, i ? bad : good, 0, 0, 0, 0, 0, 0, startup, &pi))
            return 4;
        if (!pi.pid || pi.pid == last || pi.pid == GetCurrentProcessId())
            return 5;
        last = pi.pid;
        u32 status = 0x1234;
        if (WaitForSingleObject(pi.process, 0xffffffffu) ||
            !GetExitCodeProcess(pi.process, &status) || status != i)
            return 6;
        if (!CloseHandle(pi.process) || CloseHandle(pi.process) || !CloseHandle(pi.thread))
            return 7;
        if (WaitForSingleObject(pi.process, 0) != 0xffffffffu || GetLastError() != 6)
            return 8;
        if (*page != 0x40ea17a0bc55ed64ULL)
            return 9;
    }
    /* Same ordinary binary, this time directly from the FAT disk: no IMP packing. */
    static W diskcmd[] = {'F', ':', '/', 'C', 'A', 'L', '.', 'E', 'X',
                          'E', ' ', '2', ' ', '2', '0', '2', '4', 0};
    if (!CreateProcessW(0, diskcmd, 0, 0, 0, 0, 0, 0, startup, &pi))
        return 10;
    u32 code = 9;
    if (!GetExitCodeProcess(pi.process, &code) || code || *page != 0x40ea17a0bc55ed64ULL)
        return 11;
    if (!CloseHandle(pi.thread) || !CloseHandle(pi.process))
        return 12;
    return !VirtualFree((void *)page, 0, 0x8000);
}
