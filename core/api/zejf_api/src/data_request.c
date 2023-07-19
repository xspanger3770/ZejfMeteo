#include "data_request.h"
#include "data_info.h"
#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_protocol.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef LinkedList Queue;

Queue *data_requests_queue;

zejf_err data_requests_init(void) {
    data_requests_queue = list_create(DATA_REQUEST_QUEUE_SIZE);
    return data_requests_queue != NULL ? ZEJF_OK : ZEJF_ERR_OUT_OF_MEMORY;
}

void *data_request_destroy(void *request) {
    free(request);
    return NULL;
}

DataRequest *data_request_create(void) {
    DataRequest *request = calloc(1, sizeof(DataRequest));
    if (request == NULL) {
        return NULL;
    }

    return request;
}

void data_requests_destroy(void) {
    list_destroy(data_requests_queue, &data_request_destroy);
}

bool request_already_exists(uint16_t to, VariableInfo variable, uint32_t hour_number) {
    Node *node = data_requests_queue->tail;
    while (node != NULL) {
        DataRequest *request = (DataRequest *) node->item;
        if (request->target_device == to && request->hour_number == hour_number && request->variable.id == variable.id) {
            return true;
        }
        node = node->next;
        if (node == data_requests_queue->tail) {
            break;
        }
    }
    return false;
}

zejf_err data_request_add(uint16_t to, VariableInfo variable, uint32_t hour_number, uint32_t start_log, uint32_t end_log) {
    if (request_already_exists(to, variable, hour_number)) {
        return ZEJF_ERR_REQUEST_ALREADY_EXISTS;
    }
    DataRequest *request = data_request_create();
    if (request == NULL) {
        return ZEJF_ERR_NULL;
    }

    request->hour_number = hour_number;
    request->current_log = start_log;
    request->start_log = start_log;
    request->end_log = end_log;
    request->target_device = to;
    request->variable = variable;
    request->errors = 0;

    return list_push(data_requests_queue, request) ? ZEJF_OK : ZEJF_ERR_GENERIC;
}

zejf_err data_request_send(uint16_t to, VariableInfo variable, uint32_t hour_number, uint32_t start_log, uint32_t end_log, TIME_TYPE time) {
    char msg[PACKET_MAX_LENGTH];

    if (snprintf(msg, PACKET_MAX_LENGTH, "%" SCNu16 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32, variable.id, variable.samples_per_hour, hour_number, start_log, end_log) <= 0) {
        return ZEJF_ERR_GENERIC;
    }

    Packet *packet = network_prepare_packet(to, DATA_REQUEST, msg);
    if (packet == NULL) {
        return ZEJF_ERR_GENERIC;
    }

    return network_send_packet(packet, time);
}

zejf_err data_request_receive(Packet *packet) {
    VariableInfo variable;
    uint32_t hour_number;
    uint32_t start_log;
    uint32_t end_log;

    if (sscanf(packet->message, "%" SCNu16 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32, &variable.id, &variable.samples_per_hour, &hour_number, &start_log, &end_log) != 5) {
        return ZEJF_ERR_GENERIC;
    }

    return data_request_add(packet->from, variable, hour_number, start_log, end_log);
}

bool request_finished(DataRequest *request) {
    return request->current_log > request->end_log;
}

void data_requests_process(TIME_TYPE time) {
    size_t remaining_slots = allocate_packet_queue(PRIORITY_HIGH);
    int request_count = 1;
    while (remaining_slots > 0 && !list_is_empty(data_requests_queue)) {
        DataRequest *request = list_peek(data_requests_queue)->item;
        if (request == NULL) {
            break; // still null, nothing left
        }

        if (request_finished(request) || request->errors > 50) {
            printf("Request finished with %d errors\n", request->errors);
            
            data_request_destroy(list_pop(data_requests_queue));
            request = NULL;
            request_count++;
            if (request_count == 3) {
                break; // so that this function doesn't take too long
            }
            goto next;
        }

        float val = 0.0;

        bool success = (data_get_val(request->variable, request->hour_number, request->current_log, true, false, &val) == ZEJF_OK) &&
                (data_send_log(request->target_device, request->variable, request->hour_number, request->current_log, val, time) == ZEJF_OK);

        if (!success) {
            request->errors++;
            break;
        }

        request->current_log++;

    next:
        remaining_slots--;
    }
}
