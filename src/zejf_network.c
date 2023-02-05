#include "zejf_network.h"
#include "zejf_protocol.h"

#include <stdio.h>

Packet* packet_queue[PACKET_QUEUE_SIZE];
uint8_t queue_head;
uint8_t queue_tail;

RoutingEntry routing_table[ROUTING_TABLE_SIZE];
uint8_t routing_table_top;

void network_init(void){
    queue_head = 0;
    queue_tail = 0;
    routing_table_top = 0;
}

int main(){
    printf("%ld\n", sizeof(routing_table));
    return 0;
}