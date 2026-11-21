#pragma once
#include "base.h"
void pci_init(void);
int pci_devices(void);
const char *pci_device(int);
int vm_pointer(int *, int *, int *);
const char *pointer_name(void);

int vm_absolute(void);
