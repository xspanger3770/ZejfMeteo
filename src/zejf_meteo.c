#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>

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

void print_info(Settings* settings) {
    printf("\n==== ZejfMeteo ====\n");
    printf("Serial port: %s\n", settings->serial);
    printf("Server ip: %s\n", settings->ip);
    printf("Server port: %d\n\n", settings->tcp_port);

    printf("Serial port running: %d\n", serial_running);
    printf("Server running: %d\n\n", server_running);

    pthread_mutex_lock(&zejf_lock);

    printf("Interfaces: %ld/%d\n", interface_count, INTERFACES_MAX);
    for(size_t i = 0; i < interface_count; i++){
        Interface* interace = all_interfaces[i];
        printf("    Interface #%d\n", interace->uid);
        printf("        handle: %d\n", interace->handle);
        printf("        rx_id: %"SCNu32"\n", interace->rx_id);
        printf("        tx_id: %"SCNu32"\n", interace->tx_id);
        printf("        type: %d\n", interace->type);
    }

    pthread_mutex_unlock(&zejf_lock);

    pthread_rwlock_rdlock(&clients_lock);

    printf("\nClients: %ld/%ld\n", clients->item_count, clients->capacity);
    Node* node = clients->tail;
    while(node != NULL){
        Client* client = (Client*)node->item;
        
        printf("    Client #%d\n", client->uid);
        printf("        last_seen: %"SCNd64" ms ago\n", (current_millis() - client->last_seen));
        printf("        fd: %d\n", client->fd);
        printf("        interface_uid: %d\n", client->interface.uid);

        if(node == clients->head){
            break;
        }
        node = node->next;
    }

    printf("\n");

    pthread_rwlock_unlock(&clients_lock);

    pthread_mutex_lock(&zejf_lock);
    print_routing_table(current_millis());
    pthread_mutex_unlock(&zejf_lock);

    printf("===================\n");
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

bool process_command(char *cmd, int argc, char **argv, Settings* settings)
{
    if (strcmp(cmd, "exit") == 0) {
        return true;
    }

    if(strcmp(cmd, "status") == 0){
        if (argc < 1) {
            printf("usage: status [device_id]\n");
            return false;
        }

        long device_id = 0;
        if (!parse_int(argv[1], &device_id)) {
            printf("cannot parse device id\n");
            return false;
        }

        pthread_mutex_lock(&zejf_lock);
        Packet* packet = network_prepare_packet(device_id, STATUS_REQUEST, NULL);
        int rv = network_send_packet(packet, current_millis());
        pthread_mutex_unlock(&zejf_lock);
        if(rv == 0){
            printf("Status request sent.\n");
        } else{
            printf("Failed to send status request with error code %d.\n", rv);
        }
        return false;
    }

    if(strcmp(cmd, "info") == 0){
        print_info(settings);
        return false;
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
            printf("cannot parse input\n");
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

void command_line(Settings* settings)
{
    ZEJF_LOG(0, "command line active\n");
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

        bool result = process_command(argv[0], argc, argv, settings);
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

    command_line(settings);

    meteo_stop();
}
