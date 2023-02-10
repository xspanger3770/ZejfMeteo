#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>

#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_routing.h"
#include "zejf_network.h"
#include "zejf_protocol.h"

RoutingEntry* routing_table[ROUTING_TABLE_SIZE];
size_t routing_table_top;

RoutingEntry* routing_entry_create();

int routing_table_update(uint16_t device_id, Interface* interface, uint8_t distance, TIME_TYPE time);

void print_table();

void routing_entry_destroy(RoutingEntry* entry);

void routing_entry_remove(size_t index);

void routing_init(void){
    routing_table_top = 0;

    for(int i = 0; i < ROUTING_TABLE_SIZE; i++){
        routing_table[i] = NULL;
    }
}

void routing_destroy(void) {
    for(size_t i = 0; i < routing_table_top; i++){
        routing_entry_destroy(routing_table[i]);
    }
}

RoutingEntry* routing_entry_create(){
    RoutingEntry* entry = calloc(1, sizeof(RoutingEntry));
    if(entry == NULL){
        return NULL;
    }

    entry->tx_id = 0;
    entry->rx_id = 0;
    entry->paused = 0;
    
    entry->provided_count = 0;
    entry->provided_variables = NULL;

    entry->demand_count = 0;
    entry->demanded_variables = NULL;

    return entry;
}

void routing_entry_destroy(RoutingEntry* entry){
    if(entry == NULL){
        return;
    }

    if(entry->demanded_variables != NULL){
        free(entry->demanded_variables);
    }

    if(entry->provided_variables != NULL){
        free(entry->provided_variables);
    }

    free(entry);
}

bool routing_entry_add_provided_variable(RoutingEntry* entry, VariableInfo provided_variable){
    size_t new_size = (entry->provided_count++) * sizeof(VariableInfo);

    VariableInfo* ptr = realloc(entry->provided_variables, new_size);
    if(ptr == NULL){
        return false;
    }

    entry->provided_variables = ptr;
    entry->provided_variables[entry->provided_count-1] = provided_variable;

    return true;
}

bool routing_entry_add_demanded_variable(RoutingEntry* entry, uint16_t demanded_variable){
    size_t new_size = (entry->demand_count++) * sizeof(uint16_t);

    uint16_t* ptr = realloc(entry->demanded_variables, new_size);
    if(ptr == NULL){
        return false;
    }

    entry->demanded_variables = ptr;
    entry->demanded_variables[entry->demand_count-1] = demanded_variable;

    return true;
}

RoutingEntry* routing_entry_find(uint16_t device_id){
    for(size_t i = 0; i < routing_table_top; i++){
        RoutingEntry* entry = routing_table[i];
        if(entry->device_id == device_id){
            return entry;
        }
    }
    return NULL;
}

int routing_table_insert(uint16_t device_id, Interface* interface, uint8_t distance, TIME_TYPE time)
{
    if(routing_table_top >= ROUTING_TABLE_SIZE){
        return UPDATE_FAIL;
    }
    RoutingEntry* entry = routing_entry_create();
    entry->device_id=device_id;
    entry->distance=distance;
    entry->interface=interface;
    entry->last_seen=time;
    
    routing_table[routing_table_top] = entry;
    routing_table_top++;
    
    return UPDATE_SUCCESS;
}

int routing_table_update(uint16_t device_id, Interface* interface, uint8_t distance, TIME_TYPE time)
{
    if(device_id == DEVICE_ID){
        return UPDATE_FAIL; // cannot add itself
    }

    RoutingEntry* existing_entry = routing_entry_find(device_id);
    if(existing_entry == NULL){
        return routing_table_insert(device_id, interface, distance, time);
    }

    int result = UPDATE_NO_CHANGE;

    if((existing_entry)->distance > distance){
        (existing_entry)->distance=distance;
        (existing_entry)->interface=interface;
        (existing_entry)->tx_id = 0;
        result = UPDATE_SUCCESS;
    }

    if((existing_entry)->distance == distance){
        (existing_entry)->last_seen = time;
        return UPDATE_FAIL;
    }

    return result;
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
}

void routing_entry_remove(size_t index){
    routing_entry_destroy(routing_table[index]);
    for(size_t i = index; i < routing_table_top - 1; i++){
        routing_table[i] = routing_table[i+1];
    }

    routing_table_top--;
    routing_table[routing_table_top] = NULL;
}

bool network_send_routing_info(void) {    
    Packet* packet = network_prepare_packet(0, RIP, NULL);
    if(packet == NULL){
        return false;
    }

    network_send_everywhere(packet);

    packet_destroy(packet);

    return true;
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

bool _parse_rip(char* msg, int length, uint8_t* variable_count, VariableInfo** variables){
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

// finish if needed
/*void print_table(){ 
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
}*/