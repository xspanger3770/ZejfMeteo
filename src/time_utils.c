#include <stdbool.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#include "data.h"
#include "time_utils.h"

int64_t millis(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    int64_t s1 = (int64_t)(time.tv_sec) * 1000;
    int64_t s2 = (time.tv_usec / 1000);
    return s1 + s2;
}

int64_t micros(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    int64_t s1 = (int64_t)(time.tv_sec) * 1000000;
    int64_t s2 = (time.tv_usec);
    return s1 + s2;
}

int64_t seconds(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec;
}

int32_t hours(void)
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (int32_t)(time.tv_sec / (60 * 60));
}