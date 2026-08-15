#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

/* ============================================================
 * PrinterHMI runtime theme contract
 * ============================================================ */

typedef enum {
    UI_THEME_CLASSIC = 0,
    UI_THEME_OPERATOR,
    UI_THEME_GLASS,
    /* Operator Shell is a separate layout selection using the Operator palette. */
    UI_THEME_OPERATOR_SHELL
} ui_theme_id_t;

typedef enum {
    UI_ACCENT_DEFAULT = 0,
    UI_ACCENT_ID_CYAN,
    UI_ACCENT_ID_BLUE,
    UI_ACCENT_ID_GREEN,
    UI_ACCENT_ID_AMBER,
    UI_ACCENT_ID_VIOLET,
    UI_ACCENT_COUNT
} ui_accent_id_t;

typedef enum {
    UI_DENSITY_COMPACT = 0,
    UI_DENSITY_COMFORTABLE,
    UI_DENSITY_LARGE,
    UI_DENSITY_COUNT
} ui_density_id_t;

typedef struct {
    bool large_text;
    bool high_contrast;
    bool reduced_transparency;
    bool reduced_motion;
} ui_accessibility_t;

void ui_theme_set_active(ui_theme_id_t theme);
ui_theme_id_t ui_theme_get_active(void);
const char *ui_theme_name(ui_theme_id_t theme);

/* Layout code uses this to opt into the eight-item operator shell. */
bool ui_theme_is_operator_shell(void);

/* Resolve shared tokens through the active runtime palette. */
lv_color_t ui_theme_color(uint32_t operator_rgb,
                          uint32_t foundry_rgb,
                          uint32_t glass_rgb);
int32_t ui_theme_metric(int32_t operator_value,
                        int32_t foundry_value,
                        int32_t glass_value);
void ui_theme_set_accent(ui_accent_id_t accent);
ui_accent_id_t ui_theme_get_accent(void);
void ui_theme_set_density(ui_density_id_t density);
ui_density_id_t ui_theme_get_density(void);
void ui_theme_set_accessibility(ui_accessibility_t options);
ui_accessibility_t ui_theme_get_accessibility(void);
lv_color_t ui_theme_accent_color(uint32_t operator_rgb,
                                 uint32_t foundry_rgb,
                                 uint32_t glass_rgb,
                                 uint8_t tone);
int32_t ui_theme_density_metric(int32_t compact_value,
                                int32_t comfortable_value,
                                int32_t large_value);
const lv_font_t *ui_theme_density_font(const lv_font_t *compact,
                                       const lv_font_t *comfortable,
                                       const lv_font_t *spacious,
                                       const lv_font_t *accessible);
lv_color_t ui_theme_accessible_color(lv_color_t normal,
                                     uint32_t high_contrast_rgb);
lv_opa_t ui_theme_accessible_opacity(lv_opa_t normal);
int32_t ui_theme_accessible_border_width(int32_t normal);
bool ui_theme_motion_enabled(void);

/* ------------------------------------------------------------
 * Color tokens
 * ------------------------------------------------------------ */

/* Application surfaces */
#define UI_BG                   ui_theme_color(0x0B1118, 0x18130F, 0x03040A)
#define UI_BG_DEEP              ui_theme_color(0x06101B, 0x0F0B09, 0x010207)
#define UI_BG_POPUP             ui_theme_color(0x0B1324, 0x261A14, 0x080B16)
#define UI_BG_DANGER_POPUP      ui_theme_color(0x171018, 0x2C1513, 0x1A0815)
#define UI_TOPBAR               ui_theme_color(0x111A24, 0x2A1C14, 0x080C18)
#define UI_NAV                  ui_theme_color(0x101821, 0x211711, 0x060A14)
#define UI_PANEL                ui_theme_color(0x101B2A, 0x2B211B, 0x0A1020)
#define UI_PANEL_ALT            ui_theme_color(0x101A25, 0x35271E, 0x11152A)
#define UI_CARD                 ui_theme_color(0x101B2A, 0x32251E, 0x0C1428)
#define UI_CARD_DARK            ui_theme_color(0x0B1118, 0x15100D, 0x040711)

