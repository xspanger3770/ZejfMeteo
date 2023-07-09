#include "sd_card_handler.h"

/* Instead of a statically linked hw_config.c,
   create configuration dynamically */

#include <cstdlib>
#include <stdio.h>
#include <string.h>
//
#include "f_util.h"
#include "ff.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "rtc.h"
//
#include "hw_config.h"
//
#include "diskio.h" /* Declarations of disk functions */
#include "time_utils.h"

extern "C" {
#include "zejf_api.h"
}

sd_card_stats_t sd_stats;

void add_spi(spi_t *const spi);
void add_sd_card(sd_card_t *const sd_card);

static spi_t *p_spi;
void spi0_dma_isr() {
    spi_irq_handler(p_spi);
}

sd_card_t *p_sd_card;

void test(sd_card_t *pSD) {
    // See FatFs - Generic FAT Filesystem Module, "Application Interface",
    // http://elm-chan.org/fsw/ff/00index_e.html

    FIL fil;
    const char *const filename = "filename.txt";
    FRESULT fr = f_open(&fil, filename, FA_OPEN_APPEND | FA_WRITE);
    if (FR_OK != fr && FR_EXIST != fr)
        panic("f_open(%s) error: %s (%d)\n", filename, FRESULT_str(fr), fr);
    if (f_printf(&fil, "Hello, world!\n") < 0) {
        ZEJF_LOG(2, "f_printf failed\n");
    }
    fr = f_close(&fil);
    if (FR_OK != fr) {
        ZEJF_LOG(2, "f_close error: %s (%d)\n", FRESULT_str(fr), fr);
    }

    f_unmount(pSD->pcName);
}

void file_path(uint32_t hour_number, char *buff, size_t size) {
    datetime_t dt;
    rtc_time(hour_number * 60ul * 60ul, &dt);
    snprintf(buff, size, "/%d/%02d/%02d/%ld.dat", dt.year, dt.month, dt.day, hour_number);
}

