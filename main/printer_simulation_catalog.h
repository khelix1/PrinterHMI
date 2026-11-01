#pragma once
#include "cJSON.h"
#include "moonraker.h"
#include <stdbool.h>
/* Caller owns each returned JSON tree. No I/O, tasks, preferences or controller
 * mutation. */
cJSON *printer_simulation_device_objects(bool full);
cJSON *printer_simulation_macro_objects(bool full);
cJSON *printer_simulation_macro_config(void);
cJSON *printer_simulation_device_status(const moonraker_state_t *state,
                                        double light);
/* Supported public macros only; unknown/invalid commands leave state unchanged.
 */
bool printer_simulation_macro_apply(moonraker_state_t *state,
                                    const char *command,
                                    bool *filament_present);
cJSON *printer_simulation_device_defaults(void);
cJSON *printer_simulation_motion_config(void);
cJSON *printer_simulation_endstops(const moonraker_state_t *state,
                                   uint32_t request);

bool printer_simulation_is_macro(const char *command);
bool printer_simulation_macro_starts_job(const char *command);
