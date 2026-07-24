/*
 * Theme C: Dark Glass
 *
 * Smoked translucent surfaces over the shared Operator component contract.
 * Neon colors are reserved for reflected edges, status, and restrained glow.
 */

#include "ui_theme_c.h"

#include "ui_theme_b.h"


static void glass_surface(lv_obj_t *obj,
                          lv_color_t top,
                          lv_color_t bottom,
                          lv_color_t edge,
                          int32_t radius,
                          lv_opa_t opacity,
                          lv_opa_t glow_opacity)
{
    if (!obj) return;

    lv_obj_set_style_bg_color(obj, top, 0);
    lv_obj_set_style_bg_grad_color(obj, bottom, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop(obj, 28, 0);
    lv_obj_set_style_bg_grad_stop(obj, 224, 0);
    lv_obj_set_style_bg_opa(
        obj,
        ui_theme_accessible_opacity(opacity),
        0);
    lv_obj_set_style_border_color(obj, UI_GLASS_EDGE, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_70, 0);
    lv_obj_set_style_border_width(obj, UI_BORDER_THIN, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_outline_color(obj, edge, 0);
    lv_obj_set_style_outline_width(obj, 1, 0);
    lv_obj_set_style_outline_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_outline_pad(obj, 2, 0);
    lv_obj_set_style_shadow_color(obj, edge, 0);
    lv_obj_set_style_shadow_width(obj, 24, 0);
    lv_obj_set_style_shadow_spread(obj, 0, 0);
    lv_obj_set_style_shadow_opa(obj, glow_opacity, 0);
    lv_obj_set_style_shadow_offset_y(obj, 6, 0);
}


static void glass_button(lv_obj_t *obj,
                         lv_color_t top,
                         lv_color_t bottom,
                         lv_color_t glow)
{
    if (!obj) return;

    lv_obj_set_style_bg_color(obj, top, 0);
    lv_obj_set_style_bg_grad_color(obj, bottom, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_main_stop(obj, 24, 0);
    lv_obj_set_style_bg_grad_stop(obj, 230, 0);
    lv_obj_set_style_bg_opa(
        obj,
        ui_theme_accessible_opacity(LV_OPA_70),
        0);
    lv_obj_set_style_border_color(obj, UI_GLASS_EDGE, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_80, 0);
    lv_obj_set_style_border_width(obj, UI_BORDER_THIN, 0);
    lv_obj_set_style_radius(obj, UI_RADIUS_BTN, 0);
    lv_obj_set_style_outline_color(obj, glow, 0);
    lv_obj_set_style_outline_width(obj, 1, 0);
    lv_obj_set_style_outline_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_outline_pad(obj, 1, 0);
    lv_obj_set_style_shadow_color(obj, glow, 0);
    lv_obj_set_style_shadow_width(obj, 18, 0);
    lv_obj_set_style_shadow_spread(obj, 0, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_shadow_offset_y(obj, 4, 0);
    lv_obj_set_style_translate_y(obj, 1, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(obj, UI_TEXT_BRIGHT, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(obj, 0, 0);
}


void ui_theme_c_apply_root_style(lv_obj_t *obj)
{
    ui_theme_b_apply_root_style(obj);
    if (!obj) return;

    lv_obj_set_style_bg_color(obj, UI_BG, 0);
    lv_obj_set_style_bg_grad_color(obj, UI_BG_DEEP, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
}


void ui_theme_c_apply_panel_style(lv_obj_t *obj)
{
    ui_theme_b_apply_panel_style(obj);
    glass_surface(obj, UI_GLASS_SHEEN, UI_PANEL, UI_BORDER_BRIGHT,
                  UI_RADIUS_PANEL, LV_OPA_60, LV_OPA_20);
}


void ui_theme_c_apply_card_style(lv_obj_t *obj)
{
    ui_theme_b_apply_card_style(obj);
    glass_surface(obj, UI_GLASS_SHEEN, UI_CARD, UI_BORDER_CONTROL,
                  UI_RADIUS_CARD, LV_OPA_60, LV_OPA_20);
}


void ui_theme_c_apply_banner_style(lv_obj_t *obj)
{
    ui_theme_b_apply_banner_style(obj);
    glass_surface(obj, UI_GLASS_SHEEN, UI_PANEL_ALT, UI_ACCENT_CYAN,
                  UI_RADIUS_BANNER, LV_OPA_70, LV_OPA_30);
}


void ui_theme_c_apply_banner_status_style(lv_obj_t *obj,
                                          ui_status_kind_t kind)
{
    lv_color_t glow = ui_theme_c_status_color(kind);
    ui_theme_b_apply_banner_status_style(obj, kind);
    glass_surface(obj, UI_GLASS_SHEEN, UI_PANEL_ALT, glow,
                  UI_RADIUS_BANNER, LV_OPA_70, LV_OPA_30);
}


void ui_theme_c_apply_preview_style(lv_obj_t *obj)
{
    ui_theme_b_apply_preview_style(obj);
    glass_surface(obj, UI_PANEL, UI_CARD_DARK, UI_ACCENT_PURPLE,
                  UI_RADIUS_PREVIEW, LV_OPA_60, LV_OPA_20);
}


void ui_theme_c_apply_info_box_style(lv_obj_t *obj)
{
    ui_theme_b_apply_info_box_style(obj);
    glass_surface(obj, UI_GLASS_SHEEN, UI_CARD, UI_BORDER_CONTROL,
                  UI_RADIUS_CARD, LV_OPA_60, LV_OPA_20);
}


void ui_theme_c_apply_popup_style(lv_obj_t *obj)
{
    ui_theme_b_apply_popup_style(obj);
    glass_surface(obj, UI_GLASS_SHEEN, UI_BG_POPUP, UI_ACCENT_PURPLE,
                  UI_RADIUS_POPUP, LV_OPA_80, LV_OPA_30);
}


void ui_theme_c_apply_dialog_style(lv_obj_t *obj)
{
    ui_theme_b_apply_dialog_style(obj);
    glass_surface(obj, UI_GLASS_SHEEN, UI_BG_POPUP, UI_ACCENT_CYAN,
                  UI_RADIUS_DIALOG, LV_OPA_80, LV_OPA_30);
}


void ui_theme_c_apply_surface_role(lv_obj_t *obj, ui_surface_role_t role)
{
    ui_theme_b_apply_surface_role(obj, role);
    if (!obj) return;

    switch (role) {
        case UI_SURFACE_TRANSPARENT:
        case UI_SURFACE_DIVIDER:
        case UI_SURFACE_INDICATOR:
            return;

        case UI_SURFACE_SHELL_TOPBAR:
            glass_surface(obj, UI_GLASS_SHEEN, UI_TOPBAR, UI_ACCENT_PURPLE,
                          0, LV_OPA_70, LV_OPA_20);
            return;

        case UI_SURFACE_SHELL_NAV:
            glass_surface(obj, UI_PANEL, UI_NAV, UI_ACCENT_CYAN,
                          0, LV_OPA_70, LV_OPA_20);
            return;

        case UI_SURFACE_PAGE_DEEP:
        case UI_SURFACE_TELEMETRY_ROOT:
            lv_obj_set_style_bg_color(obj, UI_BG, 0);
            lv_obj_set_style_bg_grad_color(obj, UI_BG_DEEP, 0);
            lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(obj, 0, 0);
            return;

        case UI_SURFACE_STATUS_PILL:
            glass_surface(obj, UI_CONTROL_ALT, UI_BG_POPUP,
                          UI_ACCENT_BRIGHT, LV_RADIUS_CIRCLE,
                          LV_OPA_60, LV_OPA_20);
            return;

        case UI_SURFACE_TEXT_INPUT:
            glass_surface(obj, UI_CONTROL, UI_BG_DEEP, UI_ACCENT_CYAN,
                          UI_RADIUS_BTN, LV_OPA_60, LV_OPA_20);
            return;

        case UI_SURFACE_KEYBOARD:
            glass_surface(obj, UI_GLASS_SHEEN, UI_BG_POPUP, UI_ACCENT_PURPLE,
                          UI_RADIUS_CARD, LV_OPA_80, LV_OPA_20);
            lv_obj_set_style_bg_color(obj, UI_GLASS_SHEEN, LV_PART_ITEMS);
            lv_obj_set_style_bg_opa(
                obj,
                ui_theme_accessible_opacity(LV_OPA_60),
                LV_PART_ITEMS);
            lv_obj_set_style_border_color(obj, UI_BORDER_CONTROL,
                                          LV_PART_ITEMS);
            return;

        case UI_SURFACE_TELEMETRY_CARD:
        case UI_SURFACE_TELEMETRY_PANEL:
        case UI_SURFACE_TELEMETRY_CHART:
            glass_surface(obj, UI_TELEMETRY_CARD_BG,
                          UI_TELEMETRY_CHART_BG, UI_ACCENT_PURPLE,
                          UI_RADIUS_CARD, LV_OPA_60, LV_OPA_20);
            return;

        case UI_SURFACE_LIST_ROW:
            glass_surface(obj, UI_CONTROL, UI_BG_POPUP, UI_BORDER_CONTROL,
                          UI_RADIUS_BAR, LV_OPA_60, LV_OPA_20);
            return;

        case UI_SURFACE_SECTION:
        case UI_SURFACE_PREVIEW_WELL:
        case UI_SURFACE_POPUP_LIST:
        default:
            glass_surface(obj, UI_GLASS_SHEEN, UI_CARD, UI_BORDER_CONTROL,
                          UI_RADIUS_CARD, LV_OPA_60, LV_OPA_20);
            return;
    }
}


void ui_theme_c_apply_slider_style(lv_obj_t *obj)
{
    ui_theme_b_apply_slider_style(obj);
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, UI_ACCENT_CYAN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(obj, UI_ACCENT_PURPLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_color(obj, UI_ACCENT_CYAN, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(obj, 16, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_60, LV_PART_KNOB);
}


void ui_theme_c_apply_progress_bar_style(lv_obj_t *obj)
{
    ui_theme_b_apply_progress_bar_style(obj);
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, UI_ACCENT_CYAN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(obj, UI_ACCENT_BRIGHT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_color(obj, UI_ACCENT_CYAN, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_width(obj, 10, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_50, LV_PART_INDICATOR);
}


void ui_theme_c_apply_telemetry_plot_style(lv_obj_t *obj)
{
    ui_theme_b_apply_telemetry_plot_style(obj);
    glass_surface(obj, UI_TELEMETRY_CHART_CARD_BG,
                  UI_TELEMETRY_CHART_BG, UI_BORDER_CONTROL,
                  UI_RADIUS_CARD, LV_OPA_70, LV_OPA_20);
}


void ui_theme_c_apply_trace_marker_style(lv_obj_t *obj, lv_color_t color)
{
    ui_theme_b_apply_trace_marker_style(obj, color);
    if (!obj) return;
    lv_obj_set_style_shadow_width(obj, 16, 0);
    lv_obj_set_style_shadow_spread(obj, 2, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_80, 0);
}


void ui_theme_c_apply_reference_line_style(lv_obj_t *obj, lv_color_t color)
{
    ui_theme_b_apply_reference_line_style(obj, color);
    if (!obj) return;
    lv_obj_set_style_shadow_color(obj, color, 0);
    lv_obj_set_style_shadow_width(obj, 8, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_50, 0);
}


void ui_theme_c_apply_button_style(lv_obj_t *obj)
{
    ui_theme_b_apply_button_style(obj);
    glass_button(obj, UI_CONTROL, UI_CONTROL_ALT, UI_ACCENT_CYAN);
}


void ui_theme_c_apply_button_dark_style(lv_obj_t *obj)
{
    ui_theme_b_apply_button_dark_style(obj);
    glass_button(obj, UI_CONTROL, UI_CONTROL_ALT, UI_BORDER_BRIGHT);
}


void ui_theme_c_apply_button_success_style(lv_obj_t *obj)
{
    ui_theme_b_apply_button_success_style(obj);
    glass_button(obj, UI_CONTROL, UI_BG_POPUP, UI_OK_BRIGHT);
}


void ui_theme_c_apply_button_warning_style(lv_obj_t *obj)
{
    ui_theme_b_apply_button_warning_style(obj);
    glass_button(obj, UI_CONTROL, UI_BG_POPUP, UI_WARN);
}


void ui_theme_c_apply_button_danger_style(lv_obj_t *obj)
{
    ui_theme_b_apply_button_danger_style(obj);
    glass_button(obj, UI_CONTROL_CLOSE, UI_BG_POPUP, UI_DANGER_BRIGHT);
}


void ui_theme_c_apply_button_cancel_style(lv_obj_t *obj)
{
    ui_theme_b_apply_button_cancel_style(obj);
    glass_button(obj, UI_CONTROL_CANCEL, UI_CONTROL_ALT, UI_TEXT_DIM);
}


void ui_theme_c_apply_button_close_style(lv_obj_t *obj)
{
    ui_theme_b_apply_button_close_style(obj);
    glass_button(obj, UI_CONTROL_CLOSE, UI_CONTROL_ALT, UI_DANGER_BRIGHT);
}


void ui_theme_c_apply_button_outlined_style(lv_obj_t *obj)
{
    ui_theme_b_apply_button_outlined_style(obj);
    glass_button(obj, UI_CONTROL, UI_BG_POPUP, UI_ACCENT_CYAN);
}


void ui_theme_c_apply_button_status_style(lv_obj_t *obj,
                                          ui_status_kind_t kind)
{
    switch (kind) {
        case UI_STATUS_OK:
            ui_theme_c_apply_button_success_style(obj);
            break;
        case UI_STATUS_WARNING:
            ui_theme_c_apply_button_warning_style(obj);
            break;
        case UI_STATUS_DANGER:
            ui_theme_c_apply_button_danger_style(obj);
            break;
        case UI_STATUS_NEUTRAL:
            ui_theme_c_apply_button_dark_style(obj);
            break;
        case UI_STATUS_INFO:
        case UI_STATUS_ACTIVE:
        default:
            ui_theme_c_apply_button_style(obj);
            break;
    }
}


#define THEME_C_DELEGATE_VOID(name)                    \
    void ui_theme_c_##name(lv_obj_t *obj)               \
    {                                                    \
        ui_theme_b_##name(obj);                          \
    }

THEME_C_DELEGATE_VOID(apply_label_primary)
THEME_C_DELEGATE_VOID(apply_label_bright)
THEME_C_DELEGATE_VOID(apply_label_dim)
THEME_C_DELEGATE_VOID(apply_label_muted)
THEME_C_DELEGATE_VOID(apply_label_success)
THEME_C_DELEGATE_VOID(apply_label_warning)
THEME_C_DELEGATE_VOID(apply_label_error)
THEME_C_DELEGATE_VOID(apply_text_caption)
THEME_C_DELEGATE_VOID(apply_text_body)
THEME_C_DELEGATE_VOID(apply_text_body_large)
THEME_C_DELEGATE_VOID(apply_text_button)
THEME_C_DELEGATE_VOID(apply_text_value_small)
THEME_C_DELEGATE_VOID(apply_text_title)
THEME_C_DELEGATE_VOID(apply_text_dialog_title)
THEME_C_DELEGATE_VOID(apply_text_popup_title)
THEME_C_DELEGATE_VOID(apply_text_value)
THEME_C_DELEGATE_VOID(apply_text_heading)
THEME_C_DELEGATE_VOID(apply_text_percent)


void ui_theme_c_apply_custom_label_style(lv_obj_t *obj,
                                         const lv_font_t *font,
                                         lv_color_t color)
{
    ui_theme_b_apply_custom_label_style(obj, font, color);
}


lv_color_t ui_theme_c_status_color(ui_status_kind_t kind)
{
    return ui_theme_b_status_color(kind);
}
