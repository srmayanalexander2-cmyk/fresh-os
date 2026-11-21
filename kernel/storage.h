/* Storage is intentionally narrow: IDE, AHCI and virtio; only our own media is eligible for
 * writes. */
/* home_read: nonnegative length, -2 for absent/no volume, -1 for error. */
void storage_init(void);
void storage_list(void (*emit)(const char *));
int home_scan(void (*emit)(const char *),unsigned limit);
int home_ready(void), home_save(const char *, const void *, unsigned),
    home_read(const char *, void *, unsigned), home_list(void (*)(const char *));
int install_probe(void), install_begin(void), install_step(void), install_percent(void);
const char *disk_status(void);
