#ifndef _ZEJF_DEFS_H
#define _ZEJF_DEFS_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "float.h"
#include "zejf_settings.h"

#define VALUE_EMPTY -999.0
#define VALUE_NOT_MEASURED -998.0

#define UPDATE_SUCCESS 0
#define UPDATE_FAIL 1
#define UPDATE_NO_CHANGE 2

#define SEND_SUCCES 0
#define SEND_RETRY 1
#define SEND_UNABLE 2

#define BROADCAST 0xFFFF

#define TIME_TYPE uint32_t

#define ALL_VARIABLES 0xFFFF

#define PRIORITY_HIGH 2
#define PRIORITY_MEDIUM 4
#define PRIORITY_LOW 8

#define HOUR (1000l * 60 * 60l)

#define ZEJF_ERR_NULL 1
#define ZEJF_ERR_PACKET_FORMAT 2
#define ZEJF_ERR_CHECKSUM 3
#define ZEJF_ERR_ITSELF 4
#define ZEJF_ERR_QUEUE_FULL 5
#define ZEJF_ERR_TXID 6
#define ZEJF_ERR_RXID 7
#define ZEJF_ERR_TTL 8

/* =======  DATA ============ */

typedef struct interface_t
{
    int uid;
    enum interface_type_t type;
    int handle;
    uint32_t rx_id;
    uint32_t tx_id;
} Interface;

typedef struct variable_info_t
{
    uint32_t samples_per_hour;
    uint16_t id;
} VariableInfo;

typedef struct variable_t
{
    VariableInfo variable_info;
    float *data;
} Variable;

typedef struct datahour_t
{
    uint32_t hour_id;
    uint32_t checksum;
    uint16_t variable_count;
    uint8_t flags;
    Variable *variables;
} DataHour;

typedef struct data_request_t
{
    VariableInfo variable;
    uint16_t target_device;
    uint32_t hour_number;
    uint32_t start_log;
    uint32_t end_log;
    uint32_t current_log;
    uint16_t errors;
} DataRequest;

/* =======  NETWORK ============ */

enum commands
{
    RIP = 0x01,
    ACK = 0x02,
    ID_SYNC = 0x03,
    DATA_PROVIDE = 0x04,
    DATA_DEMAND = 0x05,
    DATA_LOG = 0x06,
    DATA_REQUEST = 0x07,
    DATA_CHECK = 0x08,
    TIME_CHECK = 0x09,
    MESSAGE = 0x0a,
};

typedef struct routing_table_entry_t
{
    TIME_TYPE last_seen;
    uint16_t device_id;
    uint16_t demand_count;
    uint16_t *demanded_variables;
    Interface *interface;
    uint8_t paused;
    uint8_t distance;
    uint16_t provided_count;
    VariableInfo *provided_variables;
} RoutingEntry;

typedef struct packet_t
{
    Interface *source_interface;
    Interface *destination_interface;
    uint32_t time_received;
    uint32_t time_sent;
    uint32_t tx_id;
    uint32_t checksum;
    uint16_t from;
    uint16_t to;
    uint16_t message_size;
    uint8_t ttl;
    uint8_t command;
    char *message; // WARNING: \0 MUST BE AT THE END, can be NULL
} Packet;

#endif