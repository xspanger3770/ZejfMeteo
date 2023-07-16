#ifndef _DS3231_ENGINE_H
#define _DS3231_ENGINE_H

#include <ctime>

#include "DS3231.h"
#include "time_utils.h"
#include "zejf_defs.h"

extern DS3231 ds3231_rtc;
extern volatile bool time_set;
extern volatile bool time_checked;
extern bool ds3231_working;

extern "C" {
    extern volatile uint32_t millis_since_boot;
}
extern volatile uint32_t millis_overflows;

bool ds3231_get(time_t *time); 
bool ds3231_set(datetime_t dt);
void receive_time(char* msg);
void time_check(void);
void update_millis(void);
void time_engine_init(void);

#endif //_DS3231_ENGINE_H