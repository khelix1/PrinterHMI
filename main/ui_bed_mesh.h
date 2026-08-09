#pragma once
#include <stdbool.h>
#include "lvgl.h"
typedef void (*ui_bed_mesh_command_cb_t)(const char *command);
void ui_bed_mesh_show(ui_bed_mesh_command_cb_t command_cb);
void ui_bed_mesh_close(void);
bool ui_bed_mesh_is_open(void);
void ui_bed_mesh_refresh(void);
