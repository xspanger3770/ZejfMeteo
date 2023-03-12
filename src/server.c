#include "server.h"
#include "interface_manager.h"
#include "time_utils.h"
#include "zejf_api.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#include <poll.h>

static pthread_t server_thread;
static pthread_t server_watchdog;
static pthread_t server_poll_thread;

pthread_rwlock_t clients_lock;

volatile bool server_running = false;

LinkedList *clients;
int next_client_uid;

// [0] for read, [1] for write
int pipe_fds[2];

Client *client_create(int fd)
{
    Client *client = calloc(1, sizeof(Client));
    if (client == NULL) {
        return NULL;
    }

    client->fd = fd;
    client->buffer_ptr = 0;
    client->buffer_parse_ptr = 0;
    client->last_seen = 0;
    client->uid = next_client_uid++;

    Interface *interface = &client->interface;
    interface->handle = fd;
    interface->type = TCP;

    return client;
}

void *client_destroy(void *ptr)
{
    free(ptr);
    return NULL;
}

void client_connect(int client_fd)
{
    Client *client = client_create(client_fd);
    if (client == NULL) {
        return;
    }

    client->last_seen = current_millis();

    pthread_rwlock_wrlock(&clients_lock);
    if (!list_push(clients, client)) {
        pthread_rwlock_unlock(&clients_lock);
        client_destroy(client);
        return;
    }

    pthread_rwlock_unlock(&clients_lock);

    if (!interface_add(&client->interface)) {
        client_destroy(client);
        return;
    }

    char buff = 'a';
    if (write(pipe_fds[1], &buff, 1) == -1) {
        perror("write");
    }

#if !ZEJF_HIDE_PRINTS
    printf("CLIENT #%d added\n", client->uid);
#endif
}

void client_remove(Node *node)
{
    if (node == NULL) {
        return;
    }

    Client *cl = node->item;
    if (close(cl->fd) != 0) {
        perror("close");
    }

    // disconnect

    pthread_rwlock_wrlock(&clients_lock);
    list_remove(clients, node);
    pthread_rwlock_unlock(&clients_lock);

#if !ZEJF_HIDE_PRINTS
    printf("Client #%d timeout\n", cl->uid);
#endif

    pthread_mutex_lock(&zejf_lock);

    interface_remove(cl->interface.uid);
    free(cl);

    pthread_mutex_unlock(&zejf_lock);
}

Node *client_get(int fd)
{
    Node *node = clients->head;
    for (int i = 0; i < clients->item_count; i++) {
        Client *client = (Client *) (node->item);
        if (client->fd == fd) {
            return node;
        }
        node = node->next;
    }
    return NULL;
}

void prepare_fds(struct pollfd *fds)
{
    (fds)[0].fd = pipe_fds[0];
    (fds)[0].events = POLLIN;

    Node *node = clients->head;
    for (int i = 0; i < clients->item_count; i++) {
        (fds)[i + 1].fd = ((Client *) (node->item))->fd;
        (fds)[i + 1].events = POLLIN;
        node = node->next;
    }
}
void read_dummy(int fd)
{
    char buff[16];
    int rv = -1;
    do {
        rv = read(fd, buff, 16);
    } while (rv == 16);
}

void read_client(int fd)
{
    Node *node = client_get(fd);
    Client *client = (Client *) node->item;
    if (client == NULL) {
        read_dummy(fd);
        return;
    }

    int64_t millis = current_millis();
    int rv = -1;
    size_t n_bytes = -1;

    do {
        n_bytes = CLIENT_BUFFER_SIZE - client->buffer_ptr;
        rv = read(fd, &client->buffer[client->buffer_ptr], n_bytes);

#if !ZEJF_HIDE_PRINTS
        printf("read %d/%zu bytes from fd %d\n", rv, n_bytes, fd);
#endif
        if (rv <= 0) {
            perror("read");
            client_remove(node);
            break;
        }

        client->last_seen = millis;

        client->buffer_ptr += rv;

        for (; client->buffer_parse_ptr < client->buffer_ptr; client->buffer_parse_ptr++) {
            if (client->buffer[client->buffer_parse_ptr] == '\n') {
                client->buffer[client->buffer_parse_ptr] = '\0';

                pthread_mutex_lock(&zejf_lock);
                network_accept(client->buffer, client->buffer_parse_ptr, &client->interface, millis);
                pthread_mutex_unlock(&zejf_lock);

#if ZEJF_DEBUG
                printf("ACCEPTING [%s]\n", client->buffer);
#endif

                if (client->buffer_parse_ptr < CLIENT_BUFFER_SIZE) {
                    memcpy(client->buffer, &client->buffer[client->buffer_parse_ptr + 1], client->buffer_ptr - client->buffer_parse_ptr - 1);
                }

                client->buffer_ptr -= client->buffer_parse_ptr + 1;
                client->buffer_parse_ptr = 0;
            }
        }

        if (client->buffer_ptr >= CLIENT_BUFFER_SIZE) {
            client->buffer_ptr = 0;
            client->buffer_parse_ptr = 0;
        }

    } while (rv == n_bytes);
}

