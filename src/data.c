#include <pthread.h>

#include "data.h"

pthread_mutex_t data_mutex;

void data_init(){
    pthread_mutex_init(&data_mutex, NULL);
}

void data_destroy(){
    pthread_mutex_destroy(&data_mutex);
}