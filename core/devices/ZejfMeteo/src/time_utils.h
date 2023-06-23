#ifndef _TIME_UTILS_H
#define _TIME_UTILS_H

#include <stdint.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

int64_t current_micros(void);

int64_t current_millis(void);

int64_t current_seconds(void);

int32_t current_hours(void);

int32_t current_day(void);

#endif