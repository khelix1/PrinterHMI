#include "ui_printer_motion.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void ui_printer_motion_init(void)
{
}


lv_obj_t *ui_printer_motion_button(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y,
    int w,
    int h,
    lv_event_cb_t cb,
    const char *user)
{
    lv_obj_t *btn =
        ui_button_create(
            parent,
            UI_BUTTON_OUTLINED,
            text);

    if (!btn) {
        return NULL;
    }

    lv_obj_set_size(
        btn,
        w,
        h);

    lv_obj_set_pos(
        btn,
        x,
        y);

    if (cb) {
        lv_obj_add_event_cb(
            btn,
            cb,
            LV_EVENT_CLICKED,
            (void *)user);
    }

    return btn;
}

static lv_obj_t *ui_printer_motion_icon_button(
    lv_obj_t *parent,
    const char *symbol,
    const char *text,
    lv_color_t icon_color,
    ui_button_icon_layout_t layout,
    int x,
    int y,
    int w,
    int h,
    lv_event_cb_t cb,
    const char *user)
{
    lv_obj_t *btn =
        ui_button_create_icon(
            parent,
            UI_BUTTON_OUTLINED,
            symbol,
            text,
            icon_color,
            layout);

    if (!btn) {
        return NULL;
    }

    lv_obj_set_size(
        btn,
        w,
        h);

    lv_obj_set_pos(
        btn,
        x,
        y);

    /*
     * Realign vertical content after applying the final button size.
     */
    if (layout == UI_BUTTON_ICON_VERTICAL ||
        layout == UI_BUTTON_ICON_VERTICAL_REVERSE) {
        lv_obj_t *icon =
            lv_obj_get_child(btn, 0);

        lv_obj_t *label =
            lv_obj_get_child(btn, 1);

        lv_obj_set_style_pad_top(
            btn,
            0,
            0);

        lv_obj_set_style_pad_bottom(
            btn,
            0,
            0);

        if (icon && label) {
            if (layout == UI_BUTTON_ICON_VERTICAL_REVERSE) {
                lv_obj_align(
                    label,
                    LV_ALIGN_TOP_MID,
                    0,
                    8);

                lv_obj_align(
                    icon,
                    LV_ALIGN_BOTTOM_MID,
                    0,
                    -8);
            } else {
                lv_obj_align(
                    icon,
                    LV_ALIGN_TOP_MID,
                    0,
                    8);

                lv_obj_align(
                    label,
                    LV_ALIGN_BOTTOM_MID,
                    0,
                    -8);
            }
        }
    }

    if (cb) {
        lv_obj_add_event_cb(
            btn,
            cb,
            LV_EVENT_CLICKED,
            (void *)user);
    }

    return btn;
}

void ui_printer_motion_update_step_highlight(lv_obj_t *step1_btn,
                                             lv_obj_t *step10_btn,
                                             lv_obj_t *step50_btn,
                                             double jog_step)
{
    if (step1_btn) {
        ui_button_apply_kind(
            step1_btn,
            jog_step == 1.0
                ? UI_BUTTON_SUCCESS
                : UI_BUTTON_OUTLINED);
    }

    if (step10_btn) {
        ui_button_apply_kind(
            step10_btn,
            jog_step == 10.0
                ? UI_BUTTON_SUCCESS
                : UI_BUTTON_OUTLINED);
    }

    if (step50_btn) {
        ui_button_apply_kind(
            step50_btn,
            jog_step == 50.0
                ? UI_BUTTON_SUCCESS
                : UI_BUTTON_OUTLINED);
    }
}

bool ui_printer_motion_format_jog_command(const char *axis,
                                          double jog_step,
                                          char *cmd,
                                          size_t cmd_size)
{
    if (!axis || !axis[0] || !cmd || cmd_size == 0) return false;

    if (strcmp(axis, LV_SYMBOL_HOME " HOME") == 0) {
        snprintf(cmd, cmd_size, "G28");
        return true;
    }

    double dist = jog_step;

    if (strcmp(axis, "X-") == 0 || strcmp(axis, "Y-") == 0 || strcmp(axis, "Z-") == 0) {
        dist = -dist;
    }

    if (axis[0] == 'Z') {
        snprintf(cmd, cmd_size, "G91\nG1 Z%.2f F600\nG90", dist);
    } else {
        snprintf(cmd, cmd_size, "G91\nG1 %c%.2f F6000\nG90", axis[0], dist);
    }

    return true;
}

bool ui_printer_motion_format_extrude_command(const char *dir,
                                              char *cmd,
                                              size_t cmd_size)
{
    if (!dir || !cmd || cmd_size == 0) return false;

    if (strcmp(dir, "EXTRUDE") == 0) {
        snprintf(cmd, cmd_size, "G91\nG1 E10 F300\nG90");
        return true;
    }

    if (strcmp(dir, "RETRACT") == 0) {
        snprintf(cmd, cmd_size, "G91\nG1 E-10 F300\nG90");
        return true;
    }

    return false;
}

