#pragma once
#include "compat.h"
#define PKG_SLOTS 8
#define PKG_MAX (1u << 20)
void packages_init(void);
const char *package_name(int);
int package_run(int, PeResult *, void (*dialog)(const char *));
int home_package_sector(unsigned slot, unsigned index, void *sector);

int package_pending(int);
int package_install(int, PeResult *);
int home_package_publish(unsigned);
