#pragma once

#include "lvgl.h"

lv_obj_t *ui_command_bar_create(lv_obj_t *parent, int x, int y, int w, int h);
void ui_command_bar_update(const char *printer_state,
                               bool cancel_object_available);

/* Implemented by main.c so the current UI can reuse existing app actions. */
void ui_command_bar_action(const char *action);
