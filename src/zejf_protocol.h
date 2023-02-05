#ifndef _PACKET_H
#define _PACKET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#define PACKET_MAX_LENGTH 64

enum commands{
    PING = 0x01,
    PONG = 0x02,
    DATA_REQUEST = 0x03,
    DATA_LOG = 0x04,
    MESSAGE = 0x05,
    TIME_CHECK = 0x06
};

typedef struct packet_t{
    uint16_t from;
    uint16_t to;
    uint8_t ttl;
    uint8_t command;
    int32_t checksum;
    uint16_t message_size;
    char* message;
} Packet;

Packet* packet_create(void);

void packet_destroy(Packet* pack);

int packet_from_string(Packet* packet, char* data, int length);

bool packet_to_string(Packet* pack, char* buff, size_t max_length);

int32_t checksum(void* ptr, size_t size);

#endif