#pragma once
#include "moonraker.h"
/* Pure simulation logic: no task, LVGL, storage or transport operations. The
 * caller owns each printer's state and supplies elapsed time explicitly. */
bool printer_simulation_scenario(moonraker_state_t *state,const char *scenario);
void printer_simulation_advance(moonraker_state_t *state,double seconds);
bool printer_simulation_apply(moonraker_state_t *state,bool excluded[2],bool *sensor_enabled,const char *command);

/* Begin a local sample job only from a ready/complete/cancelled state. */
bool printer_simulation_start_file(moonraker_state_t *state,const char *path);

/* Fill the two sample objects in caller-owned snapshot storage. */
void printer_simulation_objects(moonraker_exclude_state_t *out,const bool excluded[2]);
