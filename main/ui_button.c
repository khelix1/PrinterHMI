#include "ui_button.h"
#include "ui_text.h"

#include "ui_theme.h"

void ui_button_apply_kind(
    lv_obj_t *button,
    ui_button_kind_t kind)
{
    if (!button) {
        return;
    }

    switch (kind) {
        case UI_BUTTON_SECONDARY:
            ui_apply_button_dark_style(button);
            break;

        case UI_BUTTON_SUCCESS:
            ui_apply_button_success_style(button);
            break;

        case UI_BUTTON_WARNING:
            ui_apply_button_warning_style(button);
            break;

        case UI_BUTTON_DANGER:
            ui_apply_button_danger_style(button);
            break;

        case UI_BUTTON_CANCEL:
            ui_apply_button_cancel_style(button);
            break;

        case UI_BUTTON_CLOSE:
            ui_apply_button_close_style(button);
            break;

        case UI_BUTTON_OUTLINED:
            ui_apply_button_outlined_style(button);
            break;

        case UI_BUTTON_PRIMARY:
        default:
            ui_apply_button_style(button);
            break;
    }
}

lv_obj_t *ui_button_create_label(
    lv_obj_t *button,
    const char *text)
{
    if (!button) {
        return NULL;
    }

    lv_obj_t *label =
        lv_label_create(button);

    if (!label) {
        return NULL;
    }

    lv_label_set_text(
        label,
        text ? text : ui_text(""));

    ui_apply_text_button(label);
    ui_apply_label_bright(label);

    lv_obj_center(label);

    return label;
}

lv_obj_t *ui_button_create_empty(
    lv_obj_t *parent,
    ui_button_kind_t kind)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *button =
        lv_button_create(parent);

    if (!button) {
        return NULL;
    }

    ui_button_apply_kind(
        button,
        kind);

    ui_button_expand_touch_target(button);

    return button;
}


lv_obj_t *ui_button_create(
    lv_obj_t *parent,
    ui_button_kind_t kind,
    const char *text)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *button =
        ui_button_create_empty(
            parent,
            kind);

    if (!button) {
        return NULL;
    }

    ui_button_create_label(
        button,
        text);

    return button;
}


lv_obj_t *ui_button_create_icon(
    lv_obj_t *parent,
    ui_button_kind_t kind,
    const char *symbol,
    const char *text,
    lv_color_t icon_color,
    ui_button_icon_layout_t layout)
{
    /*
     * This deliberately starts from ui_button_create().
     *
     * Drybox and Dashboard therefore receive the exact same themed
     * button object, including border color, border width, radius,
     * focused state and pressed state.
     */
    lv_obj_t *button =
        ui_button_create(
            parent,
            kind,
            "");

    if (!button) {
        return NULL;
    }

    /*
     * ui_button_create() creates a centered empty label. Remove it
     * before adding the shared icon/text pair.
     */
    lv_obj_clean(button);

    lv_obj_clear_flag(
        button,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon =
        lv_label_create(button);

    if (!icon) {
        lv_obj_delete(button);
        return NULL;
    }

    lv_label_set_text(
        icon,
        symbol ? symbol : ui_text(""));

    lv_obj_set_style_text_color(
        icon,
        icon_color,
        0);

    lv_obj_t *label =
        lv_label_create(button);

    if (!label) {
        lv_obj_delete(button);
        return NULL;
    }

    lv_label_set_text(
        label,
        text ? text : ui_text(""));

    ui_apply_text_button(label);
    ui_apply_label_bright(label);

    if (layout == UI_BUTTON_ICON_VERTICAL ||
        layout == UI_BUTTON_ICON_VERTICAL_REVERSE) {
        ui_apply_text_title(icon);

        if (layout == UI_BUTTON_ICON_VERTICAL_REVERSE) {
            lv_obj_align(
                label,
                LV_ALIGN_TOP_MID,
                0,
                10);

            lv_obj_align(
                icon,
                LV_ALIGN_BOTTOM_MID,
                0,
                -10);
        } else {
            lv_obj_align(
                icon,
                LV_ALIGN_TOP_MID,
                0,
                10);

            lv_obj_align(
                label,
                LV_ALIGN_BOTTOM_MID,
                0,
                -10);
        }
    } else {
        ui_apply_text_button(icon);

        lv_obj_set_flex_flow(
            button,
            layout == UI_BUTTON_ICON_HORIZONTAL_REVERSE
                ? LV_FLEX_FLOW_ROW_REVERSE
                : LV_FLEX_FLOW_ROW);

        lv_obj_set_flex_align(
            button,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_pad_left(
            button,
            ui_theme_density_metric(5, 7, 9),
            0);

        lv_obj_set_style_pad_right(
            button,
            ui_theme_density_metric(5, 7, 9),
            0);

        lv_obj_set_style_pad_top(
            button,
            0,
            0);

        lv_obj_set_style_pad_bottom(
            button,
            0,
            0);

        lv_obj_set_style_pad_column(
            button,
            ui_theme_density_metric(4, 6, 8),
            0);
    }

    return button;
}