/* Interactive surfaces */
#define UI_CONTROL              ui_theme_color(0x1A2633, 0x49362B, 0x111D36)
#define UI_CONTROL_ALT          ui_theme_color(0x162235, 0x3B2C24, 0x17142E)
#define UI_CONTROL_CLOSE        ui_theme_color(0x402020, 0x66332B, 0x351026)
#define UI_CONTROL_CANCEL       ui_theme_color(0x39465C, 0x59483B, 0x222840)
#define UI_PROGRESS_TRACK       ui_theme_color(0x182230, 0x3A2B23, 0x0B1020)

/* Borders */
#define UI_BORDER               ui_theme_accessible_color(ui_theme_color(0x25476A, 0x6B4B36, 0x29466A), 0xA8C0D8)
#define UI_BORDER_SOFT          ui_theme_accessible_color(ui_theme_color(0x31445C, 0x594236, 0x1B2A43), 0xB8CEE2)
#define UI_BORDER_CONTROL       ui_theme_accessible_color(ui_theme_color(0x35506F, 0x8A6245, 0x3A67A0), 0xD0E2F2)
#define UI_BORDER_POPUP         ui_theme_color(0x3D6F99, 0xB87545, 0x7440B5)
#define UI_BORDER_BRIGHT        ui_theme_color(0x8FD3FF, 0xF4B860, 0x7FEAFF)
#define UI_WIFI_INACTIVE        ui_theme_color(0x3A4654, 0x705C4D, 0x29334A)

/* Text */
#define UI_TEXT                 ui_theme_color(0xE8F1FF, 0xF4E8DA, 0xEAF3FF)
#define UI_TEXT_BRIGHT          ui_theme_color(0xFFFFFF, 0xFFF9F2, 0xFFFFFF)
#define UI_TEXT_DIM             ui_theme_accessible_color(ui_theme_color(0x8FA7C2, 0xC5A88E, 0x91A6D8), 0xE7F1FA)
#define UI_TEXT_MUTED           ui_theme_accessible_color(ui_theme_color(0xAFC7E8, 0xD7C3AF, 0xB7C9F5), 0xFFFFFF)
#define UI_TEXT_ERROR           ui_theme_color(0xFF6B6B, 0xFF8580, 0xFF79A8)

/* Accent colors */
#define UI_ACCENT               ui_theme_accent_color(0x255A91, 0xC9693B, 0x6537D8, 0)
#define UI_ACCENT_2             ui_theme_accent_color(0x1266C3, 0xE07A3F, 0xB33CFF, 1)
#define UI_ACCENT_BRIGHT        ui_theme_accent_color(0x33C7FF, 0xFFAD66, 0xF45BFF, 2)
#define UI_ACCENT_CYAN          ui_theme_accent_color(0x19C7E8, 0x55C2B8, 0x38E8FF, 1)
#define UI_ACCENT_INFO          ui_theme_accent_color(0x3AA8FF, 0x71B7C5, 0x5C91FF, 1)
#define UI_ACCENT_PURPLE        ui_theme_accent_color(0xB65CFF, 0xC889D8, 0xD86CFF, 2)
#define UI_ACCENT_ORANGE        ui_theme_color(0xFF8C2A, 0xFF934F, 0xFF9A5C)
#define UI_ACCENT_SKY           ui_theme_accent_color(0x00D1FF, 0x77D4D0, 0x48F5FF, 2)

/* Dark Glass optical layers. Other themes map these to neutral tokens. */
#define UI_GLASS_EDGE           ui_theme_color(0x8FD3FF, 0xF4B860, 0xD9FAFF)
#define UI_GLASS_SHEEN          ui_theme_color(0x35506F, 0x8A6245, 0x1D3152)
#define UI_GLASS_AURORA         ui_theme_color(0x255A91, 0xC9693B, 0xD85CFF)

