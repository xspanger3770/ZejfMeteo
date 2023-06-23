#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdbool.h>

#include "zejf_meteo.h"

extern volatile bool serial_running;

void run_serial(Settings *settings);

void stop_serial();

#endif