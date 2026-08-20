#include "ui_printer_toolhead.h"
#include "ui_text.h"

#include "moonraker.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_theme.h"

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
static double s_z_step = 0.10;

static lv_obj_t *s_position_label = NULL;
static lv_obj_t *s_homed_label = NULL;
static lv_obj_t *s_offset_label = NULL;
static lv_timer_t *s_refresh_timer = NULL;

static ui_printer_toolhead_send_gcode_cb_t s_send_gcode = NULL;

void ui_printer_toolhead_init(void)
{
}

static lv_obj_t *make_button(lv_obj_t *parent,
                             ui_button_kind_t kind,
                             const char *text,
                             int x,
                             int y,
                             int w,
                             int h,
                             lv_event_cb_t cb,
                             const char *user)
{
    lv_obj_t *button = ui_button_create(parent, kind, text);
    if (!button) return NULL;

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);

    if (cb) {
        lv_obj_add_event_cb(
            button,
            cb,
            LV_EVENT_CLICKED,
            (void *)user);
    }

    return button;
}

static void apply_selected(lv_obj_t *button, bool selected)
{
    if (!button) return;
    ui_button_apply_kind(
        button,
        selected ? UI_BUTTON_SUCCESS : UI_BUTTON_OUTLINED);
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

bool ui_printer_toolhead_format_extrude_command(const char *dir,
                                              char *cmd,
                                              size_t cmd_size)
{
    if (!dir || !cmd || cmd_size == 0) return false;

    if (strcmp(dir, "EXTRUDE") == 0) {
        snprintf(cmd, cmd_size, "M83\nG1 E10 F300");
        return true;
    }

    if (strcmp(dir, "RETRACT") == 0) {
        snprintf(cmd, cmd_size, "M83\nG1 E-10 F300");
        return true;
    }

    return false;
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
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (s_position_label) {
        if (state.toolhead_position_valid) {
            lv_label_set_text_fmt(
                s_position_label,
                "X  %.2f      Y  %.2f      Z  %.3f mm",
                state.toolhead_x,
                state.toolhead_y,
                state.toolhead_z);
        } else {
            lv_label_set_text(
                s_position_label,
                ui_text("X  --.--      Y  --.--      Z  --.--- mm"));
        }
    }

    if (s_homed_label) {
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
            "HOMED  %s",
            axes[0] ? axes : "--");
    }

    if (s_offset_label) {
        lv_label_set_text_fmt(
            s_offset_label,
            "%+.3f mm",
            state.z_offset);
    }
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_live_labels();
}

static void xy_step_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_xy_step) return;
    const char *value = lv_event_get_user_data(event);
    if (!value) return;

    *s_xy_step = atof(value);
    refresh_step_highlights();
}

static void z_step_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    const char *value = lv_event_get_user_data(event);
    if (!value) return;

    s_z_step = atof(value);
    refresh_step_highlights();
}

static void jog_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_send_gcode) return;

    const char *axis = lv_event_get_user_data(event);
    if (!axis) return;

    double step =
        axis[0] == 'Z'
            ? s_z_step
            : (s_xy_step ? *s_xy_step : 10.0);

    char command[96];
    if (ui_printer_toolhead_format_jog_command(
            axis, step, command, sizeof(command))) {
        s_send_gcode(command);
    }
}

static void extrude_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_send_gcode) return;

    const char *direction = lv_event_get_user_data(event);
    char command[96];

    if (ui_printer_toolhead_format_extrude_command(
            direction, command, sizeof(command))) {
        s_send_gcode(command);
    }
}

static void z_offset_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_send_gcode) return;

    const char *adjustment = lv_event_get_user_data(event);
    char command[96];

    if (ui_printer_toolhead_format_z_offset_command(
            adjustment, command, sizeof(command))) {
        s_send_gcode(command);
    }
}

static void close_cb(lv_event_t *event)
{
    lv_obj_t *popup = lv_event_get_user_data(event);

    if (s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }

    s_position_label = NULL;
    s_homed_label = NULL;
    s_offset_label = NULL;
    s_z_step001_btn = NULL;
    s_z_step005_btn = NULL;
    s_z_step010_btn = NULL;
    s_z_step100_btn = NULL;

    if (s_xy_step1_btn) *s_xy_step1_btn = NULL;
    if (s_xy_step10_btn) *s_xy_step10_btn = NULL;
    if (s_xy_step50_btn) *s_xy_step50_btn = NULL;

    if (popup) lv_obj_delete(popup);
}

