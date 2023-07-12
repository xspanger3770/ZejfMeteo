#include <cmath>
#include <memory>
#include <queue>
#include <stdio.h>
#include <array>

#include <hardware/rtc.h>
#include <hardware/timer.h>
#include <hardware/watchdog.h>
#include <hardware/adc.h>
#include <pico/stdlib.h>

#include "sd_card_handler.h"
#include "time_utils.h"
#include "zejf_outside.h"

int __dso_handle = 0; // some random shit that needs to be here because some smart engineer forgot it somewhere

extern "C" {
#include "htu21.h"
#include "tcp_handler.h"
#include "zejf_api.h"
}

volatile uint32_t millis_since_boot = 0;
static uint32_t millis_overflows = 0;
static bool reboot_by_watchdog = false;

static long htu_errors = 0;
static int htu_reset_countdown = 0;
static int htu_next_reset = 1;

static volatile bool time_set = false; // dont forgor
static volatile bool htu_initialised = false;

static volatile bool queue_lock = false;

static volatile uint16_t rr_c = 0;
static volatile uint16_t rr_c_last = 0;

static uint32_t time_check_count = 0;

static std::queue<std::unique_ptr<zejf_log>> logs_queue;

class htu_log : public zejf_log
{
  public:
    uint16_t log_count = 0;
    double temp_sum = 0;
    double rh_sum = 0;
    double td_sum = 0;

    htu_log(uint32_t sample_rate)
        : zejf_log(sample_rate){};

    void reset() override {
        temp_sum = 0;
        rh_sum = 0;
        log_count = 0;
        td_sum = 0;
    }

    void finish() override {
    }

    void sample(htu_measurement &measurement) {
        temp_sum += measurement.temperature;
        rh_sum += measurement.humidity;
        td_sum += measurement.dewpoint;
        log_count++;
    }

    void log_data(uint32_t millis) const override {
        if (log_count == 0) {
            return;
        }
        data_log(VAR_T2M, hour_num, log_num, temp_sum / log_count, millis, true);
        data_log(VAR_RH, hour_num, log_num, rh_sum / log_count, millis, true);
        data_log(VAR_TD, hour_num, log_num, td_sum / log_count, millis, true);
    }

    std::unique_ptr<zejf_log> clone() const override {
        return std::make_unique<htu_log>(*this);
    }
};

#define RR_COEFF 0.254

class rr_log : public zejf_log
{
  public:
    uint16_t log_count = 0;

    rr_log(uint32_t sample_rate)
        : zejf_log(sample_rate){};

    void reset() override {
        log_count = 0;
    }

    void finish() override {
    }

    void sample(uint16_t counts) {
        log_count += counts;
    }

    void log_data(uint32_t millis) const override {
        data_log(VAR_RR, hour_num, log_num, log_count * RR_COEFF, millis, true);
    }

    std::unique_ptr<zejf_log> clone() const override {
        return std::make_unique<rr_log>(*this);
    }
};

class voltages_log : public zejf_log
{
  public:
    uint16_t log_count = 0;
    double solar_sum = 0;
    double battery_sum = 0;
    double current_sum = 0;

    voltages_log(uint32_t sample_rate)
        : zejf_log(sample_rate){};

    void reset() override {
        log_count = 0;
        solar_sum = 0;
        battery_sum = 0;
        current_sum = 0;
    }

    void finish() override {
    }

    void sample(double u_solar, double u_batt, double i_batt) {
        log_count++;
        solar_sum += u_solar;
        battery_sum += u_batt;
        current_sum += i_batt;
    }

    void log_data(uint32_t millis) const override {
        if(log_count == 0){
            return;
        }
        data_log(VAR_VOLTAGE_SOLAR, hour_num, log_num, solar_sum / log_count, millis, true);
        data_log(VAR_VOLTAGE_BATTERY, hour_num, log_num, battery_sum / log_count, millis, true);
        data_log(VAR_CURRENT_BATTERY, hour_num, log_num, current_sum / log_count, millis, true);
    }

    std::unique_ptr<zejf_log> clone() const override {
        return std::make_unique<voltages_log>(*this);
    }
};

static htu_log current_htu_log(SECONDS_5);
static rr_log current_rr_log(SECONDS_5);
static voltages_log current_voltages_log(EVERY_MINUTE);

static std::array<zejf_log *, 3> all_logs = { &current_htu_log, &current_rr_log, &current_voltages_log };

