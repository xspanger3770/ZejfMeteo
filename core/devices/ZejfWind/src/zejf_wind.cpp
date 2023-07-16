#include <memory>
#include <queue>
#include <array>
#include <stdio.h>
#include <cmath>
#include <inttypes.h>

int __dso_handle = 0; // some random shit that needs to be here because some smart engineer forgot it somewhere

#include <hardware/adc.h>
#include <hardware/rtc.h>
#include <hardware/timer.h>
#include <hardware/watchdog.h>
#include <pico/stdlib.h>

#include "sd_card_handler.h"
#include "time_utils.h"

#include "zejf_wind.hpp"

extern "C" {
#include "bmp085.h"
#include "tcp_handler.h"
#include "zejf_api.h"
}

volatile uint32_t millis_since_boot = 0;
static uint32_t millis_overflows = 0;

static bool reboot_by_watchdog = false;

static volatile bool time_set = false; // dont forgor
static volatile bool bmp_initialised = false;

static volatile bool queue_lock = false;

static volatile uint16_t wspd_c = 0;
static volatile uint16_t wspd_c_last = 0;

static uint64_t last_wspd_callback = 0;
static bool gust_measured = false;
static volatile double latest_max_gust = 0.0;

static uint32_t time_check_count = 0;

static std::queue<std::unique_ptr<zejf_log>> logs_queue;

class bmp_log : public zejf_log
{
  public:
    double sum_press = 0;
    double sum_temp = 0;
    uint16_t log_count = 0;

    bmp_log(uint32_t sample_rate)
        : zejf_log(sample_rate){};

    void sample(double press, double temp)
    {
        sum_press += press;
        sum_temp += temp;
        log_count++;
    }

    void reset() override
    {
        sum_press = 0;
        sum_temp = 0;
        log_count = 0;
    }

    void finish() override
    {
    }

    void log_data(uint32_t millis) const override
    {
        if (log_count == 0) {
            return;
        }
        data_log(VAR_PRESS, hour_num, log_num, get_press(), millis, true);
        data_log(VAR_TEMP, hour_num, log_num, get_temp(), millis, true);
    }

    double get_press() const
    {
        return log_count == 0 ? 0 : sum_press / log_count;
    }

    double get_temp() const
    {
        return log_count == 0 ? 0 : sum_temp / log_count;
    }

    std::unique_ptr<zejf_log> clone() const override
    {
        return std::make_unique<bmp_log>(*this);
    }
};

class wind_log : public zejf_log
{
    VariableInfo wspd_var;
    VariableInfo wgust_var;
    VariableInfo wdir_var;

  public:
    double dx = 0.0;
    double dy = 0.0;
    double wgust = 0;
    double wdir = 0;
    double actual_wdir = 0;
    double wspd = 0;
    unsigned int log_count = 0;
    unsigned int running_index = 0;

    unsigned int average_count;
    unsigned int averages_sampled = 0;

    std::vector<double> wspd_running;
    std::vector<double> wdir_running;

    wind_log(VariableInfo wspd_var, VariableInfo wgust_var, VariableInfo wdir_var, uint32_t sample_rate, unsigned int average_count)
        : zejf_log(sample_rate), wspd_var{ wspd_var }, wgust_var{ wgust_var }, wdir_var{ wdir_var } , average_count{average_count} {
            wspd_running.resize(average_count);
            wdir_running.resize(average_count);
        };

    void reset() override
    {
        dx = 0.0;
        dy = 0.0;
        wgust = 0;
        log_count = 0;
    }

    void finish() override
    {
        wspd_running[running_index] = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2)) * (2.4 / log_count);
        wdir_running[running_index] = (dx == 0 && dy == 0) ? actual_wdir : (std::atan2(-dx, -dy) + M_PI) * 180.0 / M_PI;

        running_index++;
        if(running_index == average_count){
            running_index = 0;
        }

        if(averages_sampled < average_count){
            averages_sampled++;
        }
        
        double my_dx = 0;
        double my_dy = 0;
        unsigned int index = running_index;
        unsigned int start = running_index;

        do{
            my_dy += wspd_running[index] * std::cos((wdir_running[index] / 180.0) * M_PI);
            my_dx += wspd_running[index] * std::sin((wdir_running[index] / 180.0) * M_PI);
            index = (index + 1) % average_count;
        } while(index != start);
        
        
        wspd = std::sqrt(std::pow(my_dx, 2) + std::pow(my_dy, 2)) / (average_count);
        wdir = (my_dx == 0 && my_dy == 0) ? actual_wdir : (std::atan2(-dx, -dy) + M_PI) * 180.0 / M_PI;
    }

    void sample(double _wdir, uint16_t counts, double gust)
    {
        dy += counts * std::cos((_wdir / 180.0) * M_PI);
        dx += counts * std::sin((_wdir / 180.0) * M_PI);

        //double wgust_now = 2.4 * counts;
        if (gust > wgust) {
            wgust = gust;
        }

        actual_wdir = _wdir;
        log_count++;
    }

    void log_data(uint32_t millis) const override
    {
        ZEJF_LOG(0, "logging with millis %ld\n", millis);
        data_log(wgust_var, hour_num, log_num, wgust, millis, true);
        if (averages_sampled < average_count) {
            return;
        }
        data_log(wspd_var, hour_num, log_num, wspd, millis, true);
        data_log(wdir_var, hour_num, log_num, wdir, millis, true);
    }

    std::unique_ptr<zejf_log> clone() const override
    {
        return std::make_unique<wind_log>(*this);
    }
};

