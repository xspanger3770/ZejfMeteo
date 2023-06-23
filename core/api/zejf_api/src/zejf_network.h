#ifndef _ZEJF_NETWORK_H
#define _ZEJF_NETWORK_H

#include "zejf_defs.h"

#include <stdint.h>

extern uint16_t provide_ptr;
extern uint16_t demand_ptr;

// Initialise buffers etc..
void network_init(void);

// Free all resources
void network_destroy(void);

void network_send_everywhere(Packet *packet, TIME_TYPE time);

void network_interface_removed(Interface *interface);

#endif