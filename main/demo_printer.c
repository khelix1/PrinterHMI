#include "bed_mesh_controller.h"
#include "cJSON.h"
#include "case_lighting.h"
#include "demo_files.h"
#include "demo_camera.h"
#include "demo_calibration.h"
#include "demo_mode.h"
#include "demo_services.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "moonraker_config_controller.h"
#include "printer_profile_health.h"
#include "printer_simulation.h"
#include "printer_simulation_catalog.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
static SemaphoreHandle_t mutex;
static moonraker_state_t states[MOONRAKER_CONFIG_MAX_PROFILES];
static bool excluded[MOONRAKER_CONFIG_MAX_PROFILES][2],
    sensors[MOONRAKER_CONFIG_MAX_PROFILES];
static int lights[MOONRAKER_CONFIG_MAX_PROFILES];
static bool filament_present[MOONRAKER_CONFIG_MAX_PROFILES];
static int64_t last_us;
static uint32_t owner;
static int active_index(void) {
  int index = moonraker_config_active_profile_index();
  return index >= 0 && index < MOONRAKER_CONFIG_MAX_PROFILES ? index : 0;
}
static void publish(void) {
  int index = active_index();
  moonraker_state_t *s = &states[index];
  s->hotends[0].temperature = s->nozzle_temp;
  s->hotends[0].target = s->nozzle_target;
  moonraker_filament_state_t filament = {
      .discovered = true, .total_count = 1, .sensor_count = 1};
  strcpy(filament.sensors[0].object_name, "filament_switch_sensor runout");
  filament.sensors[0].enabled = sensors[index];
  filament.sensors[0].status_known = true;
  filament.sensors[0].filament_detected = filament_present[index];
  moonraker_state_publish_demo(s, &filament, excluded[index]);
  demo_services_publish(s, lights[index] / 100.);
  demo_calibration_refresh(s);
  for (int i = 0; i < MOONRAKER_CONFIG_MAX_PROFILES; i++)
    if (moonraker_config_profile(i)->configured)
      printer_profile_health_set_live_state(i, true, states[i].moonraker_ok,
                                            states[i].printer_state);
  uint32_t generation = moonraker_config_generation();
  if (owner != generation) {
    owner = generation;
    case_lighting_reset(owner);
    cJSON *objects =
        cJSON_Parse(index == 0 ? "[\"output_pin case_led\"]" : "[]");
    case_lighting_objects(objects, owner);
    cJSON_Delete(objects);
    if (index == 0) {
      cJSON *config = cJSON_Parse("{\"output_pin case_led\":{\"pwm\":true}}");
      case_lighting_config(config, owner);
      cJSON_Delete(config);
    }
    cJSON *mesh =
        cJSON_Parse("{\"bed_mesh\":{\"profile_name\":\"DEMO\",\"mesh_min\":[20,"
                    "20],\"mesh_max\":[220,220],\"probed_matrix\":[[0.10,0.02,-"
                    "0.06],[0.04,0,-0.03],[0.09,0.01,-0.08]]}}");
    if (mesh) {
      bed_mesh_controller_merge_status(mesh);
    }
    cJSON_Delete(mesh);
  }
  if (index == 0) {
    char json[128];
    int n = snprintf(
        json, sizeof(json),
        "{\"result\":{\"status\":{\"output_pin case_led\":{\"value\":%.2f}}}}",
        lights[index] / 100.);
    case_lighting_status_json(json, (size_t)n, owner);
  }
}
bool demo_printer_init(void) {
  if (!demo_mode_enabled())
    return false;
  mutex = xSemaphoreCreateMutex();
  if (!mutex)
    return false;
  moonraker_profile_t profiles[MOONRAKER_CONFIG_MAX_PROFILES] = {0};
  for (int i = 0; i < 2; i++) {
    profiles[i].configured = true;
    profiles[i].port = 7125;
    snprintf(profiles[i].name, sizeof(profiles[i].name), "DEMO %s",
             i ? "Workbench" : "Printing");
    strcpy(profiles[i].host, "demo.invalid");
  }
  if (!moonraker_config_replace_profiles(profiles,
                                         MOONRAKER_CONFIG_MAX_PROFILES, 0))
    return false;
  if(!demo_services_init() || !demo_camera_seed() || !demo_calibration_init())return false;
  for (int i = 0; i < MOONRAKER_CONFIG_MAX_PROFILES; i++) {
    moonraker_state_t *s = &states[i];
    s->speed_factor = s->flow_factor = 100;
    s->nozzle_temp = 205;
    s->bed_temp = 59;
    s->chamber_temp = 29;
    s->air_temp = 24;
    s->humidity = 32;
    s->toolhead_position_valid = true;
    strcpy(s->homed_axes, "xyz");
    s->toolhead_x = 110;
    s->toolhead_y = 110;
    s->toolhead_z = 7;
    s->hotend_count = 1;
    strcpy(s->active_hotend, "extruder");
    strcpy(s->hotends[0].object_name, "extruder");
    s->hotends[0].active = true;
    s->capabilities = (moonraker_capabilities_t){.discovered = true,
                                                 .has_heated_bed = true,
                                                 .has_part_fan = true,
                                                 .has_exclude_object = true,
                                                 .has_bed_mesh = true,
                                                 .filament_sensor_count = 1};
    s->capabilities.has_drybox_center_sensor = i == 0;
    s->capabilities.has_drybox_environment_sensor = i == 0;
    s->capabilities.has_drybox_heater = i == 0;
    s->capabilities.has_drybox_fan = i == 0;
    s->capabilities.has_drybox_macros = i == 0;
    sensors[i] = filament_present[i] = true;
    printer_simulation_scenario(s, i ? "idle" : "printing");
  }
  last_us = esp_timer_get_time();
  publish();
  return true;
}
void demo_printer_tick(void) {
  if (!demo_mode_enabled() || !mutex)
    return;
  xSemaphoreTake(mutex, portMAX_DELAY);
  int64_t now = esp_timer_get_time();
  double dt = (now - last_us) / 1000000.;
  last_us = now;
  for (int i = 0; i < MOONRAKER_CONFIG_MAX_PROFILES; i++)
    printer_simulation_advance(&states[i], dt);
  publish();
  xSemaphoreGive(mutex);
}
/* Caller owns mutex. Files and literal console starts share this same guard. */
static bool start_file(const char *path) {
  int i = active_index();
  if (!demo_files_find(path) ||
      !printer_simulation_start_file(&states[i], path))
    return false;
  excluded[i][0] = excluded[i][1] = false;
  return true;
}
bool demo_printer_start_file(const char *path) {
  if (!demo_mode_enabled() || !mutex)
    return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = start_file(path);
  if (ok)
    publish();
  xSemaphoreGive(mutex);
  return ok;
}
bool demo_printer_command(const char *command) {
  if (!demo_mode_enabled() || !mutex)
    return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  int i = active_index();
  bool ok = false;
  const char *prefix = "SDCARD_PRINT_FILE FILENAME=\"";
  if (command && !strncmp(command, prefix, strlen(prefix))) {
    const char *path = command + strlen(prefix);
    size_t length = strlen(path);
    char file[160];
    if (length > 1 && length - 1 < sizeof(file) && path[length - 1] == '"') {
      memcpy(file, path, length - 1);
      file[length - 1] = 0;
      ok = start_file(file);
    }
  } else if (printer_simulation_is_macro(command))
    ok = printer_simulation_macro_apply(&states[i], command,
                                        &filament_present[i]);
  else {
    ok =
        printer_simulation_apply(&states[i], excluded[i], &sensors[i], command);
  }
  if (ok) {
    if (printer_simulation_macro_starts_job(command))
      excluded[i][0] = excluded[i][1] = false;
    publish();
  }
  xSemaphoreGive(mutex);
  return ok;
}
bool demo_printer_scenario(const char *scenario) {
  if (!demo_mode_enabled() || !mutex)
    return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  int index = active_index();
  bool ok = printer_simulation_scenario(&states[index], scenario);
  if (ok) {
    if (!strcmp(scenario, "idle") || !strcmp(scenario, "printing") ||
        !strcmp(scenario, "paused"))
      excluded[index][0] = excluded[index][1] = false;
    publish();
  }
  xSemaphoreGive(mutex);
  return ok;
}
bool demo_printer_light(uint32_t generation, int percent) {
  if (!demo_mode_enabled() || !mutex ||
      generation != moonraker_config_generation() || percent < 0 ||
      percent > 100)
    return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = active_index() == 0 && states[0].live_data_ok;
  if (ok) {
    lights[0] = percent;
    publish();
  }
  xSemaphoreGive(mutex);
  return ok;
}

bool demo_printer_endstops(uint32_t request) {
  if (!demo_mode_enabled() || !mutex)
    return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = demo_services_endstops(&states[active_index()], request);
  xSemaphoreGive(mutex);
  return ok;
}

bool demo_printer_calibration_step(double amount) {
  if (!demo_mode_enabled() || !mutex || !isfinite(amount) || amount < -1 || amount > 1) return false;
  xSemaphoreTake(mutex, portMAX_DELAY);
  int index = active_index();
  states[index].toolhead_z += amount;
  publish();
  xSemaphoreGive(mutex);
  return true;
}
