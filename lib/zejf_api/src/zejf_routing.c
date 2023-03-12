#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_network.h"
#include "zejf_protocol.h"
#include "zejf_routing.h"

RoutingEntry *routing_table[ROUTING_TABLE_SIZE];
size_t routing_table_top;

RoutingEntry *routing_entry_create();

int routing_table_update(uint16_t device_id, Interface *interface, uint8_t distance, TIME_TYPE time);

void routing_entry_destroy(RoutingEntry *entry);

void routing_entry_remove(size_t index);

void routing_init(void)
{
    routing_table_top = 0;

    for (int i = 0; i < ROUTING_TABLE_SIZE; i++) {
        routing_table[i] = NULL;
    }
}

void routing_destroy(void)
{
    for (size_t i = 0; i < routing_table_top; i++) {
        routing_entry_destroy(routing_table[i]);
    }
}

RoutingEntry *routing_entry_create()
{
    RoutingEntry *entry = calloc(1, sizeof(RoutingEntry));
    if (entry == NULL) {
        return NULL;
    }

    entry->paused = 0;

    entry->provided_count = 0;
    entry->provided_variables = NULL;

    entry->demand_count = 0;
    entry->demanded_variables = NULL;

    return entry;
}

void routing_entry_destroy(RoutingEntry *entry)
{
    if (entry == NULL) {
        return;
    }

    if (entry->demanded_variables != NULL) {
        free(entry->demanded_variables);
    }

    if (entry->provided_variables != NULL) {
        free(entry->provided_variables);
    }

    free(entry);
}

bool routing_entry_add_provided_variable(RoutingEntry *entry, VariableInfo provided_variable)
{
    for (uint16_t i = 0; i < entry->provided_count; i++) {
        if (entry->provided_variables[i].id == provided_variable.id) {
            // TODO what about differeing sample rates?
            return false;
        }
    }

    size_t new_size = (++entry->provided_count) * sizeof(VariableInfo);

    VariableInfo *ptr = realloc(entry->provided_variables, new_size);
    if (ptr == NULL) {
        return false;
    }

    entry->provided_variables = ptr;
    entry->provided_variables[entry->provided_count - 1] = provided_variable;

    return true;
}

bool routing_entry_add_demanded_variable(RoutingEntry *entry, uint16_t demanded_variable)
{
    for (uint16_t i = 0; i < entry->demand_count; i++) {
        if (entry->demanded_variables[i] == demanded_variable) {
            return false;
        }
    }
    size_t new_size = (++entry->demand_count) * sizeof(uint16_t);

    uint16_t *ptr = realloc(entry->demanded_variables, new_size);
    if (ptr == NULL) {
        return false;
    }

    entry->demanded_variables = ptr;
    entry->demanded_variables[entry->demand_count - 1] = demanded_variable;

    return true;
}

RoutingEntry *routing_entry_find(uint16_t device_id)
{
    for (size_t i = 0; i < routing_table_top; i++) {
        RoutingEntry *entry = routing_table[i];
        if (entry->device_id == device_id) {
            return entry;
        }
    }
    return NULL;
}

RoutingEntry *routing_entry_find_by_interface(int uid)
{
    for (size_t i = 0; i < routing_table_top; i++) {
        RoutingEntry *entry = routing_table[i];
        if (entry->interface->uid == uid) {
            return entry;
        }
    }
    return NULL;
}

int routing_table_insert(uint16_t device_id, Interface *interface, uint8_t distance, TIME_TYPE time)
{
    if (routing_table_top >= ROUTING_TABLE_SIZE) {
        return UPDATE_FAIL;
    }

    RoutingEntry *entry = routing_entry_create();
    entry->device_id = device_id;
    entry->distance = distance;
    interface->tx_id = 0;
    interface->rx_id = 0;
    entry->interface = interface;
    entry->last_seen = time;

    routing_table[routing_table_top] = entry;
    routing_table_top++;

    return UPDATE_SUCCESS;
}

int routing_table_update(uint16_t device_id, Interface *interface, uint8_t distance, TIME_TYPE time)
{
    if (device_id == DEVICE_ID) {
        return UPDATE_FAIL; // cannot add itself
    }

    RoutingEntry *existing_entry = routing_entry_find(device_id);
    if (existing_entry == NULL) {
        return routing_table_insert(device_id, interface, distance, time);
    }

    int result = UPDATE_NO_CHANGE;

    if ((existing_entry)->distance > distance) {
        (existing_entry)->distance = distance;
        (existing_entry)->interface = interface;
        (existing_entry)->interface->tx_id = 0;
        result = UPDATE_SUCCESS;
    }

    if ((existing_entry)->distance == distance) {
        (existing_entry)->last_seen = time;
        result = UPDATE_SUCCESS;
    }

    return result;
}

void routing_table_check(TIME_TYPE time)
{
    size_t i = 0;
    while (i < routing_table_top) {
        RoutingEntry *entry = routing_table[i];
        if (time - entry->last_seen > ROUTING_ENTRY_TIMEOUT) {
            routing_entry_remove(i);
        }
        i++;
    }
}

void routing_entry_remove(size_t index)
{
    routing_entry_destroy(routing_table[index]);
    for (size_t i = index; i < routing_table_top - 1; i++) {
        routing_table[i] = routing_table[i + 1];
    }

    routing_table_top--;
    routing_table[routing_table_top] = NULL;
}

void interface_removed(Interface *interface)
{
    size_t i = 0;
    while (i < routing_table_top) {
        RoutingEntry *entry = routing_table[i];
        if (entry->interface == interface) {
            routing_entry_remove(i);
        }
        i++;
    }

    network_interface_removed(interface);
}

bool network_send_routing_info(TIME_TYPE time)
{
    Packet *packet = network_prepare_packet(0, RIP, NULL);
    if (packet == NULL) {
        return false;
    }

    network_send_everywhere(packet, time);

    packet_destroy(packet);

    return true;
}

void print_table(void)
{
    //printf("Printing routing table size %ld\n", routing_table_top);
    for (size_t i = 0; i < routing_table_top; i++) {
        RoutingEntry *entry = routing_table[i];
        printf("    Device %" SCNx16 "\n", entry->device_id);
        printf("        provided variables: %" SCNu16 "\n", entry->provided_count);
        printf("        demanded variables: %" SCNu16 "\n", entry->demand_count);
    }
}
