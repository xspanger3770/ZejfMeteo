#ifndef _ZEJF_DATA_H
#define _ZEJF_DATA_H

#include <stdint.h>
#include "zejf_defs.h"

#define VARIABLE_INFO_BYTES (4 + 2)
#define DATAHOUR_BYTES (4 + 4 + 2 + 1)

void data_init(void);

void data_destroy(void);

Variable* get_variable(DataHour* hour, uint16_t variable_id);

DataHour* datahour_get(uint32_t hour_number, bool load, bool create);

float data_get_val(VariableInfo variable, uint32_t hour_number, uint32_t log_number);

#endif