#include "ui_settings_components.h"
#include "ui_text.h"

#include "ui_button.h"

#include "ui_theme.h"
#include "ui_page_geometry.h"

#include <stddef.h>

static lv_obj_t *settings_component_make_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text ? text : ui_text("--"));
    ui_apply_custom_label_style(label, font, color);

    return label;
}

lv_obj_t *ui_settings_section_create(
    lv_obj_t *parent,
    const char *title,
    int y,
    int h)
{
    lv_obj_t *section = lv_obj_create(parent);

    lv_obj_set_size(section, UI_PAGE_RAIL_WIDTH, h);
    lv_obj_set_pos(section, 0, y);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(section, UI_SURFACE_SECTION);

    lv_obj_t *heading = settings_component_make_label(
        section,
        title,
        UI_FONT_BODY,
        UI_TEXT_BRIGHT);

    lv_obj_set_pos(heading, 18, 13);

    lv_obj_t *line = lv_obj_create(section);
    lv_obj_set_size(line, UI_PAGE_RAIL_WIDTH - 36, 1);
    lv_obj_set_pos(line, 18, 42);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(line, UI_SURFACE_DIVIDER);

    return section;
}

void ui_settings_section_add_divider(
    lv_obj_t *section,
    int y)
{
    lv_obj_t *line = lv_obj_create(section);

    lv_obj_set_size(line, UI_PAGE_RAIL_WIDTH - 36, 1);
    lv_obj_set_pos(line, 18, y);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(line, UI_SURFACE_DIVIDER);
}

lv_obj_t *ui_settings_section_add_row(
    lv_obj_t *section,
    const char *title,
    const char *description,
    const char *value,
    int y,
    lv_event_cb_t event_cb)
{
    lv_obj_t *row = lv_obj_create(section);

    lv_obj_set_size(
        row,
        UI_PAGE_RAIL_WIDTH - 36,
        ui_theme_density_metric(54, 64, 72));
    lv_obj_set_pos(row, 18, y);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(row, UI_SURFACE_TRANSPARENT);

    lv_obj_t *title_label = settings_component_make_label(
        row,
        title,
        UI_FONT_BODY,
        UI_TEXT_BRIGHT);

    lv_obj_set_pos(
        title_label,
        0,
        description
            ? ui_theme_density_metric(4, 8, 9)
            : ui_theme_density_metric(16, 20, 23));

    if (description && description[0]) {
        lv_obj_t *description_label = settings_component_make_label(
            row,
            description,
            UI_FONT_CAPTION,
            UI_TEXT_DIM);

        lv_obj_set_pos(
            description_label,
            0,
            ui_theme_density_metric(28, 34, 39));
    }

    lv_obj_t *value_label = settings_component_make_label(
        row,
        value,
        UI_FONT_CAPTION,
        UI_TEXT_BRIGHT);

    lv_obj_set_width(value_label, 300);
    lv_obj_set_style_text_align(value_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -4, 0);

    if (event_cb) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(
            row,
            event_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    return value_label;
}

lv_obj_t *ui_settings_section_add_percent_slider_row(
    lv_obj_t *section,
    const char *title,
    const char *description,
    int value,
    int minimum,
    int maximum,
    int y,
    lv_event_cb_t event_cb)
{
    lv_obj_t *row = lv_obj_create(section);

    lv_obj_set_size(
        row,
        UI_PAGE_RAIL_WIDTH - 36,
        ui_theme_density_metric(54, 64, 72));
    lv_obj_set_pos(row, 18, y);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(row, UI_SURFACE_TRANSPARENT);

    lv_obj_t *title_label = settings_component_make_label(
        row,
        title,
        UI_FONT_BODY,
        UI_TEXT_BRIGHT);

    lv_obj_set_pos(
        title_label,
        0,
        description
            ? ui_theme_density_metric(4, 8, 9)
            : ui_theme_density_metric(16, 20, 23));

    if (description && description[0]) {
        lv_obj_t *description_label = settings_component_make_label(
            row,
            description,
            UI_FONT_CAPTION,
            UI_TEXT_DIM);

        lv_obj_set_pos(
            description_label,
            0,
            ui_theme_density_metric(28, 34, 39));
    }

    lv_obj_t *value_label = settings_component_make_label(
        row,
        "",
        UI_FONT_CAPTION,
        UI_TEXT_BRIGHT);

    char value_text[16];
    lv_snprintf(value_text, sizeof(value_text), "%d%%", value);
    lv_label_set_text(value_label, value_text);

    lv_obj_set_width(value_label, 64);
    lv_obj_set_style_text_align(
        value_label,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_align(
        value_label,
        LV_ALIGN_RIGHT_MID,
        -4,
        0);

    lv_obj_t *slider = lv_slider_create(row);

    lv_obj_set_size(
        slider,
        ui_theme_density_metric(240, 270, 300),
        ui_theme_density_metric(10, 12, 16));
    lv_obj_align(
        slider,
        LV_ALIGN_RIGHT_MID,
        -86,
        0);

    lv_slider_set_range(
        slider,
        minimum,
        maximum);

    lv_slider_set_value(
        slider,
        value,
        LV_ANIM_OFF);

    ui_apply_slider_style(slider);

    lv_obj_set_user_data(slider, value_label);

    if (event_cb) {
        lv_obj_add_event_cb(
            slider,
            event_cb,
            LV_EVENT_VALUE_CHANGED,
            NULL);
    }

    return slider;
}


lv_obj_t *ui_settings_section_add_action_row(
    lv_obj_t *section,
    const char *title,
    const char *description,
    const char *button_text,
    int y,
    lv_event_cb_t event_cb,
    bool danger)
{
    lv_obj_t *row = lv_obj_create(section);

    lv_obj_set_size(
        row,
        UI_PAGE_RAIL_WIDTH - 36,
        ui_theme_density_metric(60, 70, 80));
    lv_obj_set_pos(row, 18, y);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(row, UI_SURFACE_TRANSPARENT);

    lv_obj_t *title_label = settings_component_make_label(
        row,
        title,
        UI_FONT_BODY,
        danger ? UI_DANGER : UI_TEXT_BRIGHT);

    lv_obj_set_pos(
        title_label,
        0,
        ui_theme_density_metric(5, 10, 12));

    if (description && description[0]) {
        lv_obj_t *description_label = settings_component_make_label(
            row,
            description,
            UI_FONT_CAPTION,
            UI_TEXT_DIM);

        lv_obj_set_pos(
            description_label,
            0,
            ui_theme_density_metric(31, 38, 44));
    }

    lv_obj_t *button =
        ui_button_create(
            row,
            danger
                ? UI_BUTTON_DANGER
                : UI_BUTTON_OUTLINED,
            button_text);

    if (!button) {
        return NULL;
    }

    lv_obj_set_size(
        button,
        ui_theme_density_metric(164, 178, 194),
        ui_theme_density_metric(36, 40, 48));
    lv_obj_align(button, LV_ALIGN_RIGHT_MID, 0, 0);

    if (event_cb) {
        lv_obj_add_event_cb(
            button,
            event_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    return button;
}