void ui_printer_motion_show_popup(lv_obj_t **step1_btn,
                                  lv_obj_t **step10_btn,
                                  lv_obj_t **step50_btn,
                                  double jog_step,
                                  lv_event_cb_t jog_cb,
                                  lv_event_cb_t step_cb,
                                  lv_event_cb_t extrude_cb,
                                  lv_event_cb_t close_cb)
{
    lv_obj_t *popup =
        ui_popup_create(
            lv_layer_top(),
            700,
            520,
            UI_POPUP_STANDARD);

    if (!popup) {
        return;
    }

    ui_popup_add_title(
        popup,
        "MOTION CONTROL",
        false,
        18);

    ui_popup_add_header_divider(
        popup,
        58);

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_UP,
        "Y+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL,
        260, 80, 120, 80,
        jog_cb,
        "Y+");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_LEFT,
        "X-",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL,
        125, 175, 120, 70,
        jog_cb,
        "X-");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_HOME,
        "HOME",
        UI_OK_BRIGHT,
        UI_BUTTON_ICON_HORIZONTAL,
        260, 175, 120, 70,
        jog_cb,
        LV_SYMBOL_HOME " HOME");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_RIGHT,
        "X+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_HORIZONTAL_REVERSE,
        395, 175, 120, 70,
        jog_cb,
        "X+");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_DOWN,
        "Y-",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL_REVERSE,
        260, 260, 120, 80,
        jog_cb,
        "Y-");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_UP,
        "Z+",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL,
        540, 125, 120, 80,
        jog_cb,
        "Z+");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_DOWN,
        "Z-",
        UI_ACCENT_CYAN,
        UI_BUTTON_ICON_VERTICAL_REVERSE,
        540, 220, 120, 80,
        jog_cb,
        "Z-");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_PLUS,
        "EXTRUDE",
        UI_ACCENT_ORANGE,
        UI_BUTTON_ICON_HORIZONTAL,
        520, 310, 135, 50,
        extrude_cb,
        "EXTRUDE");

    ui_printer_motion_icon_button(
        popup,
        LV_SYMBOL_MINUS,
        "RETRACT",
        UI_ACCENT_PURPLE,
        UI_BUTTON_ICON_HORIZONTAL,
        520, 370, 135, 50,
        extrude_cb,
        "RETRACT");

    ui_popup_add_caption(
        popup,
        "STEP",
        250,
        352,
        180);

    if (step1_btn)  *step1_btn  = ui_printer_motion_button(popup, "1mm",  175, 385, 90, 50, step_cb, "1");
    if (step10_btn) *step10_btn = ui_printer_motion_button(popup, "10mm", 280, 385, 90, 50, step_cb, "10");
    if (step50_btn) *step50_btn = ui_printer_motion_button(popup, "50mm", 385, 385, 90, 50, step_cb, "50");

    ui_printer_motion_update_step_highlight(step1_btn ? *step1_btn : NULL,
                                            step10_btn ? *step10_btn : NULL,
                                            step50_btn ? *step50_btn : NULL,
                                            jog_step);

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


static lv_obj_t **s_motion_step1_btn = NULL;
static lv_obj_t **s_motion_step10_btn = NULL;
static lv_obj_t **s_motion_step50_btn = NULL;
static double *s_motion_jog_step = NULL;
static ui_printer_motion_send_gcode_cb_t s_motion_send_gcode = NULL;

static void ui_printer_motion_internal_update_step_highlight(void)
{
    ui_printer_motion_update_step_highlight(s_motion_step1_btn ? *s_motion_step1_btn : NULL,
                                            s_motion_step10_btn ? *s_motion_step10_btn : NULL,
                                            s_motion_step50_btn ? *s_motion_step50_btn : NULL,
                                            s_motion_jog_step ? *s_motion_jog_step : 10.0);
}

static void ui_printer_motion_step_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!s_motion_jog_step) return;

    const char *step = (const char *)lv_event_get_user_data(e);
    if (!step) return;

    *s_motion_jog_step = atof(step);
    ui_printer_motion_internal_update_step_highlight();
}

static void ui_printer_motion_jog_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!s_motion_send_gcode) return;

    const char *axis = (const char *)lv_event_get_user_data(e);
    char cmd[96];

    if (ui_printer_motion_format_jog_command(axis,
                                             s_motion_jog_step ? *s_motion_jog_step : 10.0,
                                             cmd,
                                             sizeof(cmd))) {
        s_motion_send_gcode(cmd);
    }
}

static void ui_printer_motion_extrude_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!s_motion_send_gcode) return;

    const char *dir = (const char *)lv_event_get_user_data(e);
    char cmd[96];

    if (ui_printer_motion_format_extrude_command(dir, cmd, sizeof(cmd))) {
        s_motion_send_gcode(cmd);
    }
}

static void ui_printer_motion_close_event_cb(lv_event_t *e)
{
    lv_obj_t *popup = (lv_obj_t *)lv_event_get_user_data(e);
    if (popup) {
        lv_obj_delete(popup);
    }

    if (s_motion_step1_btn) *s_motion_step1_btn = NULL;
    if (s_motion_step10_btn) *s_motion_step10_btn = NULL;
    if (s_motion_step50_btn) *s_motion_step50_btn = NULL;
}

void ui_printer_motion_show(lv_obj_t **step1_btn,
                            lv_obj_t **step10_btn,
                            lv_obj_t **step50_btn,
                            double *jog_step,
                            ui_printer_motion_send_gcode_cb_t send_gcode_cb)
{
    s_motion_step1_btn = step1_btn;
    s_motion_step10_btn = step10_btn;
    s_motion_step50_btn = step50_btn;
    s_motion_jog_step = jog_step;
    s_motion_send_gcode = send_gcode_cb;

    ui_printer_motion_show_popup(step1_btn,
                                 step10_btn,
                                 step50_btn,
                                 jog_step ? *jog_step : 10.0,
                                 ui_printer_motion_jog_event_cb,
                                 ui_printer_motion_step_event_cb,
                                 ui_printer_motion_extrude_event_cb,
                                 ui_printer_motion_close_event_cb);
}
