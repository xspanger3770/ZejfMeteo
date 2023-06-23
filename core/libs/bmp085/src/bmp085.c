#include "bmp085.h"
#include "hardware/i2c.h"
#include <math.h>

// Calibration data registers
#define BMP085_CAL_AC1 0xAA //!< R   Calibration data (16 bits)
#define BMP085_CAL_AC2 0xAC //!< R   Calibration data (16 bits)
#define BMP085_CAL_AC3 0xAE //!< R   Calibration data (16 bits)
#define BMP085_CAL_AC4 0xB0 //!< R   Calibration data (16 bits)
#define BMP085_CAL_AC5 0xB2 //!< R   Calibration data (16 bits)
#define BMP085_CAL_AC6 0xB4 //!< R   Calibration data (16 bits)
#define BMP085_CAL_B1 0xB6  //!< R   Calibration data (16 bits)
#define BMP085_CAL_B2 0xB8  //!< R   Calibration data (16 bits)
#define BMP085_CAL_MB 0xBA  //!< R   Calibration data (16 bits)
#define BMP085_CAL_MC 0xBC  //!< R   Calibration data (16 bits)
#define BMP085_CAL_MD 0xBE  //!< R   Calibration data (16 bits)

// Commands
#define BMP085_CONTROL 0xF4         //!< Control register
#define BMP085_TEMPDATA 0xF6        //!< Temperature data register
#define BMP085_PRESSUREDATA 0xF6    //!< Pressure data register
#define BMP085_READTEMPCMD 0x2E     //!< Read temperature control register value
#define BMP085_READPRESSURECMD 0x34 //!< Read pressure control register value

// Calibration data (will be read from sensor)
int16_t ac1, ac2, ac3, b1, b2, mb, mc, md;
uint16_t ac4, ac5, ac6;
uint8_t oversampling;

uint8_t BMP085_ADDR = 0x77;
i2c_inst_t *bmp_i2c = i2c_default;
uint16_t bmp085_sda = PICO_DEFAULT_I2C_SDA_PIN;
uint16_t bmp085_scl = PICO_DEFAULT_I2C_SCL_PIN;

// pin select function
void BMP085_setI2C(i2c_inst_t *i, uint16_t sda, uint16_t scl)
{
  bmp_i2c = i;
  bmp085_sda = sda;
  bmp085_scl = scl;
}

// I2C handling functions
uint8_t BMP085_read8(uint8_t a)
{
  uint8_t r;
  i2c_write_blocking(bmp_i2c, BMP085_ADDR, &a, 1, true);
  i2c_read_blocking(bmp_i2c, BMP085_ADDR, &r, 1, false);
  return r;
}

uint16_t BMP085_read16(uint8_t a)
{
  uint8_t retbuf[2];
  uint16_t r;
  i2c_write_blocking(bmp_i2c, BMP085_ADDR, &a, 1, true);
  i2c_read_blocking(bmp_i2c, BMP085_ADDR, retbuf, 2, false);
  r = retbuf[1] | (retbuf[0] << 8);
  return r;
}

void BMP085_write8(uint8_t a, uint8_t d)
{
  uint8_t tBuf[2];
  tBuf[0] = a;
  tBuf[1] = d;
  i2c_write_blocking(bmp_i2c, BMP085_ADDR, tBuf, 2, false);
}

uint8_t BMP085_init(uint8_t mode, bool initI2C)
{
  if(initI2C){
    i2c_init(bmp_i2c, 800 * 1000);
    gpio_set_function(bmp085_sda, GPIO_FUNC_I2C);
    gpio_set_function(bmp085_scl, GPIO_FUNC_I2C);
    gpio_pull_up(bmp085_sda);
    gpio_pull_up(bmp085_scl);
  }

  if (mode > BMP085_ULTRAHIGHRES)
    mode = BMP085_ULTRAHIGHRES;
  oversampling = mode;

  if (BMP085_read8(0xD0) != 0x55)
    return 0;

  /* read calibration data */
  ac1 = BMP085_read16(BMP085_CAL_AC1);
  ac2 = BMP085_read16(BMP085_CAL_AC2);
  ac3 = BMP085_read16(BMP085_CAL_AC3);
  ac4 = BMP085_read16(BMP085_CAL_AC4);
  ac5 = BMP085_read16(BMP085_CAL_AC5);
  ac6 = BMP085_read16(BMP085_CAL_AC6);

  b1 = BMP085_read16(BMP085_CAL_B1);
  b2 = BMP085_read16(BMP085_CAL_B2);

  mb = BMP085_read16(BMP085_CAL_MB);
  mc = BMP085_read16(BMP085_CAL_MC);
  md = BMP085_read16(BMP085_CAL_MD);

  return 1;
}

