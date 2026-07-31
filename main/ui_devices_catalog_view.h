#pragma once

#include "lvgl.h"

void ui_devices_catalog_view_create(
    lv_obj_t *owner,
    lv_obj_t *banner_status);

void ui_devices_catalog_view_refresh(void);
void ui_devices_catalog_view_close(void);
