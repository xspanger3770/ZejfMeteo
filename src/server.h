#ifndef _SERVER_H
#define _SERVER_H

#include "zejf_meteo.h"
#include "stdint.h"
#include "linked_list.h"

extern LinkedList* clients;

typedef struct client_t {
    int uid;
    int fd;
    int64_t last_seen;
} Client;

void server_init(Settings* settings);

void server_destroy(void);

#endif