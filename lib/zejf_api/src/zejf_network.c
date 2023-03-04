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

void sync_id(Interface *entry, int back, TIME_TYPE time);

bool network_catch_packet(Packet *packet, TIME_TYPE time);

void network_init(void)
{
    rx_queue = list_create(RX_QUEUE_SIZE);
    tx_queue = list_create(TX_QUEUE_SIZE);
    next_packet = NULL;

    provide_ptr = 0;
    demand_ptr = 0;

    /*printf("Attention ==========\n");
    printf("DataDays will take at most %"SCNu64"b\n", ((uint64_t)DAY_MAX_SIZE*(uint64_t)DAY_BUFFER_SIZE));
    printf("Packets will take around %"SCNu64"b\n", ((uint64_t)PACKET_MAX_LENGTH*PACKET_QUEUE_SIZE));
    printf("RoutingEntries will take at most %ldb\n", (sizeof(RoutingEntry)*ROUTING_TABLE_SIZE));*/
}

void network_destroy(void)
{
    list_destroy(rx_queue, packet_destroy);
    list_destroy(tx_queue, packet_destroy);
}

// ALL PACKETS FROM OUTSIDE MUST PASS THIS FUNCTION
bool network_accept(char *msg, int length, Interface *interface, TIME_TYPE time)
{
    Packet *packet = packet_create();
    if (packet == NULL) {
        return false;
    }

    int rv = packet_from_string(packet, msg, length);

    if (rv != 0) {
        packet_destroy(packet);
        return false;
    }

    packet->source_interface = interface;
    packet->ttl++;

    if (packet->ttl >= PACKET_MAX_TTL) {
        packet_destroy(packet);
        return false;
    }

    return network_send_packet(packet, time);
}

bool prepare_ack_message(char *buff, uint32_t tx_id)
{
    Packet packet[1];
    packet->command = ACK;
    packet->from = DEVICE_ID;
    packet->to = 0;
    packet->ttl = 0;
    packet->tx_id = tx_id;
    packet->message_size = 0;
    packet->message = NULL;

    return packet_to_string(packet, buff, PACKET_MAX_LENGTH);
}

void packet_remove(Node *node)
{
    if (node == next_packet) {
        next_packet = next_packet->next == next_packet ? NULL : next_packet->next;
    }
    packet_destroy(list_remove(tx_queue, node));
}

void ack_packet(Interface *interface, uint32_t id)
{
    Node *node = tx_queue->tail;
    if (node == NULL) {
        return;
    }
    do {
        Packet *packet = node->item;
        if (packet->destination_interface != NULL && packet->destination_interface->uid == interface->uid && packet->tx_id == id) {
            packet_remove(node);
            return;
        }
        node = node->next;
    } while (node != tx_queue->tail);
}

