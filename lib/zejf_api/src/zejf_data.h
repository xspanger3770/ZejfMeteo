#ifndef _DYNAMIC_DAY_H
#define _DYNAMIC_DAY_H

#include "zejf_defs.h"

void data_init(void);

size_t day_load(Day** day, uint32_t day_number, size_t day_max_size);

bool day_save(Day* day);

void data_save(void);

void data_destroy(void);

#endif