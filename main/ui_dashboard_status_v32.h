#pragma once

#include "lvgl.h"

typedef struct {
        lv_obj_t *root;
lv_obj_t *state;
    lv_obj_t *progress;
    lv_obj_t *progress_bar;
    lv_obj_t *elapsed;
    lv_obj_t *remaining;
    lv_obj_t *eta;

    lv_obj_t *nozzle;
    lv_obj_t *bed;
} ui_dashboard_status_v32_t;

ui_dashboard_status_v32_t ui_dashboard_status_v32_create(lv_obj_t *parent);

void ui_dashboard_status_v32_set_print_state(const char *state);
void ui_dashboard_status_v32_set_progress(const char *progress_text,
                                          int progress_pct,
                                          lv_color_t progress_color);

lv_color_t ui_dashboard_status_v32_progress_color(double progress);

void ui_dashboard_status_v32_refresh(double progress,
                                     double print_duration_seconds);
void ui_dashboard_status_v32_set_times(const char *elapsed,
                                       const char *remaining,
                                       const char *eta);
