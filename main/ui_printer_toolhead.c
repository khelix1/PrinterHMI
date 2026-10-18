#include "ui_widgets.h"
#include "ui_printer_toolhead.h"
#include "ui_text.h"

#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "printer_connection_policy.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_form_dialog.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_font_fallback.h"

static lv_obj_t **s_xy_step1_btn = NULL;
static lv_obj_t **s_xy_step10_btn = NULL;
static lv_obj_t **s_xy_step50_btn = NULL;
static double *s_xy_step = NULL;

static lv_obj_t *s_z_step001_btn = NULL;
static lv_obj_t *s_z_step005_btn = NULL;
static lv_obj_t *s_z_step010_btn = NULL;
static lv_obj_t *s_z_step100_btn = NULL;
static lv_obj_t *s_z_step050_btn, *s_z_step500_btn, *s_z_step1000_btn, *s_z_step2500_btn;
static double s_z_step = 0.10;
static const char *s_extrusion_values[] = {"1", "5", "10", "25", "50"};
static lv_obj_t *s_extrusion_buttons[5];
static double s_extrusion_step = 10.0;

static lv_obj_t *s_tab_panels[3], *s_tab_buttons[3];
static unsigned s_active_tab;

static lv_obj_t *s_position_label = NULL;
static lv_obj_t *s_homed_label = NULL;
static lv_obj_t *s_offset_label = NULL;
static lv_timer_t *s_refresh_timer = NULL;

static ui_printer_toolhead_send_gcode_cb_t s_send_gcode = NULL;
static bool (*s_checked_send)(const char *command);
static lv_obj_t *s_toolhead_popup;
static uint32_t s_toolhead_owner, s_toolhead_epoch;
static bool s_toolhead_dispatching, s_toolhead_cooldown;
static uint32_t s_toolhead_sent_at;
static uint32_t s_toolhead_sent_owner;
static char s_toolhead_feedback[40];
static uint32_t s_toolhead_feedback_at;

void ui_printer_toolhead_set_sender(bool (*send)(const char *command))
{
    s_checked_send = send;
}

static void toolhead_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != s_toolhead_popup) return;
    s_toolhead_popup = NULL;
    ++s_toolhead_epoch;
    if (s_refresh_timer) lv_timer_delete(s_refresh_timer);
    s_refresh_timer = NULL;
    s_position_label = s_homed_label = s_offset_label = NULL;
    s_z_step001_btn = s_z_step005_btn = s_z_step010_btn = s_z_step100_btn = NULL;
    s_z_step050_btn = s_z_step500_btn = s_z_step1000_btn = s_z_step2500_btn = NULL;
    memset(s_extrusion_buttons, 0, sizeof(s_extrusion_buttons));
    memset(s_tab_panels, 0, sizeof(s_tab_panels));
    memset(s_tab_buttons, 0, sizeof(s_tab_buttons));
    s_active_tab = 0;
    if (s_xy_step1_btn) *s_xy_step1_btn = NULL;
    if (s_xy_step10_btn) *s_xy_step10_btn = NULL;
    if (s_xy_step50_btn) *s_xy_step50_btn = NULL;
    s_xy_step1_btn = s_xy_step10_btn = s_xy_step50_btn = NULL;
    s_xy_step = NULL;
    s_send_gcode = NULL;
    s_toolhead_feedback[0] = 0;
}

void ui_printer_toolhead_close(void)
{
    if (s_toolhead_popup) lv_obj_delete(s_toolhead_popup);
}

