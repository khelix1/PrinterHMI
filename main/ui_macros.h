#pragma once

#include <stdbool.h>

typedef bool (*ui_macros_command_cb_t)(const char *command);

bool ui_macros_v32_init(void);
void ui_macros_v32_show(ui_macros_command_cb_t command_callback);
void ui_macros_v32_hide(void);
