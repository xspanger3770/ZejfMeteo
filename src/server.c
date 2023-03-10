#include "server.h"
#include "time_utils.h"
#include "interface_manager.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

#include <poll.h>

static pthread_t server_thread;
static pthread_t server_watchdog;
static pthread_t server_poll_thread;

volatile bool server_running = false;

LinkedList* clients;
int next_client_uid;

FILE* tmp_file = NULL;
int temp_fd;

Client* client_create(int fd) {
    Client* client = malloc(sizeof(Client));
    if(client == NULL){
        return NULL;
    }

    client->fd = fd;
    client->last_seen = 0;
    client->uid = next_client_uid++;
    Interface* interface = &client->interface;
    interface->handle = fd;
    interface->type = TCP;

    return client;
}

void* client_destroy(void* ptr) {
    free(ptr);
    return NULL; 
}

void client_connect(int client_fd) {
    Client* client = client_create(client_fd);
    if(client == NULL){
        return;
    }

    client->last_seen = current_millis();

    if(!list_push(clients, client)){
        return;
    }

    interface_add(&client->interface);

    printf("CLIENT #%d added\n", client->uid);
}

void client_remove(Node* node)
{
    Client* cl = node->item;
    if(close(cl->fd) != 0){
        perror("close");
    }

    // disconnect
    list_remove(clients, node);

    interface_remove(cl->interface.uid);
    
    printf("Client #%d timeout\n", cl->uid);
    free(cl);
}

void* poll_run(){
    while(true) {
        struct pollfd fds[clients->item_count];
        sleep(1); // todo
    }
    return NULL;
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

    pthread_create(&server_poll_thread, NULL, poll_run, NULL);

    printf("Server is open\n");

    while(true){
        if (listen(server_fd, 3) < 0) {
            perror("listen");
            break;
        }

        int client_fd = -1;
        printf("accept\n");
        if ((client_fd = accept(server_fd, (struct sockaddr *) &address, (socklen_t *) &addrlen)) < 0) {
            perror("accept");
            break;
        }

        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        client_connect(client_fd);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }

    pthread_cancel(server_poll_thread);
    pthread_join(server_poll_thread, NULL);

    server_running = false;
    pthread_exit(0);
    return NULL;
}

void* watchdog_run(void* arg){
    while(true){
        if(!server_running){
            server_running = true;
            pthread_create(&server_thread, NULL, server_run, arg);
        }

        sleep(10);

        int64_t millis = current_millis();

        if(server_running) {
            Node* node = clients->head;
            if(node == NULL){
                continue;
            }
            do{
                Node* tmp = node;
                node = node->next;

                Client* client = tmp->item;
                if(millis - client->last_seen > 10 * 1000){
                    client_remove(tmp);
                    node = clients->head;
                }
            } while(node != NULL && node != clients->head);
        }
    }
    
}

void server_init(Settings* settings) {
    tmp_file = tmpfile();
    if(tmp_file == NULL){
        return;
    }

    
    temp_fd = fileno(tmp_file);
    
    pthread_create(&server_watchdog, NULL, watchdog_run, settings);

    clients = list_create(64);
    next_client_uid = 0;
}

void server_close() {
    if(server_running){
        pthread_cancel(server_poll_thread);
        pthread_join(server_poll_thread, NULL);
        
        pthread_cancel(server_thread);
        pthread_join(server_thread, NULL);
        server_running = false;
    }
}

void server_destroy(void){
    pthread_cancel(server_watchdog);
    pthread_join(server_watchdog, NULL);

    server_close();

    list_destroy(clients, client_destroy);

    if(tmp_file != NULL){
        fclose(tmp_file);
        tmp_file = NULL;
    }
}