static void toolhead_send(const char *command)
{
    if (!s_toolhead_popup || s_toolhead_dispatching ||
        (s_toolhead_cooldown && s_toolhead_sent_owner == moonraker_config_generation() &&
         lv_tick_elaps(s_toolhead_sent_at) < 350)) return;
    if (s_toolhead_owner != moonraker_config_generation()) {
        ui_printer_toolhead_close();
        return;
    }
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    const char *error = printer_connection_reason(&state);
    if (error[0]) {
        snprintf(s_toolhead_feedback, sizeof(s_toolhead_feedback), "Offline");
        s_toolhead_feedback_at = lv_tick_get();
        lv_label_set_text(s_homed_label, "Offline");
        return;
    }
    lv_obj_t *popup = s_toolhead_popup;
    uint32_t epoch = s_toolhead_epoch, owner = s_toolhead_owner;
    s_toolhead_dispatching = true;
    bool accepted = false;
    if (s_checked_send) accepted = s_checked_send(command);
    else if (s_send_gcode) { s_send_gcode(command); accepted = true; }
    s_toolhead_dispatching = false;
    if (accepted && owner == moonraker_config_generation()) {
        s_toolhead_cooldown = true;
        s_toolhead_sent_owner = owner;
        s_toolhead_sent_at = lv_tick_get();
    }
    if (epoch != s_toolhead_epoch || popup != s_toolhead_popup ||
        owner != moonraker_config_generation()) return;
    s_toolhead_cooldown = accepted;
    snprintf(s_toolhead_feedback, sizeof(s_toolhead_feedback), "%s",
        accepted ? "Sent" : "Not sent");
    s_toolhead_feedback_at = lv_tick_get();
    lv_label_set_text(s_homed_label, accepted ? "Sent" : "Not sent");
}

void ui_printer_toolhead_init(void)
{
}

static lv_obj_t *toolhead_button(lv_obj_t *parent, ui_button_kind_t kind,
    const char *text, lv_event_cb_t callback, const char *value,
    unsigned column, unsigned row)
{
    lv_obj_t *button = ui_button_create(parent, kind, text);
    if (!button) return NULL;
    lv_obj_set_height(button, 52);
    lv_obj_set_grid_cell(button, LV_GRID_ALIGN_STRETCH, column, 1,
        LV_GRID_ALIGN_START, row, 1);
    lv_obj_add_event_cb(button, ui_form_action_fit, LV_EVENT_SIZE_CHANGED, NULL);
    if (callback) lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, (void *)value);
    return button;
}

