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

Packet *packet_create(void) {
    Packet *pack = calloc(1, sizeof(Packet));
    if (pack == NULL) {
        return NULL;
    }

    return pack;
}

void *packet_destroy(void *ptr) {
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

uint32_t checksum(void *ptr, size_t size) {
    uint32_t result = 5381;
    for (size_t i = 0; i < size; i++) {
        result = ((result << 5) + result) + ((uint8_t *) ptr)[i];
    }
    return result;
}

// note that checksum itself and interfaces are not included
uint32_t packet_checksum(Packet *packet) {
    uint32_t result = checksum(&packet->command, sizeof(packet->command));
    result += checksum(&packet->message_size, sizeof(packet->message_size));
    result += checksum(packet->message, packet->message_size);
    result += checksum(&packet->from, sizeof(packet->from));
    result += checksum(&packet->to, sizeof(packet->to));
    result += checksum(&packet->ttl, sizeof(packet->ttl));
    result += checksum(&packet->flags, sizeof(packet->flags));
    return result;
}

void replace_character(char *str, char target, char replacement) {
    if (str == NULL) {
        return; // Check if the string is valid
    }

    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == target) {
            str[i] = replacement;
        }
    }
}

// fill in packet from string, malloc the message, length is the length from { to }, rv 0 is succes
zejf_err packet_from_string(Packet *packet, char *data, int length) {
    if (data[0] != '{' || data[length - 1] != '}') {
        return ZEJF_ERR_PACKET_FORMAT_BRACKETS;
    }

    uint16_t from = 0;
    uint16_t to = 0;
    uint16_t command = 0;
    uint16_t ttl = 0;
    uint32_t checksum = 0;
    uint32_t flags = 0;
    uint16_t message_size = 0;
    char message[length];

    int rv = sscanf(data, "{%" SCNu16 ";%" SCNu16 ";%" SCNu16 ";%" SCNu16 ";%" SCNu32 ";%" SCNu32 ";%" SCNu16 ";%[^\t\n]", &from, &to, &command, &ttl, &checksum, &flags, &message_size, message);

    if (rv != 8) {
        return ZEJF_ERR_PACKET_FORMAT_ITEMS;
    }

    size_t len = strlen(message);
    message[len - 1] = '\0';
    len--;

    if (len != message_size) {
        return ZEJF_ERR_PACKET_FORMAT_LENGTH;
    }

    packet->from = from;
    packet->to = to;
    packet->ttl = ttl;
    packet->flags = flags;
    packet->checksum = checksum;
    packet->command = command;
    packet->message_size = message_size;
    packet->message = message;

    if (checksum != CHECKSUM_BYPASS_VALUE && checksum != packet_checksum(packet)) {
        ZEJF_LOG(0, "CHECKSUM FAIL\n");
        packet->message = NULL;
        return ZEJF_ERR_CHECKSUM;
    }

    packet->message = malloc(message_size + 1);
    if (packet->message == NULL) {
        return ZEJF_ERR_OUT_OF_MEMORY;
    }
    memcpy(packet->message, message, message_size + 1);
    replace_character(packet->message, (char) 2, '\n');

    return 0;
}

// WITH NEWLINE
zejf_err packet_to_string(Packet *packet, char *buff, size_t max_length) {
    if (packet->message == NULL) {
        packet->message_size = 0;
    } else {
        packet->message_size = strlen(packet->message);
        replace_character(packet->message, '\n', (char) 2);
    }

    uint32_t checksum = packet_checksum(packet);

    if (packet->message != NULL) {
        return snprintf(buff, max_length, "{%" SCNu16 ";%" SCNu16 ";%" SCNu16 ";%" SCNu16 ";%" SCNu32 ";%" SCNu32 ";%" SCNu16 ";%s}", packet->from, packet->to, packet->command, packet->ttl, checksum, packet->flags, packet->message_size, packet->message) > 0 ? ZEJF_OK : ZEJF_ERR_GENERIC;
    }

    return snprintf(buff, max_length, "{%" SCNu16 ";%" SCNu16 ";%" SCNu16 ";%" SCNu16 ";%" SCNu32 ";%" SCNu32 ";%" SCNu16 ";}", packet->from, packet->to, packet->command, packet->ttl, checksum, packet->flags, packet->message_size) > 0 ? ZEJF_OK : ZEJF_ERR_GENERIC;
}