/* Telemetry surfaces and data series */
#define UI_TELEMETRY_ROOT_BG       ui_theme_color(0x040D17, 0x120E0B, 0x030513)
#define UI_TELEMETRY_CHART_BG      ui_theme_color(0x040C15, 0x17110D, 0x030612)
#define UI_TELEMETRY_CHART_CARD_BG ui_theme_color(0x06111D, 0x211813, 0x060B1A)
#define UI_TELEMETRY_PANEL_BG      ui_theme_color(0x081522, 0x291E18, 0x080E20)
#define UI_TELEMETRY_CARD_BG       ui_theme_color(0x091827, 0x30231C, 0x0A1228)
#define UI_TELEMETRY_GRID          ui_theme_color(0x173047, 0x5C4031, 0x1B3152)
#define UI_TELEMETRY_CHAMBER       ui_theme_color(0x35E0D0, 0x61D0BE, 0x3BFFDA)
#define UI_TELEMETRY_BED_TRACE     ui_theme_color(0xFFCF66, 0xF6C453, 0xFFD166)
#define UI_TELEMETRY_HUMIDITY      ui_theme_color(0xA679FF, 0xC994E8, 0xE56BFF)


/* Bed-mesh 3D plot */
#define UI_BED_MESH_BG          ui_theme_color(0x050D16, 0x15100C, 0x02040D)
#define UI_BED_MESH_WIREFRAME   ui_theme_color(0xD2ECFF, 0xFFE1C2, 0xD9FAFF)
#define UI_BED_MESH_LOW         ui_theme_color(0x168AAD, 0x4F86A6, 0x5C91FF)
#define UI_BED_MESH_LEVEL       ui_theme_color(0x30D5C8, 0x55C2B8, 0x38E8FF)
#define UI_BED_MESH_HIGH        ui_theme_color(0xE45B5B, 0xF06A66, 0xFF4FA3)

/* Semantic state colors */
#define UI_OK                   ui_theme_color(0x09743D, 0x2F7D5A, 0x087B6B)
#define UI_OK_BRIGHT            ui_theme_color(0x70E000, 0x6DD39E, 0x35FFC6)
#define UI_WARN                 ui_theme_color(0xFFC857, 0xF2B84B, 0xFFE56B)
#define UI_WARN_DARK            ui_theme_color(0x8A4B00, 0x9B5F20, 0x8B6714)
#define UI_DANGER               ui_theme_color(0xC83232, 0xC94F4F, 0xC32D78)
#define UI_DANGER_DARK          ui_theme_color(0xA83232, 0x913C3C, 0x7D225A)
#define UI_DANGER_BRIGHT        ui_theme_color(0xFF4D4D, 0xF06A66, 0xFF4FA3)
#define UI_CANCEL               UI_CONTROL_CANCEL

/* ------------------------------------------------------------
 * Geometry tokens
 * ------------------------------------------------------------ */

/* Radius */
#define UI_RADIUS_NONE          0
#define UI_RADIUS_BAR           ui_theme_metric(4, 10, 12)
#define UI_RADIUS_CARD          ui_theme_metric(12, 20, 22)
#define UI_RADIUS_BTN           ui_theme_metric(10, 18, 20)
#define UI_RADIUS_ACTION        ui_theme_metric(10, 18, 20)
#define UI_RADIUS_PANEL         ui_theme_metric(10, 20, 24)
#define UI_RADIUS_BANNER        ui_theme_metric(12, 22, 24)
#define UI_RADIUS_PREVIEW       ui_theme_metric(12, 18, 22)
#define UI_RADIUS_POPUP         ui_theme_metric(16, 24, 28)
#define UI_RADIUS_DIALOG        ui_theme_metric(16, 24, 28)
#define UI_RADIUS_SPLASH        ui_theme_metric(22, 30, 34)

