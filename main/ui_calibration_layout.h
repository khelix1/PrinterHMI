#pragma once

#include <stddef.h>
#include <stdbool.h>
#include "lvgl.h"

typedef struct {
    lv_obj_t *summary;
    lv_obj_t *status;
    const char *purpose; /* Static page copy; unavailable/discovery detail stays dynamic. */
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

/* Two aligned action rows, shared by Motion diagnostics and measurements.
 * Call after the section's final width/height and padding have been set. */
static inline void ui_calibration_layout_motion_action(lv_obj_t *button,
                                                       unsigned column,
                                                       bool upper)
{
    if(!button)return;
    lv_obj_t *section=lv_obj_get_parent(button);
    lv_obj_update_layout(section);
    int width=(lv_obj_get_content_width(section)-48)/3;
    if(width<44)width=44;
    lv_obj_set_size(button,width,44);
    lv_obj_align(button,LV_ALIGN_BOTTOM_LEFT,16+(int)column*(width+8),upper?-62:-12);
}
