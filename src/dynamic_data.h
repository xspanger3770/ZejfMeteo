#ifndef _DYNAMIC_DAY_H
#define _DYNAMIC_DAY_H

#define ERROR (-999.0)

#define ONE_PER_MINUTE (60 * 24)

#define DAY_BUFFER_SIZE (128)
#define DAY_MAX_SIZE (1 * 1024 * 1024)

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct variable_t{
    uint16_t id;
    size_t samples_per_day;
    float* _start;
} Variable;

typedef struct dynamic_day_t{
    size_t total_size;
    uint32_t day_number;
    int16_t variable_count;
    bool modified;
    Variable* variables;
    uint8_t data[];
} Day;

void data_init(void);

float* data_pointer(uint32_t day_number, Variable target_variable);

bool data_log(Variable target_variable, uint32_t day_number, size_t sample_num, float val);

size_t day_load(Day** day, uint32_t day_number, size_t day_max_size);

bool day_save(Day* day);

void data_save(void);

void data_destroy(void);

#endif