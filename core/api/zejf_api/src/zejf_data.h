#ifndef _ZEJF_DATA_H
#define _ZEJF_DATA_H

#include "zejf_defs.h"
#include <stdint.h>

#define VARIABLE_INFO_BYTES (4 + 2)
#define DATAHOUR_BYTES (4 + 4 + 2 + 1)

#define FLAG_MODIFIED (1 << 0)

zejf_err data_init(void);

void data_destroy(void);

void *hour_destroy(void *ptr);

zejf_err data_get_val(VariableInfo variable, uint32_t hour_number, uint32_t log_number, bool load, bool create_new, float *target);

zejf_err variables_request_receive(Packet *packet, TIME_TYPE time);

#endif