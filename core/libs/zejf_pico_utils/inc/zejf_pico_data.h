#ifndef _ZEJF_PICO_DATA_H
#define _ZEJF_PICO_DATA_H

#include <memory>
#include <queue>

#include "time_utils.h"

class zejf_log
{
  public:
    uint32_t log_num = 0;
    uint32_t hour_num = 0;
    uint32_t last_log_num = 0;
    uint32_t sample_rate;

    zejf_log(uint32_t sample_rate)
        : sample_rate{ sample_rate } {};

    virtual ~zejf_log() = default;

    bool is_ready(uint32_t unix_time)
    {
        uint32_t abs_log = get_log_num_abs(unix_time, sample_rate); 
        
        if(abs_log - last_log_num == 1){
            log_num = get_log_num(unix_time, sample_rate);
            last_log_num = abs_log;

            return true;
        } else if(abs_log != last_log_num){
            last_log_num = abs_log;
            reset();
        }

        return false;
    }

    virtual void log_data(uint32_t millis) const = 0;

    virtual void finish() = 0;

    virtual void reset() = 0;

    virtual std::unique_ptr<zejf_log> clone() const = 0;
};

extern std::queue<std::unique_ptr<zejf_log>> logs_queue;
extern volatile bool queue_lock;

template <unsigned N>
bool process_queue(std::array<zejf_log *, N> all_logs);

#endif