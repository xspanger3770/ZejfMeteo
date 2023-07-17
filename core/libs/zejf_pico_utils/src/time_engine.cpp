
#include <pico/stdlib.h>
#include <hardware/rtc.h>
#include <string>

#include "time_engine.h"

extern "C"{ 
    #include "zejf_api.h"
}

DS3231 ds3231_rtc(i2c0);
bool ds3231_working = false;

volatile bool time_set = false; // dont forgor
volatile bool time_checked = false;

static uint32_t time_check_count = 0;

volatile uint32_t millis_since_boot = 0;
volatile uint32_t millis_overflows = 0;

void time_engine_init(i2c_inst_t* i2c) {
    ds3231_rtc = {i2c};
    rtc_init();

    time_t time;

    if (ds3231_get(&time)) {
        ds3231_working = true;
        datetime_t dt;
        rtc_time(time, &dt);
        time_set = rtc_set_datetime(&dt);
    } else {
        ds3231_working = false;
        datetime_t dt = {
            .year = 2014,
            .month = 05,
            .day = 28,
            .dotw = 5,
            .hour = 15,
            .min = 45,
            .sec = 50
        };

        rtc_set_datetime(&dt);
    }

    ZEJF_LOG(1, "time_set = %d\n", time_set);

    sleep_us(64);

    millis_since_boot = to_ms_since_boot(get_absolute_time());
}

void update_millis(void) {
    uint32_t last_millis = millis_since_boot;
    millis_since_boot = to_ms_since_boot(get_absolute_time());
    if (millis_since_boot < last_millis) {
        millis_overflows++;
    }
}

void time_check(void) {
    if (time_set && time_checked) {
        time_check_count++;
        if (time_check_count < 12 * 30) {
            return;
        }
        time_check_count = 0;
    }

    Packet *packet = network_prepare_packet(BROADCAST, TIME_REQUEST, NULL);
    if (packet == NULL) {
        return;
    }

    if (network_send_packet(packet, millis_since_boot) != ZEJF_OK) {
        return;
    }
}

void receive_time(char *msg) {
    time_t time = strtol(msg, NULL, 10);

    ZEJF_LOG(1, "Time will be set to %lld\n", time);

    datetime_t dt;
    rtc_time(time, &dt);

    ds3231_working = ds3231_set(dt);

    ZEJF_LOG(1, "DS3231 set: %d\n", ds3231_working);

    if (rtc_set_datetime(&dt)) {
        time_set = true;
    }

    time_checked = true;

    ZEJF_LOG(1, "Time was set\n");
}

bool ds3231_get(time_t *time) {
    uint8_t date, month, year, day;
    uint8_t hour, min, sec;

    bool res = true;

    res = res && ds3231_rtc.read_date(&date, &month, &year, &day);
    res = res && ds3231_rtc.read_time(&hour, &min, &sec);

    if (res) {
        datetime_t dt;
        dt.year = year + 2000;
        dt.month = month;
        dt.day = date;
        dt.dotw = day;
        dt.hour = hour;
        dt.min = min;
        dt.sec = sec;
        *time = unix_time(dt);
    }

    return res;
}

bool ds3231_set(datetime_t dt) {
    bool res = true;

    res = res && ds3231_rtc.set_date(dt.day, dt.month, (dt.year - 2000), dt.dotw);
    res = res && ds3231_rtc.set_time(dt.hour, dt.min, dt.sec);

    return res;
}