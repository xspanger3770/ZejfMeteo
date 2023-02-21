#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "serial.h"
#include "zejf_meteo.h"
#include "zejf_api.h"
#include "errno.h"
#include "time_utils.h"

pthread_mutex_t zejf_lock;

void display_data(uint16_t variable, uint32_t hour_id) {
    DataHour* hour = datahour_get(hour_id, true, false);
    if(hour == NULL){
        printf("Hour %d doesn´t exist\n", hour_id);
        return;
    }

    Variable* var = get_variable(hour, variable);
    if(var == NULL){
        printf("Hour %d doesn´t contain variable %.4x\n", hour_id, variable);
    }

    for(uint32_t log = 0; log < var->variable_info.samples_per_hour; log++){
        printf("[%d] %f\n", log, var->data[log]);
    }
}

bool parse_int(char* str, long* result){
    char* endptr = NULL;
    errno = 0;
    long val = strtol(str, &endptr, 10);
    if(errno != 0 || endptr == str){
        return false;
    }
    *result = val;
    return true;
}

bool process_command(char *cmd, int argc, char** argv)
{
    if (strcmp(cmd, "exit") == 0) {
        return true;
    } else if (strcmp(cmd, "data") == 0) {
        printf("SO you want to see some data..\n");
        if(argc < 2){
            printf("usage: data [variable] [hour_id]\n");
            return false;
        }

        long variable = 0;
        long hour_id = 0;
        if(!parse_int(argv[1], &variable) || !parse_int(argv[2], &hour_id)){
            printf("cannot parse variable\n");
            return false;
        }

        if(hour_id <= 0){
            hour_id += current_hours();
        }

        printf("Display variable 0x%.4lx hour %ld\n", variable, hour_id);

        display_data(variable, hour_id);

        return false;
    } else if (strcmp(cmd, "dummy") == 0) {
        printf("Insert dummy variable\n");
        VariableInfo DUMMY = {
            .id = 42,
            .samples_per_hour = 60
        };

        data_log(DUMMY, current_hours(), 69, 42, 0, false);
    } else {
        printf("unknown action: %s argc %d\n", cmd, argc);
    }
    return false;
}

void command_line()
{
    printf("command line active\n");
    size_t len = 0;
    ssize_t lineSize = 0;

    int argc = 0;
    char* argv[5];
    while (true) {
        char *line = NULL;
        lineSize = getline(&line, &len, stdin);
        if (lineSize < 0) {
            free(line);
            break;
        }

        argc = 0;
        argv[0] = line;

        for(ssize_t i = 0; i < lineSize; i++){
            char ch = line[i];
            if(ch == ' ' || ch == '\n'){
                line[i] = '\0';
                if(i < lineSize - 1){
                    argv[++argc] = &line[i + 1];
                }
                if(argc == 5){
                    break;
                }
            }
        }

        bool result = process_command(argv[0], argc, argv);
        free(line);
        if (result) {
            break;
        }
    }
}

void meteo_stop(){
    stop_serial();

    zejf_destroy();

    pthread_mutex_destroy(&zejf_lock);
}

void meteo_start(Settings* settings){
    pthread_mutex_init(&zejf_lock, NULL);

    zejf_init();

    run_serial(settings);
    
    command_line();

    meteo_stop();
}