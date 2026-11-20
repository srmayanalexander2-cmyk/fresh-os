#ifndef FRESH_DRIVERS_H
#define FRESH_DRIVERS_H
#include "platform.h"
int guest_pointer(int *x, int *y, int *buttons);
void network_poll(void), network_cmd(int acquire, void (*line)(const char *));
void drivers_init(void), drivers_list(void (*line)(const char *));
const char *vm_guest(void), *audio_name(void);
int driver_count(void);
const char *driver_at(int);
const char *pcm_wav(const u8 *, u32, int);
int pcm_tone(int hz, int level);
u32 sata_size(void);
int sata_io(u32, void *, int);
u32 vblk_size(void);
int vblk_io(u32 lba, void *buf, int op);
#endif
