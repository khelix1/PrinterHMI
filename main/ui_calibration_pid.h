#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "device_catalog_controller.h"
#include "lvgl.h"

#define UI_CALIBRATION_PID_HEATER_MAX 8

typedef bool (*ui_calibration_pid_send_gcode_cb_t)(const char *command);
typedef void (*ui_calibration_pid_show_results_cb_t)(
    const char *title,
    const char *waiting_text);
typedef void (*ui_calibration_pid_refresh_results_cb_t)(void);

typedef struct {
    lv_obj_t **popup;
    lv_obj_t **target_label;
    char (*object_names)[DEVICE_CATALOG_OBJECT_NAME_MAX];
    char (*display_names)[DEVICE_CATALOG_DISPLAY_NAME_MAX];
    size_t heater_capacity;
    size_t *heater_count;
    size_t *selected_index;
    int *target;
    int *target_min;
    int *target_max;
    ui_calibration_pid_send_gcode_cb_t send_gcode;
    ui_calibration_pid_show_results_cb_t show_results;
    ui_calibration_pid_refresh_results_cb_t refresh_results;
} ui_calibration_pid_context_t;

void ui_calibration_pid_init(ui_calibration_pid_context_t *context);
bool ui_calibration_pid_printer_ready(void);
void ui_calibration_pid_event(lv_event_t *event);
void ui_calibration_pid_close(void);
