#ifndef _ZEJF_OUTSIDE_H
#define _ZEJF_OUTSIDE_H

#include "time_engine.h"

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

#define RR_PIN 2
#define ADC_SOLAR_PIN 28
#define ADC_BATT0_PIN 27
#define ADC_BATT1_PIN 26

#endif