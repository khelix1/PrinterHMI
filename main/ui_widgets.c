#include "ui_widgets.h"
#include "ui_button.h"

/*
 * TEST2_OPERATOR_WIDGETS
 *
 * Shared operator panel shell derived from the current Drybox design.
 * Size, position, alignment, and child layout remain page-owned.
 */
lv_obj_t *ui_create_panel(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_card_style(obj);

    /*
     * Surface appearance is owned entirely by the active theme.
     * This builder owns object creation only.
     */
lv_obj_set_style_pad_all(obj, 0, 0);

    return obj;
}

lv_obj_t *ui_create_card(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_card_style(obj);

    /*
     * Surface appearance is owned entirely by the active theme.
     * This builder owns object creation only.
     */
lv_obj_set_style_pad_all(obj, UI_PAD_CARD, 0);

    return obj;
}

lv_obj_t *ui_create_label(lv_obj_t *parent, const char *text, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text ? text : "");
    ui_apply_custom_label_style(lbl, NULL, color);
    return lbl;
}

lv_obj_t *ui_create_card_title(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = ui_create_label(parent, text, UI_TEXT_DIM);
    ui_apply_custom_label_style(lbl, UI_FONT_BODY, UI_TEXT_DIM);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    return lbl;
}

lv_obj_t *ui_create_card_value(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = ui_create_label(parent, text, UI_TEXT);
    ui_apply_custom_label_style(lbl, UI_FONT_VALUE, UI_TEXT);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    return lbl;
}

lv_obj_t *ui_create_card_subtitle(lv_obj_t *parent, const char *text)
{
    lv_obj_t *lbl = ui_create_label(parent, text, UI_TEXT_DIM);
    ui_apply_custom_label_style(lbl, UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    return lbl;
}

lv_obj_t *ui_create_button(
    lv_obj_t *parent,
    const char *text,
    lv_color_t bg)
{
    lv_obj_t *btn = lv_button_create(parent);

    ui_button_expand_touch_target(btn);

    ui_apply_button_style(btn);

    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, UI_BORDER_SOFT, 0);
    lv_obj_set_style_border_width(btn, UI_BORDER_THIN, 0);
    lv_obj_set_style_radius(btn, UI_RADIUS_ACTION, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);

    lv_obj_set_style_bg_color(
        btn,
        UI_CONTROL,
        LV_STATE_PRESSED);

    lv_obj_t *lbl = lv_label_create(btn);

    lv_label_set_text(lbl, text ? text : "");

    ui_apply_text_button(lbl);
    ui_apply_label_bright(lbl);

    lv_obj_center(lbl);

    return btn;
}

lv_obj_t *ui_create_button_primary(lv_obj_t *parent, const char *text)
{
    return ui_create_button(parent, text, UI_ACCENT_2);
}

lv_obj_t *ui_create_button_secondary(lv_obj_t *parent, const char *text)
{
    return ui_create_button(parent, text, UI_NAV);
}

lv_obj_t *ui_create_button_success(lv_obj_t *parent, const char *text)
{
    return ui_create_button(parent, text, UI_OK);
}

lv_obj_t *ui_create_button_warning(lv_obj_t *parent, const char *text)
{
    return ui_create_button(parent, text, UI_WARN_DARK);
}

lv_obj_t *ui_create_button_danger(lv_obj_t *parent, const char *text)
{
    return ui_create_button(parent, text, UI_DANGER_DARK);
}


/*
 * Shared Operator center-card shell.
 *
 * This is the exact physical card treatment established by the
 * Drybox center cards. Dashboard and Drybox now instantiate the
 * same object instead of reproducing the style independently.
 */
lv_obj_t *ui_create_operator_card(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *card =
        lv_obj_create(parent);

    if (!card) {
        return NULL;
    }

    lv_obj_clear_flag(
        card,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(
        card,
        width,
        height);

    lv_obj_set_pos(
        card,
        x,
        y);

    /*
     * Theme owns the surface colors and structural border.
     * Zero shell padding matches the current Drybox card geometry.
     */
    ui_apply_card_style(card);

    lv_obj_set_style_pad_all(
        card,
        0,
        0);

    return card;
}




/*
 * Shared compact Operator information card.
 *
 * This is the compact telemetry-card counterpart to
 * ui_create_operator_card(). It follows the same physical card
 * language established by the Drybox page.
 */
lv_obj_t *ui_create_operator_info_card(
    lv_obj_t *parent,
    const char *title,
    const char *value,
    int x,
    int y,
    int width,
    int height)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *card =
        ui_create_operator_card(
            parent,
            x,
            y,
            width,
            height);

    if (!card) {
        return NULL;
    }

    lv_obj_t *title_label =
        lv_label_create(card);

    if (!title_label) {
        lv_obj_delete(card);
        return NULL;
    }

    lv_label_set_text(
        title_label,
        title ? title : "");

    lv_obj_set_width(
        title_label,
        width);

    ui_apply_text_body_large(
        title_label);

    ui_apply_label_dim(
        title_label);

    lv_obj_set_style_text_align(
        title_label,
        LV_TEXT_ALIGN_CENTER,
        0);

    lv_obj_align(
        title_label,
        LV_ALIGN_TOP_MID,
        0,
        6);

    lv_obj_t *value_label =
        lv_label_create(card);

    if (!value_label) {
        lv_obj_delete(card);
        return NULL;
    }

    lv_label_set_text(
        value_label,
        value ? value : "");

    lv_obj_set_width(
        value_label,
        width - 16);

    lv_label_set_long_mode(
        value_label,
        LV_LABEL_LONG_DOT);

    ui_apply_custom_label_style(value_label,
                                UI_FONT_VALUE_SMALL,
                                UI_TEXT);

    lv_obj_set_style_text_align(
        value_label,
        LV_TEXT_ALIGN_CENTER,
        0);

    lv_obj_align(
        value_label,
        LV_ALIGN_BOTTOM_MID,
        0,
        -8);

    return value_label;
}


