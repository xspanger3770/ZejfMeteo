#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_routing.h"
#include "zejf_network.h"
#include "zejf_protocol.h"
#include "data_info.h"
#include "linked_list.h"

typedef LinkedList Queue;

Queue* day_queue;

void* day_destroy(void* day);
Day* day_create(uint32_t day_number);

void data_init(void){
    day_queue = list_create(DAY_BUFFER_SIZE);
}

void data_destroy(void){
    list_destroy(day_queue, day_destroy);
}

void buffer_add(Day* day){
    if(list_is_full(day_queue)){
        Day* old = list_pop(day_queue);
        day_save(old);
        day_destroy(day);
    }

    list_push(day_queue, day);
}

Day** day_find(uint32_t day_number){
    Node* node = day_queue->head;
    if(node == NULL){
        return NULL;
    }
    do{
        Day** day = (Day**)&node->item;
        if((*day)->day_number == day_number){
            list_prioritise(day_queue, node);
            return (Day**)&node->item;
        }
        node=node->previous;
    }while(node != day_queue->head);

    return NULL;
}

void calculate_offsets(Day* day){
    day->variables = (Variable*)day->data;
    // calculate all offsets
    size_t temp = 0;
    for(uint16_t i = 0; i < day->variable_count; i++){
        Variable *var = &day->variables[i];
        var->_start = (float*)&day->data[day->variable_count * sizeof(Variable) + temp * sizeof(float)];
        temp += var->info.samples_per_day;
    }
}

Day** day_get(uint32_t day_number, bool load, bool create){
    Day** day = day_find(day_number);
    if(day != NULL){
        return day;
    }

    Day* result = NULL;
    bool add = false;

    if(result == NULL && load){
        size_t loaded_size = day_load(&result, day_number, DAY_MAX_SIZE);
        if(result != NULL){
            result->total_size = loaded_size;
            result->modified = false;
            calculate_offsets(result);
        }
        add = true;
    }

    if(result == NULL && create){
        result = day_create(day_number);
        add = true;
    }

    if(result != NULL && add){
        buffer_add(result);
    }
    
    return day_find(day_number);
}

Day* day_create(uint32_t day_number){
    size_t total_size = sizeof(Day);
    Day* day = (Day*)calloc(1, total_size);
    if(day == NULL){
        return NULL;
    }

    day->total_size = total_size;
    day->day_number = day_number;
    day->variable_count = 0;
    day->variables = NULL;
    day->modified = false;

    return day;
}

Variable* get_variable(Day* day, uint16_t variable_id)
{
    for(int i = 0; i < day->variable_count; i++){
        Variable* var = &day->variables[i];
        if(var->info.id == variable_id){
            return &day->variables[i];
        }
    }
    return NULL;
}

bool day_add_variable(Day** day, VariableInfo new_variable){
    // calculate new size
    size_t new_size = sizeof(Day);
    new_size += ((*day)->variable_count + 1) * sizeof(Variable);
    for(uint16_t i = 0; i < (*day)->variable_count; i++){
        new_size += (*day)->variables[i].info.samples_per_day * sizeof(float);
    }
    new_size += new_variable.samples_per_day * sizeof(float);

    // alloc new day
    
    Day* new_day = calloc(1, new_size);
    if(new_day == NULL){
        return false;
    }

    // fill-in fields
    new_day->day_number = (*day)->day_number;
    new_day->total_size = new_size;
    new_day->variable_count = (*day)->variable_count + 1;
    new_day->variables = (Variable*)new_day->data;
    new_day->modified = false;

    // copy old variables
    for(uint16_t i = 0; i < (*day)->variable_count; i++){
        new_day->variables[i] = (*day)->variables[i];
    }

    // add new variable
    new_day->variables[new_day->variable_count - 1].info = new_variable;
    new_day->variables[new_day->variable_count - 1]._start = NULL;
    
    calculate_offsets(new_day);
    
    // copy old data
    for(uint16_t i = 0; i < (*day)->variable_count; i++){
        Variable old_var = (*day)->variables[i];
        Variable new_var = new_day->variables[i];
        for(uint32_t j = 0; j < old_var.info.samples_per_day; j++){
            new_var._start[j] = old_var._start[j];
        }
    }
    

    // set all to -99
    Variable new_var = new_day->variables[new_day->variable_count - 1];
    for(uint32_t j = 0; j < new_variable.samples_per_day; j++){
        new_var._start[j] = VALUE_EMPTY;
    }

    free(*day);
    *day = new_day;
    return true;
}

bool data_log(VariableInfo target_variable, uint32_t day_number, uint32_t sample_num, float val, TIME_TYPE time, bool announce){
    Day** day = day_get(day_number, true, true);
    if(day == NULL){
        return false;
    }
    Variable* existing_variable = get_variable(*day, target_variable.id);
    if(existing_variable == NULL){
        if(!day_add_variable(day, target_variable)){
            return false;
        }
        existing_variable = get_variable(*day, target_variable.id);
    }
    if(existing_variable->info.samples_per_day != target_variable.samples_per_day){
        return false;
    }
    if(sample_num >= existing_variable->info.samples_per_day){
        return false;
    }
    existing_variable->_start[sample_num] = val;
    printf("logged %f day %d ln %d\n", val, day_number, sample_num);
    if(announce){
        network_announce_log(target_variable, day_number, sample_num, val, time);
    }
    (*day)->modified = true;
    return true;
}

float* data_pointer(uint32_t day_number, VariableInfo target_variable){
    Day** day = day_get(day_number, true, true);
    if(day == NULL){
        return NULL;
    }

    Variable* var = get_variable(*day, target_variable.id);
    if(var == NULL){
        return NULL;
    }

    return var->_start;
}

float data_get_val(VariableInfo variable, uint32_t day_number, uint32_t log_number){
    Day** day = day_get(day_number, true, true);
    if(day == NULL){
        return VALUE_EMPTY;
    }

    Variable* var = get_variable(*day, variable.id);
    if(var == NULL){
        return VALUE_EMPTY;
    }

    if(log_number >= var->info.samples_per_day){
        return VALUE_EMPTY;
    }

    return var->_start[log_number];
}

void data_save(void){
    printf("SAVE\n");
    Node* node = day_queue->head;
    if(node == NULL){
        return;
    }
    do{
        Day* day = (Day*)node->item;
        if(!day->modified){
            goto next;
        }
        if(!day_save(day)){
            goto next;
        }
        day->modified = false;

        next:
        node=node->previous;
    }while(node != day_queue->head);
}

void* day_destroy(void* day){
    free(day);
    return NULL;
}