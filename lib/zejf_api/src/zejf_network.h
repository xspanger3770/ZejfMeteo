#ifndef _ZEJF_NETWORK_H
#define _ZEJF_NETWORK_H

#include "zejf_defs.h"

#include <stdint.h>

// Initialise buffers etc..
void network_init(void);

// Free all resources
void network_destroy(void);

// tst
int network_test(void);

void network_send_everywhere(Packet* packet);

#endif