static const int32_t toolhead_one[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t toolhead_two[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t toolhead_three[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t toolhead_four[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t toolhead_five[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t toolhead_readings[] = {LV_GRID_FR(3), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t toolhead_rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

static lv_obj_t *toolhead_grid(lv_obj_t *parent, const int32_t *columns)
{
    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_grid_dsc_array(grid, columns, toolhead_rows);
    lv_obj_set_style_pad_column(grid, 6, 0);
    lv_obj_set_style_pad_row(grid, 6, 0);
    return grid;
}

static lv_obj_t *toolhead_section(lv_obj_t *parent)
{
    lv_obj_t *section = lv_obj_create(parent);
    ui_apply_surface_role(section, UI_SURFACE_SECTION);
    if (ui_theme_is_studio()) {
        ui_apply_surface_role(section, UI_SURFACE_TRANSPARENT);
        ui_apply_divider_border(section, LV_BORDER_SIDE_BOTTOM);
    }
    lv_obj_set_size(section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(section, 8, 0);
    lv_obj_set_style_pad_row(section, 6, 0);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    return section;
}

static void toolhead_panels_resize(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) != lv_event_get_current_target_obj(event)) return;
    lv_obj_t *grid = lv_event_get_current_target_obj(event);
    bool wide = lv_obj_get_content_width(grid) >= 620;
    lv_obj_set_grid_dsc_array(grid, wide ? toolhead_two : toolhead_one, toolhead_rows);
    for (unsigned i = 0; i < lv_obj_get_child_count(grid); ++i)
        lv_obj_set_grid_cell(lv_obj_get_child(grid, i), LV_GRID_ALIGN_STRETCH,
            wide ? i : 0, 1, LV_GRID_ALIGN_START, wide ? 0 : i, 1);
}

static void toolhead_caption(lv_obj_t *parent, const char *text)
{
    ui_form_text(parent, ui_text(text), UI_FONT_CAPTION, UI_TEXT_DIM);
}

static void apply_selected(lv_obj_t *button, bool selected)
{
    if (!button) return;
    ui_button_set_filter_active(button, selected);
}

static void refresh_step_highlights(void)
{
    double xy = s_xy_step ? *s_xy_step : 10.0;

    apply_selected(
        s_xy_step1_btn ? *s_xy_step1_btn : NULL,
        fabs(xy - 1.0) < 0.0001);
    apply_selected(
        s_xy_step10_btn ? *s_xy_step10_btn : NULL,
        fabs(xy - 10.0) < 0.0001);
    apply_selected(
        s_xy_step50_btn ? *s_xy_step50_btn : NULL,
        fabs(xy - 50.0) < 0.0001);

    apply_selected(s_z_step001_btn, fabs(s_z_step - 0.01) < 0.0001);
    apply_selected(s_z_step005_btn, fabs(s_z_step - 0.05) < 0.0001);
    apply_selected(s_z_step010_btn, fabs(s_z_step - 0.10) < 0.0001);
    apply_selected(s_z_step100_btn, fabs(s_z_step - 1.00) < 0.0001);
    apply_selected(s_z_step050_btn, fabs(s_z_step - 0.50) < 0.0001);
    apply_selected(s_z_step500_btn, fabs(s_z_step - 5.00) < 0.0001);
    apply_selected(s_z_step1000_btn, fabs(s_z_step - 10.00) < 0.0001);
    apply_selected(s_z_step2500_btn, fabs(s_z_step - 25.00) < 0.0001);
    for (unsigned i = 0; i < 5; ++i)
        apply_selected(s_extrusion_buttons[i], fabs(s_extrusion_step - atof(s_extrusion_values[i])) < 0.0001);
}

static void toolhead_tab_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_toolhead_popup) return;
    if (s_toolhead_owner != moonraker_config_generation()) { ui_printer_toolhead_close(); return; }
    unsigned tab = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    if (tab >= 3) return;
    s_active_tab = tab;
    for (unsigned i = 0; i < 3; ++i) {
        if (i == tab) lv_obj_remove_flag(s_tab_panels[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_tab_panels[i], LV_OBJ_FLAG_HIDDEN);
        apply_selected(s_tab_buttons[i], i == tab);
    }
    lv_obj_scroll_to_y(ui_form_body(s_toolhead_popup), 0, LV_ANIM_OFF);
}

bool ui_printer_toolhead_format_jog_command(const char *axis,
                                          double jog_step,
                                          char *cmd,
                                          size_t cmd_size)
{
    if (!axis || !axis[0] || !cmd || cmd_size == 0) return false;

    if (strcmp(axis, "HOME") == 0) {
        snprintf(cmd, cmd_size, "G28");
        return true;
    }

    if (jog_step <= 0.0) return false;

    double distance = jog_step;
    if (strcmp(axis, "X-") == 0 ||
        strcmp(axis, "Y-") == 0 ||
        strcmp(axis, "Z-") == 0) {
        distance = -distance;
    }

    if (axis[0] == 'Z') {
        snprintf(cmd, cmd_size, "G91\nG1 Z%.3f F600\nG90", distance);
    } else if (axis[0] == 'X' || axis[0] == 'Y') {
        snprintf(cmd, cmd_size, "G91\nG1 %c%.2f F6000\nG90", axis[0], distance);
    } else {
        return false;
    }

    return true;
}

static bool format_extrusion_distance(const char *dir, double distance,
                                      char *cmd, size_t cmd_size)
{
    if (!dir || !cmd || cmd_size == 0 || !isfinite(distance) || distance <= 0.0) return false;
    if (strcmp(dir, "RETRACT") == 0) distance = -distance;
    else if (strcmp(dir, "EXTRUDE") != 0) return false;
    snprintf(cmd, cmd_size, "M83\nG1 E%.0f F300", distance);
    return true;
}

bool ui_printer_toolhead_format_extrude_command(const char *dir,
                                              char *cmd, size_t cmd_size)
{
    return format_extrusion_distance(dir, 10.0, cmd, cmd_size);
}

bool ui_printer_toolhead_format_z_offset_command(const char *adjustment,
                                               char *cmd,
                                               size_t cmd_size)
{
    if (!adjustment || !cmd || cmd_size == 0) return false;

    if (strcmp(adjustment, "RESET") == 0) {
        snprintf(cmd, cmd_size, "SET_GCODE_OFFSET Z=0 MOVE=1");
        return true;
    }

    char *end = NULL;
    double delta = strtod(adjustment, &end);
    if (!end || end == adjustment || *end != '\0') return false;

    snprintf(
        cmd,
        cmd_size,
        "SET_GCODE_OFFSET Z_ADJUST=%+.3f MOVE=1",
        delta);
    return true;
}

static void refresh_live_labels(void)
{
    if (!s_toolhead_popup) return;
    if (s_toolhead_owner != moonraker_config_generation()) {
        ui_printer_toolhead_close();
        return;
    }
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (s_position_label) {
        if (state.toolhead_position_valid) {
            char position[96];
            snprintf(position, sizeof(position), "X %.2f | Y %.2f | Z %.3f mm",
                state.toolhead_x, state.toolhead_y, state.toolhead_z);
            lv_label_set_text(s_position_label, position);
        } else {
            lv_label_set_text(
                s_position_label,
                ui_text("X --.-- | Y --.-- | Z --.--- mm"));
        }
    }

    if (s_homed_label && s_toolhead_feedback[0] &&
        lv_tick_elaps(s_toolhead_feedback_at) < 3000) {
        lv_label_set_text(s_homed_label, s_toolhead_feedback);
    } else if (s_homed_label) {
        char axes[8] = "";
        size_t out = 0;

        for (const char *p = state.homed_axes;
             *p && out + 1 < sizeof(axes);
             ++p) {
            char c = *p;
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            axes[out++] = c;
        }
        axes[out] = '\0';

        lv_label_set_text_fmt(
            s_homed_label,
            "Homed: %s",
            axes[0] ? axes : "--");
    }

    if (s_offset_label) {
        char offset[32];
        snprintf(offset, sizeof(offset), "%+.3f mm", state.z_offset);
        lv_label_set_text(s_offset_label, offset);
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_live_labels();
}

static void xy_step_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_xy_step ||
        s_toolhead_owner != moonraker_config_generation()) return;
    const char *value = lv_event_get_user_data(event);
    if (!value) return;

    *s_xy_step = atof(value);
    refresh_step_highlights();
}

static void z_step_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_toolhead_popup ||
        s_toolhead_owner != moonraker_config_generation()) return;
    const char *value = lv_event_get_user_data(event);
    if (!value) return;

    s_z_step = atof(value);
    refresh_step_highlights();
}

static void jog_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_toolhead_popup) return;

    const char *axis = lv_event_get_user_data(event);
    if (!axis) return;

    double step =
        axis[0] == 'Z'
            ? s_z_step
            : (s_xy_step ? *s_xy_step : 10.0);

    char command[96];
    if (ui_printer_toolhead_format_jog_command(
            axis, step, command, sizeof(command))) {
        toolhead_send(command);
    }
}

