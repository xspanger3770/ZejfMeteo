#ifndef _ZEJF_ROUTING_H
#define _ZEJF_ROUTINH_H

#include "zejf_defs.h"

extern RoutingEntry *routing_table[ROUTING_TABLE_SIZE];
extern size_t routing_table_top;

zejf_err routing_init(void);

void routing_destroy(void);

RoutingEntry *routing_entry_create();

void routing_entry_destroy(RoutingEntry *entry);

void routing_entry_remove(size_t index);

zejf_err routing_table_update(uint16_t device_id, Interface *interface, uint8_t distance, TIME_TYPE time);

RoutingEntry *routing_entry_find(uint16_t device_id);

RoutingEntry *routing_entry_find_by_interface(int uid);

zejf_err routing_entry_add_demanded_variable(RoutingEntry *entry, uint16_t demanded_variable);

zejf_err routing_entry_add_provided_variable(RoutingEntry *entry, VariableInfo provided_variable);

uint16_t routing_find_free_id();

#endif