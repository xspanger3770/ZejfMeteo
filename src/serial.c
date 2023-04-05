// C library headers
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Linux headers
#include <errno.h> // Error integer and strerror() function
#include <fcntl.h> // Contains file controls like O_RDWR
#include <stdint.h>
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h>  // write(), read(), close()

#include <pthread.h>

#include <inttypes.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "zejf_api.h"

#include "interface_manager.h"
#include "serial.h"
#include "time_utils.h"
#include "zejf_meteo.h"

#define BUFFER_SIZE 1024
#define LINE_BUFFER_SIZE 128

pthread_t rip_thread;
pthread_t serial_reader_thread;
pthread_t packet_sender_thread;

volatile bool serial_running = false;

#define UNUSED(x) (void) (x)

bool time_request(uint16_t to)
{
    char msg[PACKET_MAX_LENGTH];

    int64_t seconds = current_seconds();
    if (snprintf(msg, 32, "%" SCNd64, seconds) < 0) {
        return false;
    }

    Packet *packet = network_prepare_packet(to, TIME_CHECK, msg);
    if (packet == NULL) {
        return false;
    }

    if (!network_send_packet(packet, current_millis())) {
        return false;
    }

    return true;
}

void network_process_packet(Packet *packet)
{
    if (packet->command == TIME_REQUEST) {
        time_request(packet->from);
    }
}

int network_send_via(char *msg, int length, Interface *interface, TIME_TYPE time)
{
    UNUSED(length);
    switch (interface->type) {
    case USB: {
        if (interface->handle == STDIN_FILENO) {
            return SEND_SUCCES;
        }
    }
        // intentionaly no break here
    case TCP: {
        char msg2[PACKET_MAX_LENGTH];
        snprintf(msg2, PACKET_MAX_LENGTH, "%s\n", msg);

        if (!write(interface->handle, msg2, strlen(msg2))) {
            perror("write");
        }
        return SEND_SUCCES;
    }
    default:
        ZEJF_DEBUG(1, "Unknown interaface: %d time %d\n", interface->type, time);
        return SEND_UNABLE;
    }
}

void process_packet(Packet *pack)
{
    switch (pack->command) {
    case MESSAGE:
        ZEJF_DEBUG(0, "Message from uC: [%s]\n", pack->message);
        break;
    default:
        ZEJF_DEBUG(0, "Weird packet, command=%d\n", pack->command);
        break;
    }
}

uint16_t demand[] = { ALL_VARIABLES };

void get_provided_variables(uint16_t *provide_count, const VariableInfo **provided_variables)
{
    *provide_count = 0;
    *provided_variables = NULL;
}

void get_demanded_variables(uint16_t *demand_count, uint16_t **demanded_variables)
{
    *demand_count = 1;
    *demanded_variables = demand;
}

void *run_timer()
{
    int count = 0;
    while (true) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&zejf_lock);

        int64_t millis = current_millis();

        if (count % 10 == 0) {
            network_send_routing_info(millis);
            routing_table_check(millis);
            network_send_demand_info(millis);
        }
        if ((count - 5) % 30 == 0 && count >= 5) {
            data_save();
        }

        if (count % (60l * 30l) == 0) {
            run_data_check(current_hours(), millis % HOUR, 1, millis);
        }

        if (count % (6l * 60l * 60l) == 0 || count == 30 || count == 120) {
            run_data_check(current_hours(), millis % HOUR, 36, millis);
        }

        pthread_mutex_unlock(&zejf_lock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        sleep(1);
        count++;
    }
}

void *packet_sender_start()
{
    sleep(5);
    while (true) {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(&zejf_lock);

        network_process_packets(current_millis());

        pthread_mutex_unlock(&zejf_lock);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        usleep(1000 * 1);
    }
}

void run_reader(int port_fd, char *serial)
{
    pthread_mutex_lock(&zejf_lock);
    usb_interface_1.handle = port_fd;
    pthread_mutex_unlock(&zejf_lock);

    ZEJF_DEBUG(0, "waiting for serial device\n");
    sleep(2);

    ZEJF_DEBUG(0, "serial port running fd %d\n", port_fd);

    char buffer[BUFFER_SIZE] = { 0 };
    char line_buffer[LINE_BUFFER_SIZE] = { 0 };
    int line_buffer_ptr = 0;

    struct stat stats;

    while (true) {
        ssize_t count = read(port_fd, buffer, BUFFER_SIZE);
        if (count <= 0) {
            if (stat(serial, &stats) == -1) {
                ZEJF_DEBUG(0, "Serial reader end, count %ld\n", count);
                break;
            }
        }
        for (ssize_t i = 0; i < count; i++) {
            line_buffer[line_buffer_ptr] = buffer[i];
            if (buffer[i] == '\n') {
                line_buffer[line_buffer_ptr - 1] = '\0';
                ZEJF_DEBUG(0, "            %s\n", line_buffer);
                if (line_buffer[0] == '{') {
                    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
                    pthread_mutex_lock(&zejf_lock);
                    int64_t millis = current_millis();
                    network_accept(line_buffer, line_buffer_ptr - 1, &usb_interface_1, millis);
                    pthread_mutex_unlock(&zejf_lock);
                    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
                }
                line_buffer_ptr = 0;
                continue;
            }

            line_buffer_ptr++;
            if (line_buffer_ptr == LINE_BUFFER_SIZE - 1) {
                ZEJF_DEBUG(2, "ERR SERIAL READER BUFF OVERFLOW\n");
                line_buffer_ptr = 0;
            }
        }
    }

    close(port_fd);

    ZEJF_DEBUG(0, "serial reader thread finish\n");
}

void open_serial(char *serial)
{
    ZEJF_DEBUG(0, "trying to open port %s\n", serial);

    // Open the serial port. Change device path as needed (currently set to an
    // standard FTDI USB-UART cable type device)
    int port_fd = open(serial, O_RDWR);

    if (port_fd == -1) {
        ZEJF_DEBUG(1, "Cannot open %s: %s\n", serial, strerror(errno));
        return;
    }

    // Create new termios struct, we call it 'tty' for convention
    struct termios tty;

    // Read in existing settings, and handle any error
    if (tcgetattr(port_fd, &tty) != 0) {
        ZEJF_DEBUG(2, "error %i from tcgetattr: %s\n", errno, strerror(errno));
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
        ZEJF_DEBUG(2, "error %i from tcsetattr: %s\n", errno, strerror(errno));
        close(port_fd);
        return;
    }

    run_reader(port_fd, serial);
}

void *start_serial(void *arg)
{
    while (true) {
        open_serial((char *) arg);
        ZEJF_DEBUG(0, "Next attempt in 5s\n");
        sleep(5);
    }
}

void run_serial(Settings *settings)
{
    pthread_create(&packet_sender_thread, NULL, &packet_sender_start, NULL);
    pthread_create(&serial_reader_thread, NULL, &start_serial, settings->serial);
    pthread_create(&rip_thread, NULL, &run_timer, NULL);
}

void stop_serial()
{
    pthread_cancel(rip_thread);
    pthread_join(rip_thread, NULL);
    pthread_cancel(serial_reader_thread);
    pthread_join(serial_reader_thread, NULL);
    pthread_cancel(packet_sender_thread);
    pthread_join(packet_sender_thread, NULL);
}
