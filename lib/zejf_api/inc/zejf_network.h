#ifndef _ZEJF_NETWORK_H
#define _ZEJF_NETWORK_H

#include "zejf_protocol.h"
#include "dynamic_data.h"

#include <stdint.h>

#define UPDATE_SUCCESS 0
#define UPDATE_FAIL 1
#define UPDATE_NO_CHANGE 2

typedef struct routing_table_entry_t {
    uint32_t rx_id;
    uint32_t tx_id;
    TIME_TYPE last_seen;
    uint16_t device_id;
    Interface* interface;
    uint8_t distance;
    uint8_t variable_count;
    VariableInfo variables[];
} RoutingEntry;

// Initialise buffers etc..
void network_init(void);

// Free all resources
void network_destroy(void);

// tst
int network_test(void);

// Packet received
bool network_accept(char* msg, int length, Interface* interface, TIME_TYPE time);

// Sending packet - adds to the queue
bool network_send_packet(Packet* packet, TIME_TYPE time);

// Iterate the queue
void network_send_all(TIME_TYPE time);

// check for inactive members of routing table
void routing_table_check(TIME_TYPE time);

// send routing info to all available devices
bool network_send_routing_info(uint8_t variable_count, VariableInfo* variables);

// create Packet handle that can be send using network_send_packet
Packet* network_prepare_packet(uint16_t to, uint8_t command, char* msg);

// handle packed meant for this device
// target platform defines this
void network_process_packet(Packet* packet);

// send package as message using given interface
// target platform defines this
void network_send_via(char* msg, int length, Interface* interface);

void get_all_interfaces(Interface*** interfaces, int* length);

bool queue_full();

#endif