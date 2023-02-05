#include "dynamic_data.h"
#include <stdlib.h>
#include <stdio.h>

size_t day_load(Day** day, uint32_t day_number, size_t max_size){
    Day* ptr = malloc(sizeof(Day));
    if(ptr == NULL){
        return 0;
    }
    ptr->total_size = sizeof(Day);
    ptr->day_number=day_number;
    ptr->modified = false;
    ptr->variable_count = 0;
    (*day) = ptr;
    return ptr->total_size;
}

bool day_save(Day* day){
    printf("SAVE %d %ld\n", day->day_number, day->total_size);
    return true;
}