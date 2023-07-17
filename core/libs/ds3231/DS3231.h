#ifndef DS3231_H
#define DS3231_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"



#define DEVICE_ADDR         _u(0x68)

#define REG_SEC             _u(0x00)
#define REG_MIN             _u(0x01)
#define REG_HOUR            _u(0x02)
#define REG_DATE            _u(0x04)
#define REG_MON             _u(0x05)
#define REG_YEAR            _u(0x06)
#define REG_DOW             _u(0x03)


class DS3231
{
	private:
		i2c_inst_t* inst;
		int sda;
		int scl;
	
		uint8_t dectobcd(const uint8_t val);
		uint8_t bcdtodec(const uint8_t val);


	public:
		bool set_date(uint8_t date, uint8_t month, uint8_t year, uint8_t day);
		bool read_date(uint8_t *date, uint8_t *month, uint8_t *year, uint8_t *day);
		
		bool set_time(uint8_t hour, uint8_t min, uint8_t sec);
		bool read_time(uint8_t *hour, uint8_t *min, uint8_t *sec);

		~DS3231();
		DS3231(i2c_inst_t *inst, int sda, int scl, uint baudrate);
		DS3231(i2c_inst_t *inst);
		
};

#endif
