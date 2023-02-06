#ifndef _ZEJF_NETWORK_H
#define _ZEJF_NETWORK_H

#define PACKET_QUEUE_SIZE 32
#define ROUTING_TABLE_SIZE 1
#define DEVICE_ID 0x0001

#include "zejf_protocol.h"

#include <stdint.h>
#include <sys/time.h>

#define UPDATE_SUCCESS 0
#define UPDATE_FAIL 1
#define UPDATE_NO_CHANGE 2

enum interface {
    USB = 0
};

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

void network_send_packet(Packet* packet, uint16_t to);

void network_accept(char* msg, enum interface interface);

#endif