// EVERY PACKET THAT ENDS UP IN THE QUEUE MUST PASS THIS FUNCTION
// PACKET MUST BE ALLOCATED
bool network_send_packet(Packet *packet, TIME_TYPE time)
{
    // HANDLE SPECIAL PACKETS

    if (packet == NULL) {
        return false;
    }

    if (packet->from == DEVICE_ID && packet->to == DEVICE_ID) {
        packet_destroy(packet); // CANNOT SEND TO ITSELF
        return false;
    }

    if (packet->command == RIP) { // always from outside
        if (!process_rip_packet(packet, time)) {
            packet_destroy(packet);
            return false;
        }
        packet_destroy(packet);
        return true;
    }

    if (packet->command == ID_SYNC) {
        if (packet->source_interface == NULL) {
            packet_destroy(packet);
            return true;
        }

        if (packet->tx_id == 1) { // back
            sync_id(packet->source_interface, 0, time);
        }

        packet->source_interface->rx_id = 1;
        packet->source_interface->tx_id = 1;

        packet_destroy(packet);
        return true;
    }

    if (packet->command == ACK) {
        ack_packet(packet->source_interface, packet->tx_id);

        packet_destroy(packet);
        return true;
    }

    if (list_is_full(packet->to == DEVICE_ID ? rx_queue : tx_queue)) {
        packet_destroy(packet);
        printf("WARN: queue full\n");
        return false;
    }

    if (packet->from != DEVICE_ID && packet->command != RIP) {
        if (packet->source_interface == NULL) {
            packet_destroy(packet);
            return true;
        }

        if (packet->source_interface->tx_id == 0) {
            packet_destroy(packet);
            printf("rejected txid not set yet\n");
            return false; // waiting for sync
        }

        if (packet->source_interface->rx_id != packet->tx_id) {
            packet_destroy(packet);
            printf("rejected wrong id\n");
            return false; // wriong txid
        }

        packet->source_interface->rx_id++;
    }

    uint32_t id_to_ack = packet->tx_id;

    packet->time_received = time; // time when it entered the queue
    packet->time_sent = 0;        // last send attempt
    packet->tx_id = 0;            // not determined yet

    if (!network_push_packet(packet)) {
        printf("THIS SHOULD HAVE NEVER HAPPENED\n");
        // fun fact: it happened
    }

    // SEND ACK BACK TO EVERY NON-SPECIAL PACKET
    if (packet->from != DEVICE_ID) { // if it came from outside it will have source_interface set
        char buff[PACKET_MAX_LENGTH];
        if (!prepare_ack_message(buff, id_to_ack)) {
            return false;
        }

        network_send_via(buff, strlen(buff), packet->source_interface, time);
    }

    return true;
}

bool network_push_packet(Packet *packet)
{
    Queue *target_queue = packet->to == DEVICE_ID ? rx_queue : tx_queue;
    return list_push(target_queue, packet);
}

int prepare_and_send(Packet *packet, Interface *destination_interface, TIME_TYPE time)
{
    if (packet->tx_id == 0 || packet->tx_id >= destination_interface->tx_id) {
        packet->tx_id = destination_interface->tx_id++;
    }

    char buff[PACKET_MAX_LENGTH];
    if (!packet_to_string(packet, buff, PACKET_MAX_LENGTH)) {
        return SEND_UNABLE;
    }

    packet->destination_interface = destination_interface;

    return network_send_via(buff, strlen(buff), destination_interface, time);
}

bool process_broadcast_packet(Packet *packet)
{
    size_t size = routing_table_top; // how many packets will be created
    size_t free_space = tx_queue->capacity - tx_queue->item_count;

    if (size <= free_space) {
        for (size_t i = 0; i < routing_table_top; i++) {
            RoutingEntry *entry = routing_table[i];
            Packet *new_packet = network_prepare_packet(entry->device_id, packet->command, packet->message);

            network_send_packet(new_packet, packet->time_received);
        }

        return true;
    }

    return false;
}

// EXCEPT BACKWARDS
void network_send_everywhere(Packet *packet, TIME_TYPE time)
{
    char buff[PACKET_MAX_LENGTH];
    if (!packet_to_string(packet, buff, PACKET_MAX_LENGTH)) {
        return;
    }
    Interface **interfaces = NULL;
    int count = 0;

    get_all_interfaces(&interfaces, &count);

    for (int i = 0; i < count; i++) {
        Interface *interface = interfaces[i];
        if (packet->source_interface == NULL || interface->uid != packet->source_interface->uid) {
            network_send_via(buff, strlen(buff), interface, time);
        }
    }
}

bool process_rip_packet(Packet *packet, TIME_TYPE now)
{
    if (routing_table_update(packet->from, packet->source_interface, packet->ttl, now) == UPDATE_SUCCESS) {
        network_send_everywhere(packet, now);
    }

    return true;
}

