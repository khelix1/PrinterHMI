#pragma once

#include <stddef.h>

#include "lvgl.h"

void ui_printer_show(void);
void ui_printer_hide(void);
void ui_printer_refresh(void);

/* Printer-page thumbnail preview ownership. */
void ui_printer_preview_create(lv_obj_t *parent);

void ui_printer_preview_show(
    const char *printer_state,
    const char *printer_file,
    const char *selected_file);

void ui_printer_preview_reset(void);
void ui_printer_preview_destroy_refs(void);

char *ui_printer_preview_canvas_file(void);
size_t ui_printer_preview_canvas_file_size(void);

lv_obj_t **ui_printer_preview_canvas_ref(void);
lv_obj_t **ui_printer_preview_image_ref(void);

