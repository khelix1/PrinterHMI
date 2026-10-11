#include "ui_calibration_geometry.h"
#include "ui_text.h"
#include <string.h>
#include "calibration_capability_controller.h"
#include "console_controller.h"
#include "moonraker.h"
#include "ui_popup.h"
#include "ui_calibration_dialog.h"
#include "ui_theme.h"
#include "ui_toast.h"
#include "ui_value_update.h"
#include "printer_connection_policy.h"
static ui_calibration_geometry_context_t *s_geometry;
static uint32_t s_geometry_epoch, s_screws_owner, s_gantry_owner;
static bool s_geometry_dispatching, s_gantry_requested, s_screws_rendered;
static lv_obj_t *s_screws_feedback, *s_gantry_feedback;

static void retire_screws(void)
{
    ++s_geometry_epoch;
    s_screws_feedback = NULL;
    s_screws_rendered = false;
    if (s_geometry) {
        *s_geometry->screws_popup = NULL;
        *s_geometry->screws_results_label = NULL;
    }
}
static void retire_gantry(void)
{
    ++s_geometry_epoch;
    s_gantry_feedback = NULL;
    s_gantry_requested = false;
    if (s_geometry) *s_geometry->gantry_popup = NULL;
}
static void geometry_deleted(lv_event_t *event)
{
    if (!s_geometry) return;
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == *s_geometry->screws_popup) retire_screws();
    else if (target == *s_geometry->gantry_popup) retire_gantry();
}
static void geometry_register(lv_obj_t *popup, uint32_t owner)
{
    lv_obj_set_user_data(popup, (void *)(uintptr_t)owner);
    lv_obj_add_event_cb(popup, geometry_deleted, LV_EVENT_DELETE, NULL);
}
static const char *geometry_run_reason(bool gantry)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    const char *reason = printer_idle_reason(&state);
    if (reason[0]) return reason;
    calibration_capabilities_t capabilities;
    calibration_capability_controller_snapshot(&capabilities);
    if (gantry ? (*s_geometry->gantry_use_qgl ? !capabilities.quad_gantry_level : !capabilities.z_tilt) : !capabilities.screws_tilt)
        return "This calibration is no longer available. Close and reopen to refresh capabilities.";
    bool homed = strchr(state.homed_axes, 'x') && strchr(state.homed_axes, 'y') && strchr(state.homed_axes, 'z');
    if (!homed && !(gantry ? *s_geometry->gantry_home_required : *s_geometry->screws_home_required))
        return "Axes are no longer homed. Go Back and reopen this check to review homing.";
    return "";
}
static void close_screws_popup(void)
{
    if (!s_geometry) {
        return;
    }

    if (*s_geometry->screws_popup) {
        lv_obj_t *popup =
            *s_geometry->screws_popup;
        retire_screws();
        lv_obj_delete(popup);
    }
}


static void close_screws_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_screws_popup();
}


static void show_screws_results_popup(void)
{
    if (!s_geometry) {
        return;
    }

    close_screws_popup();

    *s_geometry->screws_popup =
        ui_cal_dialog_create(lv_layer_top(), 660, 430, UI_POPUP_STANDARD);

    if (!*s_geometry->screws_popup) {
        return;
    }

    geometry_register(*s_geometry->screws_popup, s_screws_owner);
    ui_cal_dialog_title(*s_geometry->screws_popup, ui_text("SCREWS TILT RESULTS"));


    *s_geometry->screws_results_label =
        ui_cal_dialog_text(ui_cal_dialog_body(*s_geometry->screws_popup), "Waiting for Klipper adjustment results...");



    ui_cal_dialog_action(*s_geometry->screws_popup, UI_POPUP_ACTION_CLOSE, "CLOSE", close_screws_popup_cb, NULL, NULL);
}


static void refresh_screws_results(void)
{
    calibration_session_controller_poll();

    if (!s_geometry ||
        !*s_geometry->screws_results_label) {
        return;
    }

    if (s_screws_owner != moonraker_config_generation()) {
        ui_value_set_text(*s_geometry->screws_results_label,
            "Printer changed. Close and reopen Screws Tilt for the selected printer.");
        return;
    }
    calibration_session_snapshot_t *session =
        &*s_geometry->session_snapshot;
    calibration_session_controller_snapshot(
        session);

    if (s_screws_rendered && session->generation ==
        *s_geometry->session_generation) {
        return;
    }

    s_screws_rendered = true;
    *s_geometry->session_generation =
        session->generation;

    switch (session->status) {
    case CALIBRATION_SESSION_RESULTS:
        lv_label_set_text(
            *s_geometry->screws_results_label,
            session->results[0]
                ? session->results
                : ui_text("Klipper completed without adjustment lines."));
        break;

    case CALIBRATION_SESSION_ERROR: {
        lv_snprintf(
            s_geometry->screws_display,
            s_geometry->screws_display_size,
            "Klipper reported an error:\n\n%s",
            session->results[0]
                ? session->results
                : "Unknown calibration error.");
        lv_label_set_text(
            *s_geometry->screws_results_label,
            s_geometry->screws_display);
        break;
    }

    case CALIBRATION_SESSION_WAITING:
        lv_label_set_text(
            *s_geometry->screws_results_label,
            ui_text("Waiting for Klipper adjustment results..."));
        break;

    case CALIBRATION_SESSION_IDLE:
    default:
        break;
    }
}


