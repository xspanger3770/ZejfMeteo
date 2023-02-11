#include "data_check.h"
#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_routing.h"
#include "data_request.h"

#include <inttypes.h>
#include <stdio.h>

uint32_t calculate_data_check(VariableInfo variable, uint32_t day_num, uint32_t log_num){
    Day** day_ptr = day_get(day_num, true, false);
    if(day_ptr == NULL){
        return 0;
    }

    Variable* current_variable = get_variable(*day_ptr, variable.id);
    if(variable.samples_per_day != current_variable->info.samples_per_day){
        return 0;
    }

    uint32_t result = 0;
    for(uint32_t log = 0; log <= log_num; log++){
        if(log >= variable.samples_per_day){
            break;
        }
        float val = current_variable->_start[log];
        if(val != VALUE_EMPTY && val != VALUE_NOT_MEASURED){
            result++;
        }
    }

    return result;
}

bool data_check_send(uint16_t to, VariableInfo variable, uint32_t day_num, uint32_t log_num, TIME_TYPE time){
    uint32_t check_number = calculate_data_check(variable, day_num, log_num);

    char msg[PACKET_MAX_LENGTH];

    if(snprintf(msg, PACKET_MAX_LENGTH, "%"SCNu16",%"SCNu32",%"SCNu32",%"SCNu32",%"SCNu32, 
                        variable.id, variable.samples_per_day, day_num, log_num, check_number) <= 0){
        return false;
    }

    Packet* packet = network_prepare_packet(to, DATA_CHECK, msg);
    if(packet == NULL){
        return NULL;
    }

    return network_send_packet(packet, time);
}

bool data_check_receive(Packet* packet){
    VariableInfo variable;

    uint32_t day_num;
    uint32_t log_num;
    uint32_t check_number;

    if(sscanf(packet->message, "%"SCNu16",%"SCNu32",%"SCNu32",%"SCNu32",%"SCNu32, 
                        &variable.id, &variable.samples_per_day, &day_num, &log_num, &check_number) != 5){
        return false;
    }

    uint32_t our_check_number = calculate_data_check(variable, day_num, log_num);

    printf("DATA CHECK %d vs %d\n", our_check_number, check_number);

    if(our_check_number != check_number){
        data_request_add(packet->from, variable, day_num, 0, log_num);
    }

    return true;
}

// warning: you may vomit after seeing this function
void run_data_check(uint32_t current_day_num, uint32_t current_log_num, uint32_t days, TIME_TYPE time){
    if(days>30){
        return;
    }
    uint16_t demand_count;
    uint16_t* demanded_variables;
    get_demanded_variables(&demand_count, &demanded_variables);

    for(uint32_t day_num = current_day_num; day_num > current_day_num - days; day_num--){
        for(size_t i = 0; i < routing_table_top; i++){
            RoutingEntry* entry = routing_table[i];
            for(uint16_t j = 0; j < entry->provided_count; j++){
                VariableInfo provided_variable = entry->provided_variables[j];
                bool wanted = false;
                
                for(uint16_t k = 0; k < demand_count; k++){
                    if(demanded_variables[k] == ALL_VARIABLES || demanded_variables[k] == provided_variable.id){
                        wanted = true;
                    }
                }

                if(!wanted){
                    continue;
                }

                data_check_send(entry->device_id, provided_variable, day_num, day_num == current_day_num ? current_log_num : provided_variable.samples_per_day - 1, time);
            }
        }
    }
}