static wind_log current_short_log(VAR_WSPD_NOW, VAR_WGUST_NOW, VAR_WDIR_NOW, SECONDS_5, 12);
static wind_log current_long_log(VAR_WSPD_AVG, VAR_WGUST_AVG, VAR_WDIR_AVG, EVERY_MINUTE, 10);
static bmp_log current_bmp_log(EVERY_MINUTE);

static std::array<zejf_log*, 3> all_logs = { &current_short_log, &current_long_log, &current_bmp_log };

void get_provided_variables(uint16_t *provide_count, const VariableInfo **provided_variables)
{
    *provide_count = 8;
    *provided_variables = my_provided_variables;
}

void get_demanded_variables(uint16_t *demand_count, uint16_t **demanded_variables)
{
    *demand_count = 0;
    *demanded_variables = NULL;
}

void get_all_interfaces(Interface ***interfaces, size_t *length)
{
    *interfaces = all_interfaces;
    *length = 1;
}

zejf_err network_send_via(char *msg, int, Interface *interface, TIME_TYPE)
{
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

static void set_time(char *msg)
{
    time_t time = strtol(msg, NULL, 10);

    ZEJF_LOG(1, "Time will be set to %lld\n", time);

    datetime_t dt;
    rtc_time(time, &dt);

    if (rtc_set_datetime(&dt)) {
        time_set = true;
    }

    ZEJF_LOG(1, "time was set\n");
}

template <typename... Args>
void send_msg(uint16_t to, const char *str, Args... args) {
    char status_msg[96];
    snprintf(status_msg, sizeof(status_msg), str, args...);
    Packet *packet2 = network_prepare_packet(to, MESSAGE, status_msg);
    network_send_packet(packet2, millis_since_boot);
}

void send_sd_card_msgs(uint16_t to){
    send_msg(to, "  SD Card stats:\n    fatal_errors=%d\n    resets=%d", sd_stats.fatal_errors, sd_stats.fatal_errors);
    send_msg(to, "    successful_reads=%d\n    successful_writes=%d\n    working=%d", sd_stats.successful_reads, sd_stats.successful_writes, sd_stats.working);
}


void send_tcp_msgs(uint16_t to){
    send_msg(to, "  TCP stats:\n    tcp_reconnects=%d\n    wifi_reconnects=%d", tcp_stats.tcp_reconnects, tcp_stats.wifi_reconnects);
}

void network_process_packet(Packet *packet) {
    if (packet->command == TIME_CHECK) {
        set_time(packet->message);
    } else if (packet->command == STATUS_REQUEST) {
        send_msg(packet->from, "  Status of ZejfWind:\n    time_set=%d\n    bmp_initialised=%d", time_set, bmp_initialised);
        send_msg(packet->from, "    queue_lock=%d\n    queue_size=%d\n    wspd_count=%d", queue_lock, logs_queue.size(), wspd_c);
        send_msg(packet->from, "    millis_since_boot=%d\n    millis_overflows=%d", millis_since_boot, millis_overflows);
        send_msg(packet->from, "    rebooted_by_watchdog=%d", reboot_by_watchdog);
        send_sd_card_msgs(packet->from);
        send_tcp_msgs(packet->from);
    } else {
        ZEJF_LOG(0, "Weird packet: %d\n", packet->command);
    }
}

#define WIND_CORRECTION 20
static const uint16_t wdir_table[16] = { 3130, 1630, 1850, 353, 390, 282, 748, 512, 1163, 990, 2520, 2396, 3760, 3300, 3539, 2805 };

static double get_wdir(uint16_t adc_val)
{
    uint16_t lowest_error = std::abs(adc_val - wdir_table[0]);
    int result = 0;
    for (int i = 1; i < 16; i++) {
        uint16_t err = std::abs(adc_val - wdir_table[i]);
        if (err < lowest_error) {
            lowest_error = err;
            result = i;
        }
    }

    return result * 22.5;
}

static bool process_measurements(struct repeating_timer *)
{
    uint16_t wspd_c_temp = wspd_c;
    uint16_t _wspd_c = wspd_c_temp - wspd_c_last;
    wspd_c_last = wspd_c_temp;

    double gust = latest_max_gust;
    latest_max_gust = 0;

    datetime_t dt;
    rtc_get_datetime(&dt);
    uint32_t unixtime = unix_time(dt);
    uint32_t hour_num = get_hour_num(unixtime);

    uint32_t last_millis = millis_since_boot;
    millis_since_boot = to_ms_since_boot(get_absolute_time());
    if (millis_since_boot < last_millis) {
        millis_overflows++;
        printf( "Warn: millis overflow from %" SCNu32 " to %" SCNu32 "\n" , last_millis, millis_since_boot);
    }

    if (!time_set) {
        return true;
    }

    uint16_t adc_val = adc_read();
    double wdir = (get_wdir(adc_val) + WIND_CORRECTION);
    if (wdir > 360) {
        wdir -= 360;
    }

    current_long_log.sample(wdir, _wspd_c, gust);
    current_short_log.sample(wdir, _wspd_c, gust);

    if (queue_lock) {
        return true;
    }

    for (auto log : all_logs) {
        if (log->is_ready(unixtime)) {
            log->finish();
            log->hour_num = hour_num;

            logs_queue.push(log->clone());
            log->reset();
        }
    }

    return true;
}

static bool bmp_measure(struct repeating_timer *)
{
    if (!time_set || !bmp_initialised) {
        return true;
    }

    Measurement m = BMP085_read_all(ALTITUDE);
    current_bmp_log.sample(m.pressure_sea_level, m.temperature);

    return true;
}

// ONE INTERRUPT PER SECONDS MEANS 1.2km/h WIND
static void wspd_callback(uint gpio, uint32_t events)
{
    if (gpio == WSPD_PIN) {
        wspd_c++;

        if(events == GPIO_IRQ_EDGE_RISE){
            uint64_t current_us = to_us_since_boot(get_absolute_time());
            if(gust_measured && time_set){
                double gust = 2.4 * (1000000.0 / (current_us - last_wspd_callback));
                if((gust >= 0 && gust < 200.0) && gust > latest_max_gust){
                    latest_max_gust = gust;
                }
            }
            gust_measured = true;
            last_wspd_callback = current_us;
        }
    }
}

void time_check()
{
    if (time_set) {
        time_check_count++;
        if (time_check_count < 12 * 30) {
            return;
        }
        time_check_count = 0;
    }

    Packet *packet = network_prepare_packet(BROADCAST, TIME_REQUEST, NULL);
    if (packet == NULL) {
        return;
    }

    if (network_send_packet(packet, millis_since_boot) != ZEJF_OK) {
        return;
    }
}

static void init_all()
{
    datetime_t t = {
        .year = 2019,
        .month = 06,
        .day = 05,
        .dotw = 5,
        .hour = 15,
        .min = 45,
        .sec = 50
    };

    rtc_init();
    rtc_set_datetime(&t);

    sleep_us(64);

    millis_since_boot = to_ms_since_boot(get_absolute_time());

    gpio_init(WSPD_PIN);
    gpio_set_dir(WSPD_PIN, GPIO_IN);
    gpio_disable_pulls(WSPD_PIN);
    gpio_set_irq_enabled_with_callback(WSPD_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &wspd_callback);

    BMP085_setI2C(I2C_INST, I2C_SDA, I2C_SCK);
    bmp_initialised = BMP085_init(BMP085_ULTRAHIGHRES, true) == 1;

    adc_init();
    adc_gpio_init(WDIR_PIN);
    adc_select_input(0);

    // DONT FORGET TO SET PICO_MAX_SHARED_IRQ_HANDLERS in irq.h to at least 5! otherwise this will crash and it took me 8 hours to figure out
    zejf_tcp_init();

    wifi_connect();

    
    if(zejf_init() != ZEJF_OK){
        panic("ZEJF INIT FAILED!\n");   
    }
    

    init_card(SD_SPI, SD_MISO, SD_MOSI, SD_SCK, SD_CS, SD_SPI_SPEED);
}

int main()
{
    stdio_init_all();

    sleep_ms(2000);

    init_all();

    struct repeating_timer timer_measure;
    struct repeating_timer timer_bmp;

    add_repeating_timer_ms(-500, process_measurements, NULL, &timer_measure);
    add_repeating_timer_ms(-1000 * 5, bmp_measure, NULL, &timer_bmp);

    uint32_t last_rip = -1;
    uint32_t last_provide = -1;
    uint32_t last_save = -1;

    reboot_by_watchdog = watchdog_enable_caused_reboot();

    watchdog_enable(7000, 1);

    // dont put time getting functions here they break for some reason
    while (true) {
        watchdog_update();

        cyw43_arch_poll();

        data_requests_process(millis_since_boot);
        network_process_packets(millis_since_boot);
        routing_table_check(millis_since_boot);

        queue_lock = true;

        // LONG (60 seconds)
        while (!logs_queue.empty()) {
            std::unique_ptr<zejf_log> &log = logs_queue.front();
            log->log_data(millis_since_boot);
            cyw43_arch_poll();
            logs_queue.pop();
        }

        queue_lock = false;

        uint32_t rip_n = (millis_since_boot / (5 * 1000));
        uint32_t provide_n = (millis_since_boot / (20 * 1000));
        uint32_t save_n = (millis_since_boot / (120 * 1000));

        if (rip_n != last_rip) {
            network_send_routing_info(millis_since_boot);
            zejf_tcp_check_connect();
            watchdog_update();
            time_check();
            last_rip = rip_n;
        }

        if (provide_n != last_provide) {
            network_send_provide_info(millis_since_boot);
            last_provide = provide_n;
        }

        if (save_n != last_save) {
            data_save();
            last_save = save_n;
        }
    }

    return 0;
}