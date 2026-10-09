#pragma once

#include "lvgl.h"
#include "ui_theme.h"
#include <stdint.h>

/* A native wrapping row owns positioning; only equal card widths are resolved. */
static inline void ui_responsive_cards_update(lv_event_t *event)
{
    lv_obj_t *row = lv_event_get_target_obj(event);
    int32_t available = lv_obj_get_content_width(row);
    int32_t minimum = (int32_t)(intptr_t)lv_event_get_user_data(event);
    int32_t gap = lv_obj_get_style_pad_column(row, 0);
    int32_t columns = available >= minimum * 2 + gap ? 2 : 1;
    int32_t width = (available - (columns - 1) * gap) / columns;
    if (width < 1) width = 1;
    for (uint32_t i = 0; i < lv_obj_get_child_count(row); ++i) {
        lv_obj_t *child = lv_obj_get_child(row, i);
        if (lv_obj_check_type(child, &lv_button_class) &&
            lv_obj_get_style_width(child, 0) != width) lv_obj_set_width(child, width);
    }
}

static inline void ui_responsive_cards(lv_obj_t *row, int32_t minimum_width)
{
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(row, UI_GAP_CARD, 0);
    lv_obj_set_style_pad_column(row, UI_GAP_CARD, 0);
    lv_obj_add_event_cb(row, ui_responsive_cards_update, LV_EVENT_SIZE_CHANGED,
                        (void *)(intptr_t)minimum_width);
}

static inline void ui_responsive_column(lv_obj_t *column)
{
    ui_apply_surface_role(column, UI_SURFACE_TRANSPARENT);
    lv_obj_set_style_pad_all(column, 0, 0);
    lv_obj_set_size(column, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(column, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(column, UI_GAP_ROW, 0);
    lv_obj_clear_flag(column, LV_OBJ_FLAG_SCROLLABLE);
}

static inline void ui_responsive_action(lv_obj_t *button)
{
    lv_obj_set_size(button, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(button, 44, 0);
    lv_obj_set_style_pad_ver(button, 8, 0);
    lv_obj_set_style_pad_hor(button, 12, 0);
}
