#ifndef _SERVER_H
#define _SERVER_H

#include "zejf_meteo.h"
#include "stdint.h"
#include "linked_list.h"
#include "zejf_defs.h"

#define CLIENT_BUFFER_SIZE 256

extern LinkedList* clients;

typedef struct client_t {
    int uid;
    int fd;
    int64_t last_seen;
    Interface interface;
    char buffer[CLIENT_BUFFER_SIZE];
    int buffer_ptr;
    int buffer_parse_ptr;
} Client;

void server_init(Settings* settings);

void server_destroy(void);

#endif