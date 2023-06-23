#ifndef _TIME_UTILS_H
#define _TIME_UTILS_H

#include "pico/util/datetime.h"
#include <time.h>

#define DAY_SECS (24 * 60 * 60)

time_t unix_time(datetime_t dt);

void rtc_time(time_t t, datetime_t *dt);

uint32_t get_log_num(uint32_t unix_time, uint32_t sample_rate);

uint32_t get_log_num_abs(uint32_t unix_time, uint32_t sample_rate);

uint32_t get_hour_num(uint32_t unix_time);

#endif