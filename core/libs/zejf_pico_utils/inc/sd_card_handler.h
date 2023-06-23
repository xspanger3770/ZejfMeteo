#ifndef _SD_CARD_HANDLER_H
#define _SD_CARD_HANDLER_H

#include "hardware/spi.h"

#define CHUNK_SIZE (1024)

struct sd_card_stats_t
{
    unsigned long fatal_errors = 0;
    unsigned long card_resets = 0;
    unsigned long successful_reads = 0;
    unsigned long successful_writes = 0;
    bool working = false;
};

extern struct sd_card_stats_t sd_stats;

void init_card(spi_inst_t *spi_num, int miso, int mosi, int sck, int cs, long baud_rate);

#endif