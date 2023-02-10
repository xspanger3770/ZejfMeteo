#define _GNU_SOURCE

// C library headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

// Linux headers
#include <errno.h> // Error integer and strerror() function
#include <fcntl.h> // Contains file controls like O_RDWR
#include <stdint.h>
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h>  // write(), read(), close()

#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <inttypes.h>

#include "zejf_api.h"

#include "serial.h"
#include "time_utils.h"
#include "zejf_meteo.h"

#define BUFFER_SIZE 1024
#define LINE_BUFFER_SIZE 128

pthread_t time_check_thread;
pthread_t rip_thread;
pthread_t serial_reader_thread;
pthread_t packet_sender_thread;

volatile bool serial_running = false;
volatile bool time_threads_running = false;

Interface usb_interface_1 = {
    .uid = 1,
    .handle = 0,
    .type = USB
};

Interface* all_interfaces[] = {&usb_interface_1};

void network_process_packet(Packet* packet){
    
}

void get_all_interfaces(Interface*** interfaces, int* length){
    *interfaces = all_interfaces;
    *length = 1;
}

void network_send_via(char* msg, int length, Interface* interface){
    switch(interface->type){
        case USB:
            if(!write(interface->handle, msg, length) || !write(usb_interface_1.handle, "\n", 1)){
                perror("write");
            }else{
                printf("%s\n", msg);
            }
            break;
        default:
            printf("Unknown interaface: %d\n", interface->type);
    }
}

