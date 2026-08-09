#include "ui_calibration_results.h"

#include <string.h>

#include "console_controller.h"
#include "moonraker.h"
#include "ui_popup.h"
#include "ui_toast.h"

static ui_calibration_results_context_t *s_context;

void ui_calibration_results_init(ui_calibration_results_context_t *context)
{
    s_context = context;
}

static bool results_printer_ready(void)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!state.moonraker_ok || !state.live_data_ok) {
        ui_toast_show(UI_STATUS_DANGER, "CALIBRATION UNAVAILABLE",
                      "The active printer is offline or not ready.");
        return false;
    }
    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        ui_toast_show(UI_STATUS_DANGER, "CALIBRATION BLOCKED",
                      "Configuration cannot be saved during a print.");
        return false;
    }
    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_toast_show(UI_STATUS_DANGER, "CALIBRATION BLOCKED",
                      "Clear the printer error before saving calibration.");
        return false;
    }
    return true;
}

static void close_save_confirm_popup(void)
{
    if (!s_context) {
        return;
    }

    if (*s_context->save_confirm_popup) {
        lv_obj_t *popup =
            *s_context->save_confirm_popup;
        *s_context->save_confirm_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_save_confirm_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_save_confirm_popup();
}


void ui_calibration_results_close(void)
{
    if (!s_context) {
        return;
    }

    close_save_confirm_popup();

    if (*s_context->results_popup) {
        lv_obj_t *popup =
            *s_context->results_popup;
        *s_context->results_popup = NULL;
        *s_context->results_label = NULL;
        *s_context->apply_restart_button = NULL;
        lv_obj_delete(popup);
    }
}


static void close_calibration_results_popup_cb(
    lv_event_t *event)
{
    (void)event;
    ui_calibration_results_close();
}


static void run_save_config_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_context || !results_printer_ready()) {
        return;
    }

    calibration_session_snapshot_t snapshot;
    calibration_session_controller_snapshot(
        &snapshot);

    if (!snapshot.completed ||
        !snapshot.save_available ||
        snapshot.status != CALIBRATION_SESSION_RESULTS) {
        close_save_confirm_popup();
        ui_toast_show(
            UI_STATUS_WARNING,
            "SAVE NOT AVAILABLE",
            "Klipper has not reported a completed save-worthy calibration.");
        return;
    }

    static const char command[] = "SAVE_CONFIG";
    console_controller_add_command(command);

    bool sent =
        s_context->send_gcode &&
        s_context->send_gcode(command);

    ui_calibration_results_close();

    if (!sent) {
        ui_toast_show(
            UI_STATUS_DANGER,
            "SAVE FAILED",
            "Moonraker did not accept SAVE_CONFIG.");
        return;
    }

    calibration_session_controller_reset();
    ui_toast_show(
        UI_STATUS_OK,
        "APPLYING CONFIGURATION",
        "Klipper is saving the calibration and restarting. The printer will reconnect automatically.");
}


static void apply_restart_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_context) {
        return;
    }

    calibration_session_snapshot_t snapshot;
    calibration_session_controller_snapshot(
        &snapshot);

    if (!snapshot.completed ||
        !snapshot.save_available) {
        return;
    }

    close_save_confirm_popup();

    *s_context->save_confirm_popup =
        ui_popup_create(
            lv_layer_top(),
            620,
            370,
            UI_POPUP_DANGER);

    if (!*s_context->save_confirm_popup) {
        return;
    }

    ui_popup_add_title(
        *s_context->save_confirm_popup,
        "APPLY CALIBRATION & RESTART?",
        false,
        4);
    ui_popup_add_header_divider(
        *s_context->save_confirm_popup,
        48);
    ui_popup_add_body(
        *s_context->save_confirm_popup,
        "PrinterHMI will run SAVE_CONFIG. Klipper will write the reported calibration values and restart.\\n\\n"
        "The printer will disconnect temporarily. Do not start a print until it reconnects as Ready.",
        28,
        76,
        564);
    ui_popup_add_standard_footer_divider(
        *s_context->save_confirm_popup);
    ui_popup_add_footer_action(
        *s_context->save_confirm_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_save_confirm_popup_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        *s_context->save_confirm_popup,
        UI_POPUP_ACTION_DANGER,
        "APPLY & RESTART",
        210,
        UI_POPUP_FOOTER_RIGHT,
        run_save_config_cb,
        NULL,
        NULL);
}


void ui_calibration_results_show(
    const char *title,
    const char *waiting_text)
{
    if (!s_context) {
        return;
    }

    ui_calibration_results_close();

    *s_context->results_popup =
        ui_popup_create(
            lv_layer_top(),
            680,
            460,
            UI_POPUP_STANDARD);

    if (!*s_context->results_popup) {
        return;
    }

    ui_popup_add_title(
        *s_context->results_popup,
        title ? title : "CALIBRATION RESULTS",
        false,
        4);
    ui_popup_add_header_divider(
        *s_context->results_popup,
        48);
    *s_context->results_label =
        ui_popup_add_body(
            *s_context->results_popup,
            waiting_text ? waiting_text : "Waiting for Klipper...",
            28,
            72,
            624);
    ui_popup_add_standard_footer_divider(
        *s_context->results_popup);
    ui_popup_add_footer_action(
        *s_context->results_popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        150,
        UI_POPUP_FOOTER_LEFT,
        close_calibration_results_popup_cb,
        NULL,
        NULL);
    *s_context->apply_restart_button =
        ui_popup_add_footer_action(
            *s_context->results_popup,
            UI_POPUP_ACTION_DANGER,
            "APPLY & RESTART",
            210,
            UI_POPUP_FOOTER_RIGHT,
            apply_restart_button_cb,
            NULL,
            NULL);

    if (*s_context->apply_restart_button) {
        lv_obj_add_flag(
            *s_context->apply_restart_button,
            LV_OBJ_FLAG_HIDDEN);
    }
}


void ui_calibration_results_refresh(void)
{
    calibration_session_controller_poll();

    if (!s_context ||
        !*s_context->results_label) {
        return;
    }

    calibration_session_snapshot_t *snapshot =
        s_context->session_snapshot;
    calibration_session_controller_snapshot(
        snapshot);

    if (snapshot->generation ==
        *s_context->session_generation) {
        return;
    }

    *s_context->session_generation =
        snapshot->generation;

    if (snapshot->status == CALIBRATION_SESSION_ERROR) {
        lv_snprintf(
            s_context->display,
            s_context->display_size,
            "Calibration failed:\\n\\n%s",
            snapshot->results[0]
                ? snapshot->results
                : "Unknown Klipper error.");
        lv_label_set_text(
            *s_context->results_label,
            s_context->display);
    } else if (snapshot->status ==
                   CALIBRATION_SESSION_RESULTS &&
               snapshot->results[0]) {
        lv_snprintf(
            s_context->display,
            s_context->display_size,
            "%s%s",
            snapshot->results,
            snapshot->save_available
                ? "\\n\\nCalibration complete. Review the result, then Apply & Restart to save it."
                : "");
        lv_label_set_text(
            *s_context->results_label,
            s_context->display);
    }

    if (*s_context->apply_restart_button) {
        if (snapshot->status ==
                CALIBRATION_SESSION_RESULTS &&
            snapshot->completed &&
            snapshot->save_available) {
            lv_obj_clear_flag(
                *s_context->apply_restart_button,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(
                *s_context->apply_restart_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }
}
