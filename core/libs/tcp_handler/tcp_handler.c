/**
 * Copyright (c) 2023 xspanger3770
*/

/**
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "tcp_handler.h"
#include "pico/sync.h"
#include "zejf_api.h"

#include <stdio.h>

#include <lwip/tcp.h>
#include <pico/cyw43_arch.h>

#include "auth.h"

static int next_attempt_wifi = 0;
static int next_attempt_last = 1;

#define UNUSED(x) (void) (x)

struct tcp_stats_t tcp_stats = {0};

typedef struct TCP_CLIENT_T_
{
    struct tcp_pcb *tcp_pcb;
    ip_addr_t remote_addr;
    int idle_time;
    bool complete;
    bool connected;
} TCP_CLIENT_T;

TCP_CLIENT_T *current_state = NULL;

static err_t tcp_client_close(void *arg) {
    assert(arg != NULL);
    TCP_CLIENT_T *state = (TCP_CLIENT_T *) arg;
    err_t err = ERR_OK;
    if (state->tcp_pcb != NULL) {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK) {
            ZEJF_LOG(2, "close failed %d, calling abort\n", err);
            tcp_abort(state->tcp_pcb);
            err = ERR_ABRT;
        }
        state->tcp_pcb = NULL;
    }
    return err;
}

static err_t tcp_result(void *arg, err_t err) {
    assert(arg != NULL);
    UNUSED(err);

    TCP_CLIENT_T *state = (TCP_CLIENT_T *) arg;

    state->complete = true;

    return tcp_client_close(arg);
}

static TCP_CLIENT_T *tcp_client_init(void) {
    TCP_CLIENT_T *state = calloc(1, sizeof(TCP_CLIENT_T));
    if (!state) {
        ZEJF_LOG(2, "failed to allocate state\n");
        return NULL;
    }
    ip4addr_aton(TEST_TCP_SERVER_IP, &state->remote_addr);
    return state;
}

// event after we send something
static err_t tcp_client_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    UNUSED(tpcb);
    UNUSED(arg);

    ZEJF_LOG(0, "tcp_client_sent %u\n", len);

    return ERR_OK;
}

err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *buff, err_t err) {
    UNUSED(err);
    TCP_CLIENT_T *state = (TCP_CLIENT_T *) arg;
    if (!buff) {
        return tcp_result(arg, err);
    }

    cyw43_arch_lwip_check();

    struct pbuf *p = buff;

    if (p->tot_len > 0) {
        ZEJF_LOG(0, "receive %d bytes %d\n", p->tot_len, err);

        int len = p->len;
        char next_packet[PACKET_MAX_LENGTH];
        int i = 0;
        int j = 0;
        while (p != NULL && len > 0) {
            char ch = ((char *) (p->payload))[i];
            if (ch == '\n' || j == PACKET_MAX_LENGTH - 1) {
                next_packet[j] = '\0';
                network_accept(next_packet, j, &tcp_client_1, millis_since_boot);
                j = 0;
            } else {
                next_packet[j] = ch;
                j++;
            }

            i++;

            if (i == len) {
                p = p->next;
                len = p->len;
                i = 0;
            }
        }

        state->idle_time = 0;
        //state->has_data = true;

        tcp_recved(tpcb, buff->tot_len);
    }

    pbuf_free(buff);

    ZEJF_LOG(0, "reveive done\n");

    return ERR_OK;
}

static void tcp_client_err(void *arg, err_t err) {
    ZEJF_LOG(0, "tcp_client_err %d\n", err);

    tcp_result(arg, err);
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    assert(arg != NULL);
    UNUSED(tpcb);
    TCP_CLIENT_T *state = (TCP_CLIENT_T *) arg;
    if (err != ERR_OK) {
        ZEJF_LOG(1, "connect failed %d\n", err);
        return tcp_result(arg, err);
    }
    state->connected = true;
    ZEJF_LOG(0, "Waiting for buffer from server\n");
    return ERR_OK;
}

static err_t tcp_client_poll(void *arg, struct tcp_pcb *tpcb) {
    assert(arg != NULL);
    UNUSED(tpcb);

    TCP_CLIENT_T *state = (TCP_CLIENT_T *) arg;
    state->idle_time++;

    if (state->idle_time > 3) {
        return tcp_result(arg, -1);
    }

    return ERR_OK;
}

static bool tcp_client_open(void *arg) {
    assert(arg != NULL);

    tcp_stats.tcp_reconnects++;

    TCP_CLIENT_T *state = (TCP_CLIENT_T *) arg;
    ZEJF_LOG(1, "Connecting to %s port %u\n", ip4addr_ntoa(&state->remote_addr), TCP_PORT);
    state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->remote_addr));
    if (!state->tcp_pcb) {
        ZEJF_LOG(2, "failed to create pcb\n");
        return false;
    }

    tcp_arg(state->tcp_pcb, state);
    tcp_poll(state->tcp_pcb, tcp_client_poll, POLL_TIME_S * 2);
    tcp_sent(state->tcp_pcb, tcp_client_sent);
    tcp_recv(state->tcp_pcb, tcp_client_recv);
    tcp_err(state->tcp_pcb, tcp_client_err);

    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    err_t err = tcp_connect(state->tcp_pcb, &state->remote_addr, TCP_PORT, tcp_client_connected);

    return err == ERR_OK;
}

zejf_err zejf_tcp_send(char *msg, u16_t length) {
    if (current_state == NULL || current_state->complete || !current_state->connected) {
        return ZEJF_ERR_SEND_UNABLE;
    }

    ZEJF_LOG(0, "write %s", msg);

    err_t err;
    cyw43_arch_lwip_begin();

    if (length > tcp_sndbuf(current_state->tcp_pcb)) {
        cyw43_arch_lwip_end();
        return ZEJF_ERR_SEND_UNABLE;
    }

    if ((err = tcp_write(current_state->tcp_pcb, msg, length, TCP_WRITE_FLAG_COPY)) != ERR_OK) {
        tcp_result(current_state, err);
        cyw43_arch_lwip_end();
        return ZEJF_ERR_SEND_UNABLE;
    }

    if ((err = tcp_output(current_state->tcp_pcb)) != ERR_OK) {
        tcp_result(current_state, err);
        cyw43_arch_lwip_end();
        return ZEJF_ERR_SEND_UNABLE;
    }

    cyw43_arch_lwip_end();
    return ZEJF_OK;
}

void zejf_tcp_check_connect(void) {
    cyw43_arch_lwip_begin();
    int wifi_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    ZEJF_LOG(0, "wifi status is %d\n", wifi_status);

    if (wifi_status != CYW43_LINK_JOIN) {
        if (next_attempt_wifi < next_attempt_last) {
            next_attempt_wifi++;
            cyw43_arch_lwip_end();
            return;
        }

        next_attempt_wifi = 0;
        if (next_attempt_last < 20) {
            next_attempt_last *= 2;
        }

        ZEJF_LOG(1, "Wifi connection is down, trying to connect again...\n");
        
        if(wifi_connect() != 0){
            cyw43_arch_lwip_end();
            return;
        }

        next_attempt_last = 1;
    }

    if (current_state != NULL && (current_state->complete || !current_state->connected)) {
        ZEJF_LOG(0, "State complete!\n");
        tcp_client_close(current_state);
        current_state = NULL;
    }

    if (current_state == NULL) {
        ZEJF_LOG(0, "New state\n");
        current_state = tcp_client_init();
        if (!current_state) {
            ZEJF_LOG(2, "Failed to init state\n");

            cyw43_arch_lwip_end();
            return;
        }

        if(!tcp_client_open(current_state)){
            ZEJF_LOG(1, "Failed to connect\n");
        } else{
            ZEJF_LOG(1, "TCP connection established\n");
        }

        cyw43_arch_lwip_end();
        return;
    }
    cyw43_arch_lwip_end();
}

int wifi_connect(void) {
    cyw43_arch_enable_sta_mode();

    tcp_stats.wifi_reconnects++;

    if (cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 5000) != 0) {
        ZEJF_LOG(1, "Failed to connect WIFI\n");
        return 1;
    }

    ZEJF_LOG(1, "WIFI Connected\n");

    return 0;
}

int zejf_tcp_init(void) {
    current_state = NULL;

    if (cyw43_arch_init_with_country(COUNTRY) != 0) {
        
        ZEJF_LOG(2, "failed to initialise\n");
        return 1;
    }

    return 0;
}

void destroy(void) {
    cyw43_arch_deinit();
}