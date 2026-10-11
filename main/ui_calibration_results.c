#include "ui_calibration_results.h"
#include "ui_text.h"

#include <string.h>

#include "console_controller.h"
#include "moonraker.h"
#include "ui_popup.h"
#include "ui_calibration_dialog.h"
#include "ui_value_update.h"

static ui_calibration_results_context_t *s_context;
static uint32_t s_results_owner, s_results_epoch, s_save_generation;
static bool s_save_dispatching, s_save_requested, s_results_rendered, s_save_blocked;
static lv_obj_t *s_results_feedback, *s_save_feedback, *s_save_action;

void ui_calibration_results_init(ui_calibration_results_context_t *context)
{
    s_context = context;
}

static const char *results_unavailable_reason(void)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    if (!state.moonraker_ok || !state.live_data_ok)
        return "The active printer is offline or not ready. Reconnect before saving.";
    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0)
        return "Configuration cannot be saved during a print.";
    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0)
        return "Clear the printer error before saving calibration.";
    return NULL;
}

static void retire_save_confirm(void)
{
    ++s_results_epoch;
    s_save_feedback = NULL;
    s_save_blocked = false;
    s_save_action = NULL;
    if (s_context) *s_context->save_confirm_popup = NULL;
}

static void close_save_confirm_popup(void)
{
    if (!s_context || !*s_context->save_confirm_popup) return;
    lv_obj_t *popup = *s_context->save_confirm_popup;
    retire_save_confirm();
    lv_obj_delete(popup);
}

static void save_confirm_deleted(lv_event_t *event)
{
    if (s_context && lv_event_get_target_obj(event) == *s_context->save_confirm_popup)
        retire_save_confirm();
}

static void retire_results(void)
{
    ++s_results_epoch;
    s_results_feedback = NULL;
    s_results_rendered = false;
    s_save_requested = false;
    if (s_context) {
        *s_context->results_popup = NULL;
        *s_context->results_label = NULL;
        *s_context->apply_restart_button = NULL;
    }
    close_save_confirm_popup();
}

static void results_deleted(lv_event_t *event)
{
    if (s_context && lv_event_get_target_obj(event) == *s_context->results_popup)
        retire_results();
}

static void close_save_confirm_popup_cb(lv_event_t *event)
{
    (void)event;
    close_save_confirm_popup();
}

void ui_calibration_results_close(void)
{
    if (!s_context) return;
    lv_obj_t *popup = *s_context->results_popup;
    retire_results();
    if (popup) lv_obj_delete(popup);
}

static void close_calibration_results_popup_cb(lv_event_t *event)
{
    (void)event;
    ui_calibration_results_close();
}

static bool save_worthy(const calibration_session_snapshot_t *snapshot)
{
    return snapshot->completed && snapshot->save_available &&
           snapshot->status == CALIBRATION_SESSION_RESULTS;
}

