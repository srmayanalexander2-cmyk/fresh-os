#ifdef __x86_64__
#include "../vendor/fatfs/ff.c"
extern int app_disk_present(void), app_disk_sync(void);
extern int app_disk_io(u32, u8 *, unsigned, int);
DSTATUS disk_status(BYTE drv) { return drv || !app_disk_present() ? STA_NOINIT : 0; }
DSTATUS disk_initialize(BYTE drv) { return disk_status(drv); }
DRESULT disk_read(BYTE drv, BYTE *p, LBA_t lba, UINT n) {
    return !drv && app_disk_io(lba, p, n, 0) ? RES_OK : RES_ERROR;
}
DRESULT disk_write(BYTE drv, const BYTE *p, LBA_t lba, UINT n) {
    return !drv && app_disk_io(lba, (BYTE *)p, n, 1) ? RES_OK : RES_ERROR;
}
DRESULT disk_ioctl(BYTE drv, BYTE cmd, void *p) {
    (void)p;
    return !drv && cmd == CTRL_SYNC && app_disk_sync() ? RES_OK : RES_ERROR;
}
#endif
