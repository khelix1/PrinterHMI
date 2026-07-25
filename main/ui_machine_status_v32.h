#pragma once

#include "lvgl.h"

lv_obj_t *ui_machine_status_v32_create(lv_obj_t *parent, int x, int y, int w, int h);

void ui_machine_status_v32_set_connection(
    lv_obj_t *panel,
    bool online);

void ui_machine_status_v32_set_active_hotend(
    lv_obj_t *panel,
    const char *name,
    const char *value);

void ui_machine_status_v32_set(
    lv_obj_t *panel,
    const char *nozzle,
    const char *bed,
    const char *chamber,
    const char *humidity,
    const char *speed,
    const char *flow,
    const char *fan
);
