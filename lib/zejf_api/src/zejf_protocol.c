#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zejf_api.h"
#include "zejf_data.h"
#include "zejf_network.h"
#include "zejf_protocol.h"
#include "zejf_routing.h"

Packet *packet_create(void)
{
    Packet *pack = calloc(1, sizeof(Packet));
    if (pack == NULL) {
        return NULL;
    }

    return pack;
}

void *packet_destroy(void *ptr)
{
    Packet *pack = (Packet *) ptr;
    if (pack == NULL) {
        return NULL;
    }

    if (pack->message != NULL) {
        free(pack->message);
    }

    free(pack);

    return NULL;
}

uint32_t checksum(void *ptr, size_t size)
{
    uint32_t result = 5381;
    for (size_t i = 0; i < size; i++) {
        result = ((result << 5) + result) + ((uint8_t *) ptr)[i];
    }
    return result;
}

// note that checksum itself and interfaces are not included
uint32_t packet_checksum(Packet *packet)
{
    uint32_t result = checksum(&packet->command, sizeof(packet->command));
    result += checksum(&packet->message_size, sizeof(packet->message_size));
    result += checksum(packet->message, packet->message_size);
    result += checksum(&packet->from, sizeof(packet->from));
    result += checksum(&packet->to, sizeof(packet->to));
    result += checksum(&packet->ttl, sizeof(packet->ttl));
    result += checksum(&packet->tx_id, sizeof(packet->tx_id));
    return result;
}

// fill in packet from string, malloc the message, length is the length from { to }, rv 0 is succes
int packet_from_string(Packet *packet, char *data, int length)
{
    if (data[0] != '{' || data[length - 1] != '}') {
        return 1;
    }

    uint16_t from = 0;
    uint16_t to = 0;
    uint16_t ttl = 0;
    uint16_t command = 0;
    uint32_t checksum = 0;
    uint16_t message_size = 0;
    uint32_t tx_id;

    char message[length];

    int rv = sscanf(data, "{%" SCNu16 ";%" SCNu16 ";%" SCNu16 ";%" SCNu32 ";%" SCNu16 ";%" SCNu32 ";%" SCNu16 ";%s", &from, &to, &ttl, &tx_id, &command, &checksum, &message_size, message);

    if (rv != 8) {
        return 2;
    }

    size_t len = strlen(message);
    message[len - 1] = '\0';
    len--;

    if (len != message_size) {
        return 4;
    }

    packet->from = from;
    packet->to = to;
    packet->ttl = ttl;
    packet->tx_id = tx_id;
    packet->checksum = checksum;
    packet->command = command;
    packet->message_size = message_size;
    packet->message = message;

    if (checksum != packet_checksum(packet)) {
        #if ZEJF_DEBUG
        printf("CHECKSUM FAIL\n");
        #endif
        packet->message = NULL;
        return 5;
    }

    packet->message = malloc(message_size + 1);
    memcpy(packet->message, message, message_size + 1);

    return 0;
}

// WITH NEWLINE
bool packet_to_string(Packet *pack, char *buff, size_t max_length)
{
    if (pack->message == NULL) {
        pack->message_size = 0;
    } else {
        pack->message_size = strlen(pack->message);
    }

    uint32_t checksum = packet_checksum(pack);

    if (pack->message != NULL) {
        return snprintf(buff, max_length, "{%" SCNu16 ";%" SCNu16 ";%" SCNu16 ";%" SCNu32 ";%" SCNu16 ";%" SCNu32 ";%" SCNu16 ";%s}", pack->from, pack->to, pack->ttl, pack->tx_id, pack->command, checksum, pack->message_size, pack->message) > 0;
    }

    return snprintf(buff, max_length, "{%" SCNu16 ";%" SCNu16 ";%" SCNu16 ";%" SCNu32 ";%" SCNu16 ";%" SCNu32 ";%" SCNu16 ";}", pack->from, pack->to, pack->ttl, pack->tx_id, pack->command, checksum, pack->message_size) > 0;
}

int main444()
{
    char buff[128];
    Packet *pack = packet_create();
    char *msg = NULL;
    pack->message = msg;
    packet_to_string(pack, buff, 128);

    printf("[%s]\n", buff);

    Packet *pack2 = packet_create();
    int rv = packet_from_string(pack2, buff, (int) strlen(buff));
    printf("rv %d\n", rv);
    printf("%d [%s]\n", pack2->message_size, pack2->message);

    free(pack);
    packet_destroy(pack2);
    return 0;
}
