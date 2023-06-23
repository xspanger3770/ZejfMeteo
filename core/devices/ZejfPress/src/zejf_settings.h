#ifndef _ZEJF_SETTINGS_H
#define _ZEJF_SETTINGS_H

// NETWORK
#define DEVICE_ID 0x0002

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
#define HOUR_BUFFER_SIZE 12
#define HOUR_FILE_MAX_SIZE (1024 * 16)
#define SAMPLE_RATE_MAX (60 * 12) // 3kb
#define VARIABLES_MAX 4           // 3*4 = 12kb per hour

// REQUESTS
#define DATA_REQUEST_QUEUE_SIZE 64

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
};
/*  ===== USAGE ====

    determine values above

    define void network_process_packet(Packet* packet);
    define void network_send_via(char* msg, int length, enum interface
   interface);

    call network_init

    call network_accept for every received string
    call network_send_all to send all packets

    periodically call routing_table_check
*/

#endif