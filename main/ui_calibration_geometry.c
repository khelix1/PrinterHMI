#include "ui_calibration_geometry.h"
#include <string.h>
#include "calibration_capability_controller.h"
#include "console_controller.h"
#include "moonraker.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"
static ui_calibration_geometry_context_t *s_geometry;
static void close_screws_popup(void)
{
    if (!s_geometry) {
        return;
    }

    if (*s_geometry->screws_popup) {
        lv_obj_t *popup =
            *s_geometry->screws_popup;
        *s_geometry->screws_popup = NULL;
        *s_geometry->screws_results_label = NULL;
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
        ui_popup_create(
            lv_layer_top(),
            660,
            430,
            UI_POPUP_STANDARD);

    if (!*s_geometry->screws_popup) {
        return;
    }

    ui_popup_add_title(
        *s_geometry->screws_popup,
        "SCREWS TILT RESULTS",
        false,
        4);
    ui_popup_add_header_divider(
        *s_geometry->screws_popup,
        48);

    *s_geometry->screws_results_label =
        ui_popup_add_body(
            *s_geometry->screws_popup,
            "Waiting for Klipper adjustment results...",
            30,
            76,
            600);

    ui_popup_add_standard_footer_divider(
        *s_geometry->screws_popup);

    ui_popup_add_footer_action(
        *s_geometry->screws_popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        180,
        UI_POPUP_FOOTER_RIGHT,
        close_screws_popup_cb,
        NULL,
        NULL);
}


static void refresh_screws_results(void)
{
    calibration_session_controller_poll();

    if (!s_geometry ||
        !*s_geometry->screws_results_label) {
        return;
    }

    calibration_session_snapshot_t *session =
        &*s_geometry->session_snapshot;
    calibration_session_controller_snapshot(
        session);

    if (session->generation ==
        *s_geometry->session_generation) {
        return;
    }

    *s_geometry->session_generation =
        session->generation;

    switch (session->status) {
    case CALIBRATION_SESSION_RESULTS:
        lv_label_set_text(
            *s_geometry->screws_results_label,
            session->results[0]
                ? session->results
                : "Klipper completed without adjustment lines.");
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
            "Waiting for Klipper adjustment results...");
        break;

    case CALIBRATION_SESSION_IDLE:
    default:
        break;
    }
}


static void run_screws_tilt_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_geometry) {
        return;
    }

    const char *command =
        *s_geometry->screws_home_required
            ? "G28\nSCREWS_TILT_CALCULATE"
            : "SCREWS_TILT_CALCULATE";

    uint32_t start_sequence =
        console_controller_latest_sequence();

    calibration_session_controller_begin_screws_tilt(
        start_sequence);
    console_controller_add_command(command);

    bool sent =
        s_geometry->send_gcode &&
        s_geometry->send_gcode(command);

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the calibration command.");
    }

    show_screws_results_popup();
    refresh_screws_results();
}


static void screws_tilt_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_geometry) {
        return;
    }

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
        ui_toast_show(
            UI_STATUS_DANGER,
            "CALIBRATION UNAVAILABLE",
            "The active printer is offline or not ready.");
        return;
    }

    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        ui_toast_show(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Screws Tilt cannot run during a print.");
        return;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_toast_show(
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
        ui_popup_create(
            lv_layer_top(),
            600,
            360,
            UI_POPUP_STANDARD);

    if (!*s_geometry->screws_popup) {
        return;
    }

    ui_popup_add_title(
        *s_geometry->screws_popup,
        "RUN SCREWS TILT?",
        false,
        4);
    ui_popup_add_header_divider(
        *s_geometry->screws_popup,
        48);

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

    ui_popup_add_body(
        *s_geometry->screws_popup,
        body,
        28,
        76,
        544);

    ui_popup_add_standard_footer_divider(
        *s_geometry->screws_popup);

    ui_popup_add_footer_action(
        *s_geometry->screws_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_screws_popup_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        *s_geometry->screws_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " RUN",
        170,
        UI_POPUP_FOOTER_RIGHT,
        run_screws_tilt_cb,
        NULL,
        NULL);
}


static void close_gantry_level_popup(void)
{
    if (!s_geometry) {
        return;
    }

    if (*s_geometry->gantry_popup) {
        lv_obj_t *popup =
            *s_geometry->gantry_popup;
        *s_geometry->gantry_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_gantry_level_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_gantry_level_popup();
}


static void run_gantry_level_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_geometry) {
        return;
    }

    const char *command =
        *s_geometry->gantry_use_qgl
            ? "QUAD_GANTRY_LEVEL"
            : "Z_TILT_ADJUST";

    /*
     * The shared action gateway prefers a detected printer macro. The macro
     * owns printer-specific homing and sequencing. If no macro exists, keep
     * the proven standard Klipper fallback and add G28 only when required.
     */
    char fallback[48];
    if (*s_geometry->gantry_home_required) {
        lv_snprintf(
            fallback,
            sizeof(fallback),
            "G28\n%s",
            command);
        command = fallback;
    }

    console_controller_add_command(command);

    bool sent =
        s_geometry->send_gcode &&
        s_geometry->send_gcode(command);

    close_gantry_level_popup();

    if (!sent) {
        ui_toast_show(
            UI_STATUS_DANGER,
            "CALIBRATION FAILED",
            "Moonraker did not accept the gantry-level command.");
        return;
    }

    ui_toast_show(
        UI_STATUS_OK,
        *s_geometry->gantry_use_qgl
            ? "QGL STARTED"
            : "Z TILT STARTED",
        "Klipper is running the leveling workflow. Results remain available in Console.");
}


static void gantry_level_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_geometry) {
        return;
    }

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
        ui_toast_show(
            UI_STATUS_DANGER,
            "CALIBRATION UNAVAILABLE",
            "The active printer is offline or not ready.");
        return;
    }

    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        ui_toast_show(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Gantry leveling cannot run during a print.");
        return;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_toast_show(
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
        ui_popup_create(
            lv_layer_top(),
            620,
            370,
            UI_POPUP_STANDARD);

    if (!*s_geometry->gantry_popup) {
        return;
    }

    ui_popup_add_title(
        *s_geometry->gantry_popup,
        *s_geometry->gantry_use_qgl
            ? "RUN QUAD GANTRY LEVEL?"
            : "RUN Z TILT?",
        false,
        4);
    ui_popup_add_header_divider(
        *s_geometry->gantry_popup,
        48);

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

    ui_popup_add_body(
        *s_geometry->gantry_popup,
        body,
        28,
        76,
        564);

    ui_popup_add_standard_footer_divider(
        *s_geometry->gantry_popup);

    ui_popup_add_footer_action(
        *s_geometry->gantry_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_gantry_level_popup_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        *s_geometry->gantry_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " RUN",
        170,
        UI_POPUP_FOOTER_RIGHT,
        run_gantry_level_cb,
        NULL,
        NULL);
}



void ui_calibration_geometry_init(ui_calibration_geometry_context_t *context){s_geometry=context;}
void ui_calibration_geometry_screws_event(lv_event_t *event){screws_tilt_button_cb(event);}
void ui_calibration_geometry_gantry_event(lv_event_t *event){gantry_level_button_cb(event);}
void ui_calibration_geometry_refresh(void){refresh_screws_results();}
void ui_calibration_geometry_close(void){close_screws_popup();close_gantry_level_popup();}
