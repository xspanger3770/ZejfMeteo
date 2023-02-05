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

#include "serial.h"
#include "time_utils.h"

#define BUFFER_SIZE 1024
#define LINE_BUFFER_SIZE 128

pthread_t time_check_thread;

bool time_check(int port_fd){
    char msg[64];

    printf("c %ld\n", current_seconds());
    snprintf(msg, 64, "time %ld\n", current_seconds());
    
    if(write(port_fd, msg, strlen(msg)) == -1){
        perror("write");
        return false;
    }

    return true;
}

void time_check_start(int *fd){
    printf("time check start fd %d\n", *fd);
    while(true){
        time_check(*fd);
        sleep(120);
    }
}

bool decode(char* buffer){
    return false;
}

void run_reader(int port_fd, char* serial)
{
    printf("waiting for serial device\n");
    sleep(2);
    
    pthread_create(&time_check_thread, NULL, (void*)time_check_start, &port_fd);

    printf("serial port running fd %d\n", port_fd);

    char buffer[BUFFER_SIZE];
    char line_buffer[LINE_BUFFER_SIZE];
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
            line_buffer_ptr++;
            if(buffer[i] == '\n'){
                line_buffer[line_buffer_ptr-1] = '\0';
                pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
                if(!decode(line_buffer)){
                    printf("Arduino: %s\n", line_buffer);
                }
                pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                line_buffer_ptr = 0;
            }
            if(line_buffer_ptr == LINE_BUFFER_SIZE - 1){
                printf("ERR SERIAL READER BUFF OVERFLOW\n");
                line_buffer_ptr = 0;
            }
        }
        
    }

    close(port_fd);
    
    pthread_cancel(time_check_thread);
    pthread_join(time_check_thread, NULL);
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

void run_serial(char* serial){
    while(true){
        open_serial(serial);
        printf("Next attempt in 5s\n");
        sleep(5);
    }
}