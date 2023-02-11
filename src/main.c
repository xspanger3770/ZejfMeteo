#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "serial.h"

#include "zejf_meteo.h"
#include "zejf_api.h"

void print_usage(void)
{
    printf("Usage: -s <serial port> -i <ip address> -p <port number>\n");
}

int main2(){
    uint32_t val = UINT32_MAX;
    printf("%"SCNu32"\n", val);
    uint32_t val2 = val+10;
    printf("%"SCNu32"\n", val2);
    printf("%"SCNu32"\n", (val2-val));
    printf("packet=%ldb\n", sizeof(Packet));
    printf("routing=%ldb\n", sizeof(RoutingEntry));

    int i = 0;
    int A = 2 << 0;
    int B = 2 << 1;

    i |= B;

    printf("%d\n", i);
    printf("%d\n", (i | B));
    printf("%d\n", (i | A));

    return 0;
}

int main(int argc, char *argv[]){
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

    printf("Starting with serial port %s, ip %s:%d\n", serial, ip, port);

    Settings settings;
    settings.ip = ip;
    settings.tcp_port = port;
    settings.serial = serial;

    meteo_start(&settings);

    return 0;
}