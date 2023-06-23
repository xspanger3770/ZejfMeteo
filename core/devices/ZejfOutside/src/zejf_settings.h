#ifndef _ZEJF_SETTINGS_H
#define _ZEJF_SETTINGS_H

#include "zejf_defs.h"

struct interface_t;

extern struct interface_t tcp_client_1;

// NETWORK
#define DEVICE_ID 0x0004

// PACKETS
#define PACKET_MAX_LENGTH 128
#define PACKET_MAX_TTL 8
#define TX_QUEUE_SIZE 32
#define RX_QUEUE_SIZE 64
#define ACK_REQUIRED false

#define PACKET_RETRY_TIMEOUT (300)
#define PACKET_RESET_TIMEOUT (1000 * 2)
#define PACKET_DELETE_TIMEOUT (1000 * 5)

// ROUTING
#define ROUTING_TABLE_SIZE 16
#define ROUTING_ENTRY_TIMEOUT (1000 * 60)

// DATA
#define HOUR_BUFFER_SIZE 12 // 15 * 8 = 120kB
#define HOUR_FILE_MAX_SIZE (1024 * 32)
#define SAMPLE_RATE_MAX (60 * 12) //3kb
#define VARIABLES_MAX 8           //3*5 = 15kb per hour + slow

// REQUESTS
#define DATA_REQUEST_QUEUE_SIZE 164

// DEBUG
#define ZEJF_LOG_DEBUG 0
#define ZEJF_LOG_INFO 1
#define ZEJF_LOG_CRITICAL 2

#define ZEJF_LOG_LEVEL 1
#define ZEJF_LOG(p, x, ...)  \
    do { if(p >= ZEJF_LOG_LEVEL) printf(x, ##__VA_ARGS__); } while(0)

enum interface_type_t
{
    USB = 1,
    TCP = 2
};

#endif