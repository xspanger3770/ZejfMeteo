#include <cstdio>
#include <cstring>

#include "zejf_measure.h"
#include "time_utils.h"
#include "sd_card_handler.h"
#include "time_engine.h"
#include "zejf_runtime.h"
#include "zejf_pico_data.h"

extern "C" {
    #include "zejf_api.h"
    #include "tcp_handler.h"
}

const VariableInfo my_provided_variables[] = { VAR_T2M, VAR_RH, VAR_TD, VAR_RR, VAR_VOLTAGE_SOLAR, VAR_VOLTAGE_BATTERY, VAR_CURRENT_BATTERY, VAR_RAIN_RATE_PEAK };

Interface tcp_client_1 = {
    .uid = 2,
    .type = TCP,
    .handle = 0,
    .rx_count = 0,
    .tx_count = 0
};

Interface *all_interfaces[] = { &tcp_client_1 };


void get_provided_variables(uint16_t *provide_count, const VariableInfo **provided_variables) {
    *provide_count = 8;
    *provided_variables = my_provided_variables;
}

void get_demanded_variables(uint16_t *demand_count, uint16_t **demanded_variables) {
    *demand_count = 0;
    *demanded_variables = NULL;
}

void get_all_interfaces(Interface ***interfaces, size_t *length) {
    *interfaces = all_interfaces;
    *length = 1;
}

zejf_err network_send_via(char *msg, int, Interface *interface, TIME_TYPE) {
    switch (interface->type) {
    case TCP: {
        char msg2[PACKET_MAX_LENGTH];
        snprintf(msg2, PACKET_MAX_LENGTH, "%s\n", msg);
        return zejf_tcp_send(msg2, strlen(msg2));
    }
    default:
        ZEJF_LOG(1, "Unknown interaface: %d\n", interface->type);
        return ZEJF_ERR_SEND_UNABLE;
    }
}

template <typename... Args>
void send_msg(uint16_t to, const char *str, Args... args) {
    char status_msg[96];
    snprintf(status_msg, sizeof(status_msg), str, args...);
    Packet *packet2 = network_prepare_packet(to, MESSAGE, status_msg);
    network_send_packet(packet2, millis_since_boot);
}

void send_sd_card_msgs(uint16_t to) {
    send_msg(to, "  SD Card stats:\n    fatal_errors=%d\n    resets=%d", sd_stats.fatal_errors, sd_stats.fatal_errors);
    send_msg(to, "    successful_reads=%d\n    successful_writes=%d\n    working=%d", sd_stats.successful_reads, sd_stats.successful_writes, sd_stats.working);
}

void send_tcp_msgs(uint16_t to) {
    send_msg(to, "  TCP stats:\n    tcp_reconnects=%d\n    wifi_reconnects=%d", tcp_stats.tcp_reconnects, tcp_stats.wifi_reconnects);
}

void network_process_packet(Packet *packet) {
    if (packet->command == TIME_CHECK) {
        receive_time(packet->message);
    } else if (packet->command == STATUS_REQUEST) {
        float temperature = read_onboard_temperature('C');

        send_msg(packet->from, "  Status of ZejfOutside:\n    time_set=%d\n    htu_initialised=%d", time_set, htu_initialised);
        send_msg(packet->from, "    queue_lock=%d\n    queue_size=%d\n    rr_count=%d", queue_lock, logs_queue.size(), rr_c);
        send_msg(packet->from, "    millis_since_boot=%d\n    millis_overflows=%d", millis_since_boot, millis_overflows);
        send_msg(packet->from, "    rebooted_by_watchdog=%d\n    htu_errors=%d", rebooted_by_watchdog, htu_errors);
        send_msg(packet->from, "    onboard_temperature=%.1f˚C\n    ds3231_working=%d\n", temperature, ds3231_working);
        send_sd_card_msgs(packet->from);
        send_tcp_msgs(packet->from);
    } else {
        ZEJF_LOG(0, "Weird packet: %d\n", packet->command);
    }
}