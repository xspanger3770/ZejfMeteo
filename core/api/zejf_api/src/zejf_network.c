#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <inttypes.h>
#include <limits.h>

#include "data_check.h"
#include "data_info.h"
#include "data_request.h"
#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_network.h"
#include "zejf_protocol.h"
#include "zejf_routing.h"

typedef LinkedList Queue;

Queue *tx_queue;
Queue *rx_queue;
Node *next_packet;

uint16_t provide_ptr;
uint16_t demand_ptr;

bool network_push_packet(Packet *packet);

bool process_rip_packet(Packet *packet, TIME_TYPE now);

void sync_id(Interface *interface, int back, TIME_TYPE time);

bool network_catch_packet(Packet *packet, TIME_TYPE time);

bool network_init(void) {
    rx_queue = list_create(RX_QUEUE_SIZE);
    tx_queue = list_create(TX_QUEUE_SIZE);

    next_packet = NULL;

    provide_ptr = 0;
    demand_ptr = 0;

    return rx_queue != NULL && tx_queue != NULL;

    /*printf("Attention ==========\n");
    printf("DataDays will take at most %"SCNu64"b\n", ((uint64_t)DAY_MAX_SIZE*(uint64_t)DAY_BUFFER_SIZE));
    printf("Packets will take around %"SCNu64"b\n", ((uint64_t)PACKET_MAX_LENGTH*PACKET_QUEUE_SIZE));
    printf("RoutingEntries will take at most %ldb\n", (sizeof(RoutingEntry)*ROUTING_TABLE_SIZE));*/
}

void network_destroy(void) {
    list_destroy(rx_queue, packet_destroy);
    list_destroy(tx_queue, packet_destroy);
}

// ALL PACKETS FROM OUTSIDE MUST PASS THIS FUNCTION
int network_accept(char *msg, int length, Interface *interface, TIME_TYPE time) {
    Packet *packet = packet_create();
    if (packet == NULL) {
        return ZEJF_ERR_NULL;
    }

    int rv = packet_from_string(packet, msg, length);

    if (rv != 0) {
        packet_destroy(packet);
        return rv;
    }

    packet->source_interface = interface;
    packet->ttl++;

    if (packet->ttl >= PACKET_MAX_TTL) {
        packet_destroy(packet);
        return ZEJF_ERR_TTL;
    }

    return network_send_packet(packet, time);
}

void packet_remove(Node *node) {
    if (node == next_packet) {
        next_packet = next_packet->next == next_packet ? NULL : next_packet->next;
    }
    packet_destroy(list_remove(tx_queue, node));
}

// destroy packets that are going to the removed interface
void network_interface_removed(Interface *interface) {
    Node *node = tx_queue->tail;
    if (node == NULL) {
        return;
    }
    do {
        Packet *packet = node->item;
        if (packet->destination_interface == interface || packet->source_interface == interface) {
            Node *tmp = node->next;
            packet_remove(node);
            node = tmp;
            continue;
        }

        node = node->next;
    } while (node != tx_queue->tail);
}

bool process_id_request(Packet *packet, TIME_TYPE time) {
    uint16_t asigned_id = routing_find_free_id();
    if (asigned_id == 0) {
        return false; // no free slots
    }

    Packet *pack = network_prepare_packet(asigned_id, ID_INFO, NULL);
    if(pack == NULL){
        return false;
    }
    char buff[PACKET_MAX_LENGTH];

    if(!packet_to_string(pack, buff, PACKET_MAX_LENGTH)){
        return false;
    }

    return network_send_via(buff, strlen(buff), packet->source_interface, time) == 0;
}

// EVERY PACKET THAT ENDS UP IN THE QUEUE MUST PASS THIS FUNCTION
// PACKET MUST BE ALLOCATED
int network_send_packet(Packet *packet, TIME_TYPE time) {
    // HANDLE SPECIAL PACKETS

    if (packet == NULL) {
        return ZEJF_ERR_NULL;
    }

    if (packet->from == DEVICE_ID && packet->to == DEVICE_ID) {
        packet_destroy(packet);
        return ZEJF_ERR_ITSELF;
    }

    if (packet->from == DEVICE_ID && packet->to != BROADCAST && routing_entry_find(packet->to) == NULL) {
        packet_destroy(packet);
        return ZEJF_ERR_NO_SUCH_DEVICE;
    }

    if (packet->command == ID_REQUEST) {
        if(!process_id_request(packet, time)){
            return ZEJF_ERR_GENERIC;
        }
        return 0;
    }

    if (packet->command == RIP) { // always from outside
        if (!process_rip_packet(packet, time)) {
            packet_destroy(packet);
            return 99;
        }
        packet_destroy(packet);
        return 0;
    }

    if (list_is_full(packet->to == DEVICE_ID ? rx_queue : tx_queue)) {
        packet_destroy(packet);
        ZEJF_LOG(0, "WARN: queue full\n");
        return ZEJF_ERR_QUEUE_FULL;
    }

    if (packet->from != DEVICE_ID && packet->command != RIP) {
        if (packet->source_interface == NULL) {
            packet_destroy(packet);
            return 0;
        }
    }

    packet->time_received = time; // time when it entered the queue
    packet->time_sent = 0;        // last send attempt

    if (!network_push_packet(packet)) {
        ZEJF_LOG(0, "THIS SHOULD HAVE NEVER HAPPENED\n");
        // fun fact: it happened     edit: many times
        return ZEJF_ERR_OUT_OF_MEMORY;
    }

    return 0;
}

