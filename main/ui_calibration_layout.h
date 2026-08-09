#pragma once

#include <stddef.h>
#include "lvgl.h"

typedef struct {
    lv_obj_t *summary;
    lv_obj_t *status;
} ui_calibration_card_refs_t;

lv_obj_t *ui_calibration_layout_label(lv_obj_t *parent, const char *text,
                                      const lv_font_t *font, lv_color_t color,
                                      int x, int y, int width);
lv_obj_t *ui_calibration_layout_card(lv_obj_t *parent, const char *title,
                                     int x, int y,
                                     ui_calibration_card_refs_t *refs);
void ui_calibration_layout_append_tool(char *output, size_t output_size,
                                       const char *tool);
void ui_calibration_layout_set_card(ui_calibration_card_refs_t *refs,
                                    const char *summary, size_t count,
                                    size_t macro_count);
void ui_calibration_layout_set_action_label(lv_obj_t *button,
                                            const char *text);
