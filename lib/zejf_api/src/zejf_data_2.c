#include "zejf_data_2.h"
#include "linked_list.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef LinkedList Queue;

Queue* data_queue;

void* hour_destroy(void* ptr);

void data_2_init(void){
    data_queue = list_create(DAY_BUFFER_SIZE);
}

void data_2_destroy(void){
    list_destroy(data_queue, hour_destroy);
}

DataHour* hour_create(uint32_t hour_id){
    DataHour* hour = calloc(1, sizeof(DataHour));
    if(hour == NULL){
        return NULL;
    }

    hour->hour_id = hour_id;
    hour->flags = 0;
    hour->variable_count = 0;
    hour->variables = NULL;

    return hour;
}

bool hour_add_variable(DataHour* hour, VariableInfo variable){
    if(variable.samples_per_day == 0 || variable.samples_per_day > SAMPLE_RATE_MAX || hour->variable_count == VARIABLES_MAX){
        return false;
    }
    for(uint16_t i = 0; i < hour->variable_count; i++){
        if(hour->variables[i].variable_info.id == variable.id){
            return false;
        }
    }

    void* ptr = realloc(hour->variables, (hour->variable_count + 1) * sizeof(DataVariable));
    if(ptr == NULL){
        return false;
    }

    float* data = malloc(variable.samples_per_day * sizeof(float));
    if(data == NULL){
        return false;
    }

    for(uint32_t i = 0; i < variable.samples_per_day; i++){
        data[i] = VALUE_EMPTY;
    }
   
    DataVariable* new_var = &((DataVariable*)ptr)[hour->variable_count];
    new_var->variable_info.samples_per_day = variable.samples_per_day;
    new_var->variable_info.id = variable.id;
    new_var->data = data;

    hour->variables = ptr;
    hour->variable_count++;

    return true;
}

void* hour_destroy(void* ptr){
    if(ptr == NULL){
        return NULL;
    }

    DataHour* hour = (DataHour*) ptr;

    for(int i = 0; i < hour->variable_count; i++){
        DataVariable variable = hour->variables[i];
        free(variable.data);
    }

    free(hour->variables);
    free(hour);

    return NULL;
}

uint32_t hour_calculate_checksum(DataHour* hour){
    uint32_t result = 0;

    result += hour->flags;
    result += hour->hour_id;
    result += hour->variable_count;

    for(uint16_t i = 0; i  < hour->variable_count; i++){
        DataVariable var = hour->variables[i];
        result += var.variable_info.id;
        result += var.variable_info.samples_per_day;

        for(size_t j = 0; j < var.variable_info.samples_per_day * sizeof(float); j++){
            result += ((uint8_t*) var.data)[j];
        }
    }

    return result;
}

inline void serialize(uint8_t* data, size_t* ptr, void* val, size_t size){
    memcpy(data + *ptr, val, size);
    *ptr += size;
}

uint8_t* hour_serialize(DataHour* hour, size_t* size){
    size_t total_size = DATAHOUR_BYTES;
    for(uint32_t i = 0; i < hour->variable_count; i++){
        total_size += VARIABLE_INFO_BYTES;
        total_size += hour->variables[i].variable_info.samples_per_day * 4;
    }

    uint8_t* data = malloc(total_size);
    if(data == NULL){
        return NULL;
    }

    hour->checksum = hour_calculate_checksum(hour);

    serialize(data, size, &hour->checksum, 4);
    serialize(data, size, &hour->hour_id, 4);
    serialize(data, size, &hour->variable_count, 2);
    serialize(data, size, &hour->flags, 1);

    for(uint32_t i = 0; i < hour->variable_count; i++){
        DataVariable* variable = &hour->variables[i];
        serialize(data, size, &variable->variable_info.samples_per_day, 4);
        serialize(data, size, &variable->variable_info.id, 2);
        serialize(data, size, variable->data, sizeof(float) * variable->variable_info.samples_per_day);
    }

    return data;
}

bool deserialize(void* destination, uint8_t* data, size_t* ptr, size_t size, size_t total_size){
    if(*ptr+size > total_size){
        return false;
    }
  
    memcpy(destination, data + *ptr, size);
    *ptr += size;

    return true;
}

DataHour* hour_deserialize(uint8_t* data, size_t total_size){
    DataHour* hour = hour_create(0);
    if(hour == NULL){
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
    
    for(uint32_t i = 0; i < total_variable_count; i++){
        uint32_t samples_per_hour = 0;
        uint16_t variable_id = 0;
        
        result &= deserialize(&samples_per_hour, data, &ptr, 4, total_size);
        result &= deserialize(&variable_id, data, &ptr, 2, total_size);

        if(!result){
            goto error;
        }

        VariableInfo info = {
            .id = variable_id,
            .samples_per_day = samples_per_hour
        };

        result &= hour_add_variable(hour, info);

        if(!result){
            goto error;
        }

        result &= deserialize(hour->variables[hour->variable_count - 1].data, data, &ptr, sizeof(float) * samples_per_hour, total_size);

        if(!result){
            goto error;
        }
    }

    result &= ptr == total_size; // must use all

    result &= hour_calculate_checksum(hour) == hour->checksum;

    if(!result){
        error: 
        hour_destroy(hour);
        return NULL;
    }

    return hour;
}

int main() {
    DataHour* hour = hour_create(12345);
    printf("%d\n", hour->variable_count);

    VariableInfo TEMP = {
        .id = 42,
        .samples_per_day = (60 * 12)
    };

    hour_add_variable(hour, TEMP);
    printf("%d\n", hour->variable_count);
    //printf("%f\n", hour->variables->data[0]);

    size_t size = 0;
    uint8_t* data = hour_serialize(hour, &size);
    printf("size is %ld vs %ld\n", size, sizeof(DataHour));
    printf("old hour has id %d\n", hour->hour_id);

    /*for(size_t i = 0; i < size; i++){
        printf("%x ", data[i]);
    }*/
    printf("\n");

    hour_destroy(hour);

    DataHour* new_hour = hour_deserialize(data, size);
    free(data);

    if(new_hour != NULL){
        printf("new hour has id %d val %f\n", new_hour->hour_id, new_hour->variables[0].data[0]);
    }
    
    hour_destroy(new_hour);
    return 0;
}
