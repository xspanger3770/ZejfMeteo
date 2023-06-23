#ifndef _HTU21_H
#define _HTU21_H

#include "hardware/i2c.h"

typedef struct htu_measurement_t{
    double temperature;
    double humidity;
    double dewpoint;
    bool valid;
} htu_measurement;

bool htu21_init(i2c_inst_t* i2c, uint sda_pin, uint sck_pin, uint baudrate);

htu_measurement htu21_measure();

#endif