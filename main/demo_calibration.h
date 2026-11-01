#pragma once
#include <stdbool.h>
#include "moonraker.h"
/* Firmware demo only: no transport or persistent settings. */
bool demo_calibration_init(void);
void demo_calibration_refresh(const moonraker_state_t *state);
bool demo_calibration_send(const char *command);
