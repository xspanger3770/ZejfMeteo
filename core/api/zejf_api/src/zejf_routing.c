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

static uint16_t next_free_id = RESERVED_DEVICE_IDS;

zejf_err routing_init(void) {
    routing_table_top = 0;

    for (int i = 0; i < ROUTING_TABLE_SIZE; i++) {
        routing_table[i] = NULL;
    }

    return ZEJF_OK;
}

void routing_destroy(void) {
    for (size_t i = 0; i < routing_table_top; i++) {
        routing_entry_destroy(routing_table[i]);
    }
}

RoutingEntry *routing_entry_create(void) {
    RoutingEntry *entry = calloc(1, sizeof(RoutingEntry));
    if (entry == NULL) {
        return NULL;
    }

    entry->paused = 0;
    entry->subscribed = false;

    entry->provided_count = 0;
    entry->provided_variables = NULL;

    entry->demand_count = 0;
    entry->demanded_variables = NULL;

    return entry;
}

void routing_entry_destroy(RoutingEntry *entry) {
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

zejf_err routing_entry_add_provided_variable(RoutingEntry *entry, VariableInfo provided_variable) {
    for (uint16_t i = 0; i < entry->provided_count; i++) {
        if (entry->provided_variables[i].id == provided_variable.id) {
            // TODO what about differeing sample rates?
            return ZEJF_ERR_GENERIC;
        }
    }

    size_t new_size = (++entry->provided_count) * sizeof(VariableInfo);

    VariableInfo *ptr = realloc(entry->provided_variables, new_size);
    if (ptr == NULL) {
        return ZEJF_ERR_OUT_OF_MEMORY;
    }

    entry->provided_variables = ptr;
    entry->provided_variables[entry->provided_count - 1] = provided_variable;

    return ZEJF_OK;
}

zejf_err routing_entry_add_demanded_variable(RoutingEntry *entry, uint16_t demanded_variable) {
    for (uint16_t i = 0; i < entry->demand_count; i++) {
        if (entry->demanded_variables[i] == demanded_variable) {
            return ZEJF_ERR_GENERIC;
        }
    }
    size_t new_size = (++entry->demand_count) * sizeof(uint16_t);

    uint16_t *ptr = realloc(entry->demanded_variables, new_size);
    if (ptr == NULL) {
        return ZEJF_ERR_OUT_OF_MEMORY;
    }

    entry->demanded_variables = ptr;
    entry->demanded_variables[entry->demand_count - 1] = demanded_variable;

    return ZEJF_OK;
}

RoutingEntry *routing_entry_find(uint16_t device_id) {
    for (size_t i = 0; i < routing_table_top; i++) {
        RoutingEntry *entry = routing_table[i];
        if (entry->device_id == device_id) {
            return entry;
        }
    }
    return NULL;
}

RoutingEntry *routing_entry_find_by_interface(int uid) {
    for (size_t i = 0; i < routing_table_top; i++) {
        RoutingEntry *entry = routing_table[i];
        if (entry->interface->uid == uid) {
            return entry;
        }
    }
    return NULL;
}

zejf_err routing_table_insert(uint16_t device_id, Interface *interface, uint8_t distance, TIME_TYPE time) {
    if (routing_table_top >= ROUTING_TABLE_SIZE) {
        return ZEJF_ERR_GENERIC;
    }

    RoutingEntry *entry = routing_entry_create();
    entry->device_id = device_id;
    entry->distance = distance;
    interface->tx_count = 0;
    interface->rx_count = 0;
    entry->interface = interface;
    entry->last_seen = time;

    routing_table[routing_table_top] = entry;
    routing_table_top++;

    ZEJF_LOG(1, "Device #%d added\n", device_id);

    return ZEJF_OK;
}

zejf_err routing_table_update(uint16_t device_id, Interface *interface, uint8_t distance, TIME_TYPE time) {
    if (device_id == DEVICE_ID) {
        return ZEJF_ERR_GENERIC; // cannot add itself
    }

    RoutingEntry *existing_entry = routing_entry_find(device_id);
    if (existing_entry == NULL) {
        return routing_table_insert(device_id, interface, distance, time);
    }

    int result = ZEJF_ERR_NO_CHANGE;

    if (existing_entry->distance > distance) {
        existing_entry->distance = distance;
        existing_entry->interface = interface;
        result = ZEJF_OK;
    }

    if (existing_entry->distance == distance) {
        existing_entry->last_seen = time;
        result = ZEJF_OK;
    }

    return result;
}

void routing_table_check(TIME_TYPE time) {
    size_t i = 0;
    while (i < routing_table_top) {
        RoutingEntry *entry = routing_table[i];
        if (time - entry->last_seen > ROUTING_ENTRY_TIMEOUT) {
            ZEJF_LOG(0, "Routing entry for device %d timeout\n", entry->device_id);
            routing_entry_remove(i);
        }
        i++;
    }
}

void routing_entry_remove(size_t index) {
    ZEJF_LOG(1, "Device #%d removed\n", routing_table[index]->device_id);
    routing_entry_destroy(routing_table[index]);
    for (size_t i = index; i < routing_table_top - 1; i++) {
        routing_table[i] = routing_table[i + 1];
    }

    routing_table_top--;
    routing_table[routing_table_top] = NULL;
}

void interface_removed(Interface *interface) {
    size_t i = 0;
    while (i < routing_table_top) {
        RoutingEntry *entry = routing_table[i];
        if (entry->interface->uid == interface->uid) {
            routing_entry_remove(i);
        }
        i++;
    }

    network_interface_removed(interface);
}

zejf_err network_send_routing_info(TIME_TYPE time) {
    Packet *packet = network_prepare_packet(0, RIP, NULL);
    packet->flags = 1; // set to 1 to allow mesh network
    if (packet == NULL) {
        return ZEJF_ERR_GENERIC;
    }

    zejf_err result = network_send_everywhere(packet, time);
    if (result != ZEJF_OK) {
        packet_destroy(packet);
        return result;
    }

    packet_destroy(packet);

    return ZEJF_OK;
}

void print_routing_table(uint32_t time) {
    printf("Routing table entries: %d/%d\n", (int) routing_table_top, ROUTING_TABLE_SIZE);
    for (size_t i = 0; i < routing_table_top; i++) {
        RoutingEntry *entry = routing_table[i];
        printf("    Device %d\n", entry->device_id);
        printf("        distance: %d\n", entry->distance);
        printf("        interface_id: %d\n", entry->interface->uid);
        printf("        last_seen: %ld ms ago\n", (long int) (time - entry->last_seen));
        printf("        paused: %d\n", entry->paused);
        printf("        provided variables: %d [", entry->provided_count);
        for (size_t i = 0; i < entry->provided_count; i++) {
            printf("%d@%ld", entry->provided_variables[i].id, (long int) entry->provided_variables[i].samples_per_hour);
            if ((int) i < entry->provided_count - 1) {
                printf(", ");
            }
        }
        printf("]\n        demanded variables: %" SCNu16 " [", entry->demand_count);
        for (size_t i = 0; i < entry->demand_count; i++) {
            printf("%" SCNu16 ", ", entry->demanded_variables[i]);
            if ((int) i < entry->demand_count - 1) {
                printf(", ");
            }
        }
        printf("]\n");
    }
}

uint16_t routing_find_free_id(void) {
    uint16_t id = next_free_id;

    while (true) {
        size_t i = 0;
        bool found = false;
        while (i < routing_table_top) {
            RoutingEntry *entry = routing_table[i];
            if (entry->device_id == id) {
                id++;
                if (id == 0) {
                    id = RESERVED_DEVICE_IDS;
                }
                if (id == next_free_id) {
                    return 0; // full
                }
                found = true;
                break;
            }
            i++;
        }

        if (!found) {
            break;
        }
    }

    next_free_id = id + 1;
    if (next_free_id == 0) {
        id = RESERVED_DEVICE_IDS;
    }

    return id;
}
