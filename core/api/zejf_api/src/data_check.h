#ifndef _DATA_CHECK_H
#define _DATA_CHECK_H

#define ENTIRE_HOUR UINT32_MAX
#define DATA_CHECK_DELAY 5ul

#include "zejf_defs.h"

zejf_err data_check_receive(Packet *packet);

zejf_err data_check_send(uint16_t to, VariableInfo variable, uint32_t hour_num, uint32_t log_num, TIME_TYPE time);

#endif