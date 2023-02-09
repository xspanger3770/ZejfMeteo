#include "zejf_network.h"
#include "zejf_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <limits.h>
#include <inttypes.h>

Packet* packet_queue[PACKET_QUEUE_SIZE];
size_t packet_queue_top;
size_t packet_next;

RoutingEntry* routing_table[ROUTING_TABLE_SIZE];
size_t routing_table_top;

RoutingEntry* routing_entry_create(uint8_t variable_count, VariableInfo* variables);

void routing_entry_destroy(RoutingEntry* entry);

int routing_table_update(uint16_t device_id, Interface* interface, uint8_t distance, TIME_TYPE time, 
    uint8_t variable_count, VariableInfo* variables);

void print_table();

bool find_entry(RoutingEntry** ptr, uint16_t to);

void packet_queue_remove(size_t index);

bool network_push_packet(Packet* packet);

bool process_rip_packet(Packet* packet, TIME_TYPE now);

void sync_id(RoutingEntry* entry, int back);

void network_init(void){
    packet_queue_top = 0;
    packet_next = 0;
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
    entry->rx_id = 1;
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
    for(size_t i = 0; i < routing_table_top; i++){
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
        (*existing_entry)->tx_id = 0;
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
    if(packet->from != DEVICE_ID && packet->command == ID_SYNC){
        RoutingEntry* entry = NULL;
        if(!find_entry(&entry, packet->from)){
            packet_destroy(packet);
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


    if(packet_queue_top == PACKET_MAX_LENGTH){
        packet_destroy(packet);
        return false;
    }
        
    if(packet->from != DEVICE_ID && packet->command != RIP){
        RoutingEntry* entry = NULL;
        if(!find_entry(&entry, packet->from)){
            packet_destroy(packet);
            return false;
        }

        if(entry->tx_id == 0){
            packet_destroy(packet);
            return false; // waiting for sync
        }

        if(entry->rx_id != packet->tx_id){
            packet_destroy(packet);
            return false; // something went wrong
        }

        entry->rx_id++;
    }

    uint32_t id_to_ack = packet->tx_id; 

    packet->time_received = time;
    packet->time_sent = 0;
    packet->tx_id = 0; // not determined yet

    network_push_packet(packet);

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
    if(packet_queue_top == PACKET_MAX_LENGTH){
        return false; // queue full
    }

    packet_queue[packet_queue_top] = packet;
    packet_queue_top += 1;

    return true;
}

bool find_entry(RoutingEntry** ptr, uint16_t to){
    for(size_t i = 0; i < routing_table_top; i++){
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

bool parse_rip(char* msg, int length, uint8_t* variable_count, VariableInfo** variables){
    char msg_copy[length+1];
    strncpy(msg_copy, msg, length+1);
    char *token;

    token = strtok(msg_copy, ",");
   
    *variable_count = 0;

    int i = 0;
    unsigned long variable_id;
    unsigned long samples_per_day;

    VariableInfo tmp={
        .id=0,
        .samples_per_day=0
    };

    while( token != NULL ) {

        if(i % 2 == 0) {
            if(!parseUNumber(token, &variable_id, UINT16_MAX)){
                free(*variables);
                return false;
            }

            tmp.id=(uint16_t)variable_id;
        } else {
            if(!parseUNumber(token, &samples_per_day, UINT32_MAX)){
                free(*variables);
                return false;
            }

            tmp.samples_per_day = samples_per_day;

            (*variable_count)++;
            VariableInfo* ptr = realloc(*variables, *variable_count * sizeof(VariableInfo));

            if(ptr == NULL){
                free(*variables);
                return false;
            }

            *variables = ptr;
            (*variables)[*variable_count-1] = tmp;
        }
        token = strtok(NULL, ",");
        i++;
    }

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

            bool rv = network_send_packet(new_packet, packet->time_received);
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
    uint8_t variable_count = 0;
    VariableInfo* variables = NULL;

    if(!parse_rip(packet->message, packet->message_size, &variable_count, &variables)){
        return false;
    }

    if(routing_table_update(packet->from, packet->source_interface, packet->ttl, now, variable_count, variables) == UPDATE_SUCCESS){
        network_send_everywhere(packet);
    }

    // print_table();

    free(variables);

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
    entry->rx_id = 0;
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

    if(time - packet->time_received >= PACKET_TIMEOUT){
        RoutingEntry* entry = NULL;
        if(find_entry(&entry, packet->to)){
            entry->tx_id = 0;
        }
        goto remove;
    }

    if(packet->to == BROADCAST){
        if(!process_broadcast_packet(packet)){
            goto next_one;
        }
        goto remove;
    }

    if(packet->to == DEVICE_ID){ 
        network_process_packet(packet);
        goto remove;
    }

    if(time - packet->time_sent < PACKET_RETRY_TIMEOUT){
        goto next_one;
    }

    packet->time_sent = time;

    RoutingEntry* entry = NULL;
    if(!find_entry(&entry, packet->to)){
        goto remove;
    }

    if(entry->tx_id == 0) {
        if(entry->rx_id != 0){
            sync_id(entry, 1);  
        }
        goto next_one;
    }

    prepare_and_send(packet, entry);

    next_one:
    packet_next ++;
    packet_next %= packet_queue_top;
    return;
    
    remove:
    packet_queue_remove(packet_next);
}

bool queue_full(){
    return packet_queue_top >= PACKET_QUEUE_SIZE;
}

void routing_entry_remove(size_t index){
    routing_entry_destroy(routing_table[index]);
    for(size_t i = index; i < routing_table_top - 1; i++){
        routing_table[i] = routing_table[i+1];
    }

    routing_table_top--;
    routing_table[routing_table_top] = NULL;
}

void routing_table_check(TIME_TYPE time){
    size_t i = 0;
    while(i < routing_table_top){
        RoutingEntry* entry = routing_table[i];
        if(time - entry->last_seen > ROUTING_ENTRY_TIMEOUT){
            routing_entry_remove(i);
        }
        i++;
    }

    // print_table();
}

void network_destroy(void){
    for(size_t i = 0; i < routing_table_top; i++){
        routing_entry_destroy(routing_table[i]);
    }

    for(size_t i = 0; i < packet_queue_top; i++){
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
    packet->to = 0; // RIP is special
    packet->ttl = 0;
    packet->tx_id = 0; // doesnt matter in rip packets
    packet->message_size = strlen(msg);
    packet->message = msg;
    packet->source_interface = NULL;
    packet->destination_interface = NULL;

    network_send_everywhere(packet);

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
    for(size_t i = 0; i < routing_table_top; i++){
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
            printf("            sr=%"SCNu32"\n", var.samples_per_day);
        }
    }
}

int network_test(void){
    return 0;
}