#pragma once

#include <stdint.h>

#include "lvgl.h"
#include "ui_theme.h"

void ui_printer_banner_create(lv_obj_t *parent,
                              lv_obj_t **banner_label,
                              const char *initial_text);

void ui_printer_banner_refresh(lv_obj_t *parent,
                               lv_obj_t *banner_label,
                               lv_obj_t *state_label,
                               const char *banner_text,
                               const char *printer_state);

void ui_printer_banner_show_notice(const char *text,
                                   ui_status_kind_t kind,
                                   uint32_t duration_ms);
