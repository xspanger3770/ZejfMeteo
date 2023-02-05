#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ONE_PER_MINUTE 10

typedef struct variable_t{
    uint8_t id;
    size_t samples_per_day;
    size_t _start;
} Variable;

struct variable_info_t{
    uint8_t variable_count;
    struct variable_t variables[];
};

typedef struct dynamic_day_t{
    size_t total_size;
    uint32_t day_number;
    struct variable_info_t variables;
    float data[];
} Day;

Day* day_create(uint32_t day_number){
    size_t total_size = sizeof(struct variable_info_t);
    total_size += sizeof(Day);
    Day* day = (Day*)malloc(total_size);
    if(day == NULL){
        return NULL;
    }

    day->total_size = total_size;
    day->day_number=day_number;
    day->variables.variable_count = 0;
    
    return day;
}

Variable* get_variable(Day* day, uint8_t variable_id)
{
    for(int i = 0; i < day->variables.variable_count; i++){
        Variable* var = &day->variables.variables[i];
        if(var->id == variable_id){
            return &day->variables.variables[i];
        }
    }
    return NULL;
}

bool day_add_variable(Day** day, Variable new_variable){
    printf("add var %d\n", new_variable.id);
    
    // calculate new size
    size_t new_size = ((*day)->variables.variable_count + 1) * sizeof(Variable);
    new_size += sizeof(struct variable_info_t);
    new_size += sizeof(Day);
    for(uint8_t i = 0; i < (*day)->variables.variable_count; i++){
        new_size += (*day)->variables.variables[i].samples_per_day * sizeof(float);
    }
    new_size += new_variable.samples_per_day * sizeof(float);

    // alloc new day
    printf("new size %ld; %d\n", new_size, (*day)->variables.variable_count);
    
    Day* new_day = malloc(new_size);
    if(new_day == NULL){
        printf("NULLLL %ld\n", new_size);
        return false;
    }

    // fill in data
    new_day->total_size = new_size;
    new_day->day_number = (*day)->day_number;
    new_day->variables.variable_count = (*day)->variables.variable_count+1;
   
       
    // copy old variables
    for(uint8_t i = 0; i < (*day)->variables.variable_count; i++){
         new_day->variables.variables[i] = (*day)->variables.variables[i];
    }

    // add new variable
    new_day->variables.variables[new_day->variables.variable_count - 1] = new_variable;
    printf("VAR%d [%ld]\n", new_day->variables.variable_count - 1, new_day->variables.variables[new_day->variables.variable_count - 1].samples_per_day);

    // calculate all offsets
    size_t temp = 0;
    for(uint8_t i = 0; i < new_day->variables.variable_count; i++){
        Variable *var = &new_day->variables.variables[i];
        var->_start = temp;
        temp += var->samples_per_day;
        printf("start od %d is %ld, day starts at %p\n", i, var->_start, new_day);
    }
    
    printf("VAR%d [%ld]\n", new_day->variables.variable_count - 1, new_day->variables.variables[new_day->variables.variable_count - 1].samples_per_day);
    // copy old data
    for(uint8_t i = 0; i < (*day)->variables.variable_count; i++){
        Variable old_var = (*day)->variables.variables[i];
        Variable new_var = new_day->variables.variables[i];
        for(size_t j = 0; j < old_var.samples_per_day; j++){
            new_day->data[new_var._start + j] = (*day)->data[old_var._start + j];
        }
    }
    
    printf("VAR%d [%ld]\n", new_day->variables.variable_count - 1, new_day->variables.variables[new_day->variables.variable_count - 1].samples_per_day);
    Variable new_var = new_day->variables.variables[new_day->variables.variable_count - 1];
    printf("variables are at %d\n", (void*)&(new_day->variables.variables[new_day->variables.variable_count - 1].samples_per_day) - (void*)new_day);
     printf("data are at %d\n", (void*)(new_day->data) - (void*)new_day);
    for(size_t j = 0; j < new_variable.samples_per_day; j++){
        printf("write to byte %d\n", (void*)&(new_day->data[new_var._start + j]) - (void*)new_day);
        new_day->data[new_var._start + j] = -99;
    }

    printf("VAR%d [%ld]\n", new_day->variables.variable_count - 1, new_day->variables.variables[new_day->variables.variable_count - 1].samples_per_day);
    free(*day);
    *day = new_day;
    return true;
}

bool day_log(Day** day, Variable target_variable, size_t sample_num, float val){
    Variable* existing_variable = get_variable(*day, target_variable.id);
    if(existing_variable == NULL){
        if(!day_add_variable(day, target_variable)){
            return false;
        }
    }
    return true;
}

void day_destroy(Day* day){
    free(day);
}

int main(){
    Variable var_temp = {
        .id = 0xAA,
        .samples_per_day = ONE_PER_MINUTE
    };
    Variable var_press = {
        .id = 0xBB,
        .samples_per_day = ONE_PER_MINUTE
    };
    Day* day = day_create(12345);
    day_log(&day, var_temp, 0, 420.69);
    
    printf("%ld vs %ld\n", sizeof(day), day->total_size);
    printf("%ld\n", day->variables.variables[0].samples_per_day);

    day_log(&day, var_press, 0, 420.69);
    
    printf("%ld vs %ld\n", sizeof(day), day->total_size);
    printf("%ld\n", day->variables.variables[0].samples_per_day);

    day_log(&day, var_press, 0, 420.69);
    
    printf("%ld vs %ld\n", sizeof(day), day->total_size);
    printf("%ld\n", day->variables.variables[0].samples_per_day);

    day_destroy(day);
    return 0;
}