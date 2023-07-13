#ifndef _DATA_INFO_H
#define _DATA_INFO_H

#include "zejf_defs.h"
#include "zejf_network.h"

zejf_err process_data_demand(Packet *packet);

zejf_err process_data_provide(Packet *packet);

zejf_err process_data_subscribe(Packet *packet);

zejf_err network_announce_log(VariableInfo target_variable, uint32_t hour_number, uint32_t sample_num, float val, TIME_TYPE time);

zejf_err network_broadcast_log(VariableInfo target_variable, uint32_t hour_number, uint32_t sample_num, float val, TIME_TYPE time);

zejf_err process_data_log(Packet *packet, TIME_TYPE time);

zejf_err data_send_log(uint16_t to, VariableInfo variable, uint32_t hour_number, uint32_t sample_num, float val, TIME_TYPE time);

#endif