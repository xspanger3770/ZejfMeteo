#ifndef _ZEJF_API_H
#define _ZEJF_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "zejf_defs.h"

zejf_err zejf_init(void);

void zejf_destroy(void);

/* =========== DATA ========== */

zejf_err data_log(VariableInfo target_variable, uint32_t hour_number, uint32_t sample_number, float value, TIME_TYPE time, bool announce);

DataHour *datahour_get(uint32_t hour_number, bool load, bool create);

Variable *get_variable(DataHour *hour, uint16_t variable_id);

zejf_err hour_load(uint8_t **data_buffer, size_t* size, uint32_t hour_number);

zejf_err hour_save(uint32_t hour_number, uint8_t *buffer, size_t total_size);

void data_save(void);

void run_data_check(uint32_t current_hour_num, uint32_t current_millis_in_hour, uint32_t hours, TIME_TYPE time);

void data_requests_process(TIME_TYPE time);

bool data_requests_ready(void);

//* =========== ROUTING ========== */

// check for inactive members of routing table
void routing_table_check(TIME_TYPE time);

// sends routing info to everyone
zejf_err network_send_routing_info(TIME_TYPE time);

/* =========== NETWORK ========== */

// return number of allocated slots
size_t allocate_packet_queue(int priority);

// Packet received
zejf_err network_accept(char *msg, int length, Interface *interface, TIME_TYPE time);

// Sending packet - adds to the queue
zejf_err network_send_packet(Packet *packet, TIME_TYPE time);

// Iterate the queue
void network_process_packets(TIME_TYPE time);

// send info about data variables that this device provides
zejf_err network_send_provide_info(TIME_TYPE time);

// send info about data variables that this device wants to receive
zejf_err network_send_demand_info(TIME_TYPE time);

// create Packet handle that can be send using network_send_packet
Packet *network_prepare_packet(uint16_t to, uint8_t command, char *msg);

// handle packed meant for this device
// target platform defines this
void network_process_packet(Packet *packet);

// send package as message using given interface
// target platform defines this
zejf_err network_send_via(char *msg, int length, Interface *interface, TIME_TYPE time);

// fills the pointers with all available inerfaces on target device
// target platform defines this
void get_all_interfaces(Interface ***interfaces, size_t *length);

void interface_removed(Interface *interface);

void get_provided_variables(uint16_t *provide_count, const VariableInfo **provided_variables);

void get_demanded_variables(uint16_t *demand_count, uint16_t **demanded_variables);

void print_routing_table(uint32_t time);

bool network_has_packets(void);

#endif