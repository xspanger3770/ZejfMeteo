#ifndef ZEJF_MEASURE_H
#define ZEJF_MEASURE_H

#include <cstdint>
#include <array>

#include "time_utils.h"
#include "zejf_pico_data.h"

extern "C"{
    #include "zejf_defs.h"
}

const VariableInfo VAR_WSPD_AVG = {
    .samples_per_hour = EVERY_MINUTE,
    .id = 0x03
};

const VariableInfo VAR_WGUST_AVG = {
    .samples_per_hour = EVERY_MINUTE,
    .id = 0x04
};

const VariableInfo VAR_WDIR_AVG = {
    .samples_per_hour = EVERY_MINUTE,
    .id = 0x05
};

const VariableInfo VAR_PRESS = {
    .samples_per_hour = EVERY_MINUTE,
    .id = 0x06
};

const VariableInfo VAR_TEMP = {
    .samples_per_hour = EVERY_MINUTE,
    .id = 0x07
};

const VariableInfo VAR_WSPD_NOW = {
    .samples_per_hour = SECONDS_5,
    .id = 0x08
};

const VariableInfo VAR_WGUST_NOW = {
    .samples_per_hour = SECONDS_5,
    .id = 0x09
};

const VariableInfo VAR_WDIR_NOW = {
    .samples_per_hour = SECONDS_5,
    .id = 0x0a
};

const VariableInfo my_provided_variables[] = { VAR_WSPD_AVG, VAR_WGUST_AVG, VAR_WDIR_AVG, VAR_PRESS, VAR_TEMP, VAR_WSPD_NOW, VAR_WGUST_NOW, VAR_WDIR_NOW };

extern volatile bool bmp_initialised;
extern volatile uint16_t wspd_c;

void wspd_callback(uint gpio, uint32_t events);

bool bmp_measure(struct repeating_timer *);

bool process_measurements(struct repeating_timer *);

#endif