static void run_save_config_cb(lv_event_t *event)
{
    (void)event;
    if (!s_context || !*s_context->results_popup ||
        !*s_context->save_confirm_popup || s_save_dispatching || s_save_requested ||
        s_results_owner != moonraker_config_generation()) return;

    const char *reason = results_unavailable_reason();
    if (reason) {
        s_save_blocked = true;
        ui_value_set_text(s_save_feedback, reason);
        return;
    }
    calibration_session_snapshot_t snapshot;
    calibration_session_controller_snapshot(&snapshot);
    if (!save_worthy(&snapshot) || snapshot.generation != s_save_generation) {
        s_save_blocked = true;
        ui_value_set_text(s_save_feedback,
            "Calibration results changed. Go Back and review them before saving.");
        return;
    }

    ui_calibration_results_context_t *context = s_context;
    lv_obj_t *popup = *context->save_confirm_popup;
    uint32_t epoch = s_results_epoch;
    uint32_t owner = s_results_owner;
    static const char command[] = "SAVE_CONFIG";
    console_controller_add_command(command);
    s_save_dispatching = true;
    bool sent = context->send_gcode && context->send_gcode(command);
    s_save_dispatching = false;
    /* Transport callbacks may navigate or change the active printer. */
    if (s_context != context || s_results_epoch != epoch ||
        *context->save_confirm_popup != popup || owner != moonraker_config_generation()) return;
    if (!sent) {
        ui_value_set_text(s_save_feedback,
            "SAVE_CONFIG was not accepted. Your results are retained. Retry when connected.");
        return;
    }

    calibration_session_snapshot_t current;
    calibration_session_controller_snapshot(&current);
    if (current.generation != s_save_generation) return;
    s_save_requested = true;
    lv_obj_t *footer = lv_obj_get_child(popup, 2);
    for (uint32_t i = 0; i < lv_obj_get_child_count(footer); ++i) {
        lv_obj_add_state(lv_obj_get_child(footer, i), LV_STATE_DISABLED);
        lv_obj_add_flag(lv_obj_get_child(footer, i), LV_OBJ_FLAG_HIDDEN);
    }
    ui_cal_dialog_action(popup, UI_POPUP_ACTION_CLOSE,
                         "CLOSE", close_save_confirm_popup_cb, NULL, NULL);
    if (*context->apply_restart_button)
        lv_obj_add_flag(*context->apply_restart_button, LV_OBJ_FLAG_HIDDEN);
    calibration_session_controller_reset();
    ui_cal_dialog_title(popup, ui_text("CONFIGURATION SAVE REQUESTED"));
    ui_value_set_text(s_save_feedback,
        "SAVE_CONFIG was accepted. Klipper is expected to save and restart. Wait for Ready before starting a print.");
    ui_value_set_text(s_results_feedback,
        "Configuration save requested. Wait for the printer to reconnect as Ready.");
}

static void apply_restart_button_cb(lv_event_t *event)
{
    (void)event;
    if (!s_context || !*s_context->results_popup || s_save_requested ||
        s_save_dispatching || s_results_owner != moonraker_config_generation()) return;
    if (*s_context->save_confirm_popup) {
        lv_obj_move_foreground(*s_context->save_confirm_popup);
        return;
    }
    const char *reason = results_unavailable_reason();
    if (reason) {
        ui_value_set_text(s_results_feedback, reason);
        return;
    }
    calibration_session_snapshot_t snapshot;
    calibration_session_controller_snapshot(&snapshot);
    if (!save_worthy(&snapshot)) return;
    if (!s_results_rendered || snapshot.generation != *s_context->session_generation) {
        ui_calibration_results_refresh();
        ui_value_set_text(s_results_feedback,
            "Results updated. Review the latest values, then Apply & Restart.");
        return;
    }
    s_save_generation = snapshot.generation;

    *s_context->save_confirm_popup =
        ui_cal_dialog_create(lv_layer_top(), 620, 370, UI_POPUP_DANGER);
    if (!*s_context->save_confirm_popup) return;
    lv_obj_add_event_cb(*s_context->save_confirm_popup, save_confirm_deleted, LV_EVENT_DELETE, NULL);
    ui_cal_dialog_title(*s_context->save_confirm_popup, ui_text("APPLY CALIBRATION & RESTART?"));
    ui_cal_dialog_text(ui_cal_dialog_body(*s_context->save_confirm_popup),
        "PrinterHMI will run SAVE_CONFIG. Klipper will write the reported calibration values and restart.\n\n"
        "The printer will disconnect temporarily. Do not start a print until it reconnects as Ready.");
    s_save_feedback = ui_cal_dialog_text(ui_cal_dialog_body(*s_context->save_confirm_popup), "");
    ui_cal_dialog_action(*s_context->save_confirm_popup, UI_POPUP_ACTION_CANCEL,
                        LV_SYMBOL_LEFT " BACK", close_save_confirm_popup_cb, NULL, NULL);
    s_save_action = ui_cal_dialog_action(*s_context->save_confirm_popup, UI_POPUP_ACTION_DANGER,
                                        "APPLY & RESTART", run_save_config_cb, NULL, NULL);
}

