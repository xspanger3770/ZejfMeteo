#include "zejf_network.h"
#include "zejf_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <limits.h>
#include <inttypes.h>

Packet* packet_queue[PACKET_QUEUE_SIZE];
uint8_t packet_queue_top;

RoutingEntry* routing_table[ROUTING_TABLE_SIZE];
uint8_t routing_table_top;

RoutingEntry* routing_entry_create(uint8_t variable_count, VariableInfo* variables);

void routing_entry_destroy(RoutingEntry* entry);

int routing_table_update(uint16_t device_id, Interface* interface, uint8_t distance, TIME_TYPE time, 
    uint8_t variable_count, VariableInfo* variables);

void print_table();

bool find_entry(RoutingEntry** ptr, uint16_t to);

void packet_queue_remove(int index);

bool network_push_packet(Packet* packet);

void network_init(void){
    packet_queue_top = 0;
    routing_table_top = 0;

    for(int i = 0; i < PACKET_QUEUE_SIZE; i++){
        packet_queue[i] = NULL;
    }

    for(int i = 0; i < ROUTING_TABLE_SIZE; i++){
        routing_table[i] = NULL;
    }
}

RoutingEntry* routing_entry_create(uint8_t variable_count, VariableInfo* variables){
    size_t new_size = sizeof(RoutingEntry) + variable_count * sizeof(VariableInfo);
    RoutingEntry* entry = calloc(1, new_size);
    if(entry == NULL){
        return NULL;
    }

    entry->tx_id = 0;
    entry->rx_id = 0;
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

bool routing_entry_set_variables(RoutingEntry** entry, uint8_t variable_count, VariableInfo* variables){
    size_t new_size = sizeof(RoutingEntry) + variable_count * sizeof(VariableInfo);
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

int routing_table_insert(uint16_t device_id, Interface* interface, uint8_t distance, TIME_TYPE time, 
    uint8_t variable_count, VariableInfo* variables)
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

bool check_variables(RoutingEntry** entry, uint8_t variable_count, VariableInfo* variables){
    if((*entry)->variable_count != variable_count){
        return routing_entry_set_variables(entry, variable_count, variables);
    }

    for(int i = 0; i < variable_count; i++){
        if(((*entry)->variables[i].id != variables[i].id) || ((*entry)->variables[i].samples_per_day != variables[i].samples_per_day)){
            return routing_entry_set_variables(entry, variable_count, variables);
        }
    }
    return true;
}

int routing_table_update(uint16_t device_id, Interface* interface, uint8_t distance, TIME_TYPE time, 
    uint8_t variable_count, VariableInfo* variables)
{
    if(device_id == DEVICE_ID){
        return UPDATE_FAIL; // cannot add itself
    }

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
    for(int i = 0; i < packet_queue_top; i++){
        Packet* packet = packet_queue[i];
        if(packet->destination_interface->uid == interface->uid && packet->tx_id == id){
            packet_queue_remove(i);
            return;
        }
    }
}

// EVERY PACKET THAT ENDS UP IN THE QUEUE MUST PASS THIS FUNCTION
bool network_send_packet(Packet* packet, TIME_TYPE time){

    if(packet->from != DEVICE_ID && packet->command == ACK){
        ack_packet(packet->source_interface, packet->tx_id);

        return true;
    }


    if(packet_queue_top == PACKET_MAX_LENGTH){
        return false;
    }
        
    if(packet->from != DEVICE_ID){
        RoutingEntry* entry = NULL;
        if(!find_entry(&entry, packet->from)){
            return false;
        }

        if(entry->rx_id != packet->tx_id){
            return false; // something went wrong
        }

        entry->rx_id++;
    }

    packet->timestamp = time;
    packet->tx_id = 0; // not determined yet

    network_push_packet(packet);

    // SEND ACK HERE
    if(packet->from != DEVICE_ID){ // if it came from outside it will have source_interface set
        char buff[PACKET_MAX_LENGTH];
        if(!prepare_ack_message(buff, packet->from, packet->tx_id)){
            return false;
        }

        network_send_via(buff, strlen(buff), packet->source_interface);
    }

    return true;
}

bool network_push_packet(Packet* packet){
    if(packet_queue_top == PACKET_MAX_LENGTH){
        return false; // queue full
    }

    packet_queue[packet_queue_top] = packet;
    packet_queue_top += 1;

    printf("1 packet added to the queue\n");

    return true;
}

bool find_entry(RoutingEntry** ptr, uint16_t to){
    for(int i = 0; i < routing_table_top; i++){
        RoutingEntry* entry = routing_table[i];
        if(entry->device_id == to){
            *ptr = entry;
            return true;
        }
    }
    return false;
}

bool parseUNumber(const char *str,unsigned long *val, uint32_t max)
{
    char *temp;
    bool rc = true;
    errno = 0;
    *val = strtoul(str, &temp, 0);

    if (temp == str || *temp != '\0' ||
        ((*val == LONG_MAX) && errno == ERANGE))
        rc = false;

    if(*val > max){
        rc = false;
    }

    return rc;
}

bool parse_rip(char* msg, uint8_t* variable_count, VariableInfo** variables){
    char *token;
    token = strtok(msg, ",");
   
    *variable_count = 0;

    int i = 0;
    unsigned long variable_id;
    unsigned long samples_per_day;

    VariableInfo info={
        .id=0,
        .samples_per_day=0
    };

    while( token != NULL ) {

        if(i % 2 == 0) {
            if(!parseUNumber(token, &variable_id, UINT16_MAX)){
                free(*variables);
                printf("err 1\n");
                return false;
            }

            info.id=(uint16_t)variable_id;
        } else {
            if(!parseUNumber(token, &samples_per_day, UINT32_MAX)){
                free(*variables);
                printf("err 2\n");
                return false;
            }

            info.samples_per_day = samples_per_day;

            (*variable_count)++;
            VariableInfo* ptr = realloc(*variables, *variable_count * sizeof(VariableInfo));

            if(ptr == NULL){
                free(*variables);
                printf("err 3\n");
                return false;
            }

            *variables = ptr;
            (*variables)[*variable_count-1] = info;
        }
        token = strtok(NULL, ",");
        i++;
    }

    return true;
}

bool prepare_and_send(Packet* packet, RoutingEntry* entry){
    char buff[PACKET_MAX_LENGTH];
    if(!packet_to_string(packet, buff, PACKET_MAX_LENGTH)){
        return false;
    }

    if(packet->tx_id == 0){
        packet->tx_id = entry->tx_id++;
    }

    packet->destination_interface = entry->interface;

    network_send_via(buff, strlen(buff), entry->interface);
    return true;
}


bool process_broadcast_packet(Packet* packet){
    int size = routing_table_top; // how many packets will be created
    int free_space = PACKET_QUEUE_SIZE - packet_queue_top;

    if(size <= free_space){
        for(int i = 0; i < routing_table_top; i++){
            RoutingEntry* entry = routing_table[i];
            Packet* new_packet = network_prepare_packet(entry->device_id, packet->command, packet->message);
            network_send_packet(new_packet, packet->timestamp);
        }
    }
    
    return false;
}

void process_rip_packet(Packet* packet, TIME_TYPE now){
    uint8_t variable_count = 0;
    VariableInfo* variables = NULL;

    printf("RIP is [%s]\n", packet->message);
    if(!parse_rip(packet->message, &variable_count, &variables)){
        return;
    }

    if(packet->from == DEVICE_ID || routing_table_update(packet->from, packet->source_interface, packet->ttl, now, variable_count, variables) == UPDATE_SUCCESS){
        char buff[PACKET_MAX_LENGTH];
        if(!packet_to_string(packet, buff, PACKET_MAX_LENGTH)){
            return;
        }
        network_send_everywhere(buff, strlen(buff));
    }

    print_table();

    free(variables);
}

void packet_queue_remove(int index){
    packet_destroy(packet_queue[index]);
    packet_queue[index] = packet_queue[packet_queue_top - 1];
    packet_queue[packet_queue_top - 1] = NULL;
    packet_queue_top--;
}

void network_send_all(TIME_TYPE time){
    for(int i = 0; i < packet_queue_top; i++){
        printf("Processing queue\n");
        Packet* packet = packet_queue[i];

        if(packet == NULL){
            printf("FATAL BUG PACKET NULL\n");
            break;
        }

        if(time - packet->timestamp >= PACKET_TIMEOUT){
            goto remove;
        }

        if(packet->to == BROADCAST){
            if(!process_broadcast_packet(packet)){
                continue;
            }
            goto remove;
        }

        if(packet->to == DEVICE_ID){ 
            if(packet->command == RIP){
                process_rip_packet(packet, time);
                goto remove;
            }

            network_process_packet(packet);
            goto remove;
        }

        RoutingEntry* entry = NULL;
        if(!find_entry(&entry, packet->to)){
            goto remove;
        }

        prepare_and_send(packet, entry);
        continue;
        
        remove:
        packet_queue_remove(i);
    }
}

void routing_entry_remove(int index){
    for(int i = index; i < routing_table_top - 1; i++){
        routing_table[i] = routing_table[i+1];
    }

    routing_table_top--;
    routing_table[routing_table_top] = NULL;
}

void routing_table_check(TIME_TYPE time){
    int i = 0;
    while(i < routing_table_top){
        RoutingEntry* entry = routing_table[i];
        if(time - entry->last_seen > ROUTING_ENTRY_TIMEOUT){
            routing_entry_remove(i);
        }
        i++;
    }
}

void network_destroy(void){
    for(int i = 0; i < routing_table_top; i++){
        routing_entry_destroy(routing_table[i]);
    }

    for(int i = 0; i < packet_queue_top; i++){
        packet_destroy(packet_queue[i]);
    }
}

bool network_send_routing_info(uint8_t variable_count, VariableInfo* variables) {
    char msg[PACKET_MAX_LENGTH];
    size_t index = 0;

    for(int i = 0; i < variable_count; i++){
        VariableInfo var = variables[i];
        index += snprintf(&msg[index], PACKET_MAX_LENGTH - index, "%"SCNu16",%"SCNu32",", var.id, var.samples_per_day);
    }

    if(variable_count > 0){
        msg[index-1]='\0';
    }else{
        msg[0] = '\0';
    }
    
    Packet packet[1];
    packet->command = RIP;
    packet->from = DEVICE_ID;
    packet->to = BROADCAST;
    packet->ttl = 0;
    packet->message_size = strlen(msg);
    packet->message = msg;

    char buff[PACKET_MAX_LENGTH];
    
    if(!packet_to_string(packet, buff, PACKET_MAX_LENGTH)){
        return false;
    }

    network_send_everywhere(buff, strlen(buff));

    return true;
}

Packet* network_prepare_packet(uint16_t to, uint8_t command, char* msg){
    Packet* packet = packet_create();
    packet->command = command;
    packet->from = DEVICE_ID;
    packet->to = to;
    packet->ttl = 0;
    packet->message_size = strlen(msg);

    packet->message=malloc(packet->message_size+1);
    memcpy(packet->message, msg, packet->message_size + 1);

    return packet;
}

void print_table(){
    printf("Printing rounting table size %d\n", routing_table_top);
    for(int i = 0; i < routing_table_top; i++){
        RoutingEntry* entry = routing_table[i];
        printf("    Entry #%d\n", i);
        printf("        device=%d\n", entry->device_id);
        printf("        distanc=%d\n", entry->distance);
        printf("        interfa=%d\n", entry->interface->uid);
        printf("        lastseen=%"SCNu32"\n", entry->last_seen);
        printf("        var_count=%d\n", entry->variable_count);
        for(int i = 0; i < entry->variable_count; i++){
            VariableInfo var = entry->variables[i];
            printf("        var #%d\n", i);
            printf("            id=%d\n", var.id);
            printf("            sr=%d\n", var.samples_per_day);
        }
    }
}

int network_test(void){
    network_init();

    // there is no test

    network_destroy();
    return 0;
}