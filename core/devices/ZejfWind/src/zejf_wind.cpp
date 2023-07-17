#include <array>
#include <memory>
#include <queue>
#include <stdio.h>

int __dso_handle = 0; // some random shit that needs to be here because some smart engineer forgot it somewhere

#include <hardware/adc.h>
#include <hardware/rtc.h>
#include <pico/stdlib.h>

#include "sd_card_handler.h"
#include "time_engine.h"
#include "time_utils.h"
#include "zejf_measure.h"
#include "zejf_runtime.h"
#include "zejf_wind.hpp"

extern "C" {
#include "bmp085.h"
#include "tcp_handler.h"
#include "zejf_api.h"
}

static void init_all()
{
    BMP085_setI2C(I2C_INST, I2C_SDA, I2C_SCK);
    bmp_initialised = BMP085_init(BMP085_ULTRAHIGHRES, true) == 1;

    time_engine_init();

    gpio_init(WSPD_PIN);
    gpio_set_dir(WSPD_PIN, GPIO_IN);
    gpio_disable_pulls(WSPD_PIN);
    gpio_set_irq_enabled_with_callback(WSPD_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &wspd_callback);

    adc_init();
    adc_gpio_init(WDIR_PIN);
    adc_select_input(0);

    adc_set_temp_sensor_enabled(true);

    // DONT FORGET TO SET PICO_MAX_SHARED_IRQ_HANDLERS in irq.h to at least 5! otherwise this will crash and it took me 8 hours to figure out
    zejf_tcp_init();

    wifi_connect();

    if (zejf_init() != ZEJF_OK) {
        panic("ZEJF INIT FAILED!\n");
    }

    init_card(SD_SPI, SD_MISO, SD_MOSI, SD_SCK, SD_CS, SD_SPI_SPEED);
}

int main()
{
    stdio_init_all();

    sleep_ms(2000);

    init_all();

    struct repeating_timer timer_bmp;
    add_repeating_timer_ms(-1000 * 5, bmp_measure, NULL, &timer_bmp);

    zejf_pico_run();
}