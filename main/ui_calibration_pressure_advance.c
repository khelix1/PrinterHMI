#include "ui_calibration_pressure_advance.h"
#include "ui_text.h"

#include <stdio.h>
#include <string.h>

#include "console_controller.h"
#include "moonraker.h"

#include "ui_button.h"
#include "ui_popup.h"
#include "ui_calibration_dialog.h"
#include "ui_theme.h"
#include "ui_toast.h"

typedef struct {
    lv_obj_t *button;
    lv_obj_t *popup;
    lv_obj_t *feedback;
    uint32_t owner;
    bool armed;
    ui_calibration_pa_send_gcode_cb_t send_gcode;
    ui_calibration_pa_ready_cb_t ready;
    bool discovered;
    bool available;
} ui_calibration_pressure_advance_state_t;

static ui_calibration_pressure_advance_state_t s_pa;
static uint32_t s_pa_epoch;
static bool s_pa_dispatching;

static void pa_retire_popup(void)
{
    ++s_pa_epoch;
    s_pa.popup = NULL;
    s_pa.feedback = NULL;
    s_pa.armed = false;
}

static void pa_popup_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_pa.popup) pa_retire_popup();
}

static void pa_button_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_pa.button) s_pa.button = NULL;
}

static void pa_register_popup(void)
{
    lv_obj_add_event_cb(s_pa.popup, pa_popup_deleted, LV_EVENT_DELETE, NULL);
}


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
        pa_retire_popup();
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
        ui_cal_dialog_create(lv_layer_top(), 650, 410, UI_POPUP_STANDARD);

    if (!s_pa.popup) {
        return;
    }

    pa_register_popup();
    ui_cal_dialog_title(s_pa.popup, ui_text("Pressure advance tower armed"));


    s_pa.armed = true;
    char body[500];
    lv_snprintf(
        body,
        sizeof(body),
        "Runtime limits are now ACCEL=500 and SQUARE_CORNER_VELOCITY=1. The tower factor is %s.\n\n"
        "Start the prepared hollow-square calibration print next. After printing, measure the best corner height with calipers.\n\n"
        "No value has been saved. RESTART clears the temporary tuning state and restores configured motion limits.",
        factor ? factor : "--");
    ui_cal_dialog_text(ui_cal_dialog_body(s_pa.popup), body);

    ui_cal_dialog_action(s_pa.popup, UI_POPUP_ACTION_CLOSE, "Close", close_popup_cb, NULL, NULL);
}


static void arm_tower_cb(
    lv_event_t *event)
{
    if (!event || !s_pa.popup || s_pa.armed || s_pa_dispatching ||
        s_pa.owner != moonraker_config_generation() || !workflow_ready()) {
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
    lv_obj_t *popup = s_pa.popup;
    uint32_t epoch = s_pa_epoch;
    uint32_t owner = s_pa.owner;
    s_pa_dispatching = true;
    bool sent = s_pa.send_gcode && s_pa.send_gcode(command);
    s_pa_dispatching = false;
    if (s_pa_epoch != epoch || s_pa.popup != popup ||
        owner != moonraker_config_generation()) return;
    if (!sent) {
        if (s_pa.feedback)
            lv_label_set_text(s_pa.feedback,
                "Command was not accepted. Retry your drive choice when connected.");
        return;
    }
    show_armed_popup(factor);
}


static void button_cb(
    lv_event_t *event)
{
    (void)event;

    if (s_pa_dispatching || !workflow_ready()) {
        return;
    }

    if (!s_pa.discovered ||
        !s_pa.available) {
        return;
    }

    if (s_pa.popup) {
        lv_obj_move_foreground(s_pa.popup);
        return;
    }
    s_pa.owner = moonraker_config_generation();
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    close_popup();
    s_pa.popup =
        ui_cal_dialog_create(lv_layer_top(), 680, 450, UI_POPUP_STANDARD);

    if (!s_pa.popup) {
        return;
    }

    pa_register_popup();
    ui_cal_dialog_title(s_pa.popup, ui_text("Set up pressure advance tower"));


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
    ui_cal_dialog_text(ui_cal_dialog_body(s_pa.popup), body);

    s_pa.feedback = ui_cal_dialog_text(ui_cal_dialog_body(s_pa.popup), "");
    ui_cal_dialog_choice(s_pa.popup, UI_POPUP_ACTION_CONFIRM, ui_text("Direct drive"), 260, arm_tower_cb, (void *)ui_text(".005"), NULL);
    ui_cal_dialog_choice(s_pa.popup, UI_POPUP_ACTION_CONFIRM, ui_text("Long Bowden"), 260, arm_tower_cb, (void *)ui_text(".020"), NULL);


    ui_cal_dialog_action(s_pa.popup, UI_POPUP_ACTION_CLOSE, "Close", close_popup_cb, NULL, NULL);
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
        "PA tower");

    if (!s_pa.button) {
        return;
    }

    lv_obj_set_size(
        s_pa.button,
        150,
        44);
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
    lv_obj_add_event_cb(s_pa.button, pa_button_deleted, LV_EVENT_DELETE, NULL);
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
