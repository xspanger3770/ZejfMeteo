#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <limits.h>
#include <inttypes.h>

#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_routing.h"
#include "zejf_network.h"
#include "zejf_protocol.h"
#include "data_info.h"
#include "data_request.h"
#include "data_check.h"

Packet* packet_queue[PACKET_QUEUE_SIZE];
size_t packet_queue_top;
size_t packet_next;

uint16_t provide_ptr;
uint16_t demand_ptr;

void packet_queue_remove(size_t index);

bool network_push_packet(Packet* packet);

bool process_rip_packet(Packet* packet, TIME_TYPE now);

void sync_id(RoutingEntry* entry, int back);

bool network_catch_packet(Packet* packet, TIME_TYPE time);

void network_init(void){
    packet_queue_top = 0;
    packet_next = 0;

    provide_ptr = 0;
    demand_ptr = 0;

    for(int i = 0; i < PACKET_QUEUE_SIZE; i++){
        packet_queue[i] = NULL;
    }

    printf("Attention ==========\n");
    printf("DataDays will take at most %"SCNu64"b\n", ((uint64_t)DAY_MAX_SIZE*(uint64_t)DAY_BUFFER_SIZE));
    printf("Packets will take around %"SCNu64"b\n", ((uint64_t)PACKET_MAX_LENGTH*PACKET_QUEUE_SIZE));
    printf("RoutingEntries will take at most %ldb\n", (sizeof(RoutingEntry)*ROUTING_TABLE_SIZE));
}

void network_destroy(void){
    for(size_t i = 0; i < packet_queue_top; i++){
        packet_destroy(packet_queue[i]);
    }
}

// ALL PACKETS FROM OUTSIDE MUST PASS THIS FUNCTION
bool network_accept(char* msg, int length, Interface* interface, TIME_TYPE time){
    Packet* packet = packet_create();
    if(packet == NULL){
        return false;
    }

    int rv = packet_from_string(packet, msg, length);

    if(rv != 0){
        packet_destroy(packet);
        return false;
    }

    packet->source_interface = interface;
    packet->ttl++;

    if(packet->ttl >= PACKET_MAX_TTL){
        packet_destroy(packet);
        return false;
    }

    return network_send_packet(packet, time);
}

bool prepare_ack_message(char* buff, uint16_t to, uint32_t tx_id){
    Packet packet[1];
    packet->command = ACK;
    packet->from = DEVICE_ID;
    packet->to = to;
    packet->ttl = 0;
    packet->tx_id = tx_id;
    packet->message_size = 0;
    packet->message = NULL;
    
    return packet_to_string(packet, buff, PACKET_MAX_LENGTH);
}

void ack_packet(Interface* interface, uint32_t id){
    for(size_t i = 0; i < packet_queue_top; i++){
        Packet* packet = packet_queue[i];
        if(packet->destination_interface != NULL && packet->destination_interface->uid == interface->uid && packet->tx_id == id){
            packet_queue_remove(i);
            return;
        }
    }
}

