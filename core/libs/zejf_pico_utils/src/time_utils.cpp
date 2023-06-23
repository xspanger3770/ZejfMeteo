#include "time_utils.h"

time_t unix_time(datetime_t dt)
{
    tm t;
    t.tm_year = dt.year - 1900;
    t.tm_mon = dt.month - 1;
    t.tm_mday = dt.day;
    t.tm_hour = dt.hour;
    t.tm_min = dt.min;
    t.tm_sec = dt.sec;
    return mktime(&t);
}

void rtc_time(time_t t, datetime_t *dt)
{
    struct tm *tm = gmtime(&t);
    dt->year = tm->tm_year + 1900;
    dt->month = tm->tm_mon + 1;
    dt->day = tm->tm_mday;
    dt->dotw = 0; // not suppeoret
    dt->hour = tm->tm_hour;
    dt->min = tm->tm_min;
    dt->sec = tm->tm_sec;
}

uint32_t get_log_num(uint32_t unix_time, uint32_t sample_rate)
{
    return (unix_time % (60 * 60)) / ((60 * 60) / sample_rate);
}

uint32_t get_log_num_abs(uint32_t unix_time, uint32_t sample_rate)
{
    return (unix_time) / ((60 * 60) / sample_rate);
}

uint32_t get_hour_num(uint32_t unix_time)
{
    return unix_time / (60 * 60);
}