static void run_screws_tilt_cb(lv_event_t *event)
{
    (void)event;
    if (!s_geometry || !*s_geometry->screws_popup ||
        *s_geometry->screws_results_label || s_geometry_dispatching ||
        s_screws_owner != moonraker_config_generation()) return;
    const char *reason = geometry_run_reason(false);
    if (reason[0]) { ui_value_set_text(s_screws_feedback, reason); return; }
    const char *command = *s_geometry->screws_home_required ?
        "G28\nSCREWS_TILT_CALCULATE" : "SCREWS_TILT_CALCULATE";
    ui_calibration_geometry_context_t *context = s_geometry;
    lv_obj_t *popup = *context->screws_popup;
    uint32_t epoch = s_geometry_epoch, owner = s_screws_owner;
    calibration_session_controller_begin_screws_tilt(console_controller_latest_sequence());
    console_controller_add_command(command);
    s_geometry_dispatching = true;
    bool sent = context->send_gcode && context->send_gcode(command);
    s_geometry_dispatching = false;
    if (s_geometry != context || s_geometry_epoch != epoch ||
        *context->screws_popup != popup || owner != moonraker_config_generation()) return;
    if (!sent) {
        calibration_session_controller_mark_error("Moonraker did not accept the calibration command.");
        ui_value_set_text(s_screws_feedback, "Command was not accepted. Retry when connected.");
        return;
    }
    show_screws_results_popup();
    refresh_screws_results();
}

static void screws_tilt_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_geometry || s_geometry_dispatching) {
        return;
    }

    if (*s_geometry->screws_popup) {
        lv_obj_move_foreground(*s_geometry->screws_popup);
        return;
    }
    s_screws_owner = moonraker_config_generation();

    calibration_capabilities_t capabilities;
    calibration_capability_controller_snapshot(
        &capabilities);

    if (!capabilities.screws_tilt) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!state.moonraker_ok ||
        !state.live_data_ok) {
        ui_cal_dialog_notice(
            UI_STATUS_DANGER,
            "CALIBRATION UNAVAILABLE",
            "The active printer is offline or not ready.");
        return;
    }

    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        ui_cal_dialog_notice(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Screws Tilt cannot run during a print.");
        return;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_cal_dialog_notice(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Clear the printer error before calibration.");
        return;
    }

    *s_geometry->screws_home_required =
        !strchr(state.homed_axes, 'x') ||
        !strchr(state.homed_axes, 'y') ||
        !strchr(state.homed_axes, 'z');

    close_screws_popup();

    *s_geometry->screws_popup =
        ui_cal_dialog_create(lv_layer_top(), 600, 360, UI_POPUP_STANDARD);

    if (!*s_geometry->screws_popup) {
        return;
    }

    geometry_register(*s_geometry->screws_popup, s_screws_owner);
    ui_cal_dialog_title(*s_geometry->screws_popup, ui_text("RUN SCREWS TILT?"));


    char body[420];
    lv_snprintf(
        body,
        sizeof(body),
        "The toolhead will move to every configured screw and probe the bed.%s\n\n"
        "Clear the bed and motion area before continuing.\n\n"
        "This reads adjustment guidance only. SAVE_CONFIG will not run.",
        *s_geometry->screws_home_required
            ? " XYZ is not homed, so all axes will home first."
            : "");

    ui_cal_dialog_text(ui_cal_dialog_body(*s_geometry->screws_popup), body);
    s_screws_feedback = ui_cal_dialog_text(ui_cal_dialog_body(*s_geometry->screws_popup), "");



    ui_cal_dialog_action(*s_geometry->screws_popup, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_LEFT " BACK", close_screws_popup_cb, NULL, NULL);

    ui_cal_dialog_action(*s_geometry->screws_popup, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_PLAY " RUN", run_screws_tilt_cb, NULL, NULL);
}


static void close_gantry_level_popup(void)
{
    if (!s_geometry) {
        return;
    }

    if (*s_geometry->gantry_popup) {
        lv_obj_t *popup =
            *s_geometry->gantry_popup;
        retire_gantry();
        lv_obj_delete(popup);
    }
}


static void close_gantry_level_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_gantry_level_popup();
}


