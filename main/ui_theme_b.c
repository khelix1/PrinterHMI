/*
 * Theme B: Operator
 *
 * Visual language established by the redesigned Drybox page:
 *
 *   - deep layered backgrounds
 *   - restrained cyan outlines
 *   - minimal filled controls
 *   - strong state colors
 *   - large, high-contrast values
 *   - compact industrial typography
 */

#include "ui_theme_b.h"
#include "ui_font_fallback.h"


/*
 * Shared Theme B button frame.
 *
 * Semantic button functions continue to own their background and accent
 * colors. Every Theme B button receives this same physical frame.
 */
static void theme_b_apply_shared_button_frame(lv_obj_t *button)
{
    if (!button) {
        return;
    }

    /*
     * The shared frame owns only physical button structure.
     *
     * Semantic functions retain ownership of background and border
     * colors so PRIMARY, SUCCESS, WARNING, DANGER and OUTLINED remain
     * visually distinct.
     */
    lv_obj_set_style_radius(
        button,
        UI_RADIUS_BAR,
        LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_border_width(
        button,
        UI_BORDER_THIN,
        LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_shadow_width(
        button,
        0,
        LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_outline_width(
        button,
        0,
        LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void apply_surface(
    lv_obj_t *obj,
    lv_color_t background,
    lv_color_t border,
    int32_t border_width,
    int32_t radius,
    int32_t padding)
{
    if (!obj) {
        return;
    }

    lv_obj_set_style_bg_color(obj, background, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, border, 0);
    lv_obj_set_style_border_width(obj, border_width, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_pad_all(obj, padding, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
}

static void apply_button(
    lv_obj_t *obj,
    lv_color_t background,
    lv_color_t border,
    lv_color_t pressed_background,
    int32_t border_width)
{
    if (!obj) {
        return;
    }

    /*
     * Operator controls use a flat industrial treatment:
     * dark face, restrained semantic outline, square profile,
     * and direct mechanical-looking press feedback.
     */
    lv_obj_set_style_bg_color(obj, background, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, border, 0);
    lv_obj_set_style_border_width(obj, border_width, 0);
    lv_obj_set_style_radius(obj, UI_RADIUS_BAR, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);

    lv_obj_set_style_bg_color(
        obj,
        pressed_background,
        LV_STATE_PRESSED);
    lv_obj_set_style_border_color(
        obj,
        border,
        LV_STATE_PRESSED);
    lv_obj_set_style_translate_y(
        obj,
        1,
        LV_STATE_PRESSED);

    lv_obj_set_style_border_color(
        obj,
        UI_BORDER_BRIGHT,
        LV_STATE_FOCUSED);

    theme_b_apply_shared_button_frame(obj);
}

static void apply_label(
    lv_obj_t *obj,
    lv_color_t color)
{
    if (!obj) {
        return;
    }

    lv_obj_set_style_text_color(obj, color, 0);
}

static void apply_font(
    lv_obj_t *obj,
    const lv_font_t *font)
{
    if (!obj || !font) {
        return;
    }

    lv_obj_set_style_text_font(obj, ui_font_with_fallback(font), 0);
}

/* ------------------------------------------------------------
 * Containers
 * ------------------------------------------------------------ */

void ui_theme_b_apply_root_style(lv_obj_t *obj)
{
    apply_surface(
        obj,
        UI_BG,
        UI_BG,
        UI_BORDER_NONE,
        UI_RADIUS_NONE,
        0);
}

void ui_theme_b_apply_panel_style(lv_obj_t *obj)
{
    apply_surface(
        obj,
        UI_PANEL,
        UI_BORDER_SOFT,
        UI_BORDER_THIN,
        UI_RADIUS_BANNER,
        UI_PAD_PANEL);
}

void ui_theme_b_apply_card_style(lv_obj_t *obj)
{
    apply_surface(
        obj,
        UI_PANEL,
        UI_BORDER_SOFT,
        UI_BORDER_THIN,
        UI_RADIUS_BANNER,
        UI_PAD_CARD);
}

void ui_theme_b_apply_banner_style(lv_obj_t *obj)
{
    /*
     * Machine-state banner: flat, compact, and structural.
     * Page modules may replace the background and border with
     * the current state color after applying this base style.
     */
    apply_surface(
        obj,
        UI_BG_DEEP,
        UI_ACCENT_CYAN,
        UI_BORDER_THIN,
        UI_RADIUS_BAR,
        UI_PAD_CARD);
}

void ui_theme_b_apply_preview_style(lv_obj_t *obj)
{
    apply_surface(
        obj,
        UI_BG_DEEP,
        UI_BORDER_SOFT,
        UI_BORDER_THIN,
        UI_RADIUS_PREVIEW,
        0);
}

void ui_theme_b_apply_info_box_style(lv_obj_t *obj)
{
    apply_surface(
        obj,
        UI_PANEL,
        UI_BORDER_SOFT,
        UI_BORDER_THIN,
        UI_RADIUS_BANNER,
        UI_PAD_CARD);
}

void ui_theme_b_apply_popup_style(lv_obj_t *obj)
{
    if (!obj) {
        return;
    }

    /*
     * Operator popups use the same physical surface language as Operator
     * cards. Popups differ by scale and layering, not by visual vocabulary.
     */
    lv_obj_set_style_radius(
        obj,
        UI_RADIUS_CARD,
        0);

    lv_obj_set_style_bg_color(
        obj,
        UI_CARD,
        0);

    lv_obj_set_style_bg_opa(
        obj,
        LV_OPA_COVER,
        0);

    lv_obj_set_style_border_color(
        obj,
        UI_BORDER,
        0);

    lv_obj_set_style_border_width(
        obj,
        UI_BORDER_THIN,
        0);

    lv_obj_set_style_pad_all(
        obj,
        UI_SPACE_MD,
        0);

    lv_obj_set_style_shadow_width(
        obj,
        0,
        0);
}

void ui_theme_b_apply_dialog_style(lv_obj_t *obj)
{
    apply_surface(
        obj,
        UI_BG_POPUP,
        UI_BORDER_BRIGHT,
        UI_BORDER_STRONG,
        UI_RADIUS_DIALOG,
        UI_PAD_POPUP);
}

void ui_theme_b_apply_surface_role(lv_obj_t *obj, ui_surface_role_t role)
{
    if (!obj) return;

    switch (role) {
    case UI_SURFACE_TRANSPARENT:
        lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_radius(obj, 0, 0);
        lv_obj_set_style_pad_all(obj, 0, 0);
        lv_obj_set_style_shadow_width(obj, 0, 0);
        break;
    case UI_SURFACE_SHELL_TOPBAR:
        apply_surface(obj, UI_TOPBAR, UI_TOPBAR, 0, 0, 0);
        break;
    case UI_SURFACE_SHELL_NAV:
        apply_surface(obj, UI_NAV, UI_BORDER, 0, 0, 0);
        break;
    case UI_SURFACE_PAGE_DEEP:
        apply_surface(obj, UI_BG_DEEP, UI_BG_DEEP, 0, 0, 0);
        break;
    case UI_SURFACE_SECTION:
        apply_surface(obj, UI_CARD, UI_BORDER, 1, UI_RADIUS_CARD, 0);
        break;
    case UI_SURFACE_LIST_ROW:
        apply_surface(obj, UI_BG_DEEP, UI_BORDER_SOFT, 1, UI_RADIUS_BAR, 0);
        lv_obj_set_style_bg_color(obj, UI_PANEL, LV_STATE_PRESSED);
        lv_obj_set_style_border_color(obj, UI_ACCENT_CYAN, LV_STATE_PRESSED);
        break;
    case UI_SURFACE_PREVIEW_WELL:
        apply_surface(obj, UI_PANEL, UI_BORDER_SOFT, 1, UI_RADIUS_BAR, 0);
        break;
    case UI_SURFACE_STATUS_PILL:
        apply_surface(obj, UI_BG_DEEP, UI_BORDER, 1, LV_RADIUS_CIRCLE, 0);
        break;
    case UI_SURFACE_POPUP_LIST:
        apply_surface(obj, UI_BG_DEEP, UI_BORDER, 1, UI_RADIUS_CARD, 8);
        lv_obj_set_style_pad_row(obj, 6, 0);
        break;
    case UI_SURFACE_TEXT_INPUT:
        apply_surface(obj, UI_BG_DEEP, UI_BORDER, 1, UI_RADIUS_BTN, 0);
        lv_obj_set_style_pad_left(obj, 14, 0);
        lv_obj_set_style_pad_right(obj, 14, 0);
        lv_obj_set_style_text_color(obj, UI_TEXT, 0);
        lv_obj_set_style_border_color(obj, UI_BORDER_BRIGHT, LV_STATE_FOCUSED);
        lv_obj_set_style_border_width(obj, 2, LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(obj, UI_TEXT_MUTED, LV_PART_TEXTAREA_PLACEHOLDER);
        break;
    case UI_SURFACE_KEYBOARD:
        apply_surface(obj, UI_BG_POPUP, UI_BORDER, 1, UI_RADIUS_CARD, 8);
        lv_obj_set_style_text_font(obj, ui_font_with_fallback(UI_FONT_BODY_LARGE), 0);
        lv_obj_set_style_text_font(obj, ui_font_with_fallback(UI_FONT_TITLE), LV_PART_ITEMS);
        lv_obj_set_style_bg_color(obj, UI_BG_DEEP, LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_ITEMS);
        lv_obj_set_style_border_color(obj, UI_BORDER, LV_PART_ITEMS);
        lv_obj_set_style_border_width(obj, 1, LV_PART_ITEMS);
        lv_obj_set_style_radius(obj, UI_RADIUS_BTN, LV_PART_ITEMS);
        lv_obj_set_style_text_color(obj, UI_TEXT, LV_PART_ITEMS);
        lv_obj_set_style_bg_color(obj, UI_ACCENT, LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(obj, UI_BORDER_BRIGHT, LV_PART_ITEMS | LV_STATE_PRESSED);
        break;
    case UI_SURFACE_TELEMETRY_ROOT:
        apply_surface(obj, UI_TELEMETRY_ROOT_BG, UI_TELEMETRY_ROOT_BG, 0, 0, 0);
        break;
    case UI_SURFACE_TELEMETRY_CARD:
        apply_surface(obj, UI_TELEMETRY_CARD_BG, UI_BORDER, 1, UI_RADIUS_CARD, 0);
        break;
    case UI_SURFACE_TELEMETRY_PANEL:
        apply_surface(obj, UI_TELEMETRY_PANEL_BG, UI_BORDER, 1, UI_RADIUS_CARD, 0);
        break;
    case UI_SURFACE_TELEMETRY_CHART:
        apply_surface(obj, UI_TELEMETRY_CHART_CARD_BG, UI_BORDER_SOFT, 1, UI_RADIUS_BAR, 0);
        break;
    case UI_SURFACE_DIVIDER:
        apply_surface(obj, UI_BORDER, UI_BORDER, 0, 0, 0);
        break;
    case UI_SURFACE_INDICATOR:
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_radius(obj, UI_RADIUS_BAR, 0);
        lv_obj_set_style_pad_all(obj, 0, 0);
        lv_obj_set_style_shadow_width(obj, 0, 0);
        break;
    default:
        break;
    }
}

void ui_theme_b_apply_custom_label_style(lv_obj_t *obj,
                                         const lv_font_t *font,
                                         lv_color_t color)
{
    apply_font(obj, font);
    apply_label(obj, color);
}

void ui_theme_b_apply_slider_style(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, UI_BG_DEEP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, UI_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, UI_ACCENT_CYAN, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_color(obj, UI_TEXT_BRIGHT, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_border_color(obj, UI_ACCENT_CYAN, LV_PART_KNOB);
    lv_obj_set_style_border_width(obj, 2, LV_PART_KNOB);
    lv_obj_set_style_pad_all(obj, 6, LV_PART_KNOB);
}

void ui_theme_b_apply_progress_bar_style(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, UI_BG_DEEP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, UI_BORDER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, UI_RADIUS_BAR, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, UI_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(obj, UI_RADIUS_BAR, LV_PART_INDICATOR);
}

void ui_theme_b_apply_telemetry_plot_style(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_set_style_radius(obj, UI_RADIUS_BAR, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, UI_TELEMETRY_CHART_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_line_color(obj, UI_TELEMETRY_GRID, LV_PART_MAIN);
    lv_obj_set_style_line_opa(obj, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_size(obj, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(obj, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_opa(obj, LV_OPA_80, LV_PART_ITEMS);
}

void ui_theme_b_apply_trace_marker_style(lv_obj_t *obj, lv_color_t color)
{
    if (!obj) return;
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, UI_TEXT_BRIGHT, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_shadow_color(obj, color, 0);
    lv_obj_set_style_shadow_width(obj, 8, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

void ui_theme_b_apply_reference_line_style(lv_obj_t *obj, lv_color_t color)
{
    if (!obj) return;
    lv_obj_set_style_radius(obj, 1, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_70, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

/* ------------------------------------------------------------
 * Buttons
 * ------------------------------------------------------------ */

void ui_theme_b_apply_button_style(lv_obj_t *obj)
{
    apply_button(
        obj,
        UI_BG_DEEP,
        UI_ACCENT_CYAN,
        UI_CONTROL,
        UI_BORDER_THIN);

}

void ui_theme_b_apply_button_dark_style(lv_obj_t *obj)
{
    apply_button(
        obj,
        UI_BG_DEEP,
        UI_BORDER_SOFT,
        UI_CONTROL,
        UI_BORDER_THIN);

}

void ui_theme_b_apply_button_success_style(lv_obj_t *obj)
{
    apply_button(
        obj,
        UI_BG_DEEP,
        UI_OK,
        UI_OK,
        UI_BORDER_THIN);

}

void ui_theme_b_apply_button_warning_style(lv_obj_t *obj)
{
    apply_button(
        obj,
        UI_BG_DEEP,
        UI_WARN,
        UI_WARN_DARK,
        UI_BORDER_THIN);

}

void ui_theme_b_apply_button_danger_style(lv_obj_t *obj)
{
    apply_button(
        obj,
        UI_BG_DEEP,
        UI_DANGER_BRIGHT,
        UI_DANGER_DARK,
        UI_BORDER_THIN);

}

void ui_theme_b_apply_button_cancel_style(lv_obj_t *obj)
{
    apply_button(
        obj,
        UI_BG_DEEP,
        UI_BORDER_CONTROL,
        UI_CONTROL,
        UI_BORDER_THIN);

}

void ui_theme_b_apply_button_close_style(lv_obj_t *obj)
{
    apply_button(
        obj,
        UI_BG_DEEP,
        UI_DANGER_BRIGHT,
        UI_CONTROL_CLOSE,
        UI_BORDER_THIN);

}

void ui_theme_b_apply_button_outlined_style(lv_obj_t *obj)
{
    apply_button(
        obj,
        UI_BG_DEEP,
        UI_BORDER_CONTROL,
        UI_CONTROL,
        UI_BORDER_THIN);

}

/* ------------------------------------------------------------
 * Label colors
 * ------------------------------------------------------------ */

void ui_theme_b_apply_label_primary(lv_obj_t *obj)
{
    apply_label(obj, UI_TEXT);
}

void ui_theme_b_apply_label_bright(lv_obj_t *obj)
{
    apply_label(obj, UI_TEXT_BRIGHT);
}

void ui_theme_b_apply_label_dim(lv_obj_t *obj)
{
    apply_label(obj, UI_TEXT_DIM);
}

void ui_theme_b_apply_label_muted(lv_obj_t *obj)
{
    apply_label(obj, UI_TEXT_MUTED);
}

void ui_theme_b_apply_label_success(lv_obj_t *obj)
{
    apply_label(obj, UI_OK_BRIGHT);
}

void ui_theme_b_apply_label_warning(lv_obj_t *obj)
{
    apply_label(obj, UI_WARN);
}

void ui_theme_b_apply_label_error(lv_obj_t *obj)
{
    apply_label(obj, UI_DANGER_BRIGHT);
}

/* ------------------------------------------------------------
 * Typography
 * ------------------------------------------------------------ */

void ui_theme_b_apply_text_caption(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_CAPTION);
}

void ui_theme_b_apply_text_body(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_BODY);
}

void ui_theme_b_apply_text_body_large(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_BODY_LARGE);
}

void ui_theme_b_apply_text_button(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_BODY_LARGE);

}

void ui_theme_b_apply_text_value_small(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_VALUE_SMALL);
}

void ui_theme_b_apply_text_title(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_TITLE);
}

void ui_theme_b_apply_text_dialog_title(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_DIALOG_TITLE);
}

void ui_theme_b_apply_text_popup_title(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_POPUP_TITLE);
}

void ui_theme_b_apply_text_value(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_VALUE);
}

void ui_theme_b_apply_text_heading(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_HEADING);
}

void ui_theme_b_apply_text_percent(lv_obj_t *obj)
{
    apply_font(obj, UI_FONT_PERCENT);
}

/* ------------------------------------------------------------
 * Status colors
 * ------------------------------------------------------------ */

lv_color_t ui_theme_b_status_color(ui_status_kind_t kind)
{
    switch (kind) {
        case UI_STATUS_INFO:
            return UI_ACCENT_INFO;

        case UI_STATUS_OK:
            return UI_OK_BRIGHT;

        case UI_STATUS_WARNING:
            return UI_WARN;

        case UI_STATUS_DANGER:
            return UI_DANGER_BRIGHT;

        case UI_STATUS_ACTIVE:
            return UI_ACCENT_CYAN;

        case UI_STATUS_NEUTRAL:
        default:
            return UI_TEXT_DIM;
    }
}


/*
 * Semantic Operator banner styling.
 *
 * Theme B owns the complete banner surface. Components provide only
 * the semantic machine state.
 */
void ui_theme_b_apply_banner_status_style(
    lv_obj_t *obj,
    ui_status_kind_t kind)
{
    apply_surface(
        obj,
        UI_BG_DEEP,
        ui_theme_b_status_color(kind),
        UI_BORDER_THIN,
        UI_RADIUS_BAR,
        UI_PAD_CARD);
}

/*
 * Semantic Operator button styling.
 *
 * Theme B owns background, border, radius, press feedback and focus
 * treatment. Components provide only the semantic action category.
 */
void ui_theme_b_apply_button_status_style(
    lv_obj_t *obj,
    ui_status_kind_t kind)
{
    lv_color_t border =
        ui_theme_b_status_color(kind);

    lv_color_t pressed =
        UI_CONTROL;

    if (kind == UI_STATUS_WARNING) {
        pressed = UI_WARN_DARK;
    } else if (kind == UI_STATUS_DANGER) {
        pressed = UI_DANGER_DARK;
    } else if (kind == UI_STATUS_OK) {
        pressed = UI_OK;
    }

    apply_button(
        obj,
        UI_BG_DEEP,
        border,
        pressed,
        UI_BORDER_THIN);

}
