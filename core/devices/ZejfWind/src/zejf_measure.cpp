#include <hardware/adc.h>
#include <hardware/rtc.h>

#include <cmath>
#include <inttypes.h>

#include "zejf_pico_data.h"
#include "zejf_measure.h"
#include "time_engine.h"
#include "zejf_wind.hpp"

extern "C"{
    #include "zejf_api.h"
    #include "bmp085.h"
}

const double WIND_CORRECTION = 20;
static const uint16_t WDIR_TABLE[16] = { 3130, 1630, 1850, 353, 390, 282, 748, 512, 1163, 990, 2520, 2396, 3760, 3300, 3539, 2805 };

volatile uint16_t wspd_c = 0;
static volatile uint16_t wspd_c_last = 0;

static uint64_t last_wspd_callback = 0;
static bool gust_measured = false;
static volatile double latest_max_gust = 0.0;
volatile bool bmp_initialised = false;


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
        
        double my_dx = 0.0;
        double my_dy = 0.0;
        unsigned int index = running_index;
        unsigned int start = running_index;

        do{
            my_dy += wspd_running[index] * std::cos((wdir_running[index] / 180.0) * M_PI);
            my_dx += wspd_running[index] * std::sin((wdir_running[index] / 180.0) * M_PI);
            index = (index + 1) % average_count;
        } while(index != start);
        
        
        wspd = std::sqrt(std::pow(my_dx, 2) + std::pow(my_dy, 2)) / (average_count);
        wdir = (std::fabs(my_dx) <= 0.001 && std::fabs(my_dy) <= 0.001) ? actual_wdir : (std::atan2(-my_dx, -my_dy) + M_PI) * 180.0 / M_PI;
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

static double get_wdir(uint16_t adc_val)
{
    uint16_t lowest_error = std::abs(adc_val - WDIR_TABLE[0]);
    int result = 0;
    for (int i = 1; i < 16; i++) {
        uint16_t err = std::abs(adc_val - WDIR_TABLE[i]);
        if (err < lowest_error) {
            lowest_error = err;
            result = i;
        }
    }

    return result * 22.5;
}

bool process_measurements(struct repeating_timer *)
{
    uint16_t wspd_c_temp = wspd_c;
    uint16_t _wspd_c = wspd_c_temp - wspd_c_last;
    wspd_c_last = wspd_c_temp;

    double gust = latest_max_gust;
    latest_max_gust = 0;

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

bool bmp_measure(struct repeating_timer *)
{
    if (!time_set || !bmp_initialised) {
        return true;
    }

    Measurement m = BMP085_read_all(ALTITUDE);
    current_bmp_log.sample(m.pressure_sea_level, m.temperature);

    return true;
}

// ONE INTERRUPT PER SECONDS MEANS 1.2km/h WIND
void wspd_callback(uint gpio, uint32_t events)
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
