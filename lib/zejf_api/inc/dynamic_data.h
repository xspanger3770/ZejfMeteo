#ifndef _DYNAMIC_DAY_H
#define _DYNAMIC_DAY_H

#define ERROR (-999.0)

#define ONE_PER_MINUTE (60 * 24)

#include "zejf_settings.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct variable_info_t{
    uint16_t id;
    uint32_t samples_per_day;
} VariableInfo;

typedef struct variable_t{
    VariableInfo info;
    float* _start;
} Variable;

typedef struct dynamic_day_t{
    size_t total_size;
    uint32_t day_number; // todo checksum?
    int16_t variable_count;
    bool modified;
    Variable* variables;
    uint8_t data[];
} Day;

typedef struct data_request_t{
    VariableInfo variable;
    uint32_t start_day;
    uint32_t end_day;
    uint32_t start_log;
    uint32_t end_log;
    uint32_t current_day;
    uint32_t current_log;
} DataRequest;

void data_init(void);

float* data_pointer(uint32_t day_number, VariableInfo target_variable);

bool data_log(VariableInfo target_variable, uint32_t day_number, uint32_t sample_num, float val);

size_t day_load(Day** day, uint32_t day_number, size_t day_max_size);

bool day_save(Day* day);

void data_save(void);

void data_destroy(void);

#endif