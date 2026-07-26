#include "ui_theme.h"

#include "custom_theme.h"
#include "ui_theme_a.h"
#include "ui_theme_b.h"
#include "ui_theme_c.h"

/*
 * Public theme dispatcher.
 *
 * UI modules continue to call ui_apply_*() without knowing which
 * concrete theme is active.
 *
 * Operator is the default for newly-created objects.
 * Foundry is the independent Theme A visual test surface.
 */

static ui_theme_id_t s_active_theme = UI_THEME_OPERATOR;
static ui_accent_id_t s_active_accent = UI_ACCENT_DEFAULT;
static ui_density_id_t s_active_density = UI_DENSITY_COMFORTABLE;
static ui_accessibility_t s_accessibility = {0};

void ui_theme_set_active(ui_theme_id_t theme)
{
    switch (theme) {
        case UI_THEME_CLASSIC:
        case UI_THEME_OPERATOR:
        case UI_THEME_GLASS:
            s_active_theme = theme;
            break;

        default:
            s_active_theme = UI_THEME_OPERATOR;
            break;
    }
}

ui_theme_id_t ui_theme_get_active(void)
{
    return s_active_theme;
}

const char *ui_theme_name(ui_theme_id_t theme)
{
    switch (theme) {
        case UI_THEME_CLASSIC:
            return "Foundry";

        case UI_THEME_GLASS:
            return "Dark Glass";

        case UI_THEME_OPERATOR:
        default:
            return "Operator";
    }
}

lv_color_t ui_theme_color(uint32_t operator_rgb,
                          uint32_t foundry_rgb,
                          uint32_t glass_rgb)
{
    uint32_t custom_rgb = 0;
    if (custom_theme_color_override(
            operator_rgb,
            foundry_rgb,
            glass_rgb,
            &custom_rgb)) {
        return lv_color_hex(custom_rgb);
    }

    if (s_active_theme == UI_THEME_CLASSIC) {
        return lv_color_hex(foundry_rgb);
    }
    if (s_active_theme == UI_THEME_GLASS) {
        return lv_color_hex(glass_rgb);
    }
    return lv_color_hex(operator_rgb);
}

int32_t ui_theme_metric(int32_t operator_value,
                        int32_t foundry_value,
                        int32_t glass_value)
{
    int32_t custom_value = 0;
    if (custom_theme_metric_override(
            operator_value,
            foundry_value,
            glass_value,
            &custom_value)) {
        return custom_value;
    }

    if (s_active_theme == UI_THEME_CLASSIC) {
        return foundry_value;
    }
    if (s_active_theme == UI_THEME_GLASS) {
        return glass_value;
    }
    return operator_value;
}

void ui_theme_set_accent(ui_accent_id_t accent)
{
    s_active_accent = accent < UI_ACCENT_COUNT
        ? accent
        : UI_ACCENT_DEFAULT;
}

ui_accent_id_t ui_theme_get_accent(void)
{
    return s_active_accent;
}

void ui_theme_set_density(ui_density_id_t density)
{
    s_active_density = density < UI_DENSITY_COUNT
        ? density
        : UI_DENSITY_COMFORTABLE;
}

ui_density_id_t ui_theme_get_density(void)
{
    return s_active_density;
}

void ui_theme_set_accessibility(ui_accessibility_t options)
{
    s_accessibility = options;
}

ui_accessibility_t ui_theme_get_accessibility(void)
{
    return s_accessibility;
}

lv_color_t ui_theme_accent_color(uint32_t operator_rgb,
                                 uint32_t foundry_rgb,
                                 uint32_t glass_rgb,
                                 uint8_t tone)
{
    if (s_active_accent == UI_ACCENT_DEFAULT) {
        uint32_t custom_rgb = 0;
        if (custom_theme_accent_override(
                tone,
                &custom_rgb)) {
            return lv_color_hex(custom_rgb);
        }
        return ui_theme_color(operator_rgb, foundry_rgb, glass_rgb);
    }

    static const uint32_t colors[UI_ACCENT_COUNT][3] = {
        {0, 0, 0},
        {0x126B7A, 0x19C7E8, 0x79E7F7},
        {0x224F9C, 0x3A8DFF, 0x8FC2FF},
        {0x176B50, 0x2DBE83, 0x8BE8C1},
        {0x8A5A10, 0xE0A22E, 0xFFD27A},
        {0x53338F, 0x8C62D9, 0xC7A8FF},
    };

    if (tone > 2) tone = 1;
    return lv_color_hex(colors[s_active_accent][tone]);
}

int32_t ui_theme_density_metric(int32_t compact_value,
                                int32_t comfortable_value,
                                int32_t large_value)
{
    if (s_active_density == UI_DENSITY_COMPACT) return compact_value;
    if (s_active_density == UI_DENSITY_LARGE) return large_value;
    return comfortable_value;
}

