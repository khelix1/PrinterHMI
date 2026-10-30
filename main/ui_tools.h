#pragma once

typedef void (*ui_tools_open_cb_t)(void);
void ui_tools_set_callbacks(ui_tools_open_cb_t calibration, ui_tools_open_cb_t mesh, ui_tools_open_cb_t devices, ui_tools_open_cb_t macros);
void ui_tools_show(void);
void ui_tools_hide(void);

/* Optional header action; existing Tools destinations keep their geometry. */
void ui_tools_set_lighting_callback(ui_tools_open_cb_t lighting);
