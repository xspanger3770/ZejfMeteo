#ifndef _PACKET_H
#define _PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include "zejf_settings.h"

#define BROADCAST 0xFFFF
#define TIME_TYPE uint32_t

enum commands{
    RIP = 0x01,
    ACK = 0x02,
    ID_SYNC = 0x03,
    TIME_CHECK = 0x04,
    DATA_REQUEST = 0x05,
    DATA_LOG = 0x06,
    MESSAGE = 0x07,
};

typedef struct interface_t{
    int uid;
    enum interface_type_t type;
    int handle;
} Interface;

typedef struct packet_t{
    Interface* source_interface;
    Interface* destination_interface;
    uint32_t time_received;
    uint32_t time_sent;
    uint32_t tx_id;
    int32_t checksum;
    uint16_t from;
    uint16_t to;
    uint16_t message_size;
    uint8_t ttl;
    uint8_t command;
    char* message; // WARNING: \0 MUST BE AT THE END, can be NULL
} Packet;

Packet* packet_create(void);

void packet_destroy(Packet* pack);

int packet_from_string(Packet* packet, char* data, int length);

// WITH NEWLINE
bool packet_to_string(Packet* pack, char* buff, size_t max_length);

int32_t checksum(void* ptr, size_t size);

#endif