const lv_font_t *ui_theme_density_font(const lv_font_t *compact,
                                       const lv_font_t *comfortable,
                                       const lv_font_t *spacious,
                                       const lv_font_t *accessible)
{
    if (s_accessibility.large_text) return accessible;
    if (s_active_density == UI_DENSITY_LARGE) return spacious;
    if (s_active_density == UI_DENSITY_COMPACT) return compact;
    return comfortable;
}

lv_color_t ui_theme_accessible_color(lv_color_t normal,
                                     uint32_t high_contrast_rgb)
{
    return s_accessibility.high_contrast
        ? lv_color_hex(high_contrast_rgb)
        : normal;
}

lv_opa_t ui_theme_accessible_opacity(lv_opa_t normal)
{
    return s_accessibility.reduced_transparency
        ? LV_OPA_COVER
        : normal;
}

int32_t ui_theme_accessible_border_width(int32_t normal)
{
    return s_accessibility.high_contrast
        ? normal + 1
        : normal;
}

bool ui_theme_motion_enabled(void)
{
    return !s_accessibility.reduced_motion;
}

#define DISPATCH_VOID(function_name, object)             \
    do {                                                 \
        if (s_active_theme == UI_THEME_CLASSIC) {         \
            ui_theme_a_##function_name(object);           \
        } else if (s_active_theme == UI_THEME_GLASS) {   \
            ui_theme_c_##function_name(object);           \
        } else {                                         \
            ui_theme_b_##function_name(object);           \
        }                                                \
    } while (0)

static void apply_custom_surface_opacity(lv_obj_t *object)
{
    uint8_t opacity = 0;
    if (object &&
        custom_theme_surface_opacity(&opacity)) {
        lv_obj_set_style_bg_opa(
            object,
            (lv_opa_t)opacity,
            LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_apply_root_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_root_style, obj);
}

void ui_apply_panel_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_panel_style, obj);
    apply_custom_surface_opacity(obj);
}

void ui_apply_card_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_card_style, obj);
    apply_custom_surface_opacity(obj);
}

void ui_apply_banner_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_banner_style, obj);
    apply_custom_surface_opacity(obj);
}

void ui_apply_banner_status_style(
    lv_obj_t *obj,
    ui_status_kind_t kind)
{
    if (s_active_theme == UI_THEME_CLASSIC) {
        ui_theme_a_apply_banner_style(obj);
        apply_custom_surface_opacity(obj);
        return;
    }

    if (s_active_theme == UI_THEME_GLASS) {
        ui_theme_c_apply_banner_status_style(obj, kind);
        apply_custom_surface_opacity(obj);
        return;
    }

    ui_theme_b_apply_banner_status_style(
        obj,
        kind);
    apply_custom_surface_opacity(obj);
}

void ui_apply_preview_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_preview_style, obj);
    apply_custom_surface_opacity(obj);
}

void ui_apply_info_box_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_info_box_style, obj);
    apply_custom_surface_opacity(obj);
}

void ui_apply_popup_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_popup_style, obj);
    apply_custom_surface_opacity(obj);

    /*
     * Shared popup primitives use explicit coordinates measured from the
     * popup's inner origin. Theme-owned container padding changes LVGL's
     * alignment reference while callers continue to use the outer height,
     * which can place footer buttons across their divider. Keep popup
     * geometry theme-independent; themes still own every visual property.
     */
    if (obj) {
        lv_obj_set_style_pad_all(obj, 0, 0);
    }
}

void ui_apply_dialog_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_dialog_style, obj);
    apply_custom_surface_opacity(obj);
}

void ui_apply_surface_role(lv_obj_t *obj, ui_surface_role_t role)
{
    if (s_active_theme == UI_THEME_CLASSIC) {
        ui_theme_a_apply_surface_role(obj, role);
    } else if (s_active_theme == UI_THEME_GLASS) {
        ui_theme_c_apply_surface_role(obj, role);
    } else {
        ui_theme_b_apply_surface_role(obj, role);
    }

    switch (role) {
        case UI_SURFACE_TRANSPARENT:
        case UI_SURFACE_DIVIDER:
        case UI_SURFACE_INDICATOR:
            break;
        default:
            apply_custom_surface_opacity(obj);
            break;
    }
}

void ui_apply_custom_label_style(lv_obj_t *obj,
                                 const lv_font_t *font,
                                 lv_color_t color)
{
    if (s_active_theme == UI_THEME_CLASSIC) {
        ui_theme_a_apply_custom_label_style(obj, font, color);
    } else if (s_active_theme == UI_THEME_GLASS) {
        ui_theme_c_apply_custom_label_style(obj, font, color);
    } else {
        ui_theme_b_apply_custom_label_style(obj, font, color);
    }
}

