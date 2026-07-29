#pragma once

#include <stdbool.h>

typedef bool (*ui_console_command_cb_t)(const char *command);

void ui_console_v32_show(ui_console_command_cb_t command_callback);
void ui_console_v32_hide(void);
