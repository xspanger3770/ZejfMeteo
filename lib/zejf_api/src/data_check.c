#include "data_check.h"
#include "data_request.h"
#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_routing.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

uint32_t calculate_data_check(VariableInfo variable, uint32_t hour_num, uint32_t log_num)
{
    DataHour *hour = datahour_get(hour_num, true, false);
    if (hour == NULL) {
        return 0;
    }

    Variable *current_variable = get_variable(hour, variable.id);
    if (current_variable == NULL) {
        return 0;
    }

    if (variable.samples_per_hour != current_variable->variable_info.samples_per_hour) {
        return 0;
    }

    uint32_t result = 0;
    for (uint32_t log = 0; log <= log_num; log++) {
        if (log >= variable.samples_per_hour) {
            break;
        }
        float val = current_variable->data[log];
        if (val != VALUE_EMPTY && val != VALUE_NOT_MEASURED) {
            result++;
        }
    }

    return result;
}

bool data_check_send(uint16_t to, VariableInfo variable, uint32_t hour_num, uint32_t log_num, TIME_TYPE time)
{
    uint32_t check_number = calculate_data_check(variable, hour_num, log_num);

    char msg[PACKET_MAX_LENGTH];

    if (snprintf(msg, PACKET_MAX_LENGTH, "%" SCNu16 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32, variable.id, variable.samples_per_hour, hour_num, log_num, check_number) <= 0) {
        return false;
    }

    Packet *packet = network_prepare_packet(to, DATA_CHECK, msg);
    if (packet == NULL) {
        return false;
    }

    return network_send_packet(packet, time) == 0;
}

bool data_check_receive(Packet *packet)
{
    VariableInfo variable;

    uint32_t hour_num;
    uint32_t log_num;
    uint32_t check_number;

    if (sscanf(packet->message, "%" SCNu16 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32, &variable.id, &variable.samples_per_hour, &hour_num, &log_num, &check_number) != 5) {
        return false;
    }

    uint32_t our_check_number = calculate_data_check(variable, hour_num, log_num);

#if ZEJF_DEBUG
    printf("DATA CHECK hour %d [our %d vs their %d]\n", hour_num, our_check_number, check_number);
#endif

    if (our_check_number > check_number) {
        data_request_add(packet->from, variable, hour_num, 0, log_num);
    }

    return true;
}

void run_data_check(uint32_t current_hour_num, uint32_t current_millis_in_hour, uint32_t hours, TIME_TYPE time)
{
    // to avoid sending entire hour when just one recent log is missing

    if (hours > 24 * 5) {
        hours = 24 * 5;
    }
    uint16_t demand_count;
    uint16_t *demanded_variables;
    get_demanded_variables(&demand_count, &demanded_variables);

    for (uint32_t hour_num = current_hour_num; hour_num > current_hour_num - hours; hour_num--) {
        for (size_t i = 0; i < routing_table_top; i++) {
            RoutingEntry *entry = routing_table[i];
            for (uint16_t j = 0; j < entry->provided_count; j++) {
                VariableInfo provided_variable = entry->provided_variables[j];
                uint32_t current_log_num = (uint32_t) (((double) current_millis_in_hour / HOUR) * (provided_variable.samples_per_hour - 1.0));

                if(hour_num == current_hour_num){
                    if(current_log_num < DATA_CHECK_DELAY){
                        continue;
                    }

                    current_log_num -= DATA_CHECK_DELAY;
                }

                bool wanted = false;

                for (uint16_t k = 0; k < demand_count; k++) {
                    if (demanded_variables[k] == ALL_VARIABLES || demanded_variables[k] == provided_variable.id) {
                        wanted = true;
                        break;
                    }
                }

                if (!wanted) {
                    continue;
                }

                data_check_send(entry->device_id, provided_variable, hour_num, hour_num == current_hour_num ? current_log_num : provided_variable.samples_per_hour - 1, time);
            }
        }
    }
}
