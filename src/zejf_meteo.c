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

bool process_command(char *cmd, int argc, char** argv)
{
    if (strcmp(cmd, "exit\n") == 0) {
        return true;
    } else if (strcmp(cmd, "data\n") == 0) {
        printf("SO you want to see some data..\n");
        return false;
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