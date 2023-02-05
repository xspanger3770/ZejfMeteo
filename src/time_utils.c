#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "time_utils.h"

int64_t current_micros(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    int64_t s1 = (int64_t)(time.tv_sec) * 1000000l;
    int64_t s2 = (time.tv_usec);
    return s1 + s2;
}

int64_t current_millis(void)
{
    return current_micros() / 1000l;
}

int64_t current_seconds(void)
{
    return current_millis() / 1000l;
}

int32_t current_hours(void)
{
    return (int32_t)(current_seconds() / (60 * 60));
}

int32_t current_day(void){
    return current_hours() / 24;
}