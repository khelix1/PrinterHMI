#pragma once
#include "ui_theme.h"
#include "ui_font_fallback.h"
#include "ui_button_press.h"

/* Flat selector fields and native scrolling lists use the same typography.
 * A 44px option pitch includes spacing above/below the text, not a font jump. */
static inline void ui_selector_style(lv_obj_t *dropdown, const lv_font_t *preferred)
{
    if (!dropdown || !preferred) return;
    const lv_font_t *font = ui_font_with_fallback(preferred);
    /* Keep the owner's width, position and flex/grid sizing intact. */
    lv_obj_set_style_shadow_width(dropdown, 0, 0);
    lv_obj_set_style_text_font(dropdown, font, 0);
    lv_obj_set_style_text_color(dropdown, UI_TEXT_BRIGHT, 0);
    lv_obj_set_style_text_opa(dropdown, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_set_style_bg_color(dropdown, UI_CONTROL, 0);
    lv_obj_set_style_bg_opa(dropdown, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(dropdown, UI_BORDER_BRIGHT, 0);
    lv_obj_set_style_border_color(dropdown, UI_ACCENT_BRIGHT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(dropdown, 1, 0);
    lv_obj_set_style_radius(dropdown, 6, 0);
    lv_obj_set_style_pad_hor(dropdown, 12, 0);
    lv_obj_set_style_pad_ver(dropdown, 8, 0);
    lv_obj_set_style_min_height(dropdown, font->line_height + 18 > 44 ? font->line_height + 18 : 44, 0);
    ui_button_stable_press(dropdown);

    lv_obj_t *list = lv_dropdown_get_list(dropdown);
    if (!list) return;
    lv_obj_remove_style_all(list);
    lv_obj_set_style_bg_color(list, UI_PANEL, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list, 1, 0);
    lv_obj_set_style_border_color(list, UI_BORDER_BRIGHT, 0);
    lv_obj_set_style_radius(list, 6, 0);
    lv_obj_set_style_pad_all(list, 8, 0);
    int32_t spacing = font->line_height < 44 ? 44 - font->line_height : 8;
    lv_obj_set_style_text_font(list, font, 0);
    lv_obj_set_style_text_font(list, font, LV_PART_SELECTED);
    lv_obj_set_style_text_color(list, UI_TEXT_BRIGHT, 0);
    lv_obj_set_style_text_color(list, UI_TEXT_BRIGHT, LV_PART_SELECTED);
    lv_obj_set_style_text_line_space(list, spacing, 0);
    lv_obj_set_style_text_line_space(list, spacing, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(list, UI_CONTROL, LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(list, UI_ACCENT, LV_PART_SELECTED | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(list, LV_OPA_60, LV_PART_SELECTED | LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(list, UI_ACCENT_BRIGHT, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(list, 4, LV_PART_SCROLLBAR);
}
