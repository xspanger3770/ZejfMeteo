#ifndef _DATA_INFO_H
#define _DATA_INFO_H

#include "zejf_defs.h"
#include "zejf_network.h"

void process_data_demand(Packet *packet);

void process_data_provide(Packet *packet);

bool network_announce_log(VariableInfo target_variable, uint32_t hour_number, uint32_t sample_num, float val, TIME_TYPE time);

void process_data_log(Packet *packet, TIME_TYPE time);

int data_send_log(uint16_t to, VariableInfo variable, uint32_t hour_number, uint32_t sample_num, float val, TIME_TYPE time);

#endif