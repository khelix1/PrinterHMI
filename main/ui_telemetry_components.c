#include "ui_telemetry_components.h"
#include "ui_text.h"

#include "ui_theme.h"

lv_obj_t *telemetry_make_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text ? text : ui_text(""));
    ui_apply_custom_label_style(label, font, color);

    return label;
}

lv_obj_t *telemetry_create_metric_card(
    lv_obj_t *parent,
    int x,
    const char *title,
    lv_color_t accent,
    lv_obj_t **value_out)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_size(card, 190, 104);
    lv_obj_set_pos(card, x, 88);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(card, UI_SURFACE_TELEMETRY_CARD);

    lv_obj_t *accent_line = lv_obj_create(card);

    lv_obj_set_size(accent_line, 4, 72);
    lv_obj_set_pos(accent_line, 12, 16);
    lv_obj_clear_flag(accent_line, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(accent_line, UI_SURFACE_INDICATOR);
    lv_obj_set_style_bg_color(accent_line, accent, 0);

    lv_obj_t *heading = telemetry_make_label(
        card,
        title,
        &lv_font_montserrat_14,
        accent);

    lv_obj_set_pos(heading, 28, 16);

    lv_obj_t *value = telemetry_make_label(
        card,
        "--",
        &lv_font_montserrat_28,
        UI_TEXT_BRIGHT);

    lv_obj_set_pos(value, 28, 48);

    if (value_out) {
        *value_out = value;
    }

    return card;
}

void telemetry_create_legend_item(
    lv_obj_t *parent,
    int x,
    int y,
    lv_color_t color,
    const char *text)
{
    lv_obj_t *marker = lv_obj_create(parent);

    lv_obj_set_size(marker, 18, 3);
    lv_obj_set_pos(marker, x, y + 6);
    lv_obj_clear_flag(marker, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(marker, UI_SURFACE_INDICATOR);
    lv_obj_set_style_bg_color(marker, color, 0);

    lv_obj_t *label = telemetry_make_label(
        parent,
        text,
        &lv_font_montserrat_12,
        UI_TEXT_DIM);

    lv_obj_set_pos(label, x + 24, y);
}
