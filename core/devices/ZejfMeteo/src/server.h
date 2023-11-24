#ifndef _SERVER_H
#define _SERVER_H

#include "linked_list.h"
#include "stdint.h"
#include "zejf_defs.h"
#include "zejf_meteo.h"

#define CLIENT_BUFFER_SIZE 512

extern LinkedList *clients;
extern volatile bool server_running;
extern pthread_rwlock_t clients_lock;

typedef struct client_t
{
    int uid;
    int fd;
    int64_t last_seen;
    Interface interface;
    char buffer[CLIENT_BUFFER_SIZE];
    int buffer_ptr;
    int buffer_parse_ptr;
} Client;

void server_init(Settings *settings);

void server_destroy(void);

#endif