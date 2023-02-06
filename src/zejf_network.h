#ifndef _ZEJF_NETWORK_H
#define _ZEJF_NETWORK_H

#define PACKET_QUEUE_SIZE 32
#define ROUTING_TABLE_SIZE 16
#define DEVICE_ID 0x0001

#include "zejf_protocol.h"

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
    uint16_t variables[];
} RoutingEntry;

void network_init(void);

void network_destroy(void);

RoutingEntry* routing_entry_create(uint8_t variable_count, uint16_t* variables);

void routing_entry_destroy(RoutingEntry* entry);

int routing_table_update(uint16_t device_id, uint8_t interface, uint8_t distance, time_t time, 
    uint8_t variable_count, uint16_t* variables);

bool network_accept(char* msg, int length, enum interface interface);

bool network_push_packet(Packet* packet);

void network_send_all(time_t time);

void network_process_packet(Packet* packet); // USER

void network_send_via(char* msg, int length, enum interface interface); // USER

#endif