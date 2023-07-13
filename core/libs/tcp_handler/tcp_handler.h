#ifndef _TCP_HANDLER_T
#define _TCP_HANDLER_T

#include "zejf_defs.h"
#include <pico/cyw43_arch.h>

#define COUNTRY CYW43_COUNTRY_CZECH_REPUBLIC

#define BUF_SIZE 2048
#define POLL_TIME_S 5

struct tcp_stats_t {
    unsigned long wifi_reconnects;
    unsigned long tcp_reconnects;
};

extern struct tcp_stats_t tcp_stats;

extern volatile uint32_t millis_since_boot; // needs to be declared somewhere in source

int zejf_tcp_init(void);
int wifi_connect(void);
void zejf_tcp_check_connect(void);
zejf_err zejf_tcp_send(char *msg, u16_t length);

#endif