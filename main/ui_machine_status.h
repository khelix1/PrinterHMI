#pragma once

#include "lvgl.h"
#include "moonraker.h"
#include "ui_dashboard_layout_profile.h"

lv_obj_t *ui_machine_status_create(
    lv_obj_t *parent,
    int x,
    int y,
    int w,
    int h);

lv_obj_t *ui_machine_status_create_profile(
    lv_obj_t *parent,
    const ui_dashboard_rect_t *rect,
    const ui_dashboard_machine_layout_t *layout);

void ui_machine_status_set_filament(
    lv_obj_t *panel,
    bool moonraker_online,
    const moonraker_filament_state_t *state);

void ui_machine_status_set_connection(
    lv_obj_t *panel,
    bool online);

void ui_machine_status_set_active_hotend(
    lv_obj_t *panel,
    const char *name,
    const char *value);

void ui_machine_status_set(
    lv_obj_t *panel,
    const char *nozzle,
    const char *bed,
    const char *chamber,
    const char *humidity,
    const char *speed,
    const char *flow,
    const char *fan
);
