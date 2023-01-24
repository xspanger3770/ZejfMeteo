#ifndef DATA_H
#define DATA_H

#include <pthread.h>
#include <stdint.h>

#include "arraylist.h"

#define MAIN_FOLDER "./ZejfMeteo/"

#define WINDOW 60 //s
#define LOGS_PER_DAY ((24 * 60 * 60) / WINDOW)

#define UNKNOWN -999.0
#define NOT_MEASURED -998.0

typedef struct zejfdata_log_t {
    float pressure;
    float temperature;
    float humidity;
    float wspd;
    float wdir;
    float rr;
} ZejfLog;

typedef struct zejfdata_day_t{
    uint32_t day_number;
    ZejfLog logs[LOGS_PER_DAY];
} ZejfDay;

extern pthread_mutex_t data_mutex;
extern ArrayList* zejf_days;

void zejf_day_path(char* buff, uint32_t day_number);

ZejfDay* zejf_day_create(uint32_t day_number);

void zejf_day_destroy(ZejfDay* zejf_day);

ZejfDay* zejf_day_get(uint32_t day_number, bool load, bool create_new);

void data_init();

void data_destroy();

#endif