bool network_push_packet(Packet *packet) {
    Queue *target_queue = packet->to == DEVICE_ID ? rx_queue : tx_queue;
    return list_push(target_queue, packet);
}

int prepare_and_send(Packet *packet, Interface *destination_interface, TIME_TYPE time) {
    char buff[PACKET_MAX_LENGTH];
    if (!packet_to_string(packet, buff, PACKET_MAX_LENGTH)) {
        return SEND_UNABLE;
    }

    packet->destination_interface = destination_interface;

    return network_send_via(buff, (int) strlen(buff), destination_interface, time);
}

bool process_broadcast_packet(Packet *packet) {
    size_t size = routing_table_top; // how many packets will be created
    size_t free_space = tx_queue->capacity - tx_queue->item_count;

    if (size <= free_space) {
        for (size_t i = 0; i < routing_table_top; i++) {
            RoutingEntry *entry = routing_table[i];
            Packet *new_packet = network_prepare_packet(entry->device_id, packet->command, packet->message);
            if(new_packet == NULL){
                return false;
            }

            network_send_packet(new_packet, packet->time_received);
        }

        return true;
    }

    return false;
}

// EXCEPT BACKWARDS
void network_send_everywhere(Packet *packet, TIME_TYPE time) {
    char buff[PACKET_MAX_LENGTH];
    if (!packet_to_string(packet, buff, PACKET_MAX_LENGTH)) {
        return;
    }
    Interface **interfaces = NULL;
    size_t count = 0;

    get_all_interfaces(&interfaces, &count);

    for (size_t i = 0; i < count; i++) {
        Interface *interface = interfaces[i];
        if (packet->source_interface == NULL || interface->uid != packet->source_interface->uid) {
            network_send_via(buff, (int) strlen(buff), interface, time);
        }
    }
}

bool process_rip_packet(Packet *packet, TIME_TYPE now) {
    if (routing_table_update(packet->from, packet->source_interface, packet->ttl, now) == UPDATE_SUCCESS) {
        network_send_everywhere(packet, now);
    }

    return true;
}

void network_process_rx(TIME_TYPE time);
void network_send_tx(TIME_TYPE time);

void network_process_packets(TIME_TYPE time) {
    network_process_rx(time);
    network_send_tx(time);
}

void network_process_rx(TIME_TYPE time) {
    Packet *packet = (Packet *) list_pop(rx_queue);
    while (packet != NULL) {
        if (!network_catch_packet(packet, time)) {
            network_process_packet(packet);
        }
        packet_destroy(packet);

        packet = (Packet *) list_pop(rx_queue);
    }
}

// send everything thats in the tx queue
// WARNING: spaghetti code!!
void network_send_tx(TIME_TYPE time) {
    if (list_is_empty(tx_queue)) {
        return;
    }

    if (next_packet == NULL) {
        next_packet = tx_queue->tail;
    }

    Packet *packet = next_packet->item;

    if (packet == NULL) {
        ZEJF_LOG(0, "Fatal error: NULL packet\n");
        goto next_one;
    }

    if ((time - packet->time_received) >= PACKET_DELETE_TIMEOUT) {
        ZEJF_LOG(0, "timeout hard of packed command %" SCNu16 " from %" SCNu16 " to %" SCNu16 " after %" SCNu32 "ms\n", packet->command, packet->from, packet->to, (time - packet->time_received));
        ZEJF_LOG(0, "times were %" SCNu32 " %" SCNu32 "\n", time, packet->time_received);
        goto remove;
    }

    if (packet->to == BROADCAST) {
        if (!process_broadcast_packet(packet)) {
            goto next_one;
        }
        goto remove;
    }

    // SEND THE PACKET SOMEWHERE

    RoutingEntry *entry = routing_entry_find(packet->to);
    if (entry == NULL) {
        goto remove;
    }

    packet->time_sent = time;

    int rv = prepare_and_send(packet, entry->interface, time);
    if (rv == SEND_UNABLE) {
        goto next_one;
    }

    goto remove;

next_one:
    next_packet = next_packet->next;
    return;

remove:
    packet_remove(next_packet);
}

Packet *network_prepare_packet(uint16_t to, uint8_t command, char *msg) {
    Packet *packet = packet_create();
    packet->command = command;
    packet->from = DEVICE_ID;
    packet->to = to;
    packet->ttl = 0;

    if (msg != NULL) {
        packet->message_size = strlen(msg);
        packet->message = malloc(packet->message_size + 1);
        if(packet->message == NULL){
            return NULL;
        }
        memcpy(packet->message, msg, packet->message_size + 1);
    } else {
        packet->message_size = 0;
        packet->message = NULL;
    }

    return packet;
}

size_t allocate_packet_queue(int priority) {
    return (tx_queue->capacity - tx_queue->item_count) / priority;
}

bool network_catch_packet(Packet *packet, TIME_TYPE time) {
    switch (packet->command) {
    case DATA_DEMAND:
        process_data_demand(packet);
        break;
    case DATA_PROVIDE:
        process_data_provide(packet);
        break;
    case DATA_LOG:
        process_data_log(packet, time);
        break;
    case DATA_REQUEST:
        data_request_receive(packet);
        break;
    case DATA_CHECK:
        data_check_receive(packet);
        break;
    case VARIABLES_REQUEST:
        variables_request_receive(packet, time);
        break;
    case DATA_SUBSCRIBE:
        process_data_subscribe(packet);
        break;
    default:
        return false;
    }

    return true;
}
