#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "ui_settings_popups.h"

extern lv_obj_t *settings_panel;
extern lv_obj_t *settings_sleep_label;

typedef lv_obj_t *(*ui_settings_make_info_cb_t)(
    lv_obj_t *parent,
    const char *title,
    const char *value,
    int x,
    int y);

typedef void (*ui_settings_theme_rebuild_cb_t)(void);

void ui_settings_module_init(void);

void ui_settings_show_page(
    const char *sd_card_text,
    const char *storage_text,
    lv_event_cb_t ota_cb,
    ui_settings_theme_rebuild_cb_t theme_rebuild_cb);

void ui_settings_refresh(void);
void hide_settings_tab(void);

int ui_settings_brightness_percent(void);
uint8_t ui_settings_sleep_timeout_minutes(void);

bool ui_settings_restore_display_preferences(
    int brightness_percent,
    uint8_t sleep_timeout_minutes);

void settings_reboot_cb(lv_event_t *e);


void settings_sleep_card_cb(lv_event_t *e);
