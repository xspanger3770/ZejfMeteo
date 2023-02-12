#include "data_request.h"
#include "zejf_api.h"
#include "zejf_queue.h"
#include "zejf_protocol.h"
#include "data_info.h"
#include "zejf_data.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

Queue* data_requests_queue;

void data_requests_init(void){
    data_requests_queue = queue_create(DATA_REQUEST_QUEUE_SIZE);
}

void* data_request_destroy(void* request){
    free(request);
    return NULL;
}

DataRequest* data_request_create(){
    DataRequest* request = calloc(1, sizeof(DataRequest));
    if(request == NULL){
        return NULL;
    }

    return request;
}

void data_requests_destroy(void){
    queue_destroy(data_requests_queue, &data_request_destroy);
}

bool request_already_exists(uint16_t to, VariableInfo variable, uint32_t day_number){
    Node* node = data_requests_queue->first;
    while(node != NULL){
        DataRequest* request = (DataRequest*)node->item;
        if(request->target_device == to && request->day_number == day_number && request->variable.id == variable.id){
            return true;
        }
        node = node->next;
    }
    return false;
}

bool data_request_add(uint16_t to, VariableInfo variable, uint32_t day_number, uint32_t start_log, uint32_t end_log){
    if(request_already_exists(to, variable, day_number)){
        return false;
    }
    DataRequest* request = data_request_create();
    if(request == NULL){
        return false;
    }

    request->day_number = day_number;
    request->current_log = start_log;
    request->start_log = start_log;
    request->end_log = end_log;
    request->target_device = to;
    request->variable = variable;

    printf("adding now day %d total %ld\n", day_number, data_requests_queue->item_count);

    return queue_push(data_requests_queue, request);
}

bool data_request_send(u_int16_t to, VariableInfo variable, uint32_t day_number, uint32_t start_log, uint32_t end_log, TIME_TYPE time){
    char msg[PACKET_MAX_LENGTH];

    if(snprintf(msg, PACKET_MAX_LENGTH, "%"SCNu16",%"SCNu32",%"SCNu32",%"SCNu32",%"SCNu32, 
                        variable.id, variable.samples_per_day, day_number, start_log, end_log) <= 0){
        return false;
    }

    Packet* packet = network_prepare_packet(to, DATA_REQUEST, msg);
    if(packet == NULL){
        return NULL;
    }

    return network_send_packet(packet, time);
}

bool data_request_receive(Packet* packet){
    VariableInfo variable; 
    uint32_t day_number;
    uint32_t start_log; 
    uint32_t end_log;

    if(sscanf(packet->message, "%"SCNu16",%"SCNu32",%"SCNu32",%"SCNu32",%"SCNu32, 
                        &variable.id, &variable.samples_per_day, &day_number, &start_log, &end_log) != 5){
        return false;
    }

    return data_request_add(packet->from, variable, day_number, start_log, end_log);
}

inline bool request_finished(DataRequest* request){
    return request->current_log > request->end_log;
}

void request_increase(DataRequest* request){
    request->current_log++;
}

void data_requests_process(TIME_TYPE time){
    size_t remaining_slots = allocate_packet_queue(PRIORITY_HIGH);
    DataRequest* request = NULL;
    while(remaining_slots > 0){
        if(request == NULL){
            request = queue_peek(data_requests_queue);
            if(request == NULL){
                break; // still null, nothing left
            }
        }

        if(request_finished(request)){
            data_request_destroy(queue_pop(data_requests_queue));
            request = NULL;
            goto next;
        }

        float val = data_get_val(request->variable, request->day_number, request->current_log);

        if(!data_send_log(request->target_device, request->variable, request->day_number, request->current_log, val, time)){
            break;
        }
        
        request_increase(request);

        next:
        remaining_slots--;
    }
}

int mains(){
    float f = -999.0001;
    printf("%d\n", (f == VALUE_EMPTY));
    printf("%f\n" , FLT_MAX-100);

    return 0;
}