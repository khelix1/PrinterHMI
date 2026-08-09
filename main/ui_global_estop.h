#pragma once

#include <stdbool.h>
#include "lvgl.h"

typedef bool (*ui_global_estop_send_gcode_cb_t)(const char *command);

bool ui_global_estop_init(ui_global_estop_send_gcode_cb_t send_gcode);
void ui_global_estop_create(lv_obj_t *parent);
void ui_global_estop_set_printer_name(const char *printer_name);
void ui_global_estop_show_restart_confirmation(void);
