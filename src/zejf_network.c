#include "zejf_network.h"
#include "zejf_protocol.h"

#include <stdio.h>
#include <stdlib.h>

Packet* packet_queue[PACKET_QUEUE_SIZE];
uint8_t queue_head;
uint8_t queue_tail;

RoutingEntry* routing_table[ROUTING_TABLE_SIZE];
uint8_t routing_table_top;

void network_init(void){
    queue_head = 0;
    queue_tail = 0;
    routing_table_top = 0;

    for(int i = 0; i < PACKET_QUEUE_SIZE; i++){
        packet_queue[i] = NULL;
    }

    for(int i = 0; i < ROUTING_TABLE_SIZE; i++){
        routing_table[i] = NULL;
    }
}

RoutingEntry* routing_entry_create(uint8_t variable_count, uint16_t* variables){
    size_t new_size = sizeof(RoutingEntry) + variable_count * sizeof(uint16_t);
    RoutingEntry* entry = calloc(1, new_size);
    if(entry == NULL){
        return NULL;
    }

    entry->variable_count = variable_count;
    for(int i = 0; i < variable_count; i++){
        entry->variables[i] = variables[i];
    }

    return entry;
}

void routing_entry_destroy(RoutingEntry* entry){
    if(entry == NULL){
        return;
    }

    free(entry);
}

bool routing_entry_set_variables(RoutingEntry** entry, uint8_t variable_count, uint16_t* variables){
    size_t new_size = sizeof(RoutingEntry) + variable_count * sizeof(uint16_t);
    RoutingEntry* ptr = realloc(*entry, new_size);
    if(ptr == NULL){
        return false;
    }

    ptr->variable_count = variable_count;
    for(int i = 0; i < variable_count; i++){
        ptr->variables[i] = variables[i];
    }

    (*entry) = ptr;
    return true;
}

RoutingEntry** routing_entry_find(uint16_t device_id){
    for(int i = 0; i < routing_table_top; i++){
        RoutingEntry** entry = &routing_table[i];
        if((*entry)->device_id == device_id){
            return entry;
        }
    }
    return NULL;
}

int routing_table_insert(uint16_t device_id, uint8_t interface, uint8_t distance, time_t time, 
    uint8_t variable_count, uint16_t* variables)
{
    if(routing_table_top >= ROUTING_TABLE_SIZE){
        return UPDATE_FAIL;
    }
    RoutingEntry* entry = routing_entry_create(variable_count, variables);
    entry->device_id=device_id;
    entry->distance=distance;
    entry->interface=interface;
    entry->last_seen=time;
    
    routing_table[routing_table_top] = entry;
    routing_table_top++;
    
    return UPDATE_SUCCESS;
}

bool check_variables(RoutingEntry** entry, uint8_t variable_count, uint16_t* variables){
    if((*entry)->variable_count != variable_count){
        return routing_entry_set_variables(entry, variable_count, variables);
    }

    for(int i = 0; i < variable_count; i++){
        if((*entry)->variables[i] != variables[i]){
            return routing_entry_set_variables(entry, variable_count, variables);
        }
    }
    return true;
}

int routing_table_update(uint16_t device_id, uint8_t interface, uint8_t distance, time_t time, 
    uint8_t variable_count, uint16_t* variables)
{
    RoutingEntry** existing_entry = routing_entry_find(device_id);
    if(existing_entry == NULL){
        return routing_table_insert(device_id, interface, distance, time, variable_count, variables);
    }

    int result = UPDATE_NO_CHANGE;

    if((*existing_entry)->distance > distance){
        (*existing_entry)->distance=distance;
        (*existing_entry)->interface=interface;
        result = UPDATE_SUCCESS;
    }

    if((*existing_entry)->distance == distance){
        (*existing_entry)->last_seen = time;

        if(!check_variables(existing_entry, variable_count, variables)){
            return UPDATE_FAIL;
        }
    }

    return result;
}

bool network_accept(char* msg, int length, enum interface interface){
    Packet* packet = packet_create();
    if(packet == NULL){
        return false;
    }

    if(packet_from_string(packet, msg, length) != 0){
        packet_destroy(packet);
        return false;
    }

    return network_push_packet(packet);
}

bool network_push_packet(Packet* packet){
    if((queue_head + 1) % PACKET_QUEUE_SIZE == queue_tail){
        return false; // queue full
    }

    packet_queue[queue_head] = packet;
    queue_head += 1;
    queue_head %= PACKET_QUEUE_SIZE;

    return true;
}

bool find_interface(enum interface* iface, uint16_t to){
    for(int i = 0; i < routing_table_top; i++){
        RoutingEntry* entry = routing_table[i];
        if(entry->device_id == to){
            *iface = entry->interface;
            return true;
        }
    }
    return false;
}

void process_rip_packet(Packet* packet){

}

void process_broadcast_packet(Packet* packet){
    
}

void network_send_all(){
    while(queue_tail != queue_head){
        Packet* packet = packet_queue[queue_tail];
        if(packet->to == DEVICE_ID){
            if(packet->command == RIP){
                process_rip_packet(packet);
            } else {
                network_process_packet(packet);
            }
        } else {
            if(packet->to == BROADCAST){
                process_broadcast_packet(packet);
            }else{
                enum interface iface;
                if(find_interface(&iface, packet->to)){
                    char buff[PACKET_MAX_LENGTH];
                    packet_to_string(packet, buff, PACKET_MAX_LENGTH);
                    network_send_via(buff, strlen(buff), iface);
                }
            }
        }

        packet_destroy(packet);
        packet_queue[queue_tail] = NULL;

        queue_tail+=1;
        queue_tail %= PACKET_QUEUE_SIZE;
    }
}

void network_send_via(char* msg, int length, enum interface interface);

void network_destroy(void){
    for(int i = 0; i < routing_table_top; i++){
        routing_entry_destroy(routing_table[i]);
    }
}

void print_table(){
    printf("Printing rounting table size %d\n", routing_table_top);
    for(int i = 0; i < routing_table_top; i++){
        RoutingEntry* entry = routing_table[i];
        printf("    Entry #%d\n", i);
        printf("        device=%d\n", entry->device_id);
        printf("        distanc=%d\n", entry->distance);
        printf("        interfa=%d\n", entry->interface);
        printf("        lastseen=%ld\n", entry->last_seen);
        printf("        var_count=%d\n", entry->variable_count);
    }
}

int main(){
    network_init();
    print_table();
    uint16_t vars[1] = {42};
    routing_table_update(5, 42, 3, 0, 1, vars);
    print_table();
    routing_table_update(5, 41, 2, 0, 1, vars);
    print_table();
    routing_table_update(5, 43, 4, 0, 1, vars);
    print_table();
    routing_table_update(4, USB, 2, 0, 1, vars);
    
    print_table();

    network_destroy();
    return 0;
}