static void run_gantry_level_cb(lv_event_t *event)
{
    (void)event;
    if (!s_geometry || !*s_geometry->gantry_popup || s_geometry_dispatching ||
        s_gantry_requested || s_gantry_owner != moonraker_config_generation()) return;
    const char *reason = geometry_run_reason(true);
    if (reason[0]) { ui_value_set_text(s_gantry_feedback, reason); return; }
    const char *command = *s_geometry->gantry_use_qgl ? "QUAD_GANTRY_LEVEL" : "Z_TILT_ADJUST";
    char fallback[48];
    if (*s_geometry->gantry_home_required) {
        lv_snprintf(fallback, sizeof(fallback), "G28\n%s", command);
        command = fallback;
    }
    ui_calibration_geometry_context_t *context = s_geometry;
    lv_obj_t *popup = *context->gantry_popup;
    uint32_t epoch = s_geometry_epoch, owner = s_gantry_owner;
    console_controller_add_command(command);
    s_geometry_dispatching = true;
    bool sent = context->send_gcode && context->send_gcode(command);
    s_geometry_dispatching = false;
    if (s_geometry != context || s_geometry_epoch != epoch ||
        *context->gantry_popup != popup || owner != moonraker_config_generation()) return;
    if (!sent) {
        ui_value_set_text(s_gantry_feedback, "Command was not accepted. Retry when connected.");
        return;
    }
    s_gantry_requested = true;
    ui_value_set_text(s_gantry_feedback,
        "Leveling request accepted. Monitor Console for completion and printer-specific instructions.");
    lv_obj_t *footer = lv_obj_get_child(popup, 2);
    for (uint32_t i = 0; i < lv_obj_get_child_count(footer); ++i) {
        lv_obj_add_state(lv_obj_get_child(footer, i), LV_STATE_DISABLED);
        lv_obj_add_flag(lv_obj_get_child(footer, i), LV_OBJ_FLAG_HIDDEN);
    }
    ui_cal_dialog_action(popup, UI_POPUP_ACTION_CLOSE, "CLOSE", close_gantry_level_popup_cb, NULL, NULL);
}

static void gantry_level_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_geometry || s_geometry_dispatching) {
        return;
    }

    if (*s_geometry->gantry_popup) {
        lv_obj_move_foreground(*s_geometry->gantry_popup);
        return;
    }
    s_gantry_owner = moonraker_config_generation();

    calibration_capabilities_t capabilities;
    calibration_capability_controller_snapshot(
        &capabilities);

    if (!capabilities.z_tilt &&
        !capabilities.quad_gantry_level) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!state.moonraker_ok ||
        !state.live_data_ok) {
        ui_cal_dialog_notice(
            UI_STATUS_DANGER,
            "CALIBRATION UNAVAILABLE",
            "The active printer is offline or not ready.");
        return;
    }

    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        ui_cal_dialog_notice(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Gantry leveling cannot run during a print.");
        return;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_cal_dialog_notice(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Clear the printer error before calibration.");
        return;
    }

    *s_geometry->gantry_use_qgl =
        capabilities.quad_gantry_level;
    *s_geometry->gantry_home_required =
        !strchr(state.homed_axes, 'x') ||
        !strchr(state.homed_axes, 'y') ||
        !strchr(state.homed_axes, 'z');

    close_gantry_level_popup();

    *s_geometry->gantry_popup =
        ui_cal_dialog_create(lv_layer_top(), 620, 370, UI_POPUP_STANDARD);

    if (!*s_geometry->gantry_popup) {
        return;
    }

    geometry_register(*s_geometry->gantry_popup, s_gantry_owner);
    ui_cal_dialog_title(*s_geometry->gantry_popup, *s_geometry->gantry_use_qgl
            ? ui_text("RUN QUAD GANTRY LEVEL?")
            : ui_text("RUN Z TILT?"));


    char body[430];
    lv_snprintf(
        body,
        sizeof(body),
        "The toolhead will probe multiple bed positions and the gantry will move.%s\n\n"
        "Clear the bed and motion area before continuing.\n\n"
        "This runs the detected Klipper leveling command. SAVE_CONFIG will not run.",
        *s_geometry->gantry_home_required
            ? " XYZ is not homed, so all axes will home first."
            : "");

    ui_cal_dialog_text(ui_cal_dialog_body(*s_geometry->gantry_popup), body);
    s_gantry_feedback = ui_cal_dialog_text(ui_cal_dialog_body(*s_geometry->gantry_popup), "");



    ui_cal_dialog_action(*s_geometry->gantry_popup, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_LEFT " BACK", close_gantry_level_popup_cb, NULL, NULL);

    ui_cal_dialog_action(*s_geometry->gantry_popup, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_PLAY " RUN", run_gantry_level_cb, NULL, NULL);
}



void ui_calibration_geometry_init(ui_calibration_geometry_context_t *context){s_geometry=context;}
void ui_calibration_geometry_screws_event(lv_event_t *event){screws_tilt_button_cb(event);}
void ui_calibration_geometry_gantry_event(lv_event_t *event){gantry_level_button_cb(event);}
void ui_calibration_geometry_refresh(void){refresh_screws_results();}
void ui_calibration_geometry_close(void){close_screws_popup();close_gantry_level_popup();}
