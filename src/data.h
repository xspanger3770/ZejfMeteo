#ifndef DATA_H
#define DATA_H

#include <pthread.h>

#define WINDOW 60 //s

#define UNKNOWN -999.0
#define NOT_MEASURED -998.0

typedef struct  zejfdata_log_t {
    float pressure;
    float temperature;
    float humidity;
    float wspd;
    float wdir;
    float rr;
} ZejfLog;

extern pthread_mutex_t data_mutex;

#endif