#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "serial.h"

#include "time_utils.h"
#include "zejf_api.h"
#include "zejf_meteo.h"

void print_usage(void)
{
    printf("Usage: -s <serial port> -i <ip address> -p <port number>\n");
}

int main(int argc, char *argv[])
{
    char *serial = "/dev/ttyUSB0";
    char *ip = "0.0.0.0";
    int port = 1955;
    static struct option long_options[] = {
        { "serial", required_argument, 0, 's' },
        { "ip", required_argument, 0, 'i' },
        { "port", required_argument, 0, 'p' },
        { 0, 0, 0, 0 }
    };

    int opt = 0;
    int long_index = 0;
    while ((opt = getopt_long(argc, argv, "s:i:p:", long_options, &long_index)) != -1) {
        switch (opt) {
        case 's':
            serial = optarg;
            break;
        case 'i':
            ip = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        default:
            print_usage();
            return EXIT_FAILURE;
        }
    }

    ZEJF_LOG(1, "Starting with serial port %s, ip %s:%d\n", serial, ip, port);

    Settings settings;
    settings.ip = ip;
    settings.tcp_port = port;
    settings.serial = serial;

    meteo_start(&settings);

    return 0;
}
