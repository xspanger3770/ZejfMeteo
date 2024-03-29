#include <stdio.h>

#include <hardware/rtc.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <hardware/adc.h>

#include "sd_card_handler.h"

#include "time_utils.h"
#include "zejf_outside.h"
#include "time_engine.h"
#include "zejf_measure.h"
#include "zejf_runtime.h"

int __dso_handle = 0; // some random shit that needs to be here because some smart engineer forgot it somewhere

extern "C" {
    #include "zejf_api.h"
    #include "htu21.h"
    #include "tcp_handler.h"
}

static void init_all() {
    htu_initialised = htu21_init(I2C_INST, I2C_SDA, I2C_SCK, I2C_SPEED);
    ZEJF_LOG(1, "htu_initialised = %d\n", htu_initialised);

    time_engine_init(I2C_INST);

    gpio_init(RR_PIN);
    gpio_set_dir(RR_PIN, GPIO_IN);
    gpio_disable_pulls(RR_PIN);
    gpio_set_irq_enabled_with_callback(RR_PIN, GPIO_IRQ_EDGE_RISE, true, &rr_callback);

    adc_init();
    adc_gpio_init(ADC_SOLAR_PIN);
    adc_gpio_init(ADC_BATT0_PIN);
    adc_gpio_init(ADC_BATT1_PIN);

    adc_set_temp_sensor_enabled(true);

    // DONT FORGET TO SET PICO_MAX_SHARED_IRQ_HANDLERS in irq.h to at least 5! otherwise this will crash and it took me 8 hours to figure out
    zejf_tcp_init();

    wifi_connect();

    if (zejf_init() != ZEJF_OK) {
        panic("ZEJF INIT FAILED!\n");
    }

    init_card(SD_SPI, SD_MISO, SD_MOSI, SD_SCK, SD_CS, SD_SPI_SPEED);
}

int main() {
    stdio_init_all();

    sleep_ms(2000);

    init_all();

    struct repeating_timer timer_htu;
    add_repeating_timer_ms(-1000, htu_measure, NULL, &timer_htu);


    zejf_pico_run();
}