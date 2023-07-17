#include "DS3231.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdio.h>

DS3231::~DS3231() = default;

DS3231::DS3231(i2c_inst_t *inst, int sda, int scl, uint baudrate) {
    this->inst = inst;
    this->sda = sda;
    this->scl = scl;

    i2c_init(this->inst, baudrate);

    gpio_set_function(this->sda, GPIO_FUNC_I2C);
    gpio_set_function(this->scl, GPIO_FUNC_I2C);
    gpio_pull_up(this->sda);
    gpio_pull_up(this->scl);
}

DS3231::DS3231(i2c_inst_t *inst) {
    this->inst = inst;
}

uint8_t DS3231::dectobcd(const uint8_t val) {
    return ((val / 10 * 16) + (val % 10));
}

uint8_t DS3231::bcdtodec(const uint8_t val) {
    return ((val / 16 * 10) + (val % 16));
}

bool DS3231::set_date(uint8_t date, uint8_t month, uint8_t year, uint8_t day) {
    bool res = true;

    uint8_t sYbuf[2];
    sYbuf[0] = REG_YEAR;
    sYbuf[1] = ((year / 10) << 4) | (year % 10);
    res = res && i2c_write_blocking_until(inst, DEVICE_ADDR, sYbuf, 2, false, delayed_by_ms(get_absolute_time(), 1000)) == 2;

    uint8_t sMbuf[2];
    sMbuf[0] = REG_MON;
    sMbuf[1] = ((month / 10) << 4) | (month % 10);
    res = res && i2c_write_blocking_until(inst, DEVICE_ADDR, sMbuf, 2, false, delayed_by_ms(get_absolute_time(), 1000)) == 2;

    uint8_t sDbuf[2];
    sDbuf[0] = REG_DATE;
    sDbuf[1] = dectobcd(date);
    res = res && i2c_write_blocking_until(inst, DEVICE_ADDR, sDbuf, 2, false, delayed_by_ms(get_absolute_time(), 1000)) == 2;

    uint8_t sDwbuf[2];
    sDwbuf[0] = REG_DOW;
    sDwbuf[1] = dectobcd(day);
    res = res && i2c_write_blocking_until(inst, DEVICE_ADDR, sDwbuf, 2, false, delayed_by_ms(get_absolute_time(), 1000)) == 2;

    return res;
}

bool DS3231 ::read_date(uint8_t *date, uint8_t *month, uint8_t *year, uint8_t *day) {
    bool res = true;
    uint8_t buf[1] = { REG_DOW };
    uint8_t result[3] = { 0x00 };
    res = res && i2c_write_blocking_until(inst, DEVICE_ADDR, buf, 1, true, delayed_by_ms(get_absolute_time(), 1000)) == 1;
    res = res && i2c_read_blocking_until(inst, DEVICE_ADDR, result, 4, false, delayed_by_ms(get_absolute_time(), 1000)) == 4;
    if (res) {
        *day = result[0];
        *date = (10 * (result[1] >> 4)) + (result[1] & (0x0F));
        *month = (10 * (result[2] >> 4)) + (result[2] & (0x0F));
        *year = (10 * (result[3] >> 4)) + (result[3] & (0x0F));
    }

    return res;
}

bool DS3231::set_time(uint8_t hour, uint8_t min, uint8_t sec) {
    bool res = true;
    uint8_t setSbuf[2];
    setSbuf[0] = REG_SEC;
    setSbuf[1] = dectobcd(sec);
    res = res && i2c_write_blocking_until(inst, DEVICE_ADDR, setSbuf, 2, false, delayed_by_ms(get_absolute_time(), 1000)) == 2;

    uint8_t setMbuf[2];
    setMbuf[0] = REG_MIN;
    setMbuf[1] = dectobcd(min);
    res = res && i2c_write_blocking_until(inst, DEVICE_ADDR, setMbuf, 2, false, delayed_by_ms(get_absolute_time(), 1000)) == 2;

    uint8_t setHbuf[2];
    setHbuf[0] = REG_HOUR;
    setHbuf[1] = dectobcd(hour);
    res = res && i2c_write_blocking_until(inst, DEVICE_ADDR, setHbuf, 2, false, delayed_by_ms(get_absolute_time(), 1000)) == 2;

    return res;
}

bool DS3231::read_time(uint8_t *hour, uint8_t *min, uint8_t *sec) {
    bool res = true;
    uint8_t buf[1] = { REG_SEC };
    uint8_t result[3] = { 0x00 };
    res = res && i2c_write_blocking_until(inst, DEVICE_ADDR, buf, 1, true, delayed_by_ms(get_absolute_time(), 1000)) == 1;
    res = res && i2c_read_blocking_until(inst, DEVICE_ADDR, result, 3, false, delayed_by_ms(get_absolute_time(), 1000)) == 3;

    if (res) {
        *sec = (10 * (result[0] >> 4)) + (result[0] & (0x0f));
        *min = (10 * (result[1] >> 4)) + (result[1] & (0x0f));
        *hour = (result[2] & 15) + (10 * ((result[2] & 48) >> 4));
    }

    return res;
}
