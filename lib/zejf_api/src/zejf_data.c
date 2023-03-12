#include "zejf_data.h"
#include "data_info.h"
#include "linked_list.h"
#include "zejf_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef LinkedList Queue;

Queue *data_queue;

void *hour_destroy(void *ptr);

void data_init(void)
{
    data_queue = list_create(HOUR_BUFFER_SIZE);
}

void data_destroy(void)
{
    list_destroy(data_queue, hour_destroy);
}

DataHour *hour_create(uint32_t hour_id)
{
    DataHour *hour = calloc(1, sizeof(DataHour));
    if (hour == NULL) {
        return NULL;
    }

    hour->hour_id = hour_id;
    hour->flags = 0;
    hour->variable_count = 0;
    hour->variables = NULL;

    return hour;
}

bool hour_add_variable(DataHour *hour, VariableInfo variable)
{
    if (variable.samples_per_hour == 0 || variable.samples_per_hour > SAMPLE_RATE_MAX || hour->variable_count == VARIABLES_MAX) {
        return false;
    }
    for (uint16_t i = 0; i < hour->variable_count; i++) {
        if (hour->variables[i].variable_info.id == variable.id) {
            return false;
        }
    }

    void *ptr = realloc(hour->variables, (hour->variable_count + 1) * sizeof(Variable));
    if (ptr == NULL) {
        return false;
    }

    float *data = malloc(variable.samples_per_hour * sizeof(float));
    if (data == NULL) {
        free(ptr);
        return false;
    }

    for (uint32_t i = 0; i < variable.samples_per_hour; i++) {
        data[i] = VALUE_EMPTY;
    }

    Variable *new_var = &((Variable *) ptr)[hour->variable_count];
    new_var->variable_info.samples_per_hour = variable.samples_per_hour;
    new_var->variable_info.id = variable.id;
    new_var->data = data;

    hour->variables = ptr;
    hour->variable_count++;

    return true;
}

void *hour_destroy(void *ptr)
{
    if (ptr == NULL) {
        return NULL;
    }

    DataHour *hour = (DataHour *) ptr;

    for (int i = 0; i < hour->variable_count; i++) {
        Variable variable = hour->variables[i];
        free(variable.data);
    }

    free(hour->variables);
    free(hour);

    return NULL;
}

uint32_t hour_calculate_checksum(DataHour *hour)
{
    uint32_t result = 0;

    result += hour->flags;
    result += hour->hour_id;
    result += hour->variable_count;

    for (uint16_t i = 0; i < hour->variable_count; i++) {
        Variable var = hour->variables[i];
        result += var.variable_info.id;
        result += var.variable_info.samples_per_hour;

        for (size_t j = 0; j < var.variable_info.samples_per_hour * sizeof(float); j++) {
            result += ((uint8_t *) var.data)[j];
        }
    }

    return result;
}

void serialize(uint8_t *data, size_t *ptr, void *val, size_t size)
{
    memcpy(data + *ptr, val, size);
    *ptr += size;
}

uint8_t *hour_serialize(DataHour *hour, size_t *size)
{
    if(hour == NULL || size == NULL){
        return NULL;
    }
    size_t total_size = DATAHOUR_BYTES;
    for (uint32_t i = 0; i < hour->variable_count; i++) {
        total_size += VARIABLE_INFO_BYTES;
        total_size += (size_t) (hour->variables[i].variable_info.samples_per_hour) * 4;
    }

    uint8_t *data = malloc(total_size);
    if (data == NULL) {
        return NULL;
    }

    hour->checksum = hour_calculate_checksum(hour);

    serialize(data, size, &hour->checksum, 4);
    serialize(data, size, &hour->hour_id, 4);
    serialize(data, size, &hour->variable_count, 2);
    serialize(data, size, &hour->flags, 1);

    for (uint32_t i = 0; i < hour->variable_count; i++) {
        Variable *variable = &hour->variables[i];
        serialize(data, size, &variable->variable_info.samples_per_hour, 4);
        serialize(data, size, &variable->variable_info.id, 2);
        serialize(data, size, variable->data, sizeof(float) * variable->variable_info.samples_per_hour);
    }

    return data;
}

bool deserialize(void *destination, uint8_t *data, size_t *ptr, size_t size, size_t total_size)
{
    if (*ptr + size > total_size) {
        return false;
    }

    memcpy(destination, data + *ptr, size);
    *ptr += size;

    return true;
}

DataHour *hour_deserialize(uint8_t *data, size_t total_size)
{
    if (data == NULL) {
        return NULL;
    }
    DataHour *hour = hour_create(0);
    if (hour == NULL) {
        return NULL;
    }

    bool result = true;
    size_t ptr = 0;

    uint16_t total_variable_count = 0;
    hour->variable_count = 0;

    result &= deserialize(&hour->checksum, data, &ptr, 4, total_size);
    result &= deserialize(&hour->hour_id, data, &ptr, 4, total_size);
    result &= deserialize(&total_variable_count, data, &ptr, 2, total_size);
    result &= deserialize(&hour->flags, data, &ptr, 1, total_size);

    for (uint32_t i = 0; i < total_variable_count; i++) {
        uint32_t samples_per_hour = 0;
        uint16_t variable_id = 0;

        result &= deserialize(&samples_per_hour, data, &ptr, 4, total_size);
        result &= deserialize(&variable_id, data, &ptr, 2, total_size);

        if (!result) {
            goto error;
        }

        VariableInfo info = {
            .id = variable_id,
            .samples_per_hour = samples_per_hour
        };

        result &= hour_add_variable(hour, info);

        if (!result) {
            goto error;
        }

        result &= deserialize(hour->variables[hour->variable_count - 1].data, data, &ptr, sizeof(float) * samples_per_hour, total_size);

        if (!result) {
            goto error;
        }
    }

    result &= ptr == total_size; // must use all

    result &= hour_calculate_checksum(hour) == hour->checksum;

    if (!result) {
    error:
        hour_destroy(hour);
        return NULL;
    }

    return hour;
}