void mount_card(BYTE opt) {
    FRESULT fr = f_mount(&p_sd_card->fatfs, p_sd_card->pcName, opt);
    if (FR_OK != fr) {
        ZEJF_LOG(2, "f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        sd_stats.fatal_errors++;
        return;
    }
    fr = f_chdrive(p_sd_card->pcName);
    if (FR_OK != fr) {
        ZEJF_LOG(2, "f_chdrive error: %s (%d)\n", FRESULT_str(fr), fr);
        sd_stats.fatal_errors++;
        return;
    }
    ZEJF_LOG(1, "SD Card mounted successfully\n");
    sd_stats.working = true;
}

void reset_card() {
    sd_stats.card_resets++;
    sd_stats.working = false;
    ZEJF_LOG(2, "RESET CARD!!!!!!!!!!!\n");
    p_sd_card->m_Status = STA_NOINIT;
    f_unmount(p_sd_card->pcName);

    mount_card(1);
}

bool mkdirs(char *filename) {
    FILINFO info;
    FRESULT fr_dir = f_stat(filename, &info);
    FRESULT fr;
    switch (fr_dir) {
    case FR_OK:
    case FR_NO_FILE: // will be created soon
        return true;
    case FR_NO_PATH:
        ZEJF_LOG(1, "Creating dir %s\n", filename);

        char file_path[65];
        strncpy(file_path, filename, 64lu);
        for (char *p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
            *p = '\0';
            ZEJF_LOG(1, "creating %s\n", file_path);
            if ((fr = f_mkdir(file_path)) != FR_OK) {
                if (fr != FR_EXIST) {
                    *p = '/';
                    sd_stats.fatal_errors++;
                    return -1;
                }
            }
            *p = '/';
        }
        return true;
    default:
        sd_stats.fatal_errors++;
        reset_card();
        ZEJF_LOG(2, "f_stat error! %s\n", FRESULT_str(fr_dir));
        return false;
    }
}

bool card_write(void *ptr, size_t size, char *filename) {
    ZEJF_LOG(1, "request to write %d bytes to [%s]\n", size, filename);
    FIL fil;

    if (!mkdirs(filename)) {
        return false;
    }

    FRESULT fr = f_open(&fil, filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) {
        ZEJF_LOG(2, "f_open error: %s (%d)\ntry to mount\n", FRESULT_str(fr), fr);

        if (fr != FR_NO_FILE && fr != FR_NO_PATH) {
            sd_stats.fatal_errors++;
            reset_card();
        }
        return false;
    }

    UINT result;

    fr = f_write(&fil, ptr, size, &result);
    if (FR_OK != fr) {
        ZEJF_LOG(2, "f_write error: %s (%d)\n", FRESULT_str(fr), fr);
        sd_stats.fatal_errors++;
        goto close;
    }

    ZEJF_LOG(1, "%d bytes written to [%s]\n", result, filename);

close:
    fr = f_close(&fil);
    if (FR_OK != fr) {
        ZEJF_LOG(2, "f_close error: %s (%d)\n", FRESULT_str(fr), fr);
        sd_stats.fatal_errors++;
        return false;
    }

    bool success = result == size;
    if (success) {
        sd_stats.successful_writes++;
        sd_stats.working = true;
    }

    return success;
}

size_t card_read(void **ptr, char *filename) {
    if (ptr == NULL) {
        return 0;
    }
    ZEJF_LOG(1, "request to read at most %d bytes from [%s]\n", HOUR_FILE_MAX_SIZE, filename);

    FIL fil;

    FRESULT fr = f_open(&fil, filename, FA_READ);
    if (FR_OK != fr) {
        if (fr != FR_NO_FILE && fr != FR_NO_PATH) {
            ZEJF_LOG(2, "FATAL f_open error in read: %s (%d) [%s]\n", FRESULT_str(fr), fr, filename);
            sd_stats.fatal_errors++;
            reset_card();
            return 0;
        }
        ZEJF_LOG(1, "f_open error in read: %s (%d) [%s]\n", FRESULT_str(fr), fr, filename);
        return 0;
    }

    UINT result = 0;
    size_t total_size = f_size(&fil);
    if (total_size > HOUR_FILE_MAX_SIZE) {
        ZEJF_LOG(2, "ERR file too big\n");
        sd_stats.fatal_errors++;
        total_size = 0;
        goto close;
    }

    *ptr = malloc(total_size);
    if (*ptr == NULL) {
        total_size = 0;
        sd_stats.fatal_errors++;
        goto close;
    }

    fr = f_read(&fil, (BYTE *) *ptr, total_size, &result);

    if (FR_OK != fr) {
        free(*ptr);
        *ptr = NULL;
        total_size = 0;
        sd_stats.fatal_errors++;
        ZEJF_LOG(2, "f_read error: %s (%d)\n", FRESULT_str(fr), fr);
        goto close;
    }

    ZEJF_LOG(1, "%zu bytes successfully read.\n", total_size);

close:
    fr = f_close(&fil);
    if (FR_OK != fr) {
        ZEJF_LOG(2, "f_close error: %s (%d)\n", FRESULT_str(fr), fr);
        sd_stats.fatal_errors++;
    }

    sd_stats.successful_reads++;
    sd_stats.working = true;

    return total_size;
}

size_t hour_load(uint8_t **data_buffer, uint32_t hour_number) {
    char buff[64];
    file_path(hour_number, buff, 64);

    return card_read((void **) data_buffer, buff);
}

bool hour_save(uint32_t hour_number, uint8_t *buffer, size_t total_size) {
    char buff[64];
    file_path(hour_number, buff, 64);

    return card_write(buffer, total_size, buff);
}

void init_card(spi_inst_t *spi_num, int miso, int mosi, int sck, int cs, long baud_rate) {
    time_init(false);

    p_spi = new spi_t;
    memset(p_spi, 0, sizeof(spi_t));
    if (!p_spi)
        panic("Out of memory");
    p_spi->hw_inst = spi_num; // SPI component
    p_spi->miso_gpio = miso;  // GPIO number (not pin number)
    p_spi->mosi_gpio = mosi;
    p_spi->sck_gpio = sck;
    p_spi->baud_rate = baud_rate; // The limitation here is SPI slew rate.
    p_spi->dma_isr = spi0_dma_isr;
    p_spi->initialized = false;   // initialized flag
    add_spi(p_spi);

    // Hardware Configuration of the SD Card "object"
    p_sd_card = new sd_card_t;
    if (!p_sd_card)
        panic("Out of memory");
    memset(p_sd_card, 0, sizeof(sd_card_t));
    p_sd_card->pcName = "0:";         // Name used to mount device
    p_sd_card->spi = p_spi;           // Pointer to the SPI driving this card
    p_sd_card->ss_gpio = cs;          // The SPI slave select GPIO for this SD card
    p_sd_card->card_detect_gpio = 22; // Card detect
    // What the GPIO read returns when a card is
    // present. Use -1 if there is no card detect.
    p_sd_card->card_detected_true = -1;
    p_sd_card->use_card_detect = false;
    // State attributes:
    p_sd_card->m_Status = STA_NOINIT;
    p_sd_card->sectors = 0;
    p_sd_card->card_type = 0;
    add_sd_card(p_sd_card);

    mount_card(1);
}