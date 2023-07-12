#ifndef _ZEJF_NETWORK_H
#define _ZEJF_NETWORK_H

#include "zejf_defs.h"

#include <stdint.h>

extern uint16_t provide_ptr;
extern uint16_t demand_ptr;

zejf_err network_push_packet(Packet *packet);

zejf_err process_rip_packet(Packet *packet, TIME_TYPE now);

bool network_catch_packet(Packet *packet, TIME_TYPE time);

void network_process_rx(TIME_TYPE time);

void network_send_tx(TIME_TYPE time);

// Initialise buffers etc..
zejf_err network_init(void);

// Free all resources
void network_destroy(void);

zejf_err network_send_everywhere(Packet *packet, TIME_TYPE time);

void network_interface_removed(Interface *interface);

#endif