static void extrusion_step_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_toolhead_popup) return;
    if (s_toolhead_owner != moonraker_config_generation()) { ui_printer_toolhead_close(); return; }
    const char *value = lv_event_get_user_data(event);
    if (!value) return;
    s_extrusion_step = atof(value);
    refresh_step_highlights();
}

static void extrude_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_toolhead_popup) return;

    const char *direction = lv_event_get_user_data(event);
    char command[96];

    if (format_extrusion_distance(
            direction, s_extrusion_step, command, sizeof(command))) {
        toolhead_send(command);
    }
}

static void z_offset_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_toolhead_popup) return;

    const char *adjustment = lv_event_get_user_data(event);
    char command[96];

    if (ui_printer_toolhead_format_z_offset_command(
            adjustment, command, sizeof(command))) {
        toolhead_send(command);
    }
}

static void close_cb(lv_event_t *event)
{
    (void)event;
    ui_printer_toolhead_close();
}

void ui_printer_toolhead_show(lv_obj_t **step1_btn,
                            lv_obj_t **step10_btn,
                            lv_obj_t **step50_btn,
                            double *jog_step,
                            ui_printer_toolhead_send_gcode_cb_t send_gcode_cb)
{
    if (s_toolhead_dispatching) return;
    if (s_toolhead_popup && s_toolhead_owner != moonraker_config_generation())
        ui_printer_toolhead_close();
    if (s_toolhead_popup) { lv_obj_move_foreground(s_toolhead_popup); return; }
    lv_obj_t *popup =
        ui_form_dialog(ui_text("Toolhead control"), 960, 568, UI_POPUP_STANDARD);
    if (!popup) return;
    ui_form_divide_title(popup);
    s_toolhead_popup = popup;
    s_toolhead_owner = moonraker_config_generation();
    ++s_toolhead_epoch;
    lv_obj_add_event_cb(popup, toolhead_deleted, LV_EVENT_DELETE, NULL);
    s_xy_step1_btn = step1_btn;
    s_xy_step10_btn = step10_btn;
    s_xy_step50_btn = step50_btn;
    s_xy_step = jog_step;
    s_send_gcode = send_gcode_cb;

    lv_obj_set_style_pad_all(popup, 12, 0);
    lv_obj_set_style_pad_row(popup, 8, 0);
    lv_obj_t *body = ui_form_body(popup);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_row(body, 6, 0);
    lv_obj_t *readings = toolhead_grid(body, toolhead_readings);
    s_position_label = ui_form_text(readings, "", UI_FONT_VALUE_SMALL, UI_TEXT_BRIGHT);
    s_homed_label = ui_form_text(readings, "", UI_FONT_CAPTION, UI_ACCENT_CYAN);
    lv_obj_set_grid_cell(s_position_label, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_grid_cell(s_homed_label, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_t *tabs = toolhead_grid(body, toolhead_three);
    static const char *tab_names[] = {"Move", "Filament", "Z offset"};
    for (unsigned i = 0; i < 3; ++i) {
        s_tab_buttons[i] = toolhead_button(tabs, UI_BUTTON_OUTLINED, ui_text(tab_names[i]), NULL, NULL, i, 0);
        lv_obj_add_event_cb(s_tab_buttons[i], toolhead_tab_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
    s_active_tab = 0;
    lv_obj_t *panels = toolhead_grid(body, toolhead_two);
    s_tab_panels[0] = panels;
    lv_obj_add_event_cb(panels, toolhead_panels_resize, LV_EVENT_SIZE_CHANGED, NULL);
    lv_obj_t *xy = toolhead_section(panels);
    lv_obj_t *z = toolhead_section(panels);
    lv_obj_set_grid_cell(xy, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_grid_cell(z, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 0, 1);

    toolhead_caption(xy, "XY jog");
    lv_obj_t *jog = toolhead_grid(xy, toolhead_three);
    toolhead_button(jog, UI_BUTTON_OUTLINED, LV_SYMBOL_UP " Y+", jog_cb, "Y+", 1, 0);
    toolhead_button(jog, UI_BUTTON_OUTLINED, LV_SYMBOL_LEFT " X-", jog_cb, "X-", 0, 1);
    toolhead_button(jog, UI_BUTTON_SUCCESS, "Home", jog_cb, "HOME", 1, 1);
    toolhead_button(jog, UI_BUTTON_OUTLINED, "X+ " LV_SYMBOL_RIGHT, jog_cb, "X+", 2, 1);
    toolhead_button(jog, UI_BUTTON_OUTLINED, LV_SYMBOL_DOWN " Y-", jog_cb, "Y-", 1, 2);
    toolhead_caption(xy, "XY step (mm)");
    lv_obj_t *steps = toolhead_grid(xy, toolhead_three);
    if (step1_btn) *step1_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "1 mm", xy_step_cb, "1", 0, 0);
    if (step10_btn) *step10_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "10 mm", xy_step_cb, "10", 1, 0);
    if (step50_btn) *step50_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "50 mm", xy_step_cb, "50", 2, 0);
    toolhead_caption(z, "Z jog");
    jog = toolhead_grid(z, toolhead_two);
    toolhead_button(jog, UI_BUTTON_OUTLINED, LV_SYMBOL_UP " Z+", jog_cb, "Z+", 0, 0);
    toolhead_button(jog, UI_BUTTON_OUTLINED, LV_SYMBOL_DOWN " Z-", jog_cb, "Z-", 1, 0);
    toolhead_caption(z, "Z step (mm)");
    steps = toolhead_grid(z, toolhead_four);
    s_z_step001_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "0.01", z_step_cb, "0.01", 0, 0);
    s_z_step005_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "0.05", z_step_cb, "0.05", 1, 0);
    s_z_step010_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "0.10", z_step_cb, "0.10", 2, 0);
    s_z_step050_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "0.50", z_step_cb, "0.50", 3, 0);
    s_z_step100_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "1.00", z_step_cb, "1.00", 0, 1);
    s_z_step500_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "5.00", z_step_cb, "5.00", 1, 1);
    s_z_step1000_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "10.00", z_step_cb, "10.00", 2, 1);
    s_z_step2500_btn = toolhead_button(steps, UI_BUTTON_OUTLINED, "25.00", z_step_cb, "25.00", 3, 1);
    lv_obj_t *filament_panel = toolhead_section(body);
    s_tab_panels[1] = filament_panel;
    lv_obj_add_flag(filament_panel, LV_OBJ_FLAG_HIDDEN);
    toolhead_caption(filament_panel, "Extrusion length (mm)");
    steps = toolhead_grid(filament_panel, toolhead_five);
    for (unsigned i = 0; i < 5; ++i)
        s_extrusion_buttons[i] = toolhead_button(steps, UI_BUTTON_OUTLINED,
            s_extrusion_values[i], extrusion_step_cb, s_extrusion_values[i], i, 0);
    toolhead_caption(filament_panel, "Extrusion direction");
    lv_obj_t *filament = toolhead_grid(filament_panel, toolhead_two);
    toolhead_button(filament, UI_BUTTON_OUTLINED, "Extrude", extrude_cb, "EXTRUDE", 0, 0);
    toolhead_button(filament, UI_BUTTON_OUTLINED, "Retract", extrude_cb, "RETRACT", 1, 0);

    lv_obj_t *offset_panel = toolhead_section(body);
    s_tab_panels[2] = offset_panel;
    lv_obj_add_flag(offset_panel, LV_OBJ_FLAG_HIDDEN);
    toolhead_caption(offset_panel, "Runtime Z offset (mm)");
    s_offset_label = ui_form_text(offset_panel, "", UI_FONT_VALUE_SMALL, UI_TEXT_BRIGHT);
    lv_obj_set_style_text_align(s_offset_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *offset = toolhead_grid(offset_panel, toolhead_five);
    static const char *negative[] = {"-0.005", "-0.01", "-0.025", "-0.05", "-0.10"};
    static const char *positive[] = {"+0.005", "+0.01", "+0.025", "+0.05", "+0.10"};
    for (unsigned i = 0; i < 5; ++i) {
        toolhead_button(offset, UI_BUTTON_OUTLINED, negative[i], z_offset_cb, negative[i], i, 0);
        toolhead_button(offset, UI_BUTTON_OUTLINED, positive[i], z_offset_cb, positive[i], i, 1);
    }
    offset = toolhead_grid(offset_panel, toolhead_one);
    toolhead_button(offset, UI_BUTTON_SECONDARY, "Reset offset", z_offset_cb, "RESET", 0, 0);

    apply_selected(s_tab_buttons[0], true);
    refresh_step_highlights();
    refresh_live_labels();
    s_refresh_timer = lv_timer_create(refresh_timer_cb, 500, NULL);

    ui_form_action(popup, UI_BUTTON_OUTLINED, LV_SYMBOL_CLOSE " Close", close_cb, NULL);
    ui_form_finish(popup);
}
