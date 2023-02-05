#include "zejf_network.h"
#include "zejf_protocol.h"

#include <stdio.h>

Packet* packet_queue[PACKET_QUEUE_SIZE];
size_t queue_head;
size_t queue_tail;

RoutingEntry routing_table[ROUTING_TABLE_SIZE];

int main(){
    printf("%ld\n", sizeof(RoutingEntry));
    return 0;
}