#pragma once

#include "lvgl.h"

typedef void (*ui_settings_timezone_changed_cb_t)(void);
typedef void (*ui_settings_theme_changed_cb_t)(void);

void reset_settings_cb(lv_event_t *e);

void ui_settings_popups_show_timezone(
    ui_settings_timezone_changed_cb_t changed_cb);

void ui_settings_popups_show_theme(
    ui_settings_theme_changed_cb_t changed_cb);

void ui_settings_popups_close_all(void);
