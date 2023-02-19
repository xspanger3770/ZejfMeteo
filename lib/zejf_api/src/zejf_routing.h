#ifndef _ZEJF_ROUTING_H
#define _ZEJF_ROUTINH_H

#include "zejf_defs.h"

extern RoutingEntry* routing_table[ROUTING_TABLE_SIZE];
extern size_t routing_table_top;

void routing_init(void);

void routing_destroy(void);

RoutingEntry* routing_entry_find(uint16_t device_id);

RoutingEntry* routing_entry_find_by_interface(int uid);

int routing_table_update(uint16_t device_id, Interface* interface, uint8_t distance, TIME_TYPE time);

bool routing_entry_add_demanded_variable(RoutingEntry* entry, uint16_t demanded_variable);

bool routing_entry_add_provided_variable(RoutingEntry* entry, VariableInfo provided_variable);

void print_table(void);

#endif