// EVERY PACKET THAT ENDS UP IN THE QUEUE MUST PASS THIS FUNCTION
// PACKET MUST BE ALLOCATED
bool network_send_packet(Packet* packet, TIME_TYPE time){
    // HANDLE SPECIAL PACKETS

    if(packet == NULL){
        return false;
    }

    if(packet->from != DEVICE_ID && packet->command == ID_SYNC){
        RoutingEntry* entry = routing_entry_find(packet->from);
        if(entry == NULL){
            packet_destroy(packet);
            printf("lol he wants reset but didnt even send routing info\n");
            return false;
        }
        
        if(packet->tx_id == 1){ // back
            sync_id(entry, 0);
        }
        
        entry->rx_id = 1;
        entry->tx_id = 1;

        packet_destroy(packet);
        return true;
    }

    if(packet->from != DEVICE_ID && packet->command == ACK){
        ack_packet(packet->source_interface, packet->tx_id);

        packet_destroy(packet);
        return true;
    }

    if(packet->command == RIP){ // always from outside
        if(!process_rip_packet(packet, time)){
            packet_destroy(packet);
            return false;
        }
        packet_destroy(packet);
        return true;
    }


    if(packet_queue_top >= PACKET_QUEUE_SIZE){
        packet_destroy(packet);
        printf("WARN: queue full\n");
        return false;
    }
        
    if(packet->from != DEVICE_ID && packet->command != RIP){
        RoutingEntry* entry = routing_entry_find(packet->from);
        if(entry == NULL){
            packet_destroy(packet);
            return false;
        }

        if(entry->tx_id == 0){
            packet_destroy(packet);
            printf("rejected txid not set yet\n");
            return false; // waiting for sync
        }

        if(entry->rx_id != packet->tx_id){
            packet_destroy(packet);
            printf("rejected wrong id\n");
            return false; // wriong txid
        }

        entry->rx_id++;
    }

    uint32_t id_to_ack = packet->tx_id; 

    packet->time_received = time; // time when it entered the queue
    packet->time_sent = 0; // last send attempt
    packet->tx_id = 0; // not determined yet

    if(!network_push_packet(packet)){
        printf("THIS SHOULD HAVE NEVER HAPPENED\n");
    }

    // SEND ACK HERE
    if(packet->from != DEVICE_ID){ // if it came from outside it will have source_interface set
        char buff[PACKET_MAX_LENGTH];
        if(!prepare_ack_message(buff, packet->from, id_to_ack)){
            return false;
        }

        network_send_via(buff, strlen(buff), packet->source_interface);
    }

    return true;
}

bool network_push_packet(Packet* packet){
    if(packet_queue_top == PACKET_QUEUE_SIZE){
        return false; // queue full
    }

    packet_queue[packet_queue_top] = packet;
    packet_queue_top += 1;

    return true;
}

bool prepare_and_send(Packet* packet, RoutingEntry* entry){
    
    if(packet->tx_id == 0 || packet->tx_id >= entry->tx_id){
        packet->tx_id = entry->tx_id++;
    }

    char buff[PACKET_MAX_LENGTH];
    if(!packet_to_string(packet, buff, PACKET_MAX_LENGTH)){
        return false;
    }

    packet->destination_interface = entry->interface;

    network_send_via(buff, strlen(buff), entry->interface);
    return true;
}


bool process_broadcast_packet(Packet* packet){
    size_t size = routing_table_top; // how many packets will be created
    size_t free_space = PACKET_QUEUE_SIZE - packet_queue_top;

    if(size <= free_space){
        for(size_t i = 0; i < routing_table_top; i++){
            RoutingEntry* entry = routing_table[i];
            Packet* new_packet = network_prepare_packet(entry->device_id, packet->command, packet->message);

            network_send_packet(new_packet, packet->time_received);
        }

        return true;
    }
    
    return false;
}

void network_send_everywhere(Packet* packet){
    char buff[PACKET_MAX_LENGTH];
    if(!packet_to_string(packet, buff, PACKET_MAX_LENGTH)){
        return;
    }
    Interface** interfaces = NULL;
    int count = 0;

    get_all_interfaces(&interfaces, &count);

    for(int i = 0; i < count; i++){
        Interface* interface = interfaces[i];
        if(packet->source_interface == NULL || interface->uid != packet->source_interface->uid){
            network_send_via(buff, strlen(buff), interface);
        }
    }
}

bool process_rip_packet(Packet* packet, TIME_TYPE now){
    if(routing_table_update(packet->from, packet->source_interface, packet->ttl, now) == UPDATE_SUCCESS){
        network_send_everywhere(packet);
    }

    return true;
}

// todo linked list would be better

void packet_queue_remove(size_t index){
    packet_destroy(packet_queue[index]);

    for(size_t i = index; i < packet_queue_top - 1; i++){
        packet_queue[i] = packet_queue[i + 1];
    }

    packet_queue_top--;

    packet_queue[packet_queue_top] = NULL;

    if(packet_next > index){
        packet_next--;
    }
}

