#ifndef _PACKET_H
#define _PACKET_H

#include "zejf_defs.h"

Packet* packet_create(void);

void packet_destroy(Packet* pack);

int packet_from_string(Packet* packet, char* data, int length);

// WITH NEWLINE
bool packet_to_string(Packet* pack, char* buff, size_t max_length);

int32_t checksum(void* ptr, size_t size);

#endif