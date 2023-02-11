#ifndef _DATA_REQUEST_H
#define _DATA_REQUEST_H

#include "zejf_defs.h"
#include "zejf_queue.h"

extern Queue* data_requests_queue;

void data_requests_init(void);

void data_requests_destroy(void);

bool data_request_send(u_int16_t to, VariableInfo variable, uint32_t start_day, uint32_t end_day, uint32_t start_log, uint32_t end_log, TIME_TYPE time);

bool data_request_receive(Packet* packet);

#endif