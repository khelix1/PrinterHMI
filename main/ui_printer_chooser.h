#pragma once

#include "lvgl.h"

typedef void (*ui_printer_chooser_select_cb_t)(int profile_index);
typedef void (*ui_printer_chooser_manage_cb_t)(int profile_index);

/*
 * Theme-owned multi-printer landing page.
 *
 * Only lightweight /server/info probes are made here. Full Moonraker polling
 * remains exclusively owned by the active printer.
 */
void ui_printer_chooser_show(
    ui_printer_chooser_select_cb_t select_cb,
    ui_printer_chooser_manage_cb_t manage_cb);

void ui_printer_chooser_hide(void);
bool ui_printer_chooser_is_visible(void);
void ui_printer_chooser_refresh(void);
