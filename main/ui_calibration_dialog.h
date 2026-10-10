#pragma once

#include <stdint.h>
#include "ui_popup.h"
#include "ui_responsive_layout.h"
#include "moonraker_config_controller.h"
#include "ui_toast.h"
#include "ui_text_fit.h"

/* Calibration modal structure: title, scrolling body, pinned action row.
 * No transport, timer or command ownership is introduced by layout helpers. */
static inline lv_obj_t *ui_cal_dialog_create(lv_obj_t *parent, int32_t width,
                                            int32_t height, ui_popup_kind_t kind)
{
    int32_t available = lv_display_get_horizontal_resolution(NULL) - 32;
    if (width > available) width = available;
    available = lv_display_get_vertical_resolution(NULL) - 32;
    if (height > available) height = available;
    lv_obj_t *popup = ui_popup_create(parent, width, height, kind);
    if (!popup) return NULL;
    lv_obj_set_user_data(popup, (void *)(uintptr_t)moonraker_config_generation());
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup, 16, 0);
    lv_obj_set_style_pad_row(popup, UI_GAP_ROW, 0);
    lv_obj_t *title = lv_label_create(popup);
    lv_label_set_text(title, "");
    ui_apply_custom_label_style(title, UI_FONT_TITLE, UI_TEXT);
    lv_obj_set_size(title, LV_PCT(100), UI_FONT_TITLE->line_height);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_t *body = lv_obj_create(popup);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body, LV_PCT(100), 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, UI_GAP_ROW, 0);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(body, UI_TEXT_MUTED, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(body, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_t *footer = lv_obj_create(popup);
    ui_responsive_column(footer);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(footer, UI_GAP_ROW, 0);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_cal_dialog_register(popup);
    return popup;
}

static inline lv_obj_t *ui_cal_dialog_title(lv_obj_t *popup, const char *text)
{
    lv_obj_t *title = lv_obj_get_child(popup, 0);
    lv_label_set_text(title, text);
    return title;
}
static inline lv_obj_t *ui_cal_dialog_body(lv_obj_t *popup)
{
    return lv_obj_get_child(popup, 1);
}
static inline lv_obj_t *ui_cal_dialog_text(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    ui_apply_custom_label_style(label, UI_FONT_BODY, UI_TEXT_DIM);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    return label;
}
static inline void ui_cal_dialog_owner_guard(lv_event_t *event)
{
    uint32_t owner = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
    if (owner == moonraker_config_generation()) return;
    ui_cal_dialog_notice(UI_STATUS_WARNING, "PRINTER CHANGED", "Close and reopen calibration for this printer.");
    lv_event_stop_processing(event);
}
static inline void ui_cal_dialog_fit_action(lv_event_t *event)
{
    lv_obj_t *label = lv_obj_get_child(lv_event_get_target_obj(event), 0);
    ui_text_fit_single_line(label, UI_FONT_BODY);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
}
static inline lv_obj_t *ui_cal_dialog_button(lv_obj_t *parent,
    ui_popup_action_t action, const char *text, int32_t width,
    lv_event_cb_t callback, void *data, lv_obj_t **label)
{
    lv_obj_t *button = ui_popup_add_action_at(parent, action, text,
        0, 0, width, 48, NULL, NULL, label);
    if (!button) return NULL;
    lv_obj_add_event_cb(button, ui_cal_dialog_fit_action, LV_EVENT_SIZE_CHANGED, NULL);
    ui_text_fit_single_line(lv_obj_get_child(button, 0), UI_FONT_BODY);
    lv_label_set_long_mode(lv_obj_get_child(button, 0), LV_LABEL_LONG_CLIP);
    /* Close/Back remain usable after a printer switch; command choices do not. */
    if (action != UI_POPUP_ACTION_CLOSE && action != UI_POPUP_ACTION_CANCEL) {
        lv_obj_t *popup = ui_popup_find_owner(parent);
        lv_obj_add_event_cb(button, ui_cal_dialog_owner_guard, LV_EVENT_CLICKED,
            popup ? lv_obj_get_user_data(popup) : (void *)(uintptr_t)moonraker_config_generation());
    }
    if (callback) lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, data);
    return button;
}
static inline lv_obj_t *ui_cal_dialog_action(lv_obj_t *popup,
    ui_popup_action_t action, const char *text, lv_event_cb_t callback,
    void *data, lv_obj_t **label)
{
    lv_obj_t *button = ui_cal_dialog_button(lv_obj_get_child(popup, 2), action,
        text, action == UI_POPUP_ACTION_CLOSE ? 128 : LV_PCT(48), callback, data, label);
    if (button && action != UI_POPUP_ACTION_CLOSE) lv_obj_set_flex_grow(button, 1);
    return button;
}
static inline lv_obj_t *ui_cal_dialog_choice(lv_obj_t *popup,
    ui_popup_action_t action, const char *text, int32_t width,
    lv_event_cb_t callback, void *data, lv_obj_t **label)
{
    lv_obj_t *body = ui_cal_dialog_body(popup);
    lv_obj_t *row = lv_obj_get_user_data(body);
    if (!row) {
        row = lv_obj_create(body);
        ui_responsive_column(row);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_column(row, UI_GAP_ROW, 0);
        lv_obj_set_user_data(body, row);
    }
    return ui_cal_dialog_button(row, action, text, width, callback, data, label);
}
static inline lv_obj_t *ui_cal_dialog_select(lv_obj_t *parent, const char *text,
                                             lv_event_cb_t callback, void *data)
{
    return ui_cal_dialog_button(parent, UI_POPUP_ACTION_CHOICE, text,
        LV_PCT(100), callback, data, NULL);
}
