#ifndef _ZEJF_METEO_T
#define _ZEJF_METEO_T

#include <pthread.h>

typedef struct settings_t
{
    char *ip;
    int tcp_port;
    char *serial;
} Settings;

extern Settings settings;

extern pthread_mutex_t zejf_lock;

void meteo_start(Settings *settings);

#endif