#include <cstdlib>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "hardware/rtc.h"
#include "hardware/timer.h"
#include "pico/util/datetime.h"

#include "sd_card_handler.h"
#include "time_utils.h"
#include "zejf_press.h"

int __dso_handle = 0; // some random shit that needs to be here because some smart engineer forgot it somewhere

#define EVERY_5_SECONDS (60 * 12)
#define EVERY_MINUTE (60)

#define USB_DETECT_PIN 24
#define CHARGING_PIN 22

extern "C" {
#include "bmp390.h"
#include "zejf_api.h"
}

volatile double sum_press = 0;
volatile double sum_temp = 0;
volatile unsigned int count_press = 0;
volatile unsigned int count_temp = 0;

volatile bool save_all_flag = false;
volatile bool time_set = false;

volatile uint32_t millis_since_boot = 0;

static uint32_t time_check_count = 0;

const VariableInfo PRESSURE{ .samples_per_hour = EVERY_5_SECONDS, .id = 0x01 };

const VariableInfo TEMPERATURE{ .samples_per_hour = EVERY_MINUTE, .id = 0x02 };

const VariableInfo my_provided_variables[]{ PRESSURE, TEMPERATURE };

void get_provided_variables(uint16_t *provide_count,
        const VariableInfo **provided_variables) {
    *provide_count = 2;
    *provided_variables = my_provided_variables;
}

void get_demanded_variables(uint16_t *demand_count,
        uint16_t **demanded_variables) {
    *demand_count = 0;
    *demanded_variables = NULL;
}

Interface usb_serial_1 = { .uid = 1, .type = USB, .handle = 0, .rx_count = 0, .tx_count = 0 };

Interface *all_interfaces[] = { &usb_serial_1 };

static i2c_t my_i2c;
static bmp_t bmp;
static bool bmp_initialised = false;

void get_all_interfaces(Interface ***interfaces, size_t *length) {
    *interfaces = all_interfaces;
    *length = 1;
}

void gpio_callback(uint, uint32_t) {
    bool g = gpio_get(USB_DETECT_PIN);
    gpio_put(CHARGING_PIN, g);
}

void init_all() {
    stdio_init_all();

    sleep_ms(1000);

    datetime_t t = { .year = 2019,
        .month = 06,
        .day = 05,
        .dotw = 5,
        .hour = 15,
        .min = 45,
        .sec = 50 };

    rtc_init();
    rtc_set_datetime(&t);

    sleep_us(64);

    my_i2c = { .addr = 0x77,
        .rate = I2C0_SPEED,
        .scl = I2C0_SCK,
        .sda = I2C0_SDA,
        .inst = i2c0 };

    bmp = {};
    bmp.i2c = my_i2c;
    bmp.oss = 5;

    bmp_initialised = true;

    if (!bmp_init(&bmp)) {
        bmp_initialised = false;
    }

    // BMP085_setI2C(i2c0, I2C0_SDA, I2C0_SCK);
    // BMP085_init(BMP085_ULTRAHIGHRES, true);

    gpio_init(USB_DETECT_PIN);
    gpio_set_dir(USB_DETECT_PIN, GPIO_IN);
    gpio_init(CHARGING_PIN);
    gpio_set_dir(CHARGING_PIN, GPIO_OUT);
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    printf("init card\n");

    init_card(SD_SPI, SD_MISO, SD_MOSI, SD_SCK, SD_CS, SD_SPI_SPEED);

    gpio_set_irq_enabled_with_callback(USB_DETECT_PIN,
            GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
            true,
            &gpio_callback);
}

#define MAX_ARG 4