void ui_apply_slider_style(lv_obj_t *obj)
{
    if (s_active_theme == UI_THEME_CLASSIC) ui_theme_a_apply_slider_style(obj);
    else if (s_active_theme == UI_THEME_GLASS) ui_theme_c_apply_slider_style(obj);
    else ui_theme_b_apply_slider_style(obj);
}

void ui_apply_progress_bar_style(lv_obj_t *obj)
{
    if (s_active_theme == UI_THEME_CLASSIC) ui_theme_a_apply_progress_bar_style(obj);
    else if (s_active_theme == UI_THEME_GLASS) ui_theme_c_apply_progress_bar_style(obj);
    else ui_theme_b_apply_progress_bar_style(obj);
}

void ui_apply_telemetry_plot_style(lv_obj_t *obj)
{
    if (s_active_theme == UI_THEME_CLASSIC) ui_theme_a_apply_telemetry_plot_style(obj);
    else if (s_active_theme == UI_THEME_GLASS) ui_theme_c_apply_telemetry_plot_style(obj);
    else ui_theme_b_apply_telemetry_plot_style(obj);
}

void ui_apply_trace_marker_style(lv_obj_t *obj, lv_color_t color)
{
    if (s_active_theme == UI_THEME_CLASSIC) ui_theme_a_apply_trace_marker_style(obj, color);
    else if (s_active_theme == UI_THEME_GLASS) ui_theme_c_apply_trace_marker_style(obj, color);
    else ui_theme_b_apply_trace_marker_style(obj, color);
}

void ui_apply_reference_line_style(lv_obj_t *obj, lv_color_t color)
{
    if (s_active_theme == UI_THEME_CLASSIC) ui_theme_a_apply_reference_line_style(obj, color);
    else if (s_active_theme == UI_THEME_GLASS) ui_theme_c_apply_reference_line_style(obj, color);
    else ui_theme_b_apply_reference_line_style(obj, color);
}

void ui_apply_button_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_button_style, obj);
}

void ui_apply_button_dark_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_button_dark_style, obj);
}

void ui_apply_button_success_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_button_success_style, obj);
}

void ui_apply_button_warning_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_button_warning_style, obj);
}

void ui_apply_button_danger_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_button_danger_style, obj);
}

void ui_apply_button_cancel_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_button_cancel_style, obj);
}

void ui_apply_button_close_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_button_close_style, obj);
}

void ui_apply_button_outlined_style(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_button_outlined_style, obj);
}

void ui_apply_button_status_style(
    lv_obj_t *obj,
    ui_status_kind_t kind)
{
    if (s_active_theme == UI_THEME_GLASS) {
        ui_theme_c_apply_button_status_style(obj, kind);
        return;
    }

    if (s_active_theme == UI_THEME_OPERATOR) {
        ui_theme_b_apply_button_status_style(
            obj,
            kind);
        return;
    }

    switch (kind) {
        case UI_STATUS_OK:
            ui_theme_a_apply_button_success_style(obj);
            break;

        case UI_STATUS_WARNING:
            ui_theme_a_apply_button_warning_style(obj);
            break;

        case UI_STATUS_DANGER:
            ui_theme_a_apply_button_danger_style(obj);
            break;

        case UI_STATUS_NEUTRAL:
            ui_theme_a_apply_button_dark_style(obj);
            break;

        case UI_STATUS_INFO:
        case UI_STATUS_ACTIVE:
        default:
            ui_theme_a_apply_button_style(obj);
            break;
    }
}

void ui_apply_label_primary(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_label_primary, obj);
}

void ui_apply_label_bright(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_label_bright, obj);
}

void ui_apply_label_dim(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_label_dim, obj);
}

void ui_apply_label_muted(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_label_muted, obj);
}

void ui_apply_label_success(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_label_success, obj);
}

void ui_apply_label_warning(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_label_warning, obj);
}

void ui_apply_label_error(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_label_error, obj);
}

void ui_apply_text_caption(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_caption, obj);
}

void ui_apply_text_body(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_body, obj);
}

void ui_apply_text_body_large(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_body_large, obj);
}

void ui_apply_text_button(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_button, obj);
}

void ui_apply_text_value_small(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_value_small, obj);
}

void ui_apply_text_title(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_title, obj);
}

void ui_apply_text_dialog_title(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_dialog_title, obj);
}

void ui_apply_text_popup_title(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_popup_title, obj);
}

void ui_apply_text_value(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_value, obj);
}

void ui_apply_text_heading(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_heading, obj);
}

void ui_apply_text_percent(lv_obj_t *obj)
{
    DISPATCH_VOID(apply_text_percent, obj);
}

lv_color_t ui_status_color(ui_status_kind_t kind)
{
    if (s_active_theme == UI_THEME_CLASSIC) {
        return ui_theme_a_status_color(kind);
    }

    if (s_active_theme == UI_THEME_GLASS) {
        return ui_theme_c_status_color(kind);
    }

    return ui_theme_b_status_color(kind);
}