/* Border widths */
#define UI_BORDER_NONE          0
#define UI_BORDER_THIN          ui_theme_accessible_border_width(1)
#define UI_BORDER_STRONG        ui_theme_accessible_border_width(2)

/* Padding and spacing */
#define UI_SPACE_XS             4
#define UI_SPACE_SM             8
#define UI_SPACE_MD             10
#define UI_SPACE_LG             12
#define UI_SPACE_XL             16
#define UI_SPACE_2XL            20
#define UI_SPACE_3XL            24

#define UI_PAD_PANEL            ui_theme_density_metric(8, 12, 16)
#define UI_PAD_CARD             ui_theme_density_metric(8, 10, 14)
#define UI_PAD_POPUP            ui_theme_density_metric(12, 16, 20)
#define UI_GAP_ROW              ui_theme_density_metric(6, 8, 12)
#define UI_GAP_CARD             ui_theme_density_metric(8, 12, 16)

/* ------------------------------------------------------------
 * Typography tokens
 * ------------------------------------------------------------ */

#define UI_FONT_CAPTION         ui_theme_density_font(&lv_font_montserrat_14, &lv_font_montserrat_14, &lv_font_montserrat_16, &lv_font_montserrat_18)
#define UI_FONT_BODY            ui_theme_density_font(&lv_font_montserrat_14, &lv_font_montserrat_16, &lv_font_montserrat_18, &lv_font_montserrat_20)
#define UI_FONT_BODY_LARGE      ui_theme_density_font(&lv_font_montserrat_16, &lv_font_montserrat_18, &lv_font_montserrat_20, &lv_font_montserrat_22)
#define UI_FONT_VALUE_SMALL     ui_theme_density_font(&lv_font_montserrat_18, &lv_font_montserrat_20, &lv_font_montserrat_22, &lv_font_montserrat_24)
#define UI_FONT_TITLE           ui_theme_density_font(&lv_font_montserrat_20, &lv_font_montserrat_22, &lv_font_montserrat_24, &lv_font_montserrat_26)
#define UI_FONT_DIALOG_TITLE    ui_theme_density_font(&lv_font_montserrat_22, &lv_font_montserrat_24, &lv_font_montserrat_26, &lv_font_montserrat_28)
#define UI_FONT_POPUP_TITLE     ui_theme_density_font(&lv_font_montserrat_24, &lv_font_montserrat_26, &lv_font_montserrat_28, &lv_font_montserrat_30)
#define UI_FONT_VALUE           ui_theme_density_font(&lv_font_montserrat_26, &lv_font_montserrat_28, &lv_font_montserrat_30, &lv_font_montserrat_32)
#define UI_FONT_HEADING         ui_theme_density_font(&lv_font_montserrat_28, &lv_font_montserrat_30, &lv_font_montserrat_32, &lv_font_montserrat_32)
#define UI_FONT_SPLASH_TITLE    (&lv_font_montserrat_32)
#define UI_FONT_PERCENT         (&lv_font_montserrat_48)

/* ------------------------------------------------------------
 * Semantic status categories
 * ------------------------------------------------------------ */

typedef enum {
    UI_STATUS_NEUTRAL = 0,
    UI_STATUS_INFO,
    UI_STATUS_OK,
    UI_STATUS_WARNING,
    UI_STATUS_DANGER,
    UI_STATUS_ACTIVE
} ui_status_kind_t;

/* Specialized structural surfaces shared by application modules. */
typedef enum {
    UI_SURFACE_TRANSPARENT = 0,
    UI_SURFACE_SHELL_TOPBAR,
    UI_SURFACE_SHELL_NAV,
    UI_SURFACE_PAGE_DEEP,
    UI_SURFACE_SECTION,
    UI_SURFACE_LIST_ROW,
    UI_SURFACE_PREVIEW_WELL,
    UI_SURFACE_STATUS_PILL,
    UI_SURFACE_POPUP_LIST,
    UI_SURFACE_TEXT_INPUT,
    UI_SURFACE_KEYBOARD,
    UI_SURFACE_TELEMETRY_ROOT,
    UI_SURFACE_TELEMETRY_CARD,
    UI_SURFACE_TELEMETRY_PANEL,
    UI_SURFACE_TELEMETRY_CHART,
    UI_SURFACE_DIVIDER,
    UI_SURFACE_INDICATOR
} ui_surface_role_t;

