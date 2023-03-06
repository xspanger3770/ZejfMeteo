#include "server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <stdbool.h>

#include <pthread.h>

static pthread_t server_thread;
volatile bool server_running = false;

void client_connect(int client_fd){
    printf("CLIENT\n");
}

void* server_run(void* arg){
    Settings* settings = (Settings*) arg;

    printf("starting server... %s:%d\n", settings->ip, settings->tcp_port);

    int opt = 1;
    int server_fd;
    
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket");
        server_running = false;
        pthread_exit(0);
    }

    // Forcefully attaching socket to the port
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        server_running = false;
        pthread_exit(0);
    }

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(settings->ip);
    address.sin_port = htons(settings->tcp_port);

    // Forcefully attaching socket to the port 8080
    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        perror("bind");
        server_running = false;
        pthread_exit(0);
    }

    printf("Server is open\n");

    while(true){
        if (listen(server_fd, 3) < 0) {
            perror("listen");
            server_running = false;
            pthread_exit(0);
        }
        int client_fd = -1;
        printf("accept\n");
        if ((client_fd = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
            perror("accept");
            server_running = false;
            pthread_exit(0);
        }
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        client_connect(client_fd);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    server_running = false;
    pthread_exit(0);
    return NULL;
}

void server_init(Settings* settings) {
    server_running = true;
    pthread_create(&server_thread, NULL, server_run, settings);
}

void server_destroy(void){
    if(server_running){
        pthread_cancel(server_thread);
        pthread_join(server_thread, NULL);
    }
}
