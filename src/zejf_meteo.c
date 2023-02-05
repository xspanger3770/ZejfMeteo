#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "serial.h"
#include "zejf_meteo.h"
#include "dynamic_data.h"

pthread_t serial_thread;

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
    pthread_cancel(serial_thread);
    pthread_join(serial_thread, NULL);

    data_destroy();
}

void meteo_start(Settings* settings){
    data_init();

    pthread_create(&serial_thread, NULL, (void*)run_serial, settings->serial);
    
    command_line();

    meteo_stop();
}