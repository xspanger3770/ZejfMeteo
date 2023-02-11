#include "data_request.h"
#include "zejf_api.h"
#include "zejf_queue.h"
#include "zejf_protocol.h"

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

bool data_request_add(uint16_t to, VariableInfo variable, uint32_t start_day, uint32_t end_day, uint32_t start_log, uint32_t end_log){
    DataRequest* request = data_request_create();
    if(request == NULL){
        return false;
    }

    if(end_day < start_day || end_day-start_day > DATA_REQUEST_MAX_DAYS){
        return false;
    }

    request->current_day = start_day;
    request->current_log = start_log;
    request->end_day = end_day;
    request->end_log = end_log;
    request->start_day = start_day;
    request->start_log = start_log;
    request->target_device = to;
    request->variable = variable;

    return queue_push(data_requests_queue, request);
}

bool data_request_send(u_int16_t to, VariableInfo variable, uint32_t start_day, uint32_t end_day, uint32_t start_log, uint32_t end_log, TIME_TYPE time){
    char msg[PACKET_MAX_LENGTH];

    if(snprintf(msg, PACKET_MAX_LENGTH, "%"SCNu16",%"SCNu32",%"SCNu32",%"SCNu32",%"SCNu32",%"SCNu32, 
                        variable.id, variable.samples_per_day, start_day, end_day, start_log, end_log) <= 0){
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
    uint32_t start_day; 
    uint32_t end_day; 
    uint32_t start_log; 
    uint32_t end_log;

    if(sscanf(packet->message, "%"SCNu16",%"SCNu32",%"SCNu32",%"SCNu32",%"SCNu32",%"SCNu32, 
                        &variable.id, &variable.samples_per_day, &start_day, &end_day, &start_log, &end_log) != 6){
        return false;
    }

    return data_request_add(packet->from, variable, start_day, end_day, start_log, end_log);
}

int mains(){
    float f = -999.0001;
    printf("%d\n", (f == VALUE_EMPTY));
    printf("%f\n" , FLT_MAX-100);
}