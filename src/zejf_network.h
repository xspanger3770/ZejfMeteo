#ifndef _ZEJF_NETWORK_H
#define _ZEJF_NETWORK_H

#define PACKET_QUEUE_SIZE 32
#define ROUTING_TABLE_SIZE 16
#define DEVICE_ID 0x0001

#include <stdint.h>
#include <sys/time.h>

enum interface {
    USB = 0
};

typedef struct routing_table_entry_t {
    uint16_t device_id;
    uint8_t interface;
    uint8_t distance;
    time_t last_seen;
} RoutingEntry;

void network_init(void);

void network_send_packet(Packet* packet, uint16_t to);

void network_accept(char* msg, enum interface interface);

#endif