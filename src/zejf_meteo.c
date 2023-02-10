#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "serial.h"
#include "zejf_meteo.h"
#include "zejf_api.h"

pthread_mutex_t zejf_lock;

bool process_command(char *line)
{
    if (strcmp(line, "exit\n") == 0) {
        return true;
    } else {
        printf("unknown action: %s", line);
    }
    return false;
}

void command_line()
{
    printf("command line active\n");
    size_t len = 0;
    ssize_t lineSize = 0;
    while (true) {
        char *line = NULL;
        lineSize = getline(&line, &len, stdin);
        if (lineSize < 0) {
            free(line);
            break;
        }
        bool result = process_command(line);
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