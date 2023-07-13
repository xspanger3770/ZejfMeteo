#ifndef _ZEJF_WIND_H
#define _ZEJF_WIND_H

#include "zejf_defs.h"

#define I2C_INST i2c0
#define I2C_SDA 16
#define I2C_SCK 17
#define I2C_SPEED (100 * 1000)

#define SD_SPI spi1
#define SD_MISO 12
#define SD_CS 13
#define SD_SCK 14
#define SD_MOSI 15
#define SD_SPI_SPEED (500 * 1000)

#define ALTITUDE 395

#define EVERY_MINUTE (60)
#define SECONDS_5 (60 * 12)

#define RR_PIN 2
#define ADC_SOLAR_PIN 28
#define ADC_BATT0_PIN 27
#define ADC_BATT1_PIN 26

#ifdef __cplusplus

#include <memory>

class zejf_log
{
  public:
    uint32_t log_num = 0;
    uint32_t hour_num = 0;
    uint32_t last_log_num = 0;
    uint32_t sample_rate;

    zejf_log(uint32_t sample_rate)
        : sample_rate{ sample_rate } {};

    virtual ~zejf_log() = default;

    bool is_ready(uint32_t unix_time)
    {
        uint32_t abs_log = get_log_num_abs(unix_time, sample_rate); 
        
        if(abs_log - last_log_num == 1){
            log_num = get_log_num(unix_time, sample_rate);
            last_log_num = abs_log;

            return true;
        } else if(abs_log != last_log_num){
            last_log_num = abs_log;
            reset();
        }

        return false;
    }

    virtual void log_data(uint32_t millis) const = 0;

    virtual void finish() = 0;

    virtual void reset() = 0;

    virtual std::unique_ptr<zejf_log> clone() const = 0;
};

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

const VariableInfo my_provided_variables[] = { VAR_T2M, VAR_RH, VAR_TD, VAR_RR, VAR_VOLTAGE_SOLAR, VAR_VOLTAGE_BATTERY, VAR_CURRENT_BATTERY, VAR_RAIN_RATE_PEAK };

Interface tcp_client_1 = {
    .uid = 2,
    .type = TCP,
    .handle = 0,
    .rx_count = 0,
    .tx_count = 0
};

Interface *all_interfaces[] = { &tcp_client_1 };

#endif

#endif