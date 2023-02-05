#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ONE_PER_MINUTE (60 * 24)

typedef struct variable_t{
    size_t samples_per_day;
    float* _start;
} Variable;

typedef struct dynamic_day_t{
    size_t total_size;
    uint32_t day_number;
    Variable variables[256];
    float data[];
} Day;

Day* day_create(uint32_t day_number){
    size_t total_size = sizeof(Day);
    Day* day = (Day*)malloc(total_size);
    if(day == NULL){
        return NULL;
    }

    day->total_size = total_size;
    day->day_number = day_number;
    for(uint8_t i = 0; i != 255; i++){
        Variable* var = &day->variables[i];
        var->samples_per_day = 0;
    }

    return day;
}

bool var_is_valid(Variable* var){
    return var != NULL && var->samples_per_day > 0;
}

Variable* get_variable(Day* day, uint8_t variable_id)
{
    return &day->variables[variable_id];
}

bool day_add_variable(Day** day, Variable new_variable){
    Variable* existing_variable = get_variable(*day, new_variable.id);
    existing_variable->samples_per_day = new_variable.samples_per_day;
    existing_variable->id = new_variable.id;
    return true;
}

bool day_log(Day** day, uint8_t, size_t sample_num, float val){
    Variable* existing_variable = get_variable(*day, target_variable.id);
    if(var_is_valid(existing_variable)){
        if(!day_add_variable(day, target_variable)){
            return false;
        }
    }
    return true;
}

void day_destroy(Day* day){
    if(day == NULL){
        return;
    }
    free(day);
}

int main(void)
{
    Day* day = day_create(123456);
    if(day == NULL) {
        return 1;
    }

    printf("%ld / %ld / %ld\n", sizeof(Day), sizeof(day), day->total_size);
    return 0;
}
