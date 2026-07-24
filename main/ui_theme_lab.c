#include "ui_theme_lab.h"

#include "theme_manager.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_theme.h"

#include <stddef.h>

static lv_obj_t *s_lab_popup = NULL;

void ui_theme_lab_close(void)
{
    if (s_lab_popup) lv_obj_delete(s_lab_popup);
    s_lab_popup = NULL;
}

static void lab_close_cb(lv_event_t *event)
{
    (void)event;
    ui_theme_lab_close();
}

static lv_obj_t *lab_label(lv_obj_t *parent,
                           const char *text,
                           const lv_font_t *font,
                           lv_color_t color,
                           int32_t x,
                           int32_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return NULL;
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    ui_apply_custom_label_style(label, font, color);
    return label;
}

static lv_obj_t *lab_button(lv_obj_t *parent,
                            ui_button_kind_t kind,
                            const char *text,
                            int32_t x,
                            int32_t y)
{
    lv_obj_t *button = ui_button_create(parent, kind, text);
    if (!button) return NULL;
    lv_obj_set_size(button, 176, 44);
    lv_obj_set_pos(button, x, y);
    return button;
}

void ui_theme_lab_show(void)
{
    if (s_lab_popup) {
        lv_obj_move_foreground(s_lab_popup);
        return;
    }

    s_lab_popup = ui_popup_create(
        lv_layer_top(), 920, 520, UI_POPUP_STANDARD);
    if (!s_lab_popup) return;

    ui_popup_add_title(s_lab_popup, "THEME LABORATORY", false, 8);
    ui_popup_add_header_divider(s_lab_popup, 44);

    char summary[160];
    ui_accessibility_t access = theme_manager_accessibility();
    lv_snprintf(
        summary, sizeof(summary),
        "%s  |  %s accent  |  %s density  |  %s",
        theme_manager_active_label(),
        theme_manager_accent_label(),
        theme_manager_density_label(),
        (access.large_text || access.high_contrast ||
         access.reduced_transparency || access.reduced_motion)
            ? "accessibility active" : "standard access");
    ui_popup_add_status_label(s_lab_popup, summary, 24, 50, 872);

    lv_obj_t *controls = lv_obj_create(s_lab_popup);
    lv_obj_set_size(controls, 420, 330);
    lv_obj_set_pos(controls, 24, 86);
    lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_card_style(controls);

    lab_label(controls, "SEMANTIC CONTROLS", UI_FONT_TITLE,
              UI_TEXT_BRIGHT, 16, 12);
    lab_button(controls, UI_BUTTON_PRIMARY, "PRIMARY", 16, 52);
    lab_button(controls, UI_BUTTON_SUCCESS, "SUCCESS", 212, 52);
    lab_button(controls, UI_BUTTON_WARNING, "WARNING", 16, 106);
    lab_button(controls, UI_BUTTON_DANGER, "DANGER", 212, 106);

    lab_label(controls, "Progress", UI_FONT_BODY, UI_TEXT_DIM, 16, 168);
    lv_obj_t *bar = lv_bar_create(controls);
    lv_obj_set_size(bar, 372, 18);
    lv_obj_set_pos(bar, 16, 194);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 68, LV_ANIM_OFF);
    ui_apply_progress_bar_style(bar);

    lab_label(controls, "Adjustment", UI_FONT_BODY, UI_TEXT_DIM, 16, 226);
    lv_obj_t *slider = lv_slider_create(controls);
    lv_obj_set_size(slider, 260, 14);
    lv_obj_set_pos(slider, 16, 258);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, 62, LV_ANIM_OFF);
    ui_apply_slider_style(slider);

    lv_obj_t *toggle = lv_switch_create(controls);
    lv_obj_set_pos(toggle, 316, 245);
    lv_obj_add_state(toggle, LV_STATE_CHECKED);
    ui_apply_slider_style(toggle);

    lv_obj_t *surfaces = lv_obj_create(s_lab_popup);
    lv_obj_set_size(surfaces, 428, 330);
    lv_obj_set_pos(surfaces, 464, 86);
    lv_obj_clear_flag(surfaces, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_panel_style(surfaces);

    lab_label(surfaces, "TYPE & SURFACES", UI_FONT_TITLE,
              UI_TEXT_BRIGHT, 16, 12);
    lab_label(surfaces, "Heading / primary text", UI_FONT_HEADING,
              UI_TEXT, 16, 48);
    lab_label(surfaces, "Body copy and operational descriptions",
              UI_FONT_BODY, UI_TEXT_MUTED, 16, 88);
    lab_label(surfaces, "DIMMED SECONDARY INFORMATION",
              UI_FONT_CAPTION, UI_TEXT_DIM, 16, 116);

    lv_obj_t *info = lv_obj_create(surfaces);
    lv_obj_set_size(info, 188, 76);
    lv_obj_set_pos(info, 16, 150);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_info_box_style(info);
    lab_label(info, "INFO", UI_FONT_CAPTION, UI_ACCENT_BRIGHT, 12, 10);
    lab_label(info, "215 / 220 C", UI_FONT_VALUE_SMALL, UI_TEXT, 12, 34);

    lv_obj_t *status = lv_obj_create(surfaces);
    lv_obj_set_size(status, 188, 76);
    lv_obj_set_pos(status, 220, 150);
    lv_obj_clear_flag(status, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_banner_status_style(status, UI_STATUS_ACTIVE);
    lab_label(status, "STATUS", UI_FONT_CAPTION, UI_OK_BRIGHT, 12, 10);
    lab_label(status, "READY", UI_FONT_VALUE_SMALL, UI_TEXT, 12, 34);

    lv_obj_t *textarea = lv_textarea_create(surfaces);
    lv_obj_set_size(textarea, 392, 54);
    lv_obj_set_pos(textarea, 16, 244);
    lv_textarea_set_one_line(textarea, true);
    lv_textarea_set_placeholder_text(textarea, "Input and keyboard surface");
    ui_apply_surface_role(textarea, UI_SURFACE_TEXT_INPUT);

    ui_popup_add_standard_footer_divider(s_lab_popup);
    ui_popup_add_footer_action(
        s_lab_popup, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " CLOSE LAB",
        180, UI_POPUP_FOOTER_CENTER, lab_close_cb, NULL, NULL);
}
