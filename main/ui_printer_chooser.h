#pragma once

#include "lvgl.h"

typedef void (*ui_printer_chooser_select_cb_t)(int profile_index);
typedef void (*ui_printer_chooser_manage_cb_t)(int profile_index);

/*
 * Theme-owned multi-printer landing page.
 *
 * Renders the active snapshot and the background worker's inactive health.
 */
void ui_printer_chooser_show(
    ui_printer_chooser_select_cb_t select_cb,
    ui_printer_chooser_manage_cb_t manage_cb);

void ui_printer_chooser_hide(void);
void ui_printer_chooser_refresh(void);
/* Read under the display lock. */
bool ui_printer_chooser_is_open(void);
