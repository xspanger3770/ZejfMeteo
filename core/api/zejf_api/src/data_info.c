#include "data_info.h"
#include "zejf_api.h"
#include "zejf_routing.h"
#include <inttypes.h>
#include <stdio.h>

zejf_err p_send_provide(VariableInfo info, TIME_TYPE time) {
    char msg[PACKET_MAX_LENGTH];

    if (snprintf(msg, PACKET_MAX_LENGTH, "%" SCNu16 "@%" SCNu32, info.id, info.samples_per_hour) <= 0) {
        return ZEJF_ERR_GENERIC;
    }

    Packet *packet = network_prepare_packet(BROADCAST, DATA_PROVIDE, msg);

    if (packet == NULL) {
        return ZEJF_ERR_NULL;
    }

    return network_send_packet(packet, time);
}

zejf_err network_send_provide_info(TIME_TYPE time) {
    uint16_t provide_count;
    const VariableInfo *provided_variables;
    get_provided_variables(&provide_count, &provided_variables);

    if (provide_count == 0 || provided_variables == NULL) {
        return ZEJF_ERR_GENERIC;
    }

    if (provide_ptr >= provide_count) {
        provide_ptr = 0;
    }

    size_t max_count = allocate_packet_queue(PRIORITY_MEDIUM);
    while (max_count > 0) {
        zejf_err rv = p_send_provide(provided_variables[provide_ptr], time);
        if (rv != ZEJF_OK) {
            return rv;
        }
        provide_ptr++;
        provide_ptr %= provide_count;
        if (provide_ptr == 0) {
            break;
        }
        max_count--;
    }

    return ZEJF_OK;
}

zejf_err p_send_demand(uint16_t var, TIME_TYPE time) {
    char msg[PACKET_MAX_LENGTH];

    if (snprintf(msg, PACKET_MAX_LENGTH, "%" SCNu16, var) <= 0) {
        return ZEJF_ERR_GENERIC;
    }

    Packet *packet = network_prepare_packet(BROADCAST, DATA_DEMAND, msg);
    if (packet == NULL) {
        return ZEJF_ERR_NULL;
    }

    return network_send_packet(packet, time);
}

zejf_err network_send_demand_info(TIME_TYPE time) {
    uint16_t demand_count;
    uint16_t *demanded_variables;
    get_demanded_variables(&demand_count, &demanded_variables);

    if (demand_count == 0 || demanded_variables == NULL) {
        return ZEJF_ERR_GENERIC;
    }

    if (demand_ptr >= demand_count) {
        demand_ptr = 0;
    }

    size_t max_count = allocate_packet_queue(PRIORITY_MEDIUM);
    while (max_count > 0) {
        zejf_err rv = p_send_demand(demanded_variables[demand_ptr], time);
        if (rv != ZEJF_OK) {
            return rv;
        }
        demand_ptr++;
        demand_ptr %= demand_count;
        if (demand_ptr == 0) {
            break;
        }
        max_count--;
    }

    return ZEJF_OK;
}

zejf_err process_data_demand(Packet *packet) {
    if (packet->message == NULL) {
        return ZEJF_ERR_NULL;
    }

    RoutingEntry *entry = routing_entry_find(packet->from);
    if (entry == NULL) {
        return ZEJF_ERR_NULL;
    }

    uint16_t variable;
    if (sscanf(packet->message, "%" SCNu16, &variable) != 1) {
        return ZEJF_ERR_GENERIC;
    }

    return routing_entry_add_demanded_variable(entry, variable);
}

zejf_err process_data_provide(Packet *packet) {
    if (packet->message == NULL) {
        return ZEJF_ERR_NULL;
    }

    RoutingEntry *entry = routing_entry_find(packet->from);
    if (entry == NULL) {
        return ZEJF_ERR_NULL;
    }

    VariableInfo var_info;

    if (sscanf(packet->message, "%" SCNu16 "@%" SCNu32, &var_info.id, &var_info.samples_per_hour) != 2) {
        return ZEJF_ERR_GENERIC;
    }

    return routing_entry_add_provided_variable(entry, var_info);
}

zejf_err process_data_subscribe(Packet *packet) {
    RoutingEntry *entry = routing_entry_find(packet->from);
    if (entry == NULL) {
        return ZEJF_ERR_NULL;
    }

    entry->subscribed = true;
    return ZEJF_OK;
}

zejf_err data_send_log(uint16_t to, VariableInfo variable, uint32_t hour_number, uint32_t sample_num, float val, TIME_TYPE time) {
    char msg[PACKET_MAX_LENGTH];

    if (snprintf(msg, PACKET_MAX_LENGTH, "%" SCNu16 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%.4f", variable.id, variable.samples_per_hour, hour_number, sample_num, val) <= 0) {
        return ZEJF_ERR_PACKET_FORMAT;
    }

    Packet *packet = network_prepare_packet(to, DATA_LOG, msg);

    if (packet == NULL) {
        return ZEJF_ERR_NULL;
    }

    return network_send_packet(packet, time);
}

zejf_err network_announce_log(VariableInfo target_variable, uint32_t hour_number, uint32_t sample_num, float val, TIME_TYPE time) {
    zejf_err result = ZEJF_OK;
    for (size_t i = 0; i < routing_table_top; i++) {
        RoutingEntry *entry = routing_table[i];

        bool demanded = false;
        for (uint16_t j = 0; j < entry->demand_count; j++) {
            uint16_t var = entry->demanded_variables[j];
            if (var == ALL_VARIABLES || var == target_variable.id) {
                demanded = true;
                break;
            }
        }

        if (!demanded) {
            continue;
        }

        if (data_send_log(entry->device_id, target_variable, hour_number, sample_num, val, time) != ZEJF_OK) {
            return result = ZEJF_ERR_PARTIAL;
        }
    }

    return result;
}

zejf_err network_broadcast_log(VariableInfo target_variable, uint32_t hour_number, uint32_t sample_num, float val, TIME_TYPE time) {
    zejf_err result = ZEJF_OK;
    for (size_t i = 0; i < routing_table_top; i++) {
        RoutingEntry *entry = routing_table[i];

        if (!entry->subscribed) {
            continue;
        }

        if (data_send_log(entry->device_id, target_variable, hour_number, sample_num, val, time) != ZEJF_OK) {
            result = ZEJF_ERR_PARTIAL;
        }
    }
    return result;
}

zejf_err process_data_log(Packet *packet, TIME_TYPE time) {
    if (packet->message == NULL) {
        return ZEJF_ERR_NULL;
    }

    VariableInfo variable = { 0 };
    uint32_t hour_number = 0;
    uint32_t sample_num = 0;
    float val = 0.0f;

    if (sscanf(packet->message, "%" SCNu16 ",%" SCNu32 ",%" SCNu32 ",%" SCNu32 ",%f", &variable.id, &variable.samples_per_hour, &hour_number, &sample_num, &val) != 5) {
        return ZEJF_ERR_GENERIC;
    }

    return data_log(variable, hour_number, sample_num, val, time, false);
}