void *poll_run()
{
    while (true) {
        pthread_rwlock_rdlock(&clients_lock);
        int nfds = clients->item_count + 1;
        struct pollfd fds[nfds];

        prepare_fds(fds);

        pthread_rwlock_unlock(&clients_lock);

        if (poll(fds, nfds, -1) == -1) {
            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN) {
            read_dummy(fds[0].fd);
            continue; // just a signal that change in clients have occured
        }

        for (int i = 0; i < nfds - 1; i++) {
            if (fds[i + 1].revents & POLLIN) {
                pthread_rwlock_wrlock(&clients_lock);
                read_client(fds[i + 1].fd);
                pthread_rwlock_unlock(&clients_lock);
                continue;
            }

            if (fds[i + 1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
#if !ZEJF_HIDE_PRINTS
                printf("remove because %d\n", fds[i + 1].revents);
#endif
                pthread_rwlock_wrlock(&clients_lock);
                client_remove(client_get(fds[i + 1].fd));
                pthread_rwlock_unlock(&clients_lock);
            }
        }
    }
    return NULL;
}

void *server_run(void *arg)
{
    Settings *settings = (Settings *) arg;

#if !ZEJF_HIDE_PRINTS
    printf("starting server... %s:%d\n", settings->ip, settings->tcp_port);
#endif

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

#if !ZEJF_HIDE_PRINTS
    printf("Server is open\n");
#endif

    while (true) {
        if (listen(server_fd, 3) < 0) {
            perror("listen");
            break;
        }

        int client_fd = -1;
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

void *watchdog_run(void *arg)
{
    while (true) {
        if (!server_running) {
            server_running = true;
            pthread_create(&server_thread, NULL, server_run, arg);
        }

        sleep(10);

        int64_t millis = current_millis();

        if (server_running) {
            pthread_rwlock_rdlock(&clients_lock);
            Node *node = clients->head;
            if (node == NULL) {
                pthread_rwlock_unlock(&clients_lock);
                continue;
            }
            do {
                Node *tmp = node;
                node = node->next;

                Client *client = tmp->item;
                if (millis - client->last_seen > 10 * 1000) {
#if !ZEJF_HIDE_PRINTS
                    printf("Someone timedout after %ld\n", millis - client->last_seen);
#endif
                    pthread_rwlock_unlock(&clients_lock);
                    client_remove(tmp);
                    pthread_rwlock_rdlock(&clients_lock);
                    node = clients->head;
                }
            } while (node != NULL && node != clients->head);

            pthread_rwlock_unlock(&clients_lock);
        }
    }
}

void server_init(Settings *settings)
{
    pthread_rwlock_init(&clients_lock, NULL);
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        return;
    }

    clients = list_create(64);
    next_client_uid = 0;

    pthread_create(&server_watchdog, NULL, watchdog_run, settings);
}

void server_close()
{
    if (server_running) {
        pthread_cancel(server_thread);
        pthread_join(server_thread, NULL);
        pthread_cancel(server_poll_thread);
        pthread_join(server_poll_thread, NULL);
        server_running = false;
    }
    pthread_rwlock_destroy(&clients_lock);
}

void server_destroy(void)
{
    pthread_cancel(server_watchdog);
    pthread_join(server_watchdog, NULL);

    server_close();

    list_destroy(clients, client_destroy);
}