void sync_id(Interface *interface, int back, TIME_TYPE time)
{
    Packet packet[1];
    packet->command = ID_SYNC;
    packet->from = DEVICE_ID;
    packet->to = 0;
    packet->ttl = 0;
    packet->tx_id = back;
    packet->message_size = 0;
    packet->message = NULL;

    char buff[PACKET_MAX_LENGTH];

    if (!packet_to_string(packet, buff, PACKET_MAX_LENGTH)) {
        return;
    }
    interface->rx_id = 1;
    interface->tx_id = 0;

    network_send_via(buff, strlen(buff), interface, time);
}

void network_process_rx(TIME_TYPE time);
void network_send_tx(TIME_TYPE time);

void network_process_packets(TIME_TYPE time)
{
    network_process_rx(time);
    network_send_tx(time);
}

void network_process_rx(TIME_TYPE time)
{
    Packet *packet = (Packet *) list_pop(rx_queue);
    while (packet != NULL) {
        if (!network_catch_packet(packet, time)) {
            network_process_packet(packet);
        }
        packet_destroy(packet);

        packet = (Packet *) list_pop(rx_queue);
    }
}

void network_send_tx(TIME_TYPE time)
{
    if (list_is_empty(tx_queue)) {
        return;
    }

    if (next_packet == NULL) {
        next_packet = tx_queue->tail;
    }

    Packet *packet = next_packet->item;

    if (packet == NULL) {
        printf("FATALLLLLLLL NULKLLLLLLLL\n");
        goto next_one;
    }

    if ((time - packet->time_received) >= PACKET_DELETE_TIMEOUT) {
        printf("timeout hard of packed command %d txid %d from %d to %d after %" SCNu32 "ms\n", packet->command, packet->tx_id, packet->from, packet->to, (time - packet->time_received));
        printf("times were %d %d\n", time, packet->time_received);
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

    if (entry->paused == 1) {
        goto next_one;
    }

    if (time - packet->time_sent < PACKET_RETRY_TIMEOUT) {
        entry->paused = 1; // to keep the order of packets
        goto next_one;
    }

    // check for reset timeout
    if (packet->time_sent != 0 && (packet->time_sent - packet->time_received) / PACKET_RESET_TIMEOUT != (time - packet->time_received) / PACKET_RESET_TIMEOUT) {
        entry->interface->tx_id = 0;
        entry->interface->rx_id = 0;
        entry->paused = 1;
        printf("reset at entry for device %d packet command %d\n", entry->device_id, packet->command);
        // no goto here!
    }

    packet->time_sent = time;

    // tx,rx
    // 0,0 = not initialised
    // 0,1 = not initialised, waiting for returning sync packet
    // 1,1 = ready
    if (entry->interface->tx_id == 0) {
        printf("cannot send %d to %d\n", packet->command, packet->to);
        if (entry->interface->rx_id == 0) {
            sync_id(entry->interface, 1, time);
        }
        goto next_one;
    }

    int rv = prepare_and_send(packet, entry->interface, time);
    if (rv == SEND_UNABLE) {
        goto remove;
    }

next_one:

    next_packet = next_packet->next;

    // toggle reset flag to all routing entries
    if (next_packet == tx_queue->tail) {
        for (size_t i = 0; i < routing_table_top; i++) {
            RoutingEntry *entry = routing_table[i];
            entry->paused = 0;
        }
    }

    return;

remove:
    packet_remove(next_packet);
}

Packet *network_prepare_packet(uint16_t to, uint8_t command, char *msg)
{
    Packet *packet = packet_create();
    packet->command = command;
    packet->from = DEVICE_ID;
    packet->to = to;
    packet->ttl = 0;

    if (msg != NULL) {
        packet->message_size = strlen(msg);
        packet->message = malloc(packet->message_size + 1);
        memcpy(packet->message, msg, packet->message_size + 1);
    } else {
        packet->message_size = 0;
        packet->message = NULL;
    }

    return packet;
}

size_t allocate_packet_queue(int priority)
{
    return (tx_queue->capacity - tx_queue->item_count) / priority;
}

bool network_catch_packet(Packet *packet, TIME_TYPE time)
{
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
    default:
        return false;
    }

    return true;
}