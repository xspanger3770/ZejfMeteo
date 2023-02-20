#ifndef _ZEJF_DATA_2_H
#define _ZEJF_DATA_2_H

#include <stdint.h>
#include "zejf_defs.h"

typedef struct data_variable_t{
    VariableInfo variable_info;
    float* data;
} DataVariable;

typedef struct datahour_t{
    uint32_t hour_id;
    uint32_t checksum;
    uint16_t variable_count;
    uint8_t flags;
    DataVariable* variables;
}DataHour;

#define VARIABLE_INFO_BYTES (4 + 2)
#define DATAHOUR_BYTES (4 + 4 + 2 + 1)
#define SAMPLE_RATE_MAX (60 * 12) //3kb
#define VARIABLES_MAX 2 //3*4 = 12kb per hour

void data_2_init(void);

void data_2_destroy(void);

#endif