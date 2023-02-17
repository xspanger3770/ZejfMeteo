#ifndef _ZEJF_API_H
#define _ZEJF_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "zejf_defs.h"

void zejf_init(void);

void zejf_destroy(void);

/* =========== DATA ========== */

float* data_pointer(uint32_t day_number, VariableInfo target_variable);

bool data_log(VariableInfo target_variable, uint32_t day_number, uint32_t sample_num, float val, TIME_TYPE time, bool announce);

size_t day_load(Day** day, uint32_t day_number, size_t day_max_size);

bool day_save(Day* day);

void data_save(void);

void run_data_check(uint32_t current_day_num, uint32_t current_log_num, uint32_t days, TIME_TYPE time);

void data_requests_process(TIME_TYPE time);

//* =========== ROUTING ========== */

// check for inactive members of routing table
void routing_table_check(TIME_TYPE time);

// sends routing info to everyone
bool network_send_routing_info(TIME_TYPE time);

/* =========== NETWORK ========== */

// return number of allocated slots
size_t allocate_packet_queue(int priority);

// Packet received
bool network_accept(char* msg, int length, Interface* interface, TIME_TYPE time);

// Sending packet - adds to the queue
bool network_send_packet(Packet* packet, TIME_TYPE time);

// Iterate the queue
void network_send_all(TIME_TYPE time);

// send info about data variables that this device provides
bool network_send_provide_info(TIME_TYPE time);

// send info about data variables that this device wants to receive
bool network_send_demand_info(TIME_TYPE time);

// create Packet handle that can be send using network_send_packet
Packet* network_prepare_packet(uint16_t to, uint8_t command, char* msg);

// handle packed meant for this device
// target platform defines this
void network_process_packet(Packet* packet);

// send package as message using given interface
// target platform defines this
int network_send_via(char* msg, int length, Interface* interface, TIME_TYPE time);

// fills the pointers with all available inerfaces on target device
// target platform defines this
void get_all_interfaces(Interface*** interfaces, int* length);

void get_provided_variables(uint16_t* provide_count, VariableInfo** provided_variables);

void get_demanded_variables(uint16_t* demand_count, uint16_t** demanded_variables);

#endif