void ui_calibration_results_show(const char *title, const char *waiting_text)
{
    if (!s_context || s_save_dispatching) return;
    ui_calibration_results_close();
    s_results_owner = moonraker_config_generation();
    *s_context->results_popup =
        ui_cal_dialog_create(lv_layer_top(), 680, 460, UI_POPUP_STANDARD);
    if (!*s_context->results_popup) return;
    lv_obj_add_event_cb(*s_context->results_popup, results_deleted, LV_EVENT_DELETE, NULL);
    ui_cal_dialog_title(*s_context->results_popup, title ? title : ui_text("CALIBRATION RESULTS"));
    *s_context->results_label = ui_cal_dialog_text(ui_cal_dialog_body(*s_context->results_popup),
        waiting_text ? waiting_text : "Waiting for Klipper...");
    s_results_feedback = ui_cal_dialog_text(ui_cal_dialog_body(*s_context->results_popup), "");
    ui_cal_dialog_action(*s_context->results_popup, UI_POPUP_ACTION_CLOSE,
                        "CLOSE", close_calibration_results_popup_cb, NULL, NULL);
    *s_context->apply_restart_button =
        ui_cal_dialog_action(*s_context->results_popup, UI_POPUP_ACTION_DANGER,
                             "APPLY & RESTART", apply_restart_button_cb, NULL, NULL);
    if (*s_context->apply_restart_button)
        lv_obj_add_flag(*s_context->apply_restart_button, LV_OBJ_FLAG_HIDDEN);
}

void ui_calibration_results_refresh(void)
{
    calibration_session_controller_poll();
    if (!s_context || !*s_context->results_popup || !*s_context->results_label) return;
    if (s_results_owner != moonraker_config_generation()) {
        close_save_confirm_popup();
        ui_value_set_text(s_results_feedback,
            "Printer changed. Close and reopen calibration for the selected printer.");
        if (*s_context->apply_restart_button)
            lv_obj_add_flag(*s_context->apply_restart_button, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (s_save_requested) return;

    calibration_session_snapshot_t *snapshot = s_context->session_snapshot;
    calibration_session_controller_snapshot(snapshot);
    const char *reason = results_unavailable_reason();
    ui_value_set_text(s_results_feedback, reason);
    if (*s_context->apply_restart_button) {
        if (save_worthy(snapshot)) {
            lv_obj_clear_flag(*s_context->apply_restart_button, LV_OBJ_FLAG_HIDDEN);
            if (reason) lv_obj_add_state(*s_context->apply_restart_button, LV_STATE_DISABLED);
            else lv_obj_remove_state(*s_context->apply_restart_button, LV_STATE_DISABLED);
        } else lv_obj_add_flag(*s_context->apply_restart_button, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_save_action && *s_context->save_confirm_popup) {
        bool changed = !save_worthy(snapshot) || snapshot->generation != s_save_generation;
        if (reason || changed) {
            s_save_blocked = true;
            lv_obj_add_state(s_save_action, LV_STATE_DISABLED);
            ui_value_set_text(s_save_feedback, reason ? reason :
                "Calibration results changed. Go Back and review them before saving.");
        } else {
            lv_obj_remove_state(s_save_action, LV_STATE_DISABLED);
            if (s_save_blocked) ui_value_set_text(s_save_feedback, "");
            s_save_blocked = false;
        }
    }
    if (s_results_rendered && snapshot->generation == *s_context->session_generation) return;
    s_results_rendered = true;
    *s_context->session_generation = snapshot->generation;
    if (snapshot->status == CALIBRATION_SESSION_ERROR) {
        lv_snprintf(s_context->display, s_context->display_size, "Calibration failed:\n\n%s",
                    snapshot->results[0] ? snapshot->results : "Unknown Klipper error.");
        ui_value_set_text(*s_context->results_label, s_context->display);
    } else if (snapshot->status == CALIBRATION_SESSION_RESULTS && snapshot->results[0]) {
        lv_snprintf(s_context->display, s_context->display_size, "%s%s", snapshot->results,
            snapshot->save_available ?
            "\n\nCalibration complete. Review the result, then Apply & Restart to save it." : "");
        ui_value_set_text(*s_context->results_label, s_context->display);
    }
}
