#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "data.h"

pthread_mutex_t data_mutex;

ZejfDay* zejf_day_create(uint32_t day_number){
    ZejfDay* day = malloc(sizeof(ZejfDay));
    if(day == NULL){
        perror("malloc");
        return NULL;
    }

    day->day_number = day_number;
    for(int i = 0; i < LOGS_PER_DAY; i++){
        ZejfLog* log = &day->logs[i];
        log->humidity = UNKNOWN;
        log->pressure = UNKNOWN;
        log->rr = UNKNOWN;
        log->temperature = UNKNOWN;
        log->wdir = UNKNOWN;
        log->wspd = UNKNOWN;
    }

    return day;
}

int mkpath(char *file_path, mode_t mode)
{
    for (char *p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        if (mkdir(file_path, mode) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
}

ZejfDay *zejf_day_load(FILE *file) {
    if (file == NULL) {
        return NULL;
    }
    ZejfDay* day = malloc(sizeof(ZejfDay));
    if(day == NULL){
        perror("malloc");
        return NULL;
    }

    if (fread(day, sizeof(ZejfDay), 1, file) != 1) {
        if(errno != 0){
            perror("fread");
        } else {
            fprintf(stderr, "File corrupted\n");
        }
        free(day);
        return NULL;
    }

    return day;
}

void str_append(char* buff, char** end_ptr, char* str){
    size_t len = strlen(str);
    memcpy(*end_ptr, str, len);
    *end_ptr += len;
}

char months[12][10] = { "January\0", "February\0", "March\0", "April\0", "May\0", "June\0", 
                        "July\0", "August\0", "September\0", "October\0", "November\0", "December\0" };

void zejf_day_path(char* buff, uint32_t day_number)
{
    time_t now = day_number * 24l * 60 * 60;
    struct tm *t = localtime(&now);

    char time_buff[32];

    char* end_ptr = buff;
    str_append(buff, &end_ptr, MAIN_FOLDER);
    str_append(buff, &end_ptr, "data/");
    
    strftime(time_buff, sizeof(time_buff) - 1, "%Y/", t);

    str_append(buff, &end_ptr, time_buff);
    str_append(buff, &end_ptr, months[t->tm_mon]);

    strftime(time_buff, sizeof(time_buff) - 1, "/%d.dat", t);
    str_append(buff, &end_ptr, time_buff);

    *end_ptr = '\0';
}
bool zejf_day_save(ZejfDay* zejf_day){
    if(zejf_day == NULL){
        return false;
    }

    char path_buff[_PC_PATH_MAX];
    zejf_day_path(path_buff, zejf_day->day_number);

    char path_only[_PC_PATH_MAX];
    strcpy(path_only, path_buff);

    memset(strrchr(path_only, '/') + 1, '\0', 1);

    struct stat st = { 0 };

    if (stat(path_only, &st) == -1) {
        printf("Creating path %s\n", path_only);
        if (mkpath(path_only, 0700) != 0) {
            perror("mkdir");
            return false;
        }
    }

    FILE *actual_file = fopen(path_buff, "wb");
    if (actual_file == NULL) {
        perror("fopen");
        return false;
    }

    printf("Saving to %s\n", path_buff);

    bool result;
    if((result = (fwrite(zejf_day, sizeof(ZejfDay), 1, actual_file) == 1))){
        //dh->modified = false;
    }

    fclose(actual_file);

    return result;
}


ZejfDay* zejf_day_get(uint32_t day_number, bool load, bool create_new){
    
}

void zejf_day_destroy(ZejfDay* zejf_day){
    if(zejf_day == NULL){
        return;
    }

    free(zejf_day);
}

void zejf_day_destructor(void **ptr)
{
    if (ptr == NULL) {
        return;
    }
    ZejfDay *data = *((ZejfDay **) ptr);
    zejf_day_destroy(data);
}

void data_init(){
    pthread_mutex_init(&data_mutex, NULL);
}

void data_destroy(){
    pthread_mutex_destroy(&data_mutex);
}