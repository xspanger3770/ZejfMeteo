#include "data_check.h"
#include "data_request.h"
#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_routing.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

bool is_provided(VariableInfo variable) {
    uint16_t provide_count;
    const VariableInfo *provided_variables;
    get_provided_variables(&provide_count, &provided_variables);
    for (uint16_t i = 0; i < provide_count; i++) {
        if (provided_variables[i].id == variable.id) {
            return true;
        }
    }

    return false;
}

zejf_err calculate_data_check(VariableInfo variable, uint32_t hour_num, uint32_t log_num, uint32_t* result) {
    DataHour *hour = datahour_get(hour_num, true, false);
    if (hour == NULL) {
        return ZEJF_ERR_NULL;
    }

    Variable *current_variable = get_variable(hour, variable.id);
    if (current_variable == NULL) {
        return ZEJF_ERR_NULL;
    }

    if (variable.samples_per_hour != current_variable->variable_info.samples_per_hour) {
        return ZEJF_ERR_INVALID_SAMPLE_RATE;
    }

    bool is_prov = is_provided(variable);

    uint32_t values = 0;
    for (uint32_t log = 0; log <= log_num; log++) {
        if (log >= variable.samples_per_hour) {
            break;
        }
        float val = current_variable->data[log];
        if (val != VALUE_EMPTY) {
            values++;
        } else if (is_prov) {
            current_variable->data[log] = VALUE_NOT_MEASURED;
            values++;
        }
    }

    *result = values;

    return ZEJF_OK;
}

zejf_err data_check_send(uint16_t to, VariableInfo variable, uint32_t hour_num, uint32_t log_num, TIME_TYPE time) {
    uint32_t check_number = 0;
    zejf_err rv = calculate_data_check(variable, hour_num, log_num, &check_number);

    if(check_number == variable.samples_per_hour){
        return ZEJF_OK; // no need to check when we have everything
    }

    if(rv != ZEJF_OK){
        return rv;
    }

    char msg[PACKET_MAX_LENGTH];

    if (snprintf(msg, PACKET_MAX_LENGTH, "%" SCNu16 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32, variable.id, variable.samples_per_hour, hour_num, log_num, check_number) <= 0) {
        return ZEJF_ERR_GENERIC;
    }

    Packet *packet = network_prepare_packet(to, DATA_CHECK, msg);
    if (packet == NULL) {
        return ZEJF_ERR_NULL;
    }

    ZEJF_LOG(0, "Sending data check hour %" SCNu32 " var %" SCNu16 "\n", hour_num, variable.id);

    return network_send_packet(packet, time);
}

zejf_err data_check_receive(Packet *packet) {
    VariableInfo variable;

    uint32_t hour_num;
    uint32_t log_num;
    uint32_t check_number;

    if (sscanf(packet->message, "%" SCNu16 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32, &variable.id, &variable.samples_per_hour, &hour_num, &log_num, &check_number) != 5) {
        return ZEJF_ERR_GENERIC;
    }

    uint32_t our_check_number = 0;
    zejf_err rv = calculate_data_check(variable, hour_num, log_num, &our_check_number);

    if(rv != ZEJF_OK){
        return rv;
    }

    ZEJF_LOG(1, "DATA CHECK hour %" SCNu32 " variable %" SCNu16 " [our %" SCNu32 " vs their %" SCNu32 "]\n", hour_num, variable.id, our_check_number, check_number);

    if (our_check_number > check_number) {
        zejf_err rv = data_request_add(packet->from, variable, hour_num, 0, log_num);
        return rv;
    }

    return ZEJF_OK;
}

void run_data_check(uint32_t current_hour_num, uint32_t current_millis_in_hour, uint32_t hours, TIME_TYPE time) {
    // to avoid sending entire hour when just one recent log is missing
    // what?

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

                if (hour_num == current_hour_num) {
                    if (current_log_num < DATA_CHECK_DELAY) {
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

                if (data_check_send(entry->device_id, provided_variable, hour_num, hour_num == current_hour_num ? current_log_num : provided_variable.samples_per_hour - 1, time) != ZEJF_OK) {
                    return;
                }
            }
        }
    }
}
