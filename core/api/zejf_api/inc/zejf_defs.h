#ifndef _ZEJF_DEFS_H
#define _ZEJF_DEFS_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "float.h"
#include "zejf_settings.h"

#define VALUE_EMPTY -999.0
#define VALUE_NOT_MEASURED -998.0

#define BROADCAST 0xFFFF

#define TIME_TYPE uint32_t

#define ALL_VARIABLES 0xFFFF

#define PRIORITY_HIGH 2
#define PRIORITY_MEDIUM 4
#define PRIORITY_LOW 8

#define HOUR (1000l * 60 * 60l)

typedef enum zejf_err_t {
    ZEJF_OK,
    ZEJF_ERR_GENERIC,
    ZEJF_ERR_NULL,
    ZEJF_ERR_PACKET_FORMAT,
    ZEJF_ERR_CHECKSUM,
    ZEJF_ERR_ITSELF,
    ZEJF_ERR_QUEUE_FULL,
    ZEJF_ERR_TXID,
    ZEJF_ERR_RXID,
    ZEJF_ERR_TTL,
    ZEJF_ERR_NO_SUCH_DEVICE,
    ZEJF_ERR_PACKET_FORMAT_BRACKETS,
    ZEJF_ERR_PACKET_FORMAT_ITEMS,
    ZEJF_ERR_PACKET_FORMAT_LENGTH,
    ZEJF_ERR_OUT_OF_MEMORY,
    ZEJF_ERR_NO_CHANGE,
    ZEJF_ERR_PARTIAL,
    ZEJF_ERR_INVALID_SAMPLE_RATE,
    ZEJF_ERR_VARIABLES_MAX,
    ZEJF_ERR_VARIABLE_ALREADY_EXISTS,
    ZEJF_ERR_LOG_NUMBER,
    ZEJF_ERR_VALUE,
    ZEJF_ERR_NO_FREE_SLOTS,
    ZEJF_ERR_REQUEST_ALREADY_EXISTS,
    ZEJF_ERR_IO,
    ZEJF_ERR_FILE_DOESNT_EXIST,
    ZEJF_ERR_SEND_UNABLE
} zejf_err;

#define CHECKSUM_BYPASS_VALUE 0

#define RESERVED_DEVICE_IDS 1000

/* =======  DATA ============ */

typedef struct interface_t
{
    int uid;
    enum interface_type_t type;
    int handle;
    uint32_t rx_count;
    uint32_t tx_count;
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

/* =======  PROTOCOL ============ */

enum commands
{
    RIP = 0x01, // device info
    //ACK = 0x02, // acknowledgement
    //ID_SYNC = 0x03, // ?
    DATA_PROVIDE = 0x04, // send info about variables that device provides
    DATA_DEMAND = 0x05, // send info about variables that device wants to receive
    DATA_LOG = 0x06, // send data log
    DATA_REQUEST = 0x07, // send specific data request
    DATA_CHECK = 0x08, // check if one device has all data, otherwise data request is created
    TIME_CHECK = 0x09, // send time
    MESSAGE = 0x0a, // msg
    TIME_REQUEST = 0x0b, // send request for time
    STATUS_REQUEST = 0x0c, // request status
    VARIABLES_REQUEST = 0x0d, // request to get all available variables in given hour num
    VARIABLE_INFO = 0x0e, // info about available variable
    ID_REQUEST = 0x0f, // request to send next available device id
    ID_INFO = 0x10, // info about next available device id
    DATA_SUBSCRIBE = 0x11, // subscribe to all data changes
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
    bool subscribed;
} RoutingEntry;

typedef struct packet_t
{
    Interface *source_interface;
    Interface *destination_interface;
    uint32_t time_received;
    uint32_t time_sent;
    uint32_t flags;
    uint32_t checksum;
    uint16_t from;
    uint16_t to;
    uint16_t message_size;
    uint8_t ttl;
    uint8_t command;
    char *message; // WARNING: \0 MUST BE AT THE END, can be NULL
} Packet;

#endif