void sync_id(RoutingEntry* entry, int back){
    Packet packet[1];
    packet->command = ID_SYNC;
    packet->from = DEVICE_ID;
    packet->to = entry->device_id;
    packet->ttl = 0;
    packet->tx_id = back;
    packet->message_size = 0;
    packet->message = NULL;

    char buff[PACKET_MAX_LENGTH];

    if(!packet_to_string(packet, buff, PACKET_MAX_LENGTH)){
        return;
    }
    entry->rx_id = 1;
    entry->tx_id = 0;

    network_send_via(buff, strlen(buff), entry->interface);
}

void network_send_all(TIME_TYPE time){
    if(packet_queue_top == 0){
        return;
    }

    Packet* packet = packet_queue[packet_next];

    if(packet == NULL){
        goto next_one;
    }

    if((time - packet->time_received) >= PACKET_DELETE_TIMEOUT){
        printf("timeout hard of packed command %d txid %d from %d to %d after %ldms\n", packet->command, packet->tx_id, packet->from, packet->to, (time - packet->time_received));
        printf("times were %ld %ld\n", time, packet->time_received);
        goto remove;
    }

    if(packet->to == BROADCAST){
        if(!process_broadcast_packet(packet)){
            goto next_one;
        }
        goto remove;
    }

    if(packet->to == DEVICE_ID) {
        if(!network_catch_packet(packet, time)){ 
            network_process_packet(packet);
        }
        goto remove;
    }

    // SEND THE PACKET SOMEWHERE

    RoutingEntry* entry = routing_entry_find(packet->to);
    if(entry == NULL){
        goto remove;
    }

    if(entry->paused == 1){
        goto next_one;
    }

    if(time - packet->time_sent < PACKET_RETRY_TIMEOUT){
        entry->paused = 1; // to keep the order of packets
        goto next_one;
    }

    // check for reset timeout
    if(packet->time_sent != 0 && (packet->time_sent - packet->time_received) / PACKET_RESET_TIMEOUT != (time - packet->time_received) / PACKET_RESET_TIMEOUT){
        entry->tx_id = 0;
        entry->rx_id = 0;
        entry->paused = 1;
        printf("reset\n");
        // no goto here!
    }

    packet->time_sent = time;

    // tx,rx
    // 0,0 = not initialised
    // 0,1 = not initialised, waiting for returning sync packet
    // 1,1 = ready
    if(entry->tx_id == 0) {
        if(entry->rx_id == 0){
            sync_id(entry, 1);  
        }
        goto next_one;
    }

    prepare_and_send(packet, entry);

    next_one:
    packet_next ++;
    packet_next %= packet_queue_top;

    // toggle reset flag to all routing entries
    if(packet_next == 0){
        for(size_t i = 0; i < routing_table_top; i++){
            RoutingEntry* entry = routing_table[i];
            entry->paused = 0;
        }
    }

    return;
    
    remove:
    packet_queue_remove(packet_next);
}

Packet* network_prepare_packet(uint16_t to, uint8_t command, char* msg){
    Packet* packet = packet_create();
    packet->command = command;
    packet->from = DEVICE_ID;
    packet->to = to;
    packet->ttl = 0;

    if(msg != NULL) {
        packet->message_size = strlen(msg);
        packet->message=malloc(packet->message_size+1);
        memcpy(packet->message, msg, packet->message_size + 1);
    } else {
        packet->message_size = 0;
        packet->message = NULL;
    }

    return packet;
}

size_t allocate_packet_queue(int priority){
    return (((size_t)PACKET_QUEUE_SIZE - packet_queue_top) - 1) / priority;
}

bool network_catch_packet(Packet* packet, TIME_TYPE time){
    switch (packet->command)
    {
    case DATA_DEMAND:
        process_data_demand(packet);
        break;
    case DATA_PROVIDE:
        process_data_provide(packet);
        break;
    case DATA_LOG:
        printf("caught log\n");
        process_data_log(packet, time);
        break;
    case DATA_REQUEST:
        data_request_receive(packet);
        break;
    case DATA_CHECK:
        data_check_receive(packet);
        break;
    default:
        return false;
    }

    return true;
}