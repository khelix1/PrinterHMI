#pragma once

#include "lvgl.h"
#include <stdbool.h>

lv_obj_t *ui_settings_section_create(
    lv_obj_t *parent,
    const char *title,
    int y,
    int h);

void ui_settings_section_add_divider(
    lv_obj_t *section,
    int y);

lv_obj_t *ui_settings_section_add_row(
    lv_obj_t *section,
    const char *title,
    const char *description,
    const char *value,
    int y,
    lv_event_cb_t event_cb);

lv_obj_t *ui_settings_section_add_percent_slider_row(
    lv_obj_t *section,
    const char *title,
    const char *description,
    int value,
    int minimum,
    int maximum,
    int y,
    lv_event_cb_t event_cb);

lv_obj_t *ui_settings_section_add_action_row(
    lv_obj_t *section,
    const char *title,
    const char *description,
    const char *button_text,
    int y,
    lv_event_cb_t event_cb,
    bool danger);
