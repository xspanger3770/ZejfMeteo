#include <hardware/rtc.h>

#include "zejf_pico_data.h"

volatile bool queue_lock = false;
std::queue<std::unique_ptr<zejf_log>> logs_queue;