// Sensor read functions
int32_t computeB5(int32_t UT)
{
  int32_t X1 = (UT - (int32_t)ac6) * ((int32_t)ac5) >> 15;
  int32_t X2 = ((int32_t)mc << 11) / (X1 + (int32_t)md);
  return X1 + X2;
}

uint16_t BMP085_readRawTemperature(void)
{
  BMP085_write8(BMP085_CONTROL, BMP085_READTEMPCMD);
  busy_wait_ms(5);
  return BMP085_read16(BMP085_TEMPDATA);
}

uint32_t BMP085_readRawPressure(void)
{
  uint32_t raw;

  BMP085_write8(BMP085_CONTROL, BMP085_READPRESSURECMD + (oversampling << 6));

  if (oversampling == BMP085_ULTRALOWPOWER)
    busy_wait_ms(5);
  else if (oversampling == BMP085_STANDARD)
    busy_wait_ms(8);
  else if (oversampling == BMP085_HIGHRES)
    busy_wait_ms(14);
  else
    busy_wait_ms(26);

  raw = BMP085_read16(BMP085_PRESSUREDATA);

  raw <<= 8;
  raw |= BMP085_read8(BMP085_PRESSUREDATA + 2);
  raw >>= (8 - oversampling);

  return raw;
}

float BMP085_readTemperature(void)
{
  int32_t UT, B5; // following ds convention
  float temp;

  UT = BMP085_readRawTemperature();

  B5 = computeB5(UT);
  temp = (B5 + 8) >> 4;
  temp /= 10;

  return temp;
}

Measurement BMP085_read_all(float altitude){
  int32_t UT, UP, B3, B5, B6, X1, X2, X3, p;
  uint32_t B4, B7;
  float temp;

  UT = BMP085_readRawTemperature();
  UP = BMP085_readRawPressure();

  B5 = computeB5(UT);

  // do pressure calcs
  B6 = B5 - 4000;
  X1 = ((int32_t)b2 * ((B6 * B6) >> 12)) >> 11;
  X2 = ((int32_t)ac2 * B6) >> 11;
  X3 = X1 + X2;
  B3 = ((((int32_t)ac1 * 4 + X3) << oversampling) + 2) / 4;

  X1 = ((int32_t)ac3 * B6) >> 13;
  X2 = ((int32_t)b1 * ((B6 * B6) >> 12)) >> 16;
  X3 = ((X1 + X2) + 2) >> 2;
  B4 = ((uint32_t)ac4 * (uint32_t)(X3 + 32768)) >> 15;
  B7 = ((uint32_t)UP - B3) * (uint32_t)(50000UL >> oversampling);

  if (B7 < 0x80000000)
  {
    p = (B7 * 2) / B4;
  }
  else
  {
    p = (B7 / B4) * 2;
  }
  X1 = (p >> 8) * (p >> 8);
  X1 = (X1 * 3038) >> 16;
  X2 = (-7357 * p) >> 16;

  p = p + ((X1 + X2 + (int32_t)3791) >> 4);

  temp = (B5 + 8) >> 4;
  temp /= 10;

  Measurement m;
  m.pressure_sea_level = seaLevelForAltitude(altitude, p / 100.0);
  m.temperature = temp;

  return m;
}

int32_t BMP085_readPressure(void)
{
  int32_t UT, UP, B3, B5, B6, X1, X2, X3, p;
  uint32_t B4, B7;

  UT = BMP085_readRawTemperature();
  UP = BMP085_readRawPressure();

  B5 = computeB5(UT);

  // do pressure calcs
  B6 = B5 - 4000;
  X1 = ((int32_t)b2 * ((B6 * B6) >> 12)) >> 11;
  X2 = ((int32_t)ac2 * B6) >> 11;
  X3 = X1 + X2;
  B3 = ((((int32_t)ac1 * 4 + X3) << oversampling) + 2) / 4;

  X1 = ((int32_t)ac3 * B6) >> 13;
  X2 = ((int32_t)b1 * ((B6 * B6) >> 12)) >> 16;
  X3 = ((X1 + X2) + 2) >> 2;
  B4 = ((uint32_t)ac4 * (uint32_t)(X3 + 32768)) >> 15;
  B7 = ((uint32_t)UP - B3) * (uint32_t)(50000UL >> oversampling);

  if (B7 < 0x80000000)
  {
    p = (B7 * 2) / B4;
  }
  else
  {
    p = (B7 / B4) * 2;
  }
  X1 = (p >> 8) * (p >> 8);
  X1 = (X1 * 3038) >> 16;
  X2 = (-7357 * p) >> 16;

  p = p + ((X1 + X2 + (int32_t)3791) >> 4);

  return p;
}

float seaLevelForAltitude(float altitude, float atmospheric) {
  return atmospheric / pow(1.0 - (altitude/44330.0), 5.255);
}