/*
 * Shared muted heading used at the top of Operator center cards.
 */
lv_obj_t *ui_create_operator_card_heading(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *heading =
        lv_label_create(parent);

    if (!heading) {
        return NULL;
    }

    lv_label_set_text(
        heading,
        text ? text : "");

    ui_apply_custom_label_style(heading,
                                UI_FONT_BODY_LARGE,
                                UI_TEXT_DIM);

    lv_obj_set_pos(
        heading,
        x,
        y);

    return heading;
}


/*
 * Shared one-pixel divider used inside Operator center cards.
 */
lv_obj_t *ui_create_operator_card_divider(
    lv_obj_t *parent,
    int x,
    int y,
    int width)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *divider =
        lv_obj_create(parent);

    if (!divider) {
        return NULL;
    }

    lv_obj_clear_flag(
        divider,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(
        divider,
        width,
        1);

    lv_obj_set_pos(
        divider,
        x,
        y);

    ui_apply_surface_role(divider, UI_SURFACE_DIVIDER);

    return divider;
}


/*
 * Shared Operator status-banner shell.
 */
lv_obj_t *ui_create_operator_banner(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height,
    ui_status_kind_t kind)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *banner =
        lv_obj_create(parent);

    if (!banner) {
        return NULL;
    }

    lv_obj_clear_flag(
        banner,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(
        banner,
        width,
        height);

    lv_obj_set_pos(
        banner,
        x,
        y);

    ui_apply_banner_status_style(
        banner,
        kind);

    /*
     * Internal geometry remains page-owned.
     */
    lv_obj_set_style_pad_all(
        banner,
        0,
        0);

    return banner;
}


void ui_operator_banner_set_status(
    lv_obj_t *banner,
    ui_status_kind_t kind)
{
    if (!banner) {
        return;
    }

    ui_apply_banner_status_style(
        banner,
        kind);
}


/*
 * Child ordering for the shared navigation button:
 *
 *   child 0 = icon label
 *   child 1 = text label
 */
static lv_obj_t *operator_nav_icon(
    lv_obj_t *button)
{
    if (!button ||
        lv_obj_get_child_count(button) < 1) {
        return NULL;
    }

    return lv_obj_get_child(button, 0);
}


static lv_obj_t *operator_nav_text(
    lv_obj_t *button)
{
    if (!button ||
        lv_obj_get_child_count(button) < 2) {
        return NULL;
    }

    return lv_obj_get_child(button, 1);
}


lv_obj_t *ui_create_operator_nav_button(
    lv_obj_t *parent,
    int x,
    int y,
    int width,
    int height,
    const char *icon,
    const char *text)
{
    if (!parent) {
        return NULL;
    }

    lv_obj_t *button =
        lv_button_create(parent);

    if (!button) {
        return NULL;
    }

    lv_obj_set_size(
        button,
        width,
        height);

    lv_obj_set_pos(
        button,
        x,
        y);

    lv_obj_clear_flag(
        button,
        LV_OBJ_FLAG_SCROLLABLE);

    /*
     * Start from the common Theme B dark-control treatment.
     * The selected state is applied separately below.
     */
    ui_apply_button_dark_style(button);

    lv_obj_set_style_pad_all(
        button,
        0,
        0);

    /*
     * Mechanical-looking touch feedback.
     */
    lv_obj_set_style_translate_y(
        button,
        1,
        LV_STATE_PRESSED);

    lv_obj_t *icon_label =
        lv_label_create(button);

    lv_label_set_text(
        icon_label,
        icon ? icon : "");

    lv_obj_set_style_text_color(
        icon_label,
        UI_TEXT_DIM,
        0);

    /*
     * Keep the LVGL symbol-capable default font for icons.
     */
    lv_obj_set_width(
        icon_label,
        28);

    lv_obj_set_style_text_align(
        icon_label,
        LV_TEXT_ALIGN_CENTER,
        0);

    lv_obj_align(
        icon_label,
        LV_ALIGN_LEFT_MID,
        14,
        0);

    lv_obj_t *text_label =
        lv_label_create(button);

    lv_label_set_text(
        text_label,
        text ? text : "");

    ui_apply_text_button(text_label);

    lv_obj_set_style_text_color(
        text_label,
        UI_TEXT,
        0);

    lv_obj_align(
        text_label,
        LV_ALIGN_LEFT_MID,
        46,
        0);

    ui_operator_nav_button_set_selected(
        button,
        false);

    return button;
}


void ui_operator_nav_button_set_selected(
    lv_obj_t *button,
    bool selected)
{
    if (!button) {
        return;
    }

    lv_obj_t *icon_label =
        operator_nav_icon(button);

    lv_obj_t *text_label =
        operator_nav_text(button);

    if (selected) {
        /*
         * Selected navigation uses the Operator information blue.
         */
        ui_apply_button_status_style(button, UI_STATUS_ACTIVE);

        if (icon_label) {
            ui_apply_label_bright(icon_label);
        }

        if (text_label) {
            ui_apply_label_bright(text_label);
        }
    } else {
        /*
         * Inactive navigation remains dark and restrained.
         */
        ui_apply_button_dark_style(button);

        if (icon_label) {
            ui_apply_label_dim(icon_label);
        }

        if (text_label) {
            ui_apply_label_primary(text_label);
        }
    }

    /*
     * Maintain clear focus feedback regardless of selection state.
     */
    lv_obj_set_style_border_color(
        button,
        UI_BORDER_BRIGHT,
        LV_PART_MAIN | LV_STATE_FOCUSED);
}
