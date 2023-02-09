#ifndef _ZEJF_SETTINGS_H
#define _ZEJF_SETTINGS_H

// NETWORK
#define DEVICE_ID 0x0001

// PACKETS
#define PACKET_MAX_LENGTH 128
#define PACKET_MAX_TTL 8
#define PACKET_QUEUE_SIZE 256
#define PACKET_TIMEOUT 5000
#define PACKET_RETRY_TIMEOUT 250

// ROUTING
#define ROUTING_TABLE_SIZE 16
#define ROUTING_ENTRY_TIMEOUT (1000 * 10)

// DATA
#define DAY_BUFFER_SIZE 128
#define DAY_MAX_SIZE (16 * 1024 * 1024)

enum interface_type_t{
    USB = 1
};
/*  ===== USAGE ====  

    determine values above

    define void network_process_packet(Packet* packet);
    define void network_send_via(char* msg, int length, enum interface interface);

    call network_init
    
    call network_accept for every received string
    call network_send_all to send all packets

    periodically call routing_table_check
*/

#endif