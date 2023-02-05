#ifndef _ZEJF_METEO_T
#define _ZEJF_METEO_T

typedef struct settings_t{
    char* ip;
    int tcp_port;
    char* serial;
} Settings;

extern Settings settings;

void meteo_start(Settings* settings);

#endif