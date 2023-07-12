#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "zejf_api.h"
#include "zejf_data_loader.h"
#include "zejf_meteo.h"

int mkpath(char *file_path, mode_t mode) {
    for (char *p = strchr(file_path + 1, '/'); p; p = strchr(p + 1, '/')) {
        *p = '\0';
        if (mkdir(file_path, mode) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
}

// WTF??????

/*ZejfDay *zejf_day_load(uint32_t day_number) {
    char path_buff[128];
    zejf_day_path(path_buff, day_number);

    FILE *file = fopen(path_buff, "wb");
    if (file == NULL) {
        perror("fopen");
        return false;
    }

    ZejfDay* day = malloc(sizeof(ZejfDay));
    if(day == NULL){
        perror("malloc");
        return NULL;
    }

    if (fread(day, sizeof(ZejfDay), 1, file) != 1) {
        if(errno != 0){
            perror("fread");
        } else {
            fprintf(stderr, "File corrupted\n");
        }
        free(day);
        return NULL;
    }

    return day;
}*/

void str_append(char **end_ptr, char *str) {
    size_t len = strlen(str);
    memcpy(*end_ptr, str, len);
    *end_ptr += len;
}

char months[12][10] = { "January\0", "February\0", "March\0", "April\0", "May\0", "June\0", "July\0", "August\0", "September\0", "October\0", "November\0", "December\0" };

void zejf_day_path(char *buff, uint32_t hour_number) {
    time_t now = (time_t) hour_number * 60 * 60;
    struct tm *t = localtime(&now);

    char time_buff[32];

    char *end_ptr = buff;
    str_append(&end_ptr, MAIN_FOLDER);
    str_append(&end_ptr, "data/");

    strftime(time_buff, sizeof(time_buff) - 1, "%Y/", t);

    str_append(&end_ptr, time_buff);
    str_append(&end_ptr, months[t->tm_mon]);

    strftime(time_buff, sizeof(time_buff) - 1, "/%d_%HH.dat", t);
    str_append(&end_ptr, time_buff);

    *end_ptr = '\0';
}

zejf_err hour_save(uint32_t hour_number, uint8_t *buffer, size_t total_size) {
    if (buffer == NULL) {
        return ZEJF_ERR_NULL;
    }

    char path_buff[128];
    zejf_day_path(path_buff, hour_number);

    char path_only[128];
    strcpy(path_only, path_buff);

    memset(strrchr(path_only, '/') + 1, '\0', 1);

    struct stat st = { 0 };

    if (stat(path_only, &st) == -1) {
        ZEJF_LOG(1, "Creating path %s\n", path_only);
        if (mkpath(path_only, 0700) != 0) {
            perror("mkdir");
            return ZEJF_ERR_IO;
        }
    }

    FILE *actual_file = fopen(path_buff, "wb");
    if (actual_file == NULL) {
        printf("SAVE: %s: %s\n", path_buff, strerror(errno));
        return ZEJF_ERR_IO;
    }

    ZEJF_LOG(0, "%ld bytes will be written to [%s]\n", total_size, path_buff);

    bool result = fwrite(buffer, total_size, 1, actual_file);

    fclose(actual_file);

    return result ? ZEJF_OK : ZEJF_ERR_IO;
}

zejf_err hour_load(uint8_t **data_buffer, size_t *size, uint32_t hour_number) {
    char path_buff[128];
    zejf_day_path(path_buff, hour_number);

    if (access(path_buff, F_OK) != 0) {
        return ZEJF_ERR_FILE_DOESNT_EXIST;
    }

    FILE *file = fopen(path_buff, "rb");
    if (file == NULL) {
        printf("LOAD: %s: %s\n", path_buff, strerror(errno));
        return ZEJF_ERR_IO;
    }

    fseek(file, 0, SEEK_END);
    size_t fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fsize > (size_t) HOUR_FILE_MAX_SIZE) {
        fsize = 0;
        goto close;
    }

    ZEJF_LOG(0, "%ld bytes will be loaded from [%s]\n", fsize, path_buff);

    (*data_buffer) = malloc(fsize);
    if ((*data_buffer) == NULL) {
        perror("malloc");
        fsize = 0;
        goto close;
    }

    if (fread(*data_buffer, fsize, 1, file) != 1) {
        perror("fread");
        free(*data_buffer);
        (*data_buffer) = NULL;
        fsize = 0;
        goto close;
    }

close:
    fclose(file);

    if (fsize <= 0) {
        if ((*data_buffer) != NULL) {
            free(*data_buffer);
            *data_buffer = NULL;
        }
        return ZEJF_ERR_IO;
    }

    *size = fsize;
    return ZEJF_OK;
}