bool time_check(int port_fd){
    char msg[PACKET_MAX_LENGTH];

    int64_t seconds = current_seconds();
    if(snprintf(msg, 32, "%"SCNd64, seconds) < 0){
        return false;
    }

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_mutex_lock(&zejf_lock);
    
    Packet* packet = network_prepare_packet(BROADCAST, TIME_CHECK, msg);
    if(packet == NULL){
        pthread_mutex_unlock(&zejf_lock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        return false;
    }

    
    if(!network_send_packet(packet, current_millis())){
        pthread_mutex_unlock(&zejf_lock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        return false;
    }
    
    pthread_mutex_unlock(&zejf_lock);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);


    return true;
}

void process_packet(Packet* pack){
    switch(pack->command){
        case MESSAGE:
            printf("Message from uC: [%s]\n", pack->message);
            break;
        default:
            printf("Weird packet, command=%d\n", pack->command);
    }

    free(pack->message);
}

void* time_check_start(void *ptr){
    int fd = *((int*)ptr);
    sleep(3);
    while(true){
        if(!time_check(fd)){
            printf("time check fail\n");
        }
        sleep(120);
    }
}

void* rip_thread_start(void* fd){
    while(true){
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&zejf_lock);
        
        network_send_routing_info();
        routing_table_check(current_millis());
        
        pthread_mutex_unlock(&zejf_lock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        sleep(10);
    }
}


void* packet_sender_start(void *fd){
    sleep(1);
    while(true){
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&zejf_lock);
        
        network_send_all(current_millis());
        
        pthread_mutex_unlock(&zejf_lock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        
        usleep(1000 * 5);
    }
}

void run_reader(int port_fd, char* serial)
{
    usb_interface_1.handle = port_fd;
    printf("waiting for serial device\n");
    sleep(2);

    time_threads_running = true;
    pthread_create(&time_check_thread, NULL, &time_check_start, &port_fd);
    pthread_create(&rip_thread, NULL, &rip_thread_start, &port_fd);
    pthread_create(&packet_sender_thread, NULL, &packet_sender_start, &port_fd);

    printf("serial port running fd %d\n", port_fd);

    char buffer[BUFFER_SIZE] = {0};
    char line_buffer[LINE_BUFFER_SIZE] = {0};
    int line_buffer_ptr = 0;

    struct stat stats;
    
    while(true){
        ssize_t count = read(port_fd, buffer, BUFFER_SIZE);
        if(count <= 0){
            if(stat(serial, &stats) == -1){
                printf("Serial reader end, count %ld\n", count);
                break;
            }
        }
        for(ssize_t i = 0; i < count; i++){
            line_buffer[line_buffer_ptr] = buffer[i];
            if(buffer[i] == '\n'){
                line_buffer[line_buffer_ptr-1] = '\0';
                printf("            %s\n", line_buffer);
                if(line_buffer[0] == '{'){
                    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
                    pthread_mutex_lock(&zejf_lock);
                    network_accept(line_buffer, line_buffer_ptr - 1, &usb_interface_1, current_millis());
                    pthread_mutex_unlock(&zejf_lock);
                    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                }
                line_buffer_ptr = 0;
                continue;
            }
            
            line_buffer_ptr++;
            if(line_buffer_ptr == LINE_BUFFER_SIZE - 1){
                printf("ERR SERIAL READER BUFF OVERFLOW\n");
                line_buffer_ptr = 0;
            }
        }
        
    }

    close(port_fd);
    
    time_threads_running = false;
    pthread_cancel(time_check_thread);
    pthread_join(time_check_thread, NULL);
    pthread_cancel(rip_thread);
    pthread_join(rip_thread, NULL);
    pthread_cancel(packet_sender_thread);
    pthread_join(packet_sender_thread, NULL);
    printf("serial reader thread finish\n");
}

void open_serial(char* serial){
    printf("trying to open port %s\n", serial);
    // Open the serial port. Change device path as needed (currently set to an
    // standard FTDI USB-UART cable type device)
    int port_fd = open(serial, O_RDWR);
    
    if(port_fd == -1){
        perror("open");
        return;
    }

    // Create new termios struct, we call it 'tty' for convention
    struct termios tty;

    // Read in existing settings, and handle any error
    if (tcgetattr(port_fd, &tty) != 0) {
        printf("error %i from tcgetattr: %s\n", errno, strerror(errno));
        close(port_fd);
        return;
    }

    tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
    tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in
                            // communication (most common)
    tty.c_cflag &= ~CSIZE;  // Clear all bits that set the data size
    tty.c_cflag |= CS8;     // 8 bits per byte (most common)
    tty.c_cflag &=
            ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
    tty.c_cflag |=
            CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;                   // Disable echo
    tty.c_lflag &= ~ECHOE;                  // Disable erasure
    tty.c_lflag &= ~ECHONL;                 // Disable new-line echo
    tty.c_lflag &= ~ISIG;                   // Disable interpretation of INTR, QUIT and SUSP
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
            ICRNL); // Disable any special handling of received bytes

    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g.
                           // newline chars)
    tty.c_oflag &=
            ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    // tty.c_oflag &= ~OXTABS; // Prevent conversion of tabs to spaces (NOT
    // PRESENT ON LINUX) tty.c_oflag &= ~ONOEOT; // Prevent removal of C-d chars
    // (0x004) in output (NOT PRESENT ON LINUX)

    tty.c_cc[VTIME] = 100; // Wait for up to 1s (10 deciseconds), returning as soon
                          // as any data is received.
    tty.c_cc[VMIN] = 0;

    // Set in/out baud rate to be 38400
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    // Save tty settings, also checking for error
    if (tcsetattr(port_fd, TCSANOW, &tty) != 0) {
        printf("error %i from tcsetattr: %s\n", errno, strerror(errno));
        close(port_fd);
        return;
    }

    run_reader(port_fd, serial);
}

void *start_serial(void* arg){
    while(true){
        open_serial((char*)arg);
        printf("Next attempt in 5s\n");
        sleep(5);
    }
}

void run_serial(Settings* settings){
    pthread_create(&serial_reader_thread, NULL, &start_serial, settings->serial);
}

void stop_serial() {
    pthread_cancel(serial_reader_thread);
    pthread_join(serial_reader_thread, NULL);
    if(time_threads_running){
        pthread_cancel(time_check_thread);
        pthread_join(time_check_thread, NULL);
        pthread_cancel(rip_thread);
        pthread_join(rip_thread, NULL);
        pthread_cancel(packet_sender_thread);
        pthread_join(packet_sender_thread, NULL);
        time_threads_running = false;
    }
}