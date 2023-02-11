#ifndef _DYNAMIC_DAY_H
#define _DYNAMIC_DAY_H

#include "zejf_defs.h"

void data_init(void);

void data_destroy(void);

Variable* get_variable(Day* day, uint16_t variable_id);

Day** day_get(uint32_t day_number, bool load, bool create);

#endif