Variable *get_variable(DataHour *hour, uint16_t variable_id)
{
    for (uint16_t i = 0; i < hour->variable_count; i++) {
        Variable *var = &hour->variables[i];
        if (var->variable_info.id == variable_id) {
            return var;
        }
    }

    return NULL;
}

DataHour *hour_find(uint32_t hour_number)
{
    Node *node = data_queue->head;
    if (node == NULL) {
        return NULL;
    }
    do {
        DataHour *hour = (DataHour *) node->item;
        if (hour->hour_id == hour_number) {
            list_prioritise(data_queue, node);
            return hour;
        }
        node = node->previous;
    } while (node != data_queue->head);

    return NULL;
}

DataHour *datahour_load(uint32_t hour_number, size_t *loaded_size)
{
    uint8_t *buffer = NULL;
    size_t size = hour_load(&buffer, hour_number);
    DataHour *result = hour_deserialize(buffer, size);
    free(buffer);
    if (result != NULL) {
        *loaded_size = size;
    }
    return result;
}

bool datahour_save(DataHour *hour)
{
    size_t size = 0;
    uint8_t *buffer = hour_serialize(hour, &size);
    if (buffer == NULL) {
        return false;
    }

    bool result = hour_save(hour->hour_id, buffer, size);

    free(buffer);

    return result;
}

void hour_add(DataHour *hour)
{
    if (list_is_full(data_queue)) {
        DataHour *old = list_pop(data_queue);
        datahour_save(old);
        hour_destroy(old);
    }

    list_push(data_queue, hour);
}

DataHour *datahour_get(uint32_t hour_number, bool load, bool create)
{
    DataHour *hour = hour_find(hour_number);
    if (hour != NULL) {
        return hour;
    }

    DataHour *result = NULL;
    bool add = false;

    if (result == NULL && load) {
        size_t loaded_size = 0;
        result = datahour_load(hour_number, &loaded_size);
        if (result != NULL) {
            result->flags = 0;
            add = true;
        }
    }

    if (result == NULL && create) {
        result = hour_create(hour_number);
        add = true;
    }

    if (result != NULL && add) {
        hour_add(result);
    }

    return result;
}

float data_get_val(VariableInfo variable, uint32_t hour_number, uint32_t log_number)
{
    DataHour *hour = datahour_get(hour_number, true, true);
    if (hour == NULL) {
        return VALUE_EMPTY;
    }

    Variable *var = get_variable(hour, variable.id);
    if (var == NULL) {
        return VALUE_EMPTY;
    }

    if (log_number >= var->variable_info.samples_per_hour) {
        return VALUE_EMPTY;
    }

    return var->data[log_number];
}

void data_save(void)
{
    #if ZEJF_DEBUG
    printf("Save all\n");
    #endif
    Node *node = data_queue->head;
    if (node == NULL) {
        return;
    }
    do {
        DataHour *hour = (DataHour *) node->item;
        if ((hour->flags & FLAG_MODIFIED) == 0) {
            goto next;
        }
        if (!datahour_save(hour)) {
            goto next;
        }
        hour->flags = 0;

    next:
        node = node->previous;
    } while (node != data_queue->head);
}

bool data_log(VariableInfo target_variable, uint32_t hour_number, uint32_t sample_number, float value, TIME_TYPE time, bool announce)
{
    DataHour *hour = datahour_get(hour_number, true, true);
    if (hour == NULL) {
        return false;
    }
    Variable *existing_variable = get_variable(hour, target_variable.id);
    if (existing_variable == NULL) {
        if (!hour_add_variable(hour, target_variable)) {
            return false;
        }
        existing_variable = get_variable(hour, target_variable.id);
    }
    if (existing_variable->variable_info.samples_per_hour != target_variable.samples_per_hour) {
        return false;
    }
    if (sample_number >= existing_variable->variable_info.samples_per_hour) {
        return false;
    }
    if (existing_variable->data[sample_number] != VALUE_EMPTY &&
            existing_variable->data[sample_number] != VALUE_NOT_MEASURED &&
            (value == VALUE_EMPTY || value == VALUE_NOT_MEASURED)) {
        return false; // cannot rewrite wrong value
    }
    existing_variable->data[sample_number] = value;
    printf("logged %f hour %d ln %d\n", value, hour_number, sample_number);
    if (announce) {
        network_announce_log(target_variable, hour_number, sample_number, value, time);
    }
    hour->flags = 1;
    return true;
}

int mainTES()
{
    DataHour *hour = hour_create(12345);
    printf("%d\n", hour->variable_count);

    VariableInfo TEMP = {
        .id = 42,
        .samples_per_hour = (60 * 12)
    };

    hour_add_variable(hour, TEMP);
    printf("%d\n", hour->variable_count);
    //printf("%f\n", hour->variables->data[0]);

    size_t size = 0;
    uint8_t *data = hour_serialize(hour, &size);
    printf("size is %ld vs %ld\n", size, sizeof(DataHour));
    printf("old hour has id %d\n", hour->hour_id);

    /*for(size_t i = 0; i < size; i++){
        printf("%x ", data[i]);
    }*/
    printf("\n");

    hour_destroy(hour);

    DataHour *new_hour = hour_deserialize(data, size);
    free(data);

    if (new_hour != NULL) {
        printf("new hour has id %d", new_hour->hour_id);
    }

    hour_destroy(new_hour);
    return 0;
}
