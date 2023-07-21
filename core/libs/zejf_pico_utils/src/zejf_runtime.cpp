#include <hardware/adc.h>
#include <hardware/rtc.h>
#include <hardware/watchdog.h>
#include <pico/stdlib.h>

#include "zejf_runtime.h"
#include "zejf_pico_data.h"
#include "time_engine.h"

bool rebooted_by_watchdog;

float read_onboard_temperature(const char unit) {
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    adc_select_input(4);
    float adc = (float) adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    if (unit == 'C') {
        return tempC;
    } else if (unit == 'F') {
        return tempC * 9 / 5 + 32;
    }

    return -1.0f;
}

bool _process_measurements(repeating_timer_t* t){
    update_millis();
    return process_measurements(t);
}

void zejf_pico_run(void) {
    struct repeating_timer timer_measure;
    
    add_repeating_timer_ms(-1000, _process_measurements, NULL, &timer_measure);
    
    uint32_t last_rip = -1;
    uint32_t last_provide = -1;
    uint32_t last_save = -1;

    watchdog_enable(7000, 1);
    rebooted_by_watchdog = watchdog_enable_caused_reboot();

    while (true) {
        cyw43_arch_poll();

        watchdog_update();

        queue_lock = true;

        // LONG (60 seconds)
        while (!logs_queue.empty()) {
            std::unique_ptr<zejf_log> &log = logs_queue.front();
            log->log_data(millis_since_boot);
            logs_queue.pop();
        }

        queue_lock = false;
        
        watchdog_update();

        uint32_t rip_n = (millis_since_boot / (5 * 1000));
        uint32_t provide_n = (millis_since_boot / (20 * 1000));
        uint32_t save_n = (millis_since_boot / (120 * 1000));

        if (rip_n != last_rip) {
            network_send_routing_info(millis_since_boot);
            zejf_tcp_check_connect();
            time_check();
            watchdog_update();
            last_rip = rip_n;
        }

        if (provide_n != last_provide) {
            network_send_provide_info(millis_since_boot);
            last_provide = provide_n;
        }

        if (save_n != last_save) {
            data_save();
            watchdog_update();
            last_save = save_n;
        }

        data_requests_process(millis_since_boot);
        routing_table_check(millis_since_boot);

        unsigned int counter = 0;
        while(network_has_packets() && counter < 10000){
            network_process_packets(millis_since_boot);
            watchdog_update();
            counter++;
        }
    }
}