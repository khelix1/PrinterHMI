#include "demo_services.h"
#include "device_catalog_controller.h"
#include "endstop_status_controller.h"
#include "macro_controller.h"
#include "moonraker_config_controller.h"
#include "motion_diagnostics_controller.h"
#include "printer_simulation_catalog.h"
#include <string.h>
static uint32_t owner;
static bool known_online;
bool demo_services_init(void){return device_catalog_controller_init() && motion_diagnostics_controller_init() && macro_controller_init();}
void demo_services_publish(const moonraker_state_t *state, double light) {
  bool online = state && state->moonraker_ok && state->live_data_ok;
  uint32_t generation = moonraker_config_generation();
  if (owner != generation || known_online != online) {
    owner = generation;
    known_online = online;
    device_catalog_controller_reset();
    motion_diagnostics_controller_reset();
    endstop_status_controller_reset();
    macro_controller_init();
    macro_controller_update_from_objects(NULL);
    if (online) {
      bool full = moonraker_config_active_profile_index() == 0;
      cJSON *objects = printer_simulation_device_objects(full);
      device_catalog_controller_update_from_objects(objects);
      motion_diagnostics_controller_update_objects(objects);
      cJSON_Delete(objects);
      objects = printer_simulation_macro_objects(full);
      macro_controller_update_from_objects(objects);
      cJSON_Delete(objects);
      cJSON *config = printer_simulation_macro_config();
      macro_controller_update_parameters(config);
      cJSON_Delete(config);
      config = printer_simulation_motion_config();
      motion_diagnostics_controller_update_config(config);
      cJSON_Delete(config);
      cJSON *status = printer_simulation_device_defaults();
      device_catalog_controller_merge_status(status);
      motion_diagnostics_controller_merge_status(status);
      cJSON_Delete(status);
    }
  }
  if (online) {
    cJSON *status = printer_simulation_device_status(state, light);
    device_catalog_controller_merge_status(status);
    cJSON_Delete(status);
  }
}
bool demo_services_endstops(const moonraker_state_t *state, uint32_t request) {
  if (!state || !state->moonraker_ok || !state->live_data_ok ||
      request < ENDSTOP_REQUEST_FIRST || request >= 0x80000000U ||
      !strcmp(state->printer_state, "printing") ||
      !strcmp(state->printer_state, "paused") ||
      !strcmp(state->printer_state, "error") ||
      !strcmp(state->printer_state, "shutdown"))
    return false;
  cJSON *root = printer_simulation_endstops(state, request);
  if (!root)
    return false;
  bool ok =
      endstop_status_controller_merge(root, moonraker_config_generation());
  cJSON_Delete(root);
  return ok;
}
