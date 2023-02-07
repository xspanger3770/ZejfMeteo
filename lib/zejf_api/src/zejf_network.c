#include "zejf_network.h"
#include "zejf_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <limits.h>
#include <inttypes.h>

Packet* packet_queue[PACKET_QUEUE_SIZE];
uint8_t queue_head;
uint8_t queue_tail;

RoutingEntry* routing_table[ROUTING_TABLE_SIZE];
uint8_t routing_table_top;

RoutingEntry* routing_entry_create(uint8_t variable_count, VariableInfo* variables);

void routing_entry_destroy(RoutingEntry* entry);

int routing_table_update(uint16_t device_id, uint8_t interface, uint8_t distance, time_t time, 
    uint8_t variable_count, VariableInfo* variables);

void print_table();

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

RoutingEntry* routing_entry_create(uint8_t variable_count, VariableInfo* variables){
    size_t new_size = sizeof(RoutingEntry) + variable_count * sizeof(VariableInfo);
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

int routing_table_insert(uint16_t device_id, uint8_t interface, uint8_t distance, time_t time, 
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

int routing_table_update(uint16_t device_id, uint8_t interface, uint8_t distance, time_t time, 
    uint8_t variable_count, VariableInfo* variables)
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

    int rv = packet_from_string(packet, msg, length);

    if(rv != 0){
        packet_destroy(packet);
        return false;
    }

    packet->source_interface = interface;
    packet->ttl++;

    if(!network_push_packet(packet)){
        packet_destroy(packet);
        return false;
    }

    return true;
}

bool network_push_packet(Packet* packet){
    if((queue_head + 1) % PACKET_QUEUE_SIZE == queue_tail){
        return false; // queue full
    }

    packet_queue[queue_head] = packet;
    queue_head += 1;
    queue_head %= PACKET_QUEUE_SIZE;

    printf("1 packet added to the queue\n");

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

void process_broadcast_packet(Packet* packet, bool self){
    char buff[PACKET_MAX_LENGTH];
    for(int i = 0; i < routing_table_top; i++){
        RoutingEntry* entry = routing_table[i];

        if(entry->device_id == packet->from){
            continue; // dont send back
        }

        if(!packet_to_string(packet, buff, PACKET_MAX_LENGTH)){
            continue;
        }
        network_send_via(buff, strlen(buff), entry->interface);
    }

    if(self){
        network_process_packet(packet);
    }
}

void process_rip_packet(Packet* packet, time_t now){
    uint8_t variable_count = 0;
    VariableInfo* variables = NULL;

    printf("RIP is [%s]\n", packet->message);
    if(!parse_rip(packet->message, &variable_count, &variables)){
        return;
    }

    if(routing_table_update(packet->from, packet->source_interface, packet->ttl, now, variable_count, variables) == UPDATE_SUCCESS){
        process_broadcast_packet(packet, false);
    }

    print_table();

    free(variables);
}

void network_send_all(time_t time){
    while(queue_tail != queue_head) {
        printf("Processing queue\n");
        Packet* packet = packet_queue[queue_tail];
        if(packet->ttl >= PACKET_MAX_TTL){
            goto finish;
        }

        if(packet->to == BROADCAST){
            if(packet->command == RIP){
                process_rip_packet(packet, time);
                goto finish;
            }
            process_broadcast_packet(packet, true);
            goto finish;
        }

        if(packet->to == DEVICE_ID){ 
            network_process_packet(packet);
            goto finish;
        }

        enum interface iface;
        if(!find_interface(&iface, packet->to)){
            goto finish;
        }

        char buff[PACKET_MAX_LENGTH];
        if(!packet_to_string(packet, buff, PACKET_MAX_LENGTH)){
            goto finish;
        }
        
        // pošli to dál
        network_send_via(buff, strlen(buff), iface);
        
        finish:
        packet_destroy(packet);
        packet_queue[queue_tail] = NULL;

        queue_tail+=1;
        queue_tail %= PACKET_QUEUE_SIZE;
    }
}

void routing_entry_remove(int index){
    for(int i = index; i < routing_table_top - 1; i++){
        routing_table[i] = routing_table[i+1];
    }

    routing_table_top--;
    routing_table[routing_table_top] = NULL;
}

void routing_table_check(time_t time){
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

    while(queue_tail != queue_head){
        packet_destroy(packet_queue[queue_tail]);
        queue_tail ++;
        queue_tail %= PACKET_QUEUE_SIZE;
    }
}

bool create_routing_message(char* buff, uint8_t variable_count, VariableInfo* variables) {
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
    
    return packet_to_string(packet, buff, PACKET_MAX_LENGTH);
}

bool create_packet(char* buff, uint16_t to, uint8_t command, char* msg){
    Packet packet[1];
    packet->command = command;
    packet->from = DEVICE_ID;
    packet->to = to;
    packet->ttl = 0;
    packet->message_size = strlen(msg);
    packet->message = msg;
    
    return packet_to_string(packet, buff, PACKET_MAX_LENGTH);
}

void print_table(){
    printf("Printing rounting table size %d\n", routing_table_top);
    for(int i = 0; i < routing_table_top; i++){
        RoutingEntry* entry = routing_table[i];
        printf("    Entry #%d\n", i);
        printf("        device=%d\n", entry->device_id);
        printf("        distanc=%d\n", entry->distance);
        printf("        interfa=%d\n", entry->interface);
        printf("        lastseen=%lld\n", entry->last_seen);
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

    VariableInfo T2M = {.id=1, .samples_per_day = 1440};

    VariableInfo vars[] = {T2M};

    char buff[PACKET_MAX_LENGTH];
    create_routing_message(buff, 1, vars);

    printf("%s\n", buff);

    int rv = network_accept(buff, strlen(buff), USB);
    printf("RV %d\n", rv);
    network_send_all(0);

    char msg[] = "0";

    Packet* pack1 = packet_create();
    pack1->from = DEVICE_ID;
    pack1->to = 2;
    pack1->command = TIME_CHECK;
    pack1->message_size = strlen(msg);
    pack1->message = msg;

    packet_to_string(pack1, buff, PACKET_MAX_LENGTH);

    printf("%s\n", buff);

    free(pack1);

    network_destroy();
    return 0;
}