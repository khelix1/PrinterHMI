#include "printer_simulation_catalog.h"
#include "printer_simulation.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *const macros[] = {"BED_HEAT_SOAK",
                                     "CLEAN_NOZZLE",
                                     "DRY_PLA",
                                     "DRY_PETG",
                                     "DRY_HOLD",
                                     "DRY_RESUME",
                                     "DRY_STOP",
                                     "G34",
                                     "LOAD_FILAMENT",
                                     "PARK_TOOLHEAD",
                                     "PID_BED",
                                     "PRINT_TEST",
                                     "START_PRINT",
                                     "STOP_PRINT",
                                     "UNLOAD_FILAMENT",
                                     "Z_TILT_ALIGN",
                                     "HMI_FILAMENT_HEAT",
                                     "HMI_FILAMENT_UNLOAD",
                                     "HMI_FILAMENT_LOAD",
                                     "HMI_FILAMENT_PURGE"};
static bool add(cJSON *array, const char *name) {
  cJSON *item = cJSON_CreateString(name);
  if (!item)
    return false;
  if (!cJSON_AddItemToArray(array, item)) {
    cJSON_Delete(item);
    return false;
  }
  return true;
}
cJSON *printer_simulation_device_objects(bool full) {
  const char *const names[] = {"extruder",
                               "heater_bed",
                               "fan",
                               "heater_generic drybox_heater",
                               "fan_generic drybox_fan",
                               "temperature_sensor drybox_center",
                               "sht3x drybox_env",
                               "filament_switch_sensor runout",
                               "toolhead",
                               "tmc2209 stepper_x",
                               "tmc2209 stepper_y",
                               "tmc2209 stepper_z",
                               "output_pin case_led",
                               "neopixel status",
                               "power printer",
                               "probe",
                               "bed_mesh",
                               "gcode_macro START_PRINT",
                               "screws_tilt_adjust",
                               "z_tilt",
                               "axis_twist_compensation",
                               "input_shaper",
                               "adxl345",
                               "bltouch"};
  cJSON *array = cJSON_CreateArray();
  if (!array)
    return NULL;
  for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    if (full || i < 3 || i == 8)
      if (!add(array, names[i])) {
        cJSON_Delete(array);
        return NULL;
      }
  return array;
}
cJSON *printer_simulation_macro_objects(bool full) {
  cJSON *array = cJSON_CreateArray();
  if (!array)
    return NULL;
  for (unsigned i = 0; i < sizeof(macros) / sizeof(macros[0]); i++) {
    if (!full && strcmp(macros[i], "BED_HEAT_SOAK") &&
        strcmp(macros[i], "CLEAN_NOZZLE") && strcmp(macros[i], "PARK_TOOLHEAD"))
      continue;
    char name[96];
    snprintf(name, sizeof(name), "gcode_macro %s", macros[i]);
    if (!add(array, name)) {
      cJSON_Delete(array);
      return NULL;
    }
  }
  return array;
}
cJSON *printer_simulation_macro_config(void) {
  const char *const sections[] = {
      "BED_HEAT_SOAK",     "CLEAN_NOZZLE",        "START_PRINT",
      "LOAD_FILAMENT",     "UNLOAD_FILAMENT",     "HMI_FILAMENT_HEAT",
      "HMI_FILAMENT_LOAD", "HMI_FILAMENT_UNLOAD", "HMI_FILAMENT_PURGE"};
  const char *const gcode[] = {
      "{% set minutes = params.MINUTES|default(2)|float %}",
      "{% set temp = params.TEMP|default(210)|float %}\n{% set speed = "
      "params.SPEED|default(5)|float %}",
      "{% set bed = params.BED_TEMP|float %}\n{% set hotend = "
      "params.EXTRUDER_TEMP|float %}\n{% set idle = "
      "params.IDLE_TEMP|default(150)|float %}\n{% set margin = "
      "params.MARGIN|default(10)|float %}",
      "{% set length = params.LENGTH|default(50)|float %}",
      "{% set length = params.LENGTH|default(50)|float %}",
      "{% set temp = params.TEMP|default(210)|float %}",
      "{% set temp = params.TEMP|default(210)|float %}\n{% set length = "
      "params.LENGTH|default(50)|float %}",
      "{% set temp = params.TEMP|default(210)|float %}\n{% set length = "
      "params.LENGTH|default(50)|float %}",
      "{% set temp = params.TEMP|default(210)|float %}\n{% set length = "
      "params.LENGTH|default(10)|float %}"};
  cJSON *config = cJSON_CreateObject();
  if (!config)
    return NULL;
  for (unsigned i = 0; i < sizeof(sections) / sizeof(sections[0]); i++) {
    char name[96];
    snprintf(name, sizeof(name), "gcode_macro %s", sections[i]);
    cJSON *definition = cJSON_AddObjectToObject(config, name);
    if (!definition ||
        !cJSON_AddStringToObject(definition, "gcode", gcode[i])) {
      cJSON_Delete(config);
      return NULL;
    }
  }
  return config;
}
cJSON *printer_simulation_device_status(const moonraker_state_t *s,
                                        double light) {
  if (!s)
    return NULL;
  cJSON *root = cJSON_CreateObject();
  if (!root)
    return NULL;
  const char *const names[] = {"extruder",
                               "heater_bed",
                               "heater_generic drybox_heater",
                               "temperature_sensor drybox_center",
                               "sht3x drybox_env",
                               "output_pin case_led"};
  for (unsigned i = 0; i < 6; i++) {
    cJSON *object = cJSON_AddObjectToObject(root, names[i]);
    if (!object) {
      cJSON_Delete(root);
      return NULL;
    }
    if (i < 4) {
      double temp = i == 0   ? s->nozzle_temp
                    : i == 1 ? s->bed_temp
                             : s->chamber_temp;
      if (!cJSON_AddNumberToObject(object, "temperature", temp))
        goto fail;
    }
    if (i < 3 && !cJSON_AddNumberToObject(object, "target",
                                          i == 0   ? s->nozzle_target
                                          : i == 1 ? s->bed_target
                                                   : s->heater_target))
      goto fail;
    if (i == 4 &&
        (!cJSON_AddNumberToObject(object, "temperature", s->air_temp) ||
         !cJSON_AddNumberToObject(object, "humidity", s->humidity)))
      goto fail;
    if (i == 5 && !cJSON_AddNumberToObject(object, "value", light))
      goto fail;
  }
  return root;
fail:
  cJSON_Delete(root);
  return NULL;
}
/* A bounded numeric parameter parser. Never expand Jinja or execute arbitrary
 * macro text; parse the exact supported names before applying a copied state.
 */
