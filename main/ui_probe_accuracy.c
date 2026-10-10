#include "ui_probe_accuracy.h"
#include <stdio.h>
#include <string.h>
#include "calibration_session_controller.h"
#include "console_controller.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "ui_button.h"
#include "ui_calibration_results.h"
#include "ui_popup.h"
#include "ui_calibration_dialog.h"
#include "ui_theme.h"
#include "ui_toast.h"
#include "ui_value_update.h"

static lv_obj_t *s_button, *s_popup, *s_samples, *s_status, *s_run;
static lv_timer_t *s_status_timer;
static ui_probe_accuracy_send_cb_t s_send;
static ui_probe_accuracy_ready_cb_t s_ready;
static uint32_t s_owner;

static void close_popup(void)
{
    if (s_status_timer) lv_timer_delete(s_status_timer);
    s_status_timer = NULL;
    if (s_popup) lv_obj_delete(s_popup);
    s_popup = s_samples = s_status = s_run = NULL;
}
static void close_cb(lv_event_t *event) { (void)event; close_popup(); }

static void run_cb(lv_event_t *event)
{
    (void)event;
    if (s_owner != moonraker_config_generation()) {
        ui_toast_show(UI_STATUS_DANGER, "PROBE CHECK BLOCKED", "Printer changed. Reopen the probe check.");
        return;
    }
    if (!s_ready || !s_ready("Probe accuracy")) return;
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    if (!strchr(state.homed_axes, 'x') || !strchr(state.homed_axes, 'y') || !strchr(state.homed_axes, 'z')) {
        ui_toast_show(UI_STATUS_DANGER, "HOME XYZ FIRST", "Home and position the probe over the bed using Printer controls.");
        return;
    }
    static const unsigned samples[] = {5, 10, 20};
    unsigned selected = lv_dropdown_get_selected(s_samples);
    if (selected >= 3) return;
    char command[48];
    snprintf(command, sizeof(command), "PROBE_ACCURACY SAMPLES=%u", samples[selected]);
    calibration_session_controller_begin(CALIBRATION_SESSION_PROBE_ACCURACY,
        console_controller_latest_sequence());
    console_controller_add_command(command);
    bool sent = s_send && s_send(command);
    close_popup();
    if (!sent) calibration_session_controller_mark_error("Moonraker did not accept the probe accuracy check.");
    ui_calibration_results_show("PROBE ACCURACY RESULTS", "Waiting for repeated probe measurements...");
    ui_calibration_results_refresh();
}

static void refresh_status(lv_timer_t *timer)
{
    (void)timer;
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    bool same = s_owner == moonraker_config_generation();
    bool live = state.moonraker_ok && state.live_data_ok;
    bool idle = strcmp(state.printer_state, "printing") && strcmp(state.printer_state, "paused") &&
        strcmp(state.printer_state, "error") && strcmp(state.printer_state, "shutdown");
    bool homed = strchr(state.homed_axes, 'x') && strchr(state.homed_axes, 'y') && strchr(state.homed_axes, 'z');
    const char *text = !same ? "Printer changed. Close and reopen the probe check." :
        !live ? "Printer offline. Homing status unavailable." :
        !idle ? "Probe checks require an idle, ready printer." :
        !homed ? "Home XYZ first, then position the probe over a clear bed area." :
        "XYZ homed. Verify the CURRENT probe position is over a clear bed area.";
    ui_value_set_text(s_status, text);
    if (same && live && idle && homed) lv_obj_remove_state(s_run, LV_STATE_DISABLED);
    else lv_obj_add_state(s_run, LV_STATE_DISABLED);
}

static void open_cb(lv_event_t *event)
{
    (void)event;
    if (!s_ready || !s_ready("Probe accuracy")) return;
    close_popup();
    s_owner = moonraker_config_generation();
    s_popup = ui_cal_dialog_create(lv_layer_top(), 660, 430, UI_POPUP_STANDARD);
    if (!s_popup) return;
    ui_cal_dialog_title(s_popup, "CHECK PROBE ACCURACY?");

    ui_cal_dialog_text(ui_cal_dialog_body(s_popup), "Runs at the CURRENT probe position.\n"
        "Home XYZ and position the probe over a clear bed area first.\n"
        "The probe will repeat and retract. Nothing is saved.");
    ui_cal_dialog_text(ui_cal_dialog_body(s_popup), "SAMPLES");
    s_samples = lv_dropdown_create(ui_cal_dialog_body(s_popup));
    lv_dropdown_set_options(s_samples, "5\n10\n20");
    lv_dropdown_set_selected(s_samples, 1);
    lv_obj_set_size(s_samples, LV_PCT(100), 54);
    ui_apply_surface_role(s_samples, UI_SURFACE_TEXT_INPUT);
    lv_obj_set_style_text_font(s_samples, ui_font_with_fallback(UI_FONT_BODY), 0);
    lv_obj_t *sample_list = lv_dropdown_get_list(s_samples);
    ui_apply_surface_role(sample_list, UI_SURFACE_SECTION);
    lv_obj_set_style_text_font(sample_list, ui_font_with_fallback(UI_FONT_BODY), 0);
    s_status = ui_cal_dialog_text(ui_cal_dialog_body(s_popup), "Waiting for homing status...");
    ui_cal_dialog_action(s_popup, UI_POPUP_ACTION_CANCEL, "BACK", close_cb, NULL, NULL);
    s_run = ui_cal_dialog_action(s_popup, UI_POPUP_ACTION_CONFIRM, "RUN CHECK", run_cb, NULL, NULL);
    s_status_timer = lv_timer_create(refresh_status, 500, NULL);
    refresh_status(NULL);
}

void ui_probe_accuracy_create(lv_obj_t *card, ui_probe_accuracy_send_cb_t send,
                             ui_probe_accuracy_ready_cb_t ready)
{
    s_send = send; s_ready = ready;
    s_button = ui_button_create(card, UI_BUTTON_OUTLINED, "ACCURACY");
    if (!s_button) return;
    lv_obj_set_size(s_button, 170, 44);
    lv_obj_align(s_button, LV_ALIGN_BOTTOM_RIGHT, -16, -62);
    lv_obj_add_event_cb(s_button, open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_button, LV_OBJ_FLAG_HIDDEN);
}
void ui_probe_accuracy_refresh(bool available)
{
    if (!s_button) return;
    if (available) lv_obj_clear_flag(s_button, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_button, LV_OBJ_FLAG_HIDDEN);
}
void ui_probe_accuracy_hide(void)
{
    close_popup(); s_button = NULL; s_send = NULL; s_ready = NULL;
}