void ui_printer_toolhead_show(lv_obj_t **step1_btn,
                            lv_obj_t **step10_btn,
                            lv_obj_t **step50_btn,
                            double *jog_step,
                            ui_printer_toolhead_send_gcode_cb_t send_gcode_cb)
{
    s_xy_step1_btn = step1_btn;
    s_xy_step10_btn = step10_btn;
    s_xy_step50_btn = step50_btn;
    s_xy_step = jog_step;
    s_send_gcode = send_gcode_cb;

    lv_obj_t *popup =
        ui_popup_create(lv_layer_top(), 760, 540, UI_POPUP_STANDARD);
    if (!popup) return;

    ui_popup_add_title(popup, ui_text("TOOLHEAD CONTROL"), false, 18);
    ui_popup_add_header_divider(popup, 58);

    s_position_label = lv_label_create(popup);
    lv_obj_set_pos(s_position_label, 38, 72);
    lv_obj_set_width(s_position_label, 500);
    lv_obj_set_style_text_font(s_position_label, ui_font_with_fallback(UI_FONT_VALUE_SMALL), 0);
    lv_obj_set_style_text_color(s_position_label, UI_TEXT_BRIGHT, 0);

    s_homed_label = lv_label_create(popup);
    lv_obj_set_pos(s_homed_label, 590, 78);
    lv_obj_set_width(s_homed_label, 130);
    lv_obj_set_style_text_align(s_homed_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(s_homed_label, ui_font_with_fallback(UI_FONT_BODY), 0);
    lv_obj_set_style_text_color(s_homed_label, UI_ACCENT_CYAN, 0);

    ui_popup_add_caption(popup, ui_text("XY JOG"), 40, 118, 340);
    make_button(popup, UI_BUTTON_OUTLINED, LV_SYMBOL_UP "  Y+", 155, 140, 110, 52, jog_cb, "Y+");
    make_button(popup, UI_BUTTON_OUTLINED, LV_SYMBOL_LEFT "  X-", 40, 198, 110, 52, jog_cb, "X-");
    make_button(popup, UI_BUTTON_SUCCESS, LV_SYMBOL_HOME "  HOME", 155, 198, 110, 52, jog_cb, "HOME");
    make_button(popup, UI_BUTTON_OUTLINED, "X+  " LV_SYMBOL_RIGHT, 270, 198, 110, 52, jog_cb, "X+");
    make_button(popup, UI_BUTTON_OUTLINED, LV_SYMBOL_DOWN "  Y-", 155, 256, 110, 52, jog_cb, "Y-");

    ui_popup_add_caption(popup, ui_text("XY STEP"), 40, 318, 340);
    if (step1_btn) *step1_btn = make_button(popup, UI_BUTTON_OUTLINED, "1 mm", 40, 340, 100, 46, xy_step_cb, "1");
    if (step10_btn) *step10_btn = make_button(popup, UI_BUTTON_OUTLINED, "10 mm", 155, 340, 100, 46, xy_step_cb, "10");
    if (step50_btn) *step50_btn = make_button(popup, UI_BUTTON_OUTLINED, "50 mm", 270, 340, 100, 46, xy_step_cb, "50");

    ui_popup_add_caption(popup, ui_text("Z JOG"), 420, 118, 300);
    make_button(popup, UI_BUTTON_OUTLINED, LV_SYMBOL_UP "  Z+", 420, 140, 140, 52, jog_cb, "Z+");
    make_button(popup, UI_BUTTON_OUTLINED, LV_SYMBOL_DOWN "  Z-", 580, 140, 140, 52, jog_cb, "Z-");

    ui_popup_add_caption(popup, ui_text("Z STEP"), 420, 208, 300);
    s_z_step001_btn = make_button(popup, UI_BUTTON_OUTLINED, "0.01", 420, 230, 68, 44, z_step_cb, "0.01");
    s_z_step005_btn = make_button(popup, UI_BUTTON_OUTLINED, "0.05", 496, 230, 68, 44, z_step_cb, "0.05");
    s_z_step010_btn = make_button(popup, UI_BUTTON_OUTLINED, "0.10", 572, 230, 68, 44, z_step_cb, "0.10");
    s_z_step100_btn = make_button(popup, UI_BUTTON_OUTLINED, "1.00", 648, 230, 68, 44, z_step_cb, "1.00");

    ui_popup_add_caption(popup, ui_text("RUNTIME Z OFFSET"), 420, 292, 300);

    s_offset_label = lv_label_create(popup);
    lv_obj_set_pos(s_offset_label, 575, 290);
    lv_obj_set_width(s_offset_label, 140);
    lv_obj_set_style_text_align(s_offset_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_font(s_offset_label, ui_font_with_fallback(UI_FONT_VALUE_SMALL), 0);
    lv_obj_set_style_text_color(s_offset_label, UI_TEXT_BRIGHT, 0);

    make_button(popup, UI_BUTTON_OUTLINED, "-0.05", 420, 324, 68, 44, z_offset_cb, "-0.05");
    make_button(popup, UI_BUTTON_OUTLINED, "-0.01", 496, 324, 68, 44, z_offset_cb, "-0.01");
    make_button(popup, UI_BUTTON_SECONDARY, "RESET", 572, 324, 68, 44, z_offset_cb, "RESET");
    make_button(popup, UI_BUTTON_OUTLINED, "+0.01", 648, 324, 68, 44, z_offset_cb, "+0.01");
    make_button(popup, UI_BUTTON_OUTLINED, "+0.05", 648, 376, 68, 44, z_offset_cb, "+0.05");

    make_button(popup, UI_BUTTON_OUTLINED, LV_SYMBOL_PLUS " EXTRUDE", 40, 410, 150, 46, extrude_cb, "EXTRUDE");
    make_button(popup, UI_BUTTON_OUTLINED, LV_SYMBOL_MINUS " RETRACT", 205, 410, 150, 46, extrude_cb, "RETRACT");

    refresh_step_highlights();
    refresh_live_labels();
    s_refresh_timer = lv_timer_create(refresh_timer_cb, 500, NULL);

    ui_popup_add_standard_footer_divider(popup);
    ui_popup_add_footer_action(
        popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_CENTER,
        close_cb,
        popup,
        NULL);
}
