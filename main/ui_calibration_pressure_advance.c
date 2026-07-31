#include "ui_calibration_pressure_advance.h"

#include <stdio.h>
#include <string.h>

#include "console_controller.h"
#include "moonraker.h"

#include "ui_button.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast_v32.h"

typedef struct {
    lv_obj_t *button;
    lv_obj_t *popup;
    ui_calibration_pa_send_gcode_cb_t send_gcode;
    ui_calibration_pa_ready_cb_t ready;
    bool discovered;
    bool available;
} ui_calibration_pressure_advance_state_t;

static ui_calibration_pressure_advance_state_t s_pa;


static bool workflow_ready(void)
{
    return s_pa.ready &&
        s_pa.ready(
            "Pressure Advance tower setup");
}


static void set_button_visible(bool visible)
{
    if (!s_pa.button) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(
            s_pa.button,
            LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(
            s_pa.button,
            LV_OBJ_FLAG_HIDDEN);
    }
}


static void close_popup(void)
{
    if (s_pa.popup) {
        lv_obj_t *popup = s_pa.popup;
        s_pa.popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_popup();
}


static void show_armed_popup(
    const char *factor)
{
    close_popup();
    s_pa.popup =
        ui_popup_create(
            lv_layer_top(),
            650,
            410,
            UI_POPUP_STANDARD);

    if (!s_pa.popup) {
        return;
    }

    ui_popup_add_title(
        s_pa.popup,
        "PRESSURE ADVANCE TOWER ARMED",
        false,
        4);
    ui_popup_add_header_divider(
        s_pa.popup,
        48);

    char body[500];
    lv_snprintf(
        body,
        sizeof(body),
        "Runtime limits are now ACCEL=500 and SQUARE_CORNER_VELOCITY=1. The tower factor is %s.\n\n"
        "Start the prepared hollow-square calibration print next. After printing, measure the best corner height with calipers.\n\n"
        "No value has been saved. RESTART clears the temporary tuning state and restores configured motion limits.",
        factor ? factor : "--");
    ui_popup_add_body(
        s_pa.popup,
        body,
        28,
        70,
        594);
    ui_popup_add_standard_footer_divider(
        s_pa.popup);
    ui_popup_add_footer_action(
        s_pa.popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        170,
        UI_POPUP_FOOTER_RIGHT,
        close_popup_cb,
        NULL,
        NULL);
}


static void arm_tower_cb(
    lv_event_t *event)
{
    if (!event || !workflow_ready()) {
        return;
    }

    const char *factor =
        (const char *)lv_event_get_user_data(event);

    if (!factor ||
        (strcmp(factor, ".005") != 0 &&
         strcmp(factor, ".020") != 0)) {
        return;
    }

    char command[240];
    int written = lv_snprintf(
        command,
        sizeof(command),
        "SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=1 ACCEL=500\n"
        "TUNING_TOWER COMMAND=SET_PRESSURE_ADVANCE PARAMETER=ADVANCE START=0 FACTOR=%s",
        factor);

    if (written <= 0 ||
        (size_t)written >= sizeof(command)) {
        return;
    }

    console_controller_add_command(command);
    bool sent =
        s_pa.send_gcode &&
        s_pa.send_gcode(command);

    close_popup();

    if (!sent) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "PA SETUP FAILED",
            "Moonraker did not accept the Pressure Advance tower setup.");
        return;
    }

    show_armed_popup(factor);
}


static void button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!workflow_ready()) {
        return;
    }

    if (!s_pa.discovered ||
        !s_pa.available) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    close_popup();
    s_pa.popup =
        ui_popup_create(
            lv_layer_top(),
            680,
            450,
            UI_POPUP_STANDARD);

    if (!s_pa.popup) {
        return;
    }

    ui_popup_add_title(
        s_pa.popup,
        "SET UP PRESSURE ADVANCE TOWER",
        false,
        4);
    ui_popup_add_header_divider(
        s_pa.popup,
        48);

    char body[520];
    lv_snprintf(
        body,
        sizeof(body),
        "Prepare the hollow-square PA tower before continuing. Dynamic acceleration and scarf seams must be disabled.\n\n"
        "Select the drive type for the currently active extruder: %s\n\n"
        "This changes runtime test limits only. It does not write printer.cfg.",
        state.active_hotend[0]
            ? state.active_hotend
            : "extruder");
    ui_popup_add_body(
        s_pa.popup,
        body,
        28,
        68,
        624);

    ui_popup_add_action_at(
        s_pa.popup,
        UI_POPUP_ACTION_CONFIRM,
        "DIRECT DRIVE",
        54,
        286,
        260,
        48,
        arm_tower_cb,
        (void *)".005",
        NULL);
    ui_popup_add_action_at(
        s_pa.popup,
        UI_POPUP_ACTION_CONFIRM,
        "LONG BOWDEN",
        366,
        286,
        260,
        48,
        arm_tower_cb,
        (void *)".020",
        NULL);

    ui_popup_add_standard_footer_divider(
        s_pa.popup);
    ui_popup_add_footer_action(
        s_pa.popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        170,
        UI_POPUP_FOOTER_RIGHT,
        close_popup_cb,
        NULL,
        NULL);
}


void ui_calibration_pressure_advance_create(
    lv_obj_t *card,
    ui_calibration_pa_send_gcode_cb_t send_gcode_cb,
    ui_calibration_pa_ready_cb_t ready_cb)
{
    if (!card) {
        return;
    }

    s_pa.send_gcode = send_gcode_cb;
    s_pa.ready = ready_cb;

    s_pa.button = ui_button_create(
        card,
        UI_BUTTON_OUTLINED,
        "PA TOWER");

    if (!s_pa.button) {
        return;
    }

    lv_obj_set_size(
        s_pa.button,
        150,
        38);
    lv_obj_align(
        s_pa.button,
        LV_ALIGN_BOTTOM_LEFT,
        16,
        -12);
    lv_obj_add_event_cb(
        s_pa.button,
        button_cb,
        LV_EVENT_CLICKED,
        NULL);
    lv_obj_add_flag(
        s_pa.button,
        LV_OBJ_FLAG_HIDDEN);
}


void ui_calibration_pressure_advance_refresh(
    bool discovered,
    bool pressure_advance)
{
    s_pa.discovered = discovered;
    s_pa.available = pressure_advance;
    set_button_visible(
        discovered && pressure_advance);
}


void ui_calibration_pressure_advance_hide(void)
{
    close_popup();
    memset(&s_pa, 0, sizeof(s_pa));
}