/* ------------------------------------------------------------
 * Runtime theme selection
 * ------------------------------------------------------------ */

/* Existing objects must be rebuilt after a runtime theme change. */

/* ------------------------------------------------------------
 * Container styles
 * ------------------------------------------------------------ */

void ui_apply_root_style(lv_obj_t *obj);
void ui_apply_panel_style(lv_obj_t *obj);
void ui_apply_card_style(lv_obj_t *obj);
void ui_apply_banner_style(lv_obj_t *obj);
void ui_apply_banner_status_style(
    lv_obj_t *obj,
    ui_status_kind_t kind);
void ui_apply_preview_style(lv_obj_t *obj);
void ui_apply_info_box_style(lv_obj_t *obj);
void ui_apply_popup_style(lv_obj_t *obj);
void ui_apply_dialog_style(lv_obj_t *obj);
void ui_apply_surface_role(lv_obj_t *obj, ui_surface_role_t role);
void ui_apply_custom_label_style(lv_obj_t *obj,
                                 const lv_font_t *font,
                                 lv_color_t color);
void ui_apply_slider_style(lv_obj_t *obj);
void ui_apply_progress_bar_style(lv_obj_t *obj);
void ui_apply_telemetry_plot_style(lv_obj_t *obj);
void ui_apply_trace_marker_style(lv_obj_t *obj, lv_color_t color);
void ui_apply_reference_line_style(lv_obj_t *obj, lv_color_t color);

/* ------------------------------------------------------------
 * Button styles
 * ------------------------------------------------------------ */

void ui_apply_button_style(lv_obj_t *obj);
void ui_apply_button_dark_style(lv_obj_t *obj);
void ui_apply_button_success_style(lv_obj_t *obj);
void ui_apply_button_warning_style(lv_obj_t *obj);
void ui_apply_button_danger_style(lv_obj_t *obj);
void ui_apply_button_cancel_style(lv_obj_t *obj);
void ui_apply_button_close_style(lv_obj_t *obj);
void ui_apply_button_outlined_style(lv_obj_t *obj);
void ui_apply_button_status_style(
    lv_obj_t *obj,
    ui_status_kind_t kind);

/* ------------------------------------------------------------
 * Label color styles
 * ------------------------------------------------------------ */

void ui_apply_label_primary(lv_obj_t *obj);
void ui_apply_label_bright(lv_obj_t *obj);
void ui_apply_label_dim(lv_obj_t *obj);
void ui_apply_label_muted(lv_obj_t *obj);
void ui_apply_label_success(lv_obj_t *obj);
void ui_apply_label_warning(lv_obj_t *obj);
void ui_apply_label_error(lv_obj_t *obj);

/* ------------------------------------------------------------
 * Typography styles
 * ------------------------------------------------------------ */

void ui_apply_text_caption(lv_obj_t *obj);
void ui_apply_text_body(lv_obj_t *obj);
void ui_apply_text_body_large(lv_obj_t *obj);
void ui_apply_text_button(lv_obj_t *obj);
void ui_apply_text_value_small(lv_obj_t *obj);
void ui_apply_text_title(lv_obj_t *obj);
void ui_apply_text_dialog_title(lv_obj_t *obj);
void ui_apply_text_popup_title(lv_obj_t *obj);
void ui_apply_text_value(lv_obj_t *obj);
void ui_apply_text_heading(lv_obj_t *obj);
void ui_apply_text_percent(lv_obj_t *obj);

/* ------------------------------------------------------------
 * Status helpers
 * ------------------------------------------------------------ */

lv_color_t ui_status_color(ui_status_kind_t kind);