void get_provided_variables(uint16_t *provide_count, const VariableInfo **provided_variables) {
    *provide_count = 7;
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

static void set_time(char *msg) {
    time_t time = strtol(msg, NULL, 10);

    ZEJF_LOG(0, "Time will be set to %lld\n", time);

    datetime_t dt;
    rtc_time(time, &dt);

    if (rtc_set_datetime(&dt)) {
        time_set = true;
    }

    ZEJF_LOG(0, "Time was set\n");
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

float read_onboard_temperature(const char unit) {
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    adc_select_input(4);
    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    if (unit == 'C') {
        return tempC;
    } else if (unit == 'F') {
        return tempC * 9 / 5 + 32;
    }

    return -1.0f;
}

void network_process_packet(Packet *packet) {
    if (packet->command == TIME_CHECK) {
        set_time(packet->message);
    } else if (packet->command == STATUS_REQUEST) {
        float temperature = read_onboard_temperature('C');
        
        send_msg(packet->from, "  Status of ZejfOutside:\n    time_set=%d\n    htu_initialised=%d", time_set, htu_initialised);
        send_msg(packet->from, "    queue_lock=%d\n    queue_size=%d\n    rr_count=%d", queue_lock, logs_queue.size(), rr_c);
        send_msg(packet->from, "    millis_since_boot=%d\n    millis_overflows=%d", millis_since_boot, millis_overflows);
        send_msg(packet->from, "    rebooted_by_watchdog=%d\n    htu_errors=%d", reboot_by_watchdog, htu_errors);
        send_msg(packet->from, "    onboard_temperature=%.1f˚C\n", temperature);
        send_sd_card_msgs(packet->from);
        send_tcp_msgs(packet->from);
    } else {
        ZEJF_LOG(0, "Weird packet: %d\n", packet->command);
    }
}

const double conversion_factor = 3.3f / (1 << 12);
const double voltage_divider = 47.0 * (1.0 / 6.8 + 1.0 / 47.0);
const double current_sensing_resistor = 0.22;

const double correction_battery = 4.933/5.0068;
const double correction_sensing = 4.933/4.9598;

static void measure_voltages(){
    adc_select_input(2); // 28 ... solar pin
    double voltage_solar = adc_read() * conversion_factor * voltage_divider;
    adc_select_input(1); // 27 ... battery pin
    double voltage_battery = adc_read() * conversion_factor * voltage_divider * correction_battery;
    adc_select_input(0); // 26 ... voltage_divider  
    double voltage_sensing = adc_read() * conversion_factor * voltage_divider * correction_sensing;
    double battery_current = ((voltage_sensing - voltage_battery) / current_sensing_resistor) * 1000.0; // mA
    current_voltages_log.sample(voltage_solar, voltage_battery, battery_current);
    //printf("VOLTAGES %f %f %f\n", voltage_solar, voltage_battery, voltage_sensing);
}

static bool process_measurements(struct repeating_timer *) {
    uint16_t rr_c_temp = rr_c;
    uint16_t _rr_c = rr_c_temp - rr_c_last;
    rr_c_last = rr_c_temp;

    datetime_t dt;
    rtc_get_datetime(&dt);
    uint32_t unixtime = unix_time(dt);
    uint32_t hour_num = get_hour_num(unixtime);

    uint32_t last_millis = millis_since_boot;
    millis_since_boot = to_ms_since_boot(get_absolute_time());
    if (millis_since_boot < last_millis) {
        millis_overflows++;
    }

    if (!time_set) {
        return true;
    }

    measure_voltages();

    current_rr_log.sample(_rr_c);

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

static bool htu_measure(struct repeating_timer *) {
    ZEJF_LOG(0, "HTU = %d %d/%d\n", htu_initialised, htu_reset_countdown, htu_next_reset);

    if(!htu_initialised){
        if (htu_reset_countdown < htu_next_reset) {
            htu_reset_countdown++;
            return true;
        }

        htu_reset_countdown = 0;
        if (htu_next_reset < 20) {
            htu_next_reset *= 2;
        }

        htu_initialised = htu21_reset();

        if(!htu_initialised){
            return true;
        }

        htu_next_reset = 1;
    }

    htu_measurement m = htu21_measure();

    if(!m.valid) {
        htu_errors++;
        htu_initialised = 0;
    }

    if (!time_set) {
        return true;
    }

    current_htu_log.sample(m);

    return true;
}

static void rr_callback(uint gpio, uint32_t) {
    if (gpio == RR_PIN) {
        rr_c++;
    }
}

void time_check() {
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

static void init_all() {
    datetime_t t = {
        .year = 2014,
        .month = 05,
        .day = 28,
        .dotw = 5,
        .hour = 15,
        .min = 45,
        .sec = 50
    };

    rtc_init();
    rtc_set_datetime(&t);

    sleep_us(64);

    millis_since_boot = to_ms_since_boot(get_absolute_time());

    gpio_init(RR_PIN);
    gpio_set_dir(RR_PIN, GPIO_IN);
    gpio_disable_pulls(RR_PIN);
    gpio_set_irq_enabled_with_callback(RR_PIN, GPIO_IRQ_EDGE_RISE, true, &rr_callback);

    adc_init();
    adc_gpio_init(ADC_SOLAR_PIN);
    adc_gpio_init(ADC_BATT0_PIN);
    adc_gpio_init(ADC_BATT1_PIN);

    adc_set_temp_sensor_enabled(true);

    htu_initialised = htu21_init(I2C_INST, I2C_SDA, I2C_SCK, I2C_SPEED);
    printf("htu_initialised = %d\n", htu_initialised);

    // DONT FORGET TO SET PICO_MAX_SHARED_IRQ_HANDLERS in irq.h to at least 5! otherwise this will crash and it took me 8 hours to figure out
    zejf_tcp_init();

    wifi_connect();


    if(zejf_init() != ZEJF_OK){
        panic("ZEJF INIT FAILED!\n");   
    }

    init_card(SD_SPI, SD_MISO, SD_MOSI, SD_SCK, SD_CS, SD_SPI_SPEED);
}

int main() {
    stdio_init_all();

    sleep_ms(2000);

    init_all();

    struct repeating_timer timer_measure;
    struct repeating_timer timer_bmp;

    add_repeating_timer_ms(-1000, process_measurements, NULL, &timer_measure);
    add_repeating_timer_ms(-1000, htu_measure, NULL, &timer_bmp);

    uint32_t last_rip = -1;
    uint32_t last_provide = -1;
    uint32_t last_save = -1;

    watchdog_enable(7000, 1);
    reboot_by_watchdog = watchdog_enable_caused_reboot();

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