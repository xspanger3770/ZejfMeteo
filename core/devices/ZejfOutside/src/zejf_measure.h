#ifndef ZEJF_MEASURE_H
#define ZEJF_MEASURE_H

#include <cstdint>
#include <array>

#include "time_utils.h"
#include "zejf_pico_data.h"

extern "C"{
    #include "zejf_defs.h"
}

const VariableInfo VAR_T2M = {
    .samples_per_hour = SECONDS_5,
    .id = 11
};

const VariableInfo VAR_RH = {
    .samples_per_hour = SECONDS_5,
    .id = 12
};

const VariableInfo VAR_TD = {
    .samples_per_hour = SECONDS_5,
    .id = 13
};

const VariableInfo VAR_RR = {
    .samples_per_hour = SECONDS_5,
    .id = 14
};

const VariableInfo VAR_VOLTAGE_SOLAR = {
    .samples_per_hour = EVERY_MINUTE,
    .id = 15
};

const VariableInfo VAR_VOLTAGE_BATTERY = {
    .samples_per_hour = EVERY_MINUTE,
    .id = 16
};

const VariableInfo VAR_CURRENT_BATTERY = {
    .samples_per_hour = EVERY_MINUTE,
    .id = 17
};

const VariableInfo VAR_RAIN_RATE_PEAK = {
    .samples_per_hour = SECONDS_5,
    .id = 18
};

extern volatile bool htu_initialised;
extern volatile uint16_t rr_c;
extern long htu_errors;
extern std::array<zejf_log *, 3> all_logs;

bool process_measurements(struct repeating_timer *);
bool htu_measure(struct repeating_timer *);
void rr_callback(uint gpio, uint32_t);

#endif