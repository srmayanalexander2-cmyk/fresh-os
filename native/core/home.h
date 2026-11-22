#pragma once
#include "base.h"
#define NOTES 16
#define NOTE_CAP 3072
struct Note {
    char name[28], body[NOTE_CAP];
};
enum { TASKBAR_LEFT, TASKBAR_BOTTOM, TASKBAR_TOP };
struct Home {
    u32 count, light, sound, accent, autohide, wallpaper, wide_apps;
    struct Note notes[NOTES];
    /* Appended so snapshots from earlier Stratus builds retain their layout. */
    u32 taskbar_position;
};
extern struct Home home;
unsigned disk_count(void);
const char *home_state(void);
void home_init(void);
int home_commit(void);
int home_new(const char *);
int home_find(const char *);
int goodname(const char *);
/* Installation writes only the system area of a separately marked Home-2 disk. */
int install_start(void);
int install_step(void);
int install_progress(void);
const char *install_status(void);
void install_cancel(void);
