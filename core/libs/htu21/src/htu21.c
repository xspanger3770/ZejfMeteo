#include "pico/binary_info.h"
#include "pico/stdlib.h"

#include <math.h>
#include <stdio.h>

#include "htu21.h"

#define HTU_ADDR 0x40
#define HTU_CRC_POLY (0x131)

#define HTU_COMMAND_SOFT_RESET 0xFE
#define HTU_COMMAND_GET_TEMPERATURE 0xE3
#define HTU_COMMAND_GET_HUMIDITY 0xE5

i2c_inst_t *i2c_inst;

uint8_t gencrc(uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    size_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if ((crc & 0x80) != 0)
                crc = (uint8_t) ((crc << 1) ^ HTU_CRC_POLY);
            else
                crc <<= 1;
        }
    }
    return crc;
}

bool htu21_reset(){
    uint8_t buf[1];
    buf[0] = HTU_COMMAND_SOFT_RESET;
    bool res = i2c_write_blocking_until(i2c_inst, HTU_ADDR, buf, 1, false, delayed_by_ms(get_absolute_time(), 1000)) == 1;
    return res;
}

bool htu21_init(i2c_inst_t *i2c, uint sda_pin, uint sck_pin, uint baudrate) {
    i2c_inst = i2c;
    i2c_init(i2c, baudrate);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(sck_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(sck_pin);

    return htu21_reset();
}

double htu_calculate_dewpoint(double t, double rh) {
    return (243.04 * (log(rh / 100.0) + ((17.625 * t) / (243.04 + t)))) / (17.625 - log(rh / 100) - ((17.625 * t) / (243.04 + t)));
}

double htu_get(uint8_t command, bool* valid){
    if(!*valid){
        return -1;
    }
    uint8_t buf[1];
    buf[0] = command;
    if (i2c_write_blocking_until(i2c_inst, HTU_ADDR, buf, 1, false, delayed_by_ms(get_absolute_time(), 1000)) != 1) {
        *valid = false;
        return -1;
    }

    uint8_t in[3] = { 0 };
    if (i2c_read_blocking_until(i2c_inst, HTU_ADDR, in, 3, false, delayed_by_ms(get_absolute_time(), 1000)) != 3) {
        *valid = false;
        return -1;
    }

    if (gencrc(in, 2) != in[2]) {
        *valid = false;
        return -1;
    }

    uint16_t sensor_data = (in[0] << 8 | in[1]) & 0xFFFC;
    return sensor_data / 65536.0;
}

htu_measurement htu21_measure() {
    htu_measurement result = { 0 };
   
    result.valid = true;
    result.temperature = -46.85 + (175.72 * htu_get(HTU_COMMAND_GET_TEMPERATURE, &result.valid));
    result.humidity = -6.0 + (125.0 * htu_get(HTU_COMMAND_GET_HUMIDITY, &result.valid));
    result.dewpoint = htu_calculate_dewpoint(result.temperature, result.humidity);
    
    return result;
}