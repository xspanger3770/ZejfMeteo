#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "errno.h"
#include "interface_manager.h"
#include "serial.h"
#include "server.h"
#include "time_utils.h"
#include "zejf_api.h"
#include "zejf_meteo.h"

#define COMMAND_ARGS_MAX 5
#define DUMMY_ID 42
#define ONE_PER_MINUTE 60
#define BASE_10 10

pthread_mutex_t zejf_lock;

void display_data(uint16_t variable, uint32_t hour_id)
{
    pthread_mutex_lock(&zejf_lock);
    DataHour *hour = datahour_get(hour_id, true, false);
    if (hour == NULL) {
        printf("Hour %d doesn´t exist\n", hour_id);
        pthread_mutex_unlock(&zejf_lock);
        return;
    }

    Variable *var = get_variable(hour, variable);
    if (var == NULL) {
        printf("Hour %d doesn´t contain variable %.4x\n", hour_id, variable);
        pthread_mutex_unlock(&zejf_lock);
        return;
    }

    for (uint32_t log = 0; log < var->variable_info.samples_per_hour; log++) {
        float val = var->data[log];
        printf("[%d] %f\n", log, val);
    }

    pthread_mutex_unlock(&zejf_lock);
}

bool parse_int(char *str, long *result)
{
    char *endptr = NULL;
    errno = 0;
    long val = strtol(str, &endptr, BASE_10);
    if (errno != 0 || endptr == str) {
        return false;
    }
    *result = val;
    return true;
}

bool process_command(char *cmd, int argc, char **argv)
{
    if (strcmp(cmd, "exit") == 0) {
        return true;
    }

    if (strcmp(cmd, "data") == 0) {
        printf("SO you want to see some data..\n");
        if (argc < 2) {
            printf("usage: data [variable] [hour_id]\n");
            return false;
        }

        long variable = 0;
        long hour_id = 0;
        if (!parse_int(argv[1], &variable) || !parse_int(argv[2], &hour_id)) {
            printf("cannot parse variable\n");
            return false;
        }

        if (hour_id <= 0) {
            hour_id += current_hours();
        }

        printf("Display variable 0x%.4lx hour %ld\n", variable, hour_id);

        display_data(variable, hour_id);

        return false;
    }
    if (strcmp(cmd, "dummy") == 0) {
        printf("Inserting dummy variable\n");
        VariableInfo DUMMY = {
            .id = DUMMY_ID,
            .samples_per_hour = ONE_PER_MINUTE
        };

        data_log(DUMMY, current_hours(), 0, DUMMY_ID, 0, false);

        return false;
    }

    printf("unknown action: %s argc %d\n", cmd, argc);
    return false;
}

void command_line()
{
#if !ZEJF_HIDE_PRINTS
    printf("command line active\n");
#endif
    size_t len = 0;
    ssize_t lineSize = 0;

    int argc = 0;
    char *argv[COMMAND_ARGS_MAX];
    while (true) {
        char *line = NULL;
        lineSize = getline(&line, &len, stdin);
        if (lineSize < 0) {
            free(line);
            break;
        }

        argc = 0;
        argv[0] = line;

        for (ssize_t i = 0; i < lineSize; i++) {
            char next_char = line[i];
            if (next_char == ' ' || next_char == '\n') {
                line[i] = '\0';
                if (i < lineSize - 1) {
                    argv[++argc] = &line[i + 1];
                }
                if (argc == COMMAND_ARGS_MAX) {
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

void meteo_stop()
{
    stop_serial();

    zejf_destroy();

    server_destroy();

    interfaces_destroy();

    pthread_mutex_destroy(&zejf_lock);
}

void meteo_start(Settings *settings)
{
    pthread_mutex_init(&zejf_lock, NULL);

    interfaces_init();

    zejf_init();

    server_init(settings);
    
    run_serial(settings);

    command_line();

    meteo_stop();
}
