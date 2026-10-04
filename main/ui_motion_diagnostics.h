#pragma once
#include "lvgl.h"
/* Read-only controls and a local geometry calculator; no G-code dispatch. */
void ui_motion_diagnostics_create(lv_obj_t *card);
void ui_motion_diagnostics_hide(void);
