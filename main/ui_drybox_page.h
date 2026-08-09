#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "ui_drybox.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_drybox_page_action_cb_t)(
    const char *command,
    lv_event_t *event);

typedef const char *(*ui_drybox_page_banner_text_cb_t)(void);

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *banner_label;
    lv_obj_t *air_label;
    lv_obj_t *center_label;
    lv_obj_t *humidity_label;
    lv_obj_t *target_label;
    lv_obj_t *heater_label;
    lv_obj_t *fan_label;
} ui_drybox_page_t;

typedef struct {
    const char *banner_text;
    float air_temp;
    float center_temp;
    float humidity;
    float heater_target;
    bool heater_on;
    float fan_speed;
    ui_drybox_program_t active_program;
} ui_drybox_page_state_t;


bool ui_drybox_page_create(
    ui_drybox_page_t *page,
    ui_drybox_page_action_cb_t action_cb,
    ui_drybox_page_banner_text_cb_t banner_text_cb);

void ui_drybox_page_refresh(
    const ui_drybox_page_t *page,
    const ui_drybox_page_state_t *state);

void ui_drybox_page_cleanup(
    ui_drybox_page_t *page);

#ifdef __cplusplus
}
#endif
