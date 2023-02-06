#include "zejf_protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

Packet* packet_create(void){
    Packet* pack = calloc(1, sizeof(Packet));
    if(pack == NULL) {
        return NULL;
    }

    return pack;
}

void packet_destroy(Packet* pack){
    if(pack == NULL){
        return;
    }

    if(pack->message != NULL){
        free(pack->message);
    }

    free(pack);
}

int32_t checksum(void* ptr, size_t size){
    int32_t result = 5381;
    for(size_t i = 0; i < size; i++){
        result = ((result<<5)+result)+((uint8_t*) ptr)[i];
    }
    return result;
}

// note that checksum itself and source interface not included
int32_t packet_checksum(Packet* packet){
    int32_t result = checksum(&packet->command, sizeof(packet->command));
    result+=checksum(&packet->message_size, sizeof(packet->message_size));
    result+=checksum(packet->message, packet->message_size);
    result+=checksum(&packet->from, sizeof(packet->from));
    result+=checksum(&packet->to, sizeof(packet->to));
    result+=checksum(&packet->ttl, sizeof(packet->ttl));
    return result;
}

// fill in packet from string, malloc the message, length is the length from { to }, rv 0 is succes
int packet_from_string(Packet* packet, char* data, int length){
    if(data[0] != '{' || data[length - 1] != '}'){
        return 1;
    }

    data[length - 1] = '\0';

    uint16_t from = 0;
    uint16_t to = 0;
    uint8_t ttl = 0;
    uint8_t command = 0;
    int32_t checksum = 0;
    uint16_t message_size = 0;
    char message[length];

    int rv = sscanf(data, "{%hu;%hu;%hhu;%hhu;%d;%hu;%s", &from, &to, &ttl, &command, &checksum, &message_size, message);

    if(rv != 7){
        return 2;
    }

    if(message == NULL){
        return 3;
    }
    size_t len = strlen(message);
    if(len != message_size){
        return 4;
    }

    packet->from = from;
    packet->to = to;
    packet->ttl = ttl;
    packet->checksum=checksum;
    packet->command=command;
    packet->message_size=message_size;
    packet->message = message;

    printf("message [%s] cs %d/%d\n", message, checksum, packet_checksum(packet));
    if(checksum != packet_checksum(packet)){
        return 5;
    }

    packet->message=malloc(message_size+1);
    memcpy(packet->message, message, message_size + 1);

    return 0;
}

bool packet_to_string(Packet* pack, char* buff, size_t max_length){
    pack->message_size = strlen(pack->message);
    int32_t checksum = packet_checksum(pack);
    return snprintf(buff, max_length, "{%hu;%hu;%hhu;%hhu;%d;%hu;%s}", pack->from, pack->to, pack->ttl,
        pack->command, checksum, pack->message_size, pack->message) > 0;
}

int main3(){
    char buff[128];
    Packet* pack = packet_create();
    pack->message = "a";
    packet_to_string(pack, buff, 128);

    printf("[%s]\n", buff);

    Packet* pack2 =packet_create();
    int rv = packet_from_string(pack2, buff, strlen(buff));
    printf("rv %d\n", rv);

    free(pack);
    packet_destroy(pack2);
    return 0;
}