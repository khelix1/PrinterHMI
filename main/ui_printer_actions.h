#pragma once

#include "lvgl.h"

typedef struct {
    lv_obj_t *motion;
    lv_obj_t *home;
    lv_obj_t *pause;
    lv_obj_t *resume;
    lv_obj_t *object;
    lv_obj_t *cancel;
} ui_printer_actions_t;

void ui_printer_actions_create(lv_obj_t *parent,
                               ui_printer_actions_t *actions,
                               lv_event_cb_t command_cb,
                               lv_event_cb_t motion_cb);
