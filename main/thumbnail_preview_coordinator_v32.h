#pragma once

#include <stddef.h>

#include "lvgl.h"

typedef void (*thumbnail_preview_coordinator_v32_void_fn_t)(void);

typedef void (*thumbnail_preview_coordinator_v32_metadata_fn_t)(
    const char *file,
    char *out,
    size_t out_size);

typedef struct {
    const char *printer_state;
    const char *printer_file;

    const char *moonraker_host;
    int moonraker_port;

    char *selected_print_file;
    size_t selected_print_file_size;

    char *selected_thumbnail_path;

    char *dashboard_canvas_file;
    char *printer_canvas_file;

    lv_obj_t **dashboard_canvas;
    lv_obj_t **dashboard_image;
    lv_obj_t **printer_canvas;
    lv_obj_t **printer_image;

    char *metadata_info;
    size_t metadata_info_size;

    thumbnail_preview_coordinator_v32_void_fn_t set_live_target;
    thumbnail_preview_coordinator_v32_void_fn_t free_thumbnail;
    thumbnail_preview_coordinator_v32_metadata_fn_t build_metadata;
    thumbnail_preview_coordinator_v32_void_fn_t start_delayed;
} thumbnail_preview_coordinator_v32_context_t;

void thumbnail_preview_coordinator_v32_reset(void);

void thumbnail_preview_coordinator_v32_update(
    thumbnail_preview_coordinator_v32_context_t *context);
