#pragma once

#include "lvgl.h"

const char *settings_system_info_idf_version(void);

void settings_system_info_bind_idf_label(lv_obj_t *label);
void settings_system_info_bind_heap_label(lv_obj_t *label);
void settings_system_info_bind_psram_label(lv_obj_t *label);
void settings_system_info_bind_uptime_label(lv_obj_t *label);

void settings_system_info_refresh(void);
void settings_system_info_unbind(void);
