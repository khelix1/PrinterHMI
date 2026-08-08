#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef struct {
    lv_obj_t *active_panel;
    lv_obj_t *status_panel;
    lv_obj_t *action_panel;
} ui_printer_layout_v32_t;

/*
 * Builds the same 20px / 800px content grid used by Drybox.
 *
 * Banner remains at:
 *   x=20, y=52, width=800, height=54
 *
 * Printer content:
 *   Active Print: x=20, y=118, width=800, height=220
 *   Status row:   x=20, y=350, width=800, height=94
 *   Actions:      x=20, y=456, width=800, height=54
 */
bool ui_printer_layout_v32_create(
    lv_obj_t *page,
    ui_printer_layout_v32_t *layout);

void ui_printer_layout_v32_clear_refs(
    ui_printer_layout_v32_t *layout);
