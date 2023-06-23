#ifndef _INTERFACE_MANAGER_H
#define _INTERFACE_MANAGER_H

#include "zejf_defs.h"

#define INTERFACES_MAX 32

extern Interface usb_interface_1;

extern Interface *all_interfaces[INTERFACES_MAX];
extern size_t interface_count;

void interfaces_init(void);

void interfaces_destroy(void);

bool interface_add(Interface *interface);

bool interface_remove(int uid);

#endif