void set_time(char *msg) {
    time_t time = strtol(msg, NULL, 10);

    ZEJF_LOG(1, "Time will be set to %lld\n", time);

    datetime_t dt;
    rtc_time(time, &dt);

    if (rtc_set_datetime(&dt)) {
        time_set = true;
    }
    ZEJF_LOG(1, "Time was set\n");
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

void network_process_packet(Packet *packet) {
    if (packet->command == TIME_CHECK) {
        set_time(packet->message);
    } else if (packet->command == STATUS_REQUEST) {
        send_msg(packet->from, "  Status of ZejfPress:\n    time_set=%d\n", time_set);
        send_sd_card_msgs(packet->from);
    } else {
        ZEJF_LOG(0, "Weird packet: %d\n", packet->command);
    }
}

zejf_err network_send_via(char *msg, int, Interface *interface, TIME_TYPE) {
    switch (interface->type) {
    case USB:
        printf("%s\n", msg);
        return ZEJF_OK;
    default:
        ZEJF_LOG(1, "Unknown interaface: %d\n", interface->type);
        return ZEJF_ERR_SEND_UNABLE;
    }
}

void check_input(char *buff, int size, int *pos, TIME_TYPE time) {
    int ch;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (ch == '\n') {
            buff[*pos] = '\0';
            if (network_accept(buff, *pos, &usb_serial_1, time) != ZEJF_OK) {
                ZEJF_LOG(2, "Unparseable input [%s]\n", buff);
            }
            *pos = 0;
            break;
        }
        buff[*pos] = (char) ch;
        *pos = (*pos + 1) % (size);
    }
}

bool bmp_measure(struct repeating_timer *) {
    if (!time_set || !bmp_initialised) {
        return true;
    }

    // Measurement m = BMP085_read_all(ALTITUDE);
    // sum_press+=m.pressure_sea_level;
    // sum_temp+=m.temperature;
    if (!bmp_get_pressure_temperature(&bmp)) {
        return true;
    }
    sum_press += bmp_get_sea_level_pressure(ALTITUDE, bmp.pressure);
    sum_temp += bmp.temperature;
    count_press++;
    count_temp++;

    return true;
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

bool save_all(struct repeating_timer *) {
    network_send_provide_info(millis_since_boot);
    save_all_flag = true;
    return true;
}

bool send_rip(struct repeating_timer *) {
    network_send_routing_info(millis_since_boot);
    time_check();

    return true;
}

int main() {
    init_all();

    sleep_ms(2000);

    if (zejf_init() != ZEJF_OK) {
        panic("ZEJF INIT FAILED!\n");
    }

    gpio_callback(0, 0);

    datetime_t dt;

    char command_buff[64];
    int command_buff_pos = 0;

    //char datetime_buf[256];
    //char *datetime_str = &datetime_buf[0];

    uint32_t last_log_num_press = -1;
    uint32_t last_log_num_temp = -1;

    millis_since_boot = to_ms_since_boot(get_absolute_time());

    struct repeating_timer timer_bmp;
    struct repeating_timer timer_save;
    struct repeating_timer timer_rip;

    add_repeating_timer_ms(-1000, bmp_measure, NULL, &timer_bmp);
    add_repeating_timer_ms(-60 * 1000, save_all, NULL, &timer_save);

    add_repeating_timer_ms(-5 * 1000, send_rip, NULL, &timer_rip);

    ZEJF_LOG(0, "Starting...\n");
    sleep_ms(2000);

    while (!rtc_running()) {
        ZEJF_LOG(2, "NOT RUNNING\n");
        sleep_ms(100);
    }

    while (true) {
        millis_since_boot = to_ms_since_boot(get_absolute_time());
        rtc_get_datetime(&dt);

        uint32_t unixtime = unix_time(dt);

        uint32_t log_num_press = get_log_num(unixtime, EVERY_5_SECONDS);
        uint32_t log_num_temp = get_log_num(unixtime, EVERY_MINUTE);
        uint32_t hour_num = get_hour_num(unixtime);

        if (log_num_press != last_log_num_press && time_set) {
            if (count_press > 0) {
                float val = sum_press / count_press;
                data_log(PRESSURE, hour_num, log_num_press, val, millis_since_boot, true);
            }
            last_log_num_press = log_num_press;
            count_press = 0;
            sum_press = 0;
        }

        if (log_num_temp != last_log_num_temp && time_set) {
            if (count_temp > 0) {
                float val = sum_temp / count_temp;
                data_log(TEMPERATURE, hour_num, log_num_temp, val, millis_since_boot, true);
            }
            last_log_num_temp = log_num_temp;
            count_temp = 0;
            sum_temp = 0;
        }

        check_input(command_buff, 64, &command_buff_pos, millis_since_boot);

        data_requests_process(millis_since_boot);

        network_process_packets(millis_since_boot);

        routing_table_check(millis_since_boot);

        if (save_all_flag) {
            data_save();
            save_all_flag = false;
        }
    }
}