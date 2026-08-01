#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"

typedef void (*dashboard_runtime_void_cb_t)(void);
typedef void (*dashboard_runtime_metadata_cb_t)(
    const char *file,
    char *out,
    size_t out_size);

typedef struct {
    double current_z;
    double meta_object_height;
    double meta_layer_height;

    lv_obj_t *nozzle_label;
    lv_obj_t *bed_label;
    lv_obj_t *chamber_label;
    lv_obj_t *humidity_label;
    lv_obj_t *target_rh_label;
    lv_obj_t *heater_label;
    lv_obj_t *fan_label;
    lv_obj_t *moonraker_label;

    lv_obj_t **dashboard_canvas;
    lv_obj_t **dashboard_image;

    char *last_print_state;
    size_t last_print_state_size;

    dashboard_runtime_void_cb_t set_live_target;
    dashboard_runtime_void_cb_t free_thumbnail;
    dashboard_runtime_metadata_cb_t build_metadata;
    dashboard_runtime_void_cb_t start_delayed;
} dashboard_runtime_context_t;

void dashboard_runtime_controller_tick(
    const dashboard_runtime_context_t *context);
