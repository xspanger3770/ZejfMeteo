#ifndef _ZEJF_NETWORK_H
#define _ZEJF_NETWORK_H

#include "zejf_protocol.h"
#include "dynamic_data.h"

#include <stdint.h>
#include <sys/time.h>

#define UPDATE_SUCCESS 0
#define UPDATE_FAIL 1
#define UPDATE_NO_CHANGE 2

typedef struct routing_table_entry_t {
    uint16_t device_id;
    uint8_t interface;
    uint8_t distance;
    time_t last_seen;
    uint8_t variable_count;
    VariableInfo variables[];
} RoutingEntry;

void network_init(void);

void network_destroy(void);

int network_test(void);

bool network_accept(char* msg, int length, enum interface interface);

bool network_push_packet(Packet* packet);

void network_send_all(time_t time);

void routing_table_check(time_t time);

bool create_routing_message(char* buff, uint8_t variable_count, VariableInfo* variables) ;

void network_process_packet(Packet* packet); // USER

void network_send_via(char* msg, int length, enum interface interface); // USER

#endif