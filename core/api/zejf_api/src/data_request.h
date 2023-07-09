#ifndef _DATA_REQUEST_H
#define _DATA_REQUEST_H

#include "linked_list.h"
#include "zejf_defs.h"

extern LinkedList *data_requests_queue;

bool data_requests_init(void);

void data_requests_destroy(void);

bool data_request_send(uint16_t to, VariableInfo variable, uint32_t hour_number, uint32_t start_log, uint32_t end_log, TIME_TYPE time);

bool data_request_add(uint16_t to, VariableInfo variable, uint32_t hour_number, uint32_t start_log, uint32_t end_log);

bool data_request_receive(Packet *packet);

#endif