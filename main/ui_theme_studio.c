#include "ui_button_press.h"
/* STUDIO Dark: independently authored opaque studio-console styles.
 * Shared UI/data contracts are reused; no other theme recipe is invoked. */
#include "ui_theme_studio.h"
#include "ui_font_fallback.h"

static void surface(lv_obj_t *obj, lv_color_t bg, int radius, int border, int pad)
{
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, bg, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_border_color(obj, UI_BORDER, 0);
    lv_obj_set_style_border_width(obj, ui_theme_accessible_border_width(border), 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_pad_all(obj, pad, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_text_color(obj, UI_TEXT, 0);
}
static void font(lv_obj_t *obj, const lv_font_t *face)
{
    if (obj && face) lv_obj_set_style_text_font(obj, ui_font_with_fallback(face), 0);
}
static void color(lv_obj_t *obj, lv_color_t ink)
{
    if (obj) lv_obj_set_style_text_color(obj, ink, 0);
}
static void button(lv_obj_t *obj, lv_color_t bg, lv_color_t edge)
{
    surface(obj, bg, 24, 1, 6);
    if (!obj) return;
    lv_obj_set_style_border_color(obj, edge, 0);
    lv_obj_set_style_bg_color(obj, UI_CONTROL, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(obj, edge, LV_STATE_PRESSED);
    ui_button_stable_press(obj);
    lv_obj_set_style_outline_color(obj, UI_ACCENT_BRIGHT, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(obj, 2, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_pad(obj, 2, LV_STATE_FOCUS_KEY);
    lv_obj_set_style_opa(obj, LV_OPA_40, LV_STATE_DISABLED);
    /* Static storage is safe for all widgets and all page lifetimes. */
    static const lv_style_prop_t props[] = {LV_STYLE_BG_COLOR, LV_STYLE_BORDER_COLOR, 0};
    static lv_style_transition_dsc_t transition;
    lv_style_transition_dsc_init(&transition, props, lv_anim_path_ease_out,
        ui_theme_motion_enabled() ? 120 : 0, 0, NULL);
    lv_obj_set_style_transition(obj, &transition, 0);
}
void ui_theme_studio_page_frame(lv_obj_t *obj)
{
    if (!obj || obj == lv_screen_active() ||
        lv_obj_get_style_width(obj,0) != 854 || lv_obj_get_style_height(obj,0) != 528) return;
    /* Independent fixed stage between the header and navigation shelf.
     * Files and Settings opt into scrolling only inside their own viewports. */
    lv_obj_set_pos(obj, 24, 80);
    lv_obj_set_size(obj, 976, 424);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(obj, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_bottom(obj, 0, 0);
}
void ui_theme_studio_apply_root_style(lv_obj_t *obj)
{
    surface(obj,UI_BG,0,0,0);
    ui_theme_studio_page_frame(obj);
}
void ui_theme_studio_apply_panel_style(lv_obj_t *o) { surface(o, UI_PANEL, 18, 1, UI_PAD_PANEL); }
void ui_theme_studio_apply_card_style(lv_obj_t *o) { surface(o, UI_CARD, 18, 1, UI_PAD_CARD); }
void ui_theme_studio_apply_banner_style(lv_obj_t *o) { surface(o, UI_PANEL, 18, 1, UI_PAD_CARD); }
void ui_theme_studio_apply_preview_style(lv_obj_t *o) { surface(o, UI_BG, 0, 0, 0); }
void ui_theme_studio_apply_info_box_style(lv_obj_t *o) { surface(o, UI_PANEL_ALT, 18, 1, UI_PAD_CARD); }
void ui_theme_studio_apply_popup_style(lv_obj_t *o) { surface(o, UI_BG_POPUP, 22, 1, 0); }
void ui_theme_studio_apply_dialog_style(lv_obj_t *o) { surface(o, UI_BG_POPUP, 22, 1, UI_PAD_POPUP); }
void ui_theme_studio_apply_surface_role(lv_obj_t *o, ui_surface_role_t role)
{
    if (!o) return;
    switch (role) {
    case UI_SURFACE_TRANSPARENT:
        surface(o, UI_BG, 0, 0, 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0); break;
    case UI_SURFACE_SHELL_TOPBAR: surface(o, UI_BG, 0, 0, 0); break;
    case UI_SURFACE_SHELL_NAV: surface(o, UI_NAV, 22, 1, 6); break;
    case UI_SURFACE_PAGE_DEEP:
        surface(o,UI_BG,0,0,0); ui_theme_studio_page_frame(o); break;
    case UI_SURFACE_TELEMETRY_ROOT: surface(o, UI_BG_DEEP, 0, 0, 0); break;
    case UI_SURFACE_PREVIEW_WELL: ui_theme_studio_apply_preview_style(o); break;
    case UI_SURFACE_STATUS_PILL: surface(o, UI_CONTROL, LV_RADIUS_CIRCLE, 0, 4); break;
    case UI_SURFACE_POPUP_LIST: surface(o, UI_BG_DEEP, 18, 1, 8); break;
    case UI_SURFACE_TEXT_INPUT:
        surface(o, UI_BG_DEEP, 14, 1, 12);
        lv_obj_set_style_border_color(o, UI_ACCENT_BRIGHT, LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(o, UI_TEXT_DIM, LV_PART_TEXTAREA_PLACEHOLDER); break;
    case UI_SURFACE_KEYBOARD:
        surface(o, UI_BG_POPUP, 18, 1, 8);
        lv_obj_set_style_bg_color(o, UI_CONTROL, LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_ITEMS);
        lv_obj_set_style_radius(o, 12, LV_PART_ITEMS);
        lv_obj_set_style_text_font(o, ui_font_with_fallback(UI_FONT_BODY_LARGE), LV_PART_ITEMS);
        lv_obj_set_style_text_color(o, UI_TEXT, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(o, UI_ACCENT, LV_PART_ITEMS | LV_STATE_PRESSED); break;
    case UI_SURFACE_DIVIDER: surface(o, UI_BORDER, 0, 0, 0); break;
    case UI_SURFACE_INDICATOR: surface(o, UI_ACCENT, 6, 0, 0); break;
    case UI_SURFACE_LIST_ROW:
        surface(o, UI_PANEL, 12, 1, 0);
        lv_obj_set_style_bg_color(o, UI_CONTROL, LV_STATE_PRESSED); break;
    default: surface(o, UI_PANEL, 18, 1, 0); break;
    }
}
void ui_theme_studio_apply_custom_label_style(lv_obj_t *o, const lv_font_t *f, lv_color_t c) { font(o,f); color(o,c); }
void ui_theme_studio_apply_progress_bar_style(lv_obj_t *o)
{
    surface(o, UI_PROGRESS_TRACK, LV_RADIUS_CIRCLE, 0, 0);
    lv_obj_set_style_bg_color(o, UI_OK_BRIGHT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
}
void ui_theme_studio_apply_slider_style(lv_obj_t *o)
{
    ui_theme_studio_apply_progress_bar_style(o);
    lv_obj_set_style_bg_color(o, UI_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(o, UI_TEXT, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(o, 6, LV_PART_KNOB);
}
void ui_theme_studio_apply_telemetry_plot_style(lv_obj_t *o)
{
    surface(o, UI_TELEMETRY_CHART_BG, 12, 0, 0);
    lv_obj_set_style_line_color(o, UI_TELEMETRY_GRID, 0);
    lv_obj_set_style_line_width(o, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(o, 0, 0, LV_PART_INDICATOR);
}
void ui_theme_studio_apply_trace_marker_style(lv_obj_t *o, lv_color_t c) { surface(o,c,LV_RADIUS_CIRCLE,0,0); }
void ui_theme_studio_apply_reference_line_style(lv_obj_t *o, lv_color_t c) { surface(o,c,0,0,0); }
void ui_theme_studio_apply_button_style(lv_obj_t *o) { button(o,UI_ACCENT_2,UI_ACCENT_2); }
void ui_theme_studio_apply_button_dark_style(lv_obj_t *o) { button(o,UI_PANEL,UI_BORDER); }
void ui_theme_studio_apply_button_success_style(lv_obj_t *o) { button(o,UI_CONTROL,UI_OK_BRIGHT); }
void ui_theme_studio_apply_button_warning_style(lv_obj_t *o) { button(o,UI_CONTROL,UI_WARN); }
void ui_theme_studio_apply_button_danger_style(lv_obj_t *o) { button(o,UI_PANEL,UI_DANGER_BRIGHT); }
void ui_theme_studio_apply_button_cancel_style(lv_obj_t *o) { button(o,UI_PANEL,UI_BORDER_CONTROL); }
void ui_theme_studio_apply_button_close_style(lv_obj_t *o) { button(o,UI_PANEL,UI_BORDER_CONTROL); }
void ui_theme_studio_apply_button_outlined_style(lv_obj_t *o) { button(o,UI_PANEL,UI_ACCENT); }
void ui_theme_studio_apply_label_primary(lv_obj_t *o) { color(o,UI_TEXT); }
void ui_theme_studio_apply_label_bright(lv_obj_t *o) { color(o,UI_TEXT_BRIGHT); }
void ui_theme_studio_apply_label_dim(lv_obj_t *o) { color(o,UI_TEXT_DIM); }
void ui_theme_studio_apply_label_muted(lv_obj_t *o) { color(o,UI_TEXT_MUTED); }
void ui_theme_studio_apply_label_success(lv_obj_t *o) { color(o,UI_OK_BRIGHT); }
void ui_theme_studio_apply_label_warning(lv_obj_t *o) { color(o,UI_WARN); }
void ui_theme_studio_apply_label_error(lv_obj_t *o) { color(o,UI_DANGER_BRIGHT); }
void ui_theme_studio_apply_text_caption(lv_obj_t *o) { font(o,UI_FONT_CAPTION); }
void ui_theme_studio_apply_text_body(lv_obj_t *o) { font(o,UI_FONT_BODY); }
void ui_theme_studio_apply_text_body_large(lv_obj_t *o) { font(o,UI_FONT_BODY_LARGE); }
void ui_theme_studio_apply_text_button(lv_obj_t *o) { font(o,UI_FONT_BODY_LARGE); }
void ui_theme_studio_apply_text_value_small(lv_obj_t *o) { font(o,UI_FONT_VALUE_SMALL); }
void ui_theme_studio_apply_text_title(lv_obj_t *o) { font(o,UI_FONT_TITLE); }
void ui_theme_studio_apply_text_dialog_title(lv_obj_t *o) { font(o,UI_FONT_DIALOG_TITLE); }
void ui_theme_studio_apply_text_popup_title(lv_obj_t *o) { font(o,UI_FONT_POPUP_TITLE); }
void ui_theme_studio_apply_text_value(lv_obj_t *o) { font(o,UI_FONT_VALUE); }
void ui_theme_studio_apply_text_heading(lv_obj_t *o) { font(o,UI_FONT_HEADING); }
void ui_theme_studio_apply_text_percent(lv_obj_t *o) { font(o,UI_FONT_PERCENT); }
lv_color_t ui_theme_studio_status_color(ui_status_kind_t k)
{
    switch(k) {
    case UI_STATUS_OK: return UI_OK_BRIGHT;
    case UI_STATUS_WARNING: return UI_WARN;
    case UI_STATUS_DANGER: return UI_DANGER_BRIGHT;
    case UI_STATUS_INFO: case UI_STATUS_ACTIVE: return UI_ACCENT;
    default: return UI_TEXT_DIM;
    }
}
bool ui_theme_studio_banner_matches(lv_obj_t *o, ui_status_kind_t k)
{
    return o && lv_color_eq(lv_obj_get_style_bg_color(o,0), UI_PANEL) &&
        lv_color_eq(lv_obj_get_style_border_color(o,0), ui_theme_studio_status_color(k)) &&
        lv_obj_get_style_radius(o,0) == 18 &&
        lv_obj_get_style_border_width(o,0) == UI_BORDER_THIN;
}
void ui_theme_studio_apply_banner_status_style(lv_obj_t *o, ui_status_kind_t k)
{
    surface(o,UI_PANEL,18,1,UI_PAD_CARD);
    if(o) lv_obj_set_style_border_color(o,ui_theme_studio_status_color(k),0);
}
void ui_theme_studio_apply_button_status_style(lv_obj_t *o, ui_status_kind_t k)
{
    button(o, k == UI_STATUS_ACTIVE || k == UI_STATUS_INFO ? UI_ACCENT_2 : UI_PANEL,
        ui_theme_studio_status_color(k));
}
