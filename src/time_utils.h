#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdint.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))


int64_t millis(void);

int64_t micros(void);

int64_t seconds(void);

int32_t hours(void);

#endif