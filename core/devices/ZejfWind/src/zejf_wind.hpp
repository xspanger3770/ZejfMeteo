#ifndef _ZEJF_WIND_H
#define _ZEJF_WIND_H

#include "zejf_defs.h"

extern volatile uint32_t millis;
extern Interface tcp_client_1;

#define I2C_INST i2c1
#define I2C_SDA 14
#define I2C_SCK 15
#define I2C_SPEED (500 * 1000)

#define SD_SPI spi0
#define SD_MISO 16
#define SD_CS 17
#define SD_SCK 18
#define SD_MOSI 19
#define SD_SPI_SPEED (1250 * 1000)

#define ALTITUDE 395

#define EVERY_MINUTE (60)
#define SECONDS_5 (60 * 12)

#define WSPD_PIN 2
#define WDIR_PIN 26

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

        if (abs_log - last_log_num == 1) {
            log_num = get_log_num(unix_time, sample_rate);
            last_log_num = abs_log;

            return true;
        } else if (abs_log != last_log_num) {
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

Interface tcp_client_1 = {
    .uid = 2,
    .type = TCP,
    .handle = 0,
    .rx_count = 0,
    .tx_count = 0
};

Interface *all_interfaces[] = { &tcp_client_1 };

#endif