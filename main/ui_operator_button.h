#pragma once

#include "ui_theme.h"

/*
 * Shared Operator button.
 *
 * A plain clickable LVGL object is used instead of lv_button_create(),
 * preventing the stock LVGL button appearance from leaking through.
 */
static inline lv_obj_t *ui_operator_button_create(
    lv_obj_t *parent,
    const char *text,
    ui_status_kind_t kind,
    lv_event_cb_t event_cb,
    void *user_data)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *button =
        lv_obj_create(parent);

    lv_obj_clear_flag(
        button,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_flag(
        button,
        LV_OBJ_FLAG_CLICKABLE);

    ui_apply_button_status_style(
        button,
        kind);

    lv_obj_set_style_pad_all(
        button,
        0,
        0);

    lv_obj_set_style_margin_all(
        button,
        0,
        0);

    if (event_cb) {
        lv_obj_add_event_cb(
            button,
            event_cb,
            LV_EVENT_CLICKED,
            user_data);
    }

    lv_obj_t *label =
        lv_label_create(button);

    lv_label_set_text(
        label,
        text ? text : "");

    ui_apply_text_button(label);
    ui_apply_label_bright(label);

    lv_obj_center(label);

    return button;
}
