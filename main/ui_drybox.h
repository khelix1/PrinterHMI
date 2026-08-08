#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef enum {
    UI_DRYBOX_PROGRAM_NONE = 0,
    UI_DRYBOX_PROGRAM_PLA,
    UI_DRYBOX_PROGRAM_PETG,
    UI_DRYBOX_PROGRAM_HOLD
} ui_drybox_program_v32_t;

typedef bool (*ui_drybox_v32_command_cb_t)(
    const char *command);

typedef void (*ui_drybox_v32_status_cb_t)(
    lv_event_t *event);

void ui_drybox_v32_set_callbacks(
    ui_drybox_v32_command_cb_t command_cb,
    ui_drybox_v32_status_cb_t status_cb);

void ui_drybox_v32_show(void);
void ui_drybox_v32_hide(void);
void ui_drybox_v32_refresh(void);
