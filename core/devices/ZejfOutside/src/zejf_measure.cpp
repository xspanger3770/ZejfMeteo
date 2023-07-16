
#include <hardware/adc.h>
#include <hardware/rtc.h>

#include "zejf_pico_data.h"
#include "zejf_measure.h"
#include "time_engine.h"
#include "zejf_outside.h"

extern "C"{
    #include "zejf_api.h"
    #include "htu21.h"
}

const double RR_COEFF = 0.254;

static bool rr_first = false;

const double conversion_factor = 3.3f / (1 << 12);
const double voltage_divider = 47.0 * (1.0 / 6.8 + 1.0 / 47.0);
const double current_sensing_resistor = 0.22;

const double correction_battery = 4.933 / 5.0068;
const double correction_sensing = 4.933 / 4.9598;

volatile bool htu_initialised = false;

volatile uint16_t rr_c = 0;
static volatile uint16_t rr_c_last = 0;

long htu_errors = 0;
static int htu_reset_countdown = 0;
static int htu_next_reset = 1;

static volatile uint64_t last_rr_callback = 0;
static volatile bool rr_measured = false;
static volatile double latest_max_rain_rate = 0.0;

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

class rr_log : public zejf_log
{
  public:
    uint16_t log_count = 0;
    double max_rain_rate = 0.0;

    rr_log(uint32_t sample_rate)
        : zejf_log(sample_rate){};

    void reset() override {
        log_count = 0;
        max_rain_rate = 0.0;
    }

    void finish() override {
    }

    void sample(uint16_t counts, double _latest_max_rain_rate) {
        log_count += counts;
        if (_latest_max_rain_rate > max_rain_rate) {
            max_rain_rate = _latest_max_rain_rate;
        }
    }

    void log_data(uint32_t millis) const override {
        data_log(VAR_RR, hour_num, log_num, log_count * RR_COEFF, millis, true);
        data_log(VAR_RAIN_RATE_PEAK, hour_num, log_num, max_rain_rate, millis, true);
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
        if (log_count == 0) {
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

std::array<zejf_log *, 3> all_logs = { &current_htu_log, &current_rr_log, &current_voltages_log };

static void measure_voltages() {
    adc_select_input(2);                                                                                // 28 ... solar pin
    double voltage_solar = adc_read() * conversion_factor * voltage_divider;
    adc_select_input(1);                                                                                // 27 ... battery pin
    double voltage_battery = adc_read() * conversion_factor * voltage_divider * correction_battery;
    adc_select_input(0);                                                                                // 26 ... voltage_divider
    double voltage_sensing = adc_read() * conversion_factor * voltage_divider * correction_sensing;
    double battery_current = ((voltage_sensing - voltage_battery) / current_sensing_resistor) * 1000.0; // mA
    current_voltages_log.sample(voltage_solar, voltage_battery, battery_current);
    //printf("VOLTAGES %f %f %f\n", voltage_solar, voltage_battery, voltage_sensing);
}

bool process_measurements(struct repeating_timer *) {
    uint16_t rr_c_temp = rr_c;
    uint16_t _rr_c = rr_c_temp - rr_c_last;
    rr_c_last = rr_c_temp;

    if (rr_measured && (millis_since_boot - last_rr_callback / 1000) >= (15 * 1000 * 60)) {
        latest_max_rain_rate = 0.0;
    }

    if (!time_set) {
        return true;
    }

    measure_voltages();

    current_rr_log.sample(_rr_c, latest_max_rain_rate);

    if (queue_lock) {
        return true;
    }

    datetime_t dt;
    rtc_get_datetime(&dt);
    uint32_t unixtime = unix_time(dt);
    uint32_t hour_num = get_hour_num(unixtime);

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

bool htu_measure(struct repeating_timer *) {
    ZEJF_LOG(0, "HTU = %d %d/%d\n", htu_initialised, htu_reset_countdown, htu_next_reset);

    if (!htu_initialised) {
        if (htu_reset_countdown < htu_next_reset) {
            htu_reset_countdown++;
            return true;
        }

        htu_reset_countdown = 0;
        if (htu_next_reset < 20) {
            htu_next_reset *= 2;
        }

        htu_initialised = htu21_reset();

        if (!htu_initialised) {
            return true;
        }

        htu_next_reset = 1;
    }

    htu_measurement m = htu21_measure();

    if (!m.valid) {
        htu_errors++;
        htu_initialised = 0;
    }

    if (!time_set) {
        return true;
    }

    current_htu_log.sample(m);

    return true;
}

// one interrupt per second means 914.4 mm/h
void rr_callback(uint gpio, uint32_t) {
    if (gpio == RR_PIN) {
        if (!rr_first) {
            rr_first = true;
            return;
        }

        rr_c++;

        uint64_t current_us = to_us_since_boot(get_absolute_time());
        if (rr_measured) {
            double rain_rate = (RR_COEFF * 60.0 * 60.0) * (1000000.0 / (current_us - last_rr_callback));
            if ((rain_rate >= 0 && rain_rate < 2000.0) && rain_rate > latest_max_rain_rate) {
                latest_max_rain_rate = rain_rate;
            }
        }
        last_rr_callback = current_us;
        rr_measured = true;
    }
}