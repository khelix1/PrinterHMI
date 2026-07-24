#pragma once

#include "lvgl.h"
#include "ui_theme.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_theme_b_apply_root_style(lv_obj_t *obj);
void ui_theme_b_apply_panel_style(lv_obj_t *obj);
void ui_theme_b_apply_card_style(lv_obj_t *obj);
void ui_theme_b_apply_banner_style(lv_obj_t *obj);
void ui_theme_b_apply_preview_style(lv_obj_t *obj);
void ui_theme_b_apply_info_box_style(lv_obj_t *obj);
void ui_theme_b_apply_popup_style(lv_obj_t *obj);
void ui_theme_b_apply_dialog_style(lv_obj_t *obj);
void ui_theme_b_apply_surface_role(lv_obj_t *obj, ui_surface_role_t role);
void ui_theme_b_apply_custom_label_style(lv_obj_t *obj,
                                         const lv_font_t *font,
                                         lv_color_t color);
void ui_theme_b_apply_slider_style(lv_obj_t *obj);
void ui_theme_b_apply_progress_bar_style(lv_obj_t *obj);
void ui_theme_b_apply_telemetry_plot_style(lv_obj_t *obj);
void ui_theme_b_apply_trace_marker_style(lv_obj_t *obj, lv_color_t color);
void ui_theme_b_apply_reference_line_style(lv_obj_t *obj, lv_color_t color);

void ui_theme_b_apply_button_style(lv_obj_t *obj);
void ui_theme_b_apply_button_dark_style(lv_obj_t *obj);
void ui_theme_b_apply_button_success_style(lv_obj_t *obj);
void ui_theme_b_apply_button_warning_style(lv_obj_t *obj);
void ui_theme_b_apply_button_danger_style(lv_obj_t *obj);
void ui_theme_b_apply_button_cancel_style(lv_obj_t *obj);
void ui_theme_b_apply_button_close_style(lv_obj_t *obj);
void ui_theme_b_apply_button_outlined_style(lv_obj_t *obj);

void ui_theme_b_apply_label_primary(lv_obj_t *obj);
void ui_theme_b_apply_label_bright(lv_obj_t *obj);
void ui_theme_b_apply_label_dim(lv_obj_t *obj);
void ui_theme_b_apply_label_muted(lv_obj_t *obj);
void ui_theme_b_apply_label_success(lv_obj_t *obj);
void ui_theme_b_apply_label_warning(lv_obj_t *obj);
void ui_theme_b_apply_label_error(lv_obj_t *obj);

void ui_theme_b_apply_text_caption(lv_obj_t *obj);
void ui_theme_b_apply_text_body(lv_obj_t *obj);
void ui_theme_b_apply_text_body_large(lv_obj_t *obj);
void ui_theme_b_apply_text_button(lv_obj_t *obj);
void ui_theme_b_apply_text_value_small(lv_obj_t *obj);
void ui_theme_b_apply_text_title(lv_obj_t *obj);
void ui_theme_b_apply_text_dialog_title(lv_obj_t *obj);
void ui_theme_b_apply_text_popup_title(lv_obj_t *obj);
void ui_theme_b_apply_text_value(lv_obj_t *obj);
void ui_theme_b_apply_text_heading(lv_obj_t *obj);
void ui_theme_b_apply_text_percent(lv_obj_t *obj);

lv_color_t ui_theme_b_status_color(ui_status_kind_t kind);

#ifdef __cplusplus
}
#endif

void ui_theme_b_apply_banner_status_style(
    lv_obj_t *obj,
    ui_status_kind_t kind);

void ui_theme_b_apply_button_status_style(
    lv_obj_t *obj,
    ui_status_kind_t kind);
