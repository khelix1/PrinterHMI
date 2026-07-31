#pragma once

#include <stddef.h>

#include "lvgl.h"

void ui_devices_live_values_init(
    lv_obj_t *owner);

void ui_devices_live_values_clear(void);

void ui_devices_live_values_register(
    size_t visible_index,
    lv_obj_t *value_label,
    size_t catalog_index);

void ui_devices_live_values_update(void);
void ui_devices_live_values_close(void);
