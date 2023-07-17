#ifndef ZEJF_RUNTIME_H
#define ZEJF_RUNTIME_H

extern "C"{
    #include "tcp_handler.h"
    #include "zejf_api.h"
}

extern bool rebooted_by_watchdog;

void zejf_pico_run(void);
float read_onboard_temperature(const char unit);

extern bool process_measurements(struct repeating_timer *);

#endif