bool printer_simulation_macro_apply(moonraker_state_t *state,
                                    const char *command, bool *present) {
  if (!state || !command || !present || !state->moonraker_ok ||
      !state->live_data_ok || !strcmp(state->printer_state, "error") ||
      !strcmp(state->printer_state, "shutdown"))
    return false;
  char input[384];
  size_t length = strlen(command);
  if (!length || length >= sizeof(input))
    return false;
  for (size_t i = 0; i < length; i++)
    if ((unsigned char)command[i] < 32 || command[i] == ';')
      return false;
  memcpy(input, command, length + 1);
  char *space = strchr(input, ' ');
  if (space)
    *space++ = 0;
  bool known = false;
  for (unsigned i = 0; i < sizeof(macros) / sizeof(macros[0]); i++)
    if (!strcmp(input, macros[i]))
      known = true;
  if (!known)
    return false;
  const char *names[5] = {0};
  double values[5] = {0};
  unsigned count = 0;
  while (space && *space) {
    while (*space == ' ')
      space++;
    if (!*space)
      break;
    if (count == 5)
      return false;
    char *end = strchr(space, ' ');
    if (end)
      *end++ = 0;
    char *equal = strchr(space, '=');
    if (!equal || equal == space || !equal[1])
      return false;
    *equal++ = 0;
    for (unsigned i = 0; i < count; i++)
      if (!strcmp(names[i], space))
        return false;
    char *tail = NULL;
    double value = strtod(equal, &tail);
    if (tail == equal || *tail || !isfinite(value))
      return false;
    names[count] = space;
    values[count++] = value;
    space = end;
  }
  double temp = 210, bed = 60;
  bool have_bed = false, have_hotend = false;
  for (unsigned i = 0; i < count; i++) {
    const char *key = names[i];
    double value = values[i];
    bool valid = false;
    if (!strcmp(input, "BED_HEAT_SOAK"))
      valid = !strcmp(key, "MINUTES") && value >= 0 && value <= 120;
    else if (!strcmp(input, "CLEAN_NOZZLE")) {
      if (!strcmp(key, "TEMP")) {
        valid = value >= 0 && value <= 300;
        temp = value;
      } else
        valid = !strcmp(key, "SPEED") && value > 0 && value <= 50;
    } else if (!strcmp(input, "START_PRINT")) {
      if (!strcmp(key, "BED_TEMP")) {
        valid = value >= 0 && value <= 120;
        bed = value;
        have_bed = true;
      } else if (!strcmp(key, "EXTRUDER_TEMP")) {
        valid = value >= 0 && value <= 300;
        temp = value;
        have_hotend = true;
      } else if (!strcmp(key, "IDLE_TEMP"))
        valid = value >= 0 && value <= 300;
      else if (!strcmp(key, "MARGIN"))
        valid = value >= 0 && value <= 50;
    } else if (strstr(input, "FILAMENT")) {
      if (!strcmp(key, "TEMP") && !strncmp(input, "HMI_", 4)) {
        valid = value >= 0 && value <= 300;
        temp = value;
      } else
        valid = !strcmp(key, "LENGTH") && strcmp(input, "HMI_FILAMENT_HEAT") &&
                value > 0 && value <= 300;
    }
    if (!valid)
      return false;
  }
  moonraker_state_t next = *state;
  bool next_present = *present;
  if (!strncmp(input, "DRY_", 4)) {
    if(!strcmp(input,"DRY_RESUME") && !next.drybox_selected_program)return false;
    if (!next.capabilities.has_drybox_macros)
      return false;
    bool excluded[2] = {false, false}, enabled = true;
    if (!printer_simulation_apply(&next, excluded, &enabled, input))
      return false;
  } else if (!strcmp(input, "START_PRINT") || !strcmp(input, "PRINT_TEST")) {
    if (!strcmp(input, "START_PRINT") && (!have_bed || !have_hotend))
      return false;
    if (!printer_simulation_start_file(&next, !strcmp(input, "PRINT_TEST")
                                                  ? "Calibration cube.gcode"
                                                  : "Spool holder.gcode"))
      return false;
    next.bed_target = bed;
    next.nozzle_target = temp;
  } else if (!strcmp(input, "STOP_PRINT")) {
    strcpy(next.printer_state, "cancelled");
    next.nozzle_target = next.bed_target = next.part_fan_speed = 0;
  } else if (!strcmp(input, "BED_HEAT_SOAK") || !strcmp(input, "PID_BED"))
    next.bed_target = 60;
  else if (!strcmp(input, "G34") || !strcmp(input, "Z_TILT_ALIGN")) {
    next.toolhead_x = next.toolhead_y = next.toolhead_z = next.z_offset = 0;
    strcpy(next.homed_axes, "xyz");
  } else if (!strcmp(input, "PARK_TOOLHEAD")) {
    next.toolhead_x = 10;
    next.toolhead_y = 10;
    next.toolhead_z = 20;
  } else if (!strcmp(input, "CLEAN_NOZZLE")) {
    next.nozzle_target = temp;
    next.toolhead_x = 10;
    next.toolhead_y = 10;
  } else if (strstr(input, "FILAMENT")) {
    next.nozzle_target = temp;
    if (strstr(input, "UNLOAD"))
      next_present = false;
    else if (strstr(input, "LOAD"))
      next_present = true;
  } else
    return false;
  *state = next;
  *present = next_present;
  return true;
}
cJSON *printer_simulation_device_defaults(void) {
  return cJSON_Parse("{\"tmc2209 "
                     "stepper_x\":{\"run_current\":0.65,\"hold_current\":0.2,"
                     "\"temperature\":45.5,\"drv_status\":{}},\"tmc2209 "
                     "stepper_y\":{\"run_current\":0.8,\"hold_current\":0.25,"
                     "\"temperature\":43,\"drv_status\":{}},\"tmc2209 "
                     "stepper_z\":{\"run_current\":0.6,\"hold_current\":0.2,"
                     "\"temperature\":40,\"drv_status\":{}},\"neopixel "
                     "status\":{\"color_data\":[[0.2,0.8,0.3,0]]},\"power "
                     "printer\":{\"status\":\"on\"},\"toolhead\":{\"max_"
                     "velocity\":200,\"max_accel\":2500,\"square_corner_"
                     "velocity\":5,\"minimum_cruise_ratio\":0.5}}");
}
cJSON *printer_simulation_motion_config(void) {
  return cJSON_Parse(
      "{\"printer\":{\"kinematics\":\"cartesian\"},\"stepper_x\":{\"rotation_"
      "distance\":\"40\"},\"stepper_y\":{\"rotation_distance\":\"40\"},"
      "\"stepper_z\":{\"rotation_distance\":\"8\"}}");
}
cJSON *printer_simulation_endstops(const moonraker_state_t *s,
                                   uint32_t request) {
  if (!s)
    return NULL;
  cJSON *root = cJSON_CreateObject();
  if (!root)
    return NULL;
  cJSON *result = cJSON_AddObjectToObject(root, "result");
  if (!result || !cJSON_AddNumberToObject(root, "id", request))
    goto fail;
  const char *axes = "xyz";
  double position[] = {s->toolhead_x, s->toolhead_y, s->toolhead_z};
  for (unsigned i = 0; i < 3; i++) {
    char name[2] = {axes[i], 0};
    bool triggered = strchr(s->homed_axes, axes[i]) && position[i] <= 0;
    if (!cJSON_AddStringToObject(result, name,
                                 triggered ? "TRIGGERED" : "open"))
      goto fail;
  }
  return root;
fail:
  cJSON_Delete(root);
  return NULL;
}

static bool command_name(const char *command, const char *name) {
  size_t n = strlen(name);
  return command && !strncmp(command, name, n) &&
         (!command[n] || isspace((unsigned char)command[n]));
}
bool printer_simulation_is_macro(const char *command) {
  for (unsigned i = 0; i < sizeof(macros) / sizeof(macros[0]); i++)
    if (command_name(command, macros[i]))
      return true;
  return false;
}
bool printer_simulation_macro_starts_job(const char *command) {
  return command_name(command, "START_PRINT") ||
         command_name(command, "PRINT_TEST");
}
