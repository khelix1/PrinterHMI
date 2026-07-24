#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef bool (*ui_printer_live_status_send_gcode_cb_t)(
    const char *command);

void ui_printer_live_status_create(
    lv_obj_t *parent,
    lv_obj_t **active_file_label,
    lv_obj_t **speed_label,
    lv_obj_t **flow_label,
    lv_obj_t **layer_label,
    double initial_speed_factor,
    double initial_flow_factor,
    ui_printer_live_status_send_gcode_cb_t send_gcode_cb);

void ui_printer_live_status_refresh(
    lv_obj_t *printer_panel,
    lv_obj_t *active_file_box,
    lv_obj_t *active_file_label,
    lv_obj_t *speed_label,
    lv_obj_t *flow_label,
    lv_obj_t *layer_label,
    const char *printer_state,
    const char *printer_file,
    double printer_live_velocity,
    double printer_live_flow,
    double printer_speed_factor,
    double printer_flow_factor,
    int printer_current_layer,
    int printer_total_layer,
    double printer_meta_object_height,
    double printer_meta_layer_height,
    double printer_progress);
