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
#include "ui_theme.h"
#include "ui_toast.h"

static lv_obj_t *s_button, *s_popup, *s_samples;
static ui_probe_accuracy_send_cb_t s_send;
static ui_probe_accuracy_ready_cb_t s_ready;
static uint32_t s_owner;

static void close_popup(void)
{
    if (s_popup) lv_obj_delete(s_popup);
    s_popup = s_samples = NULL;
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
    unsigned selected = lv_roller_get_selected(s_samples);
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

static void open_cb(lv_event_t *event)
{
    (void)event;
    if (!s_ready || !s_ready("Probe accuracy")) return;
    close_popup();
    s_owner = moonraker_config_generation();
    s_popup = ui_popup_create(lv_layer_top(), 660, 430, UI_POPUP_STANDARD);
    if (!s_popup) return;
    ui_popup_add_title(s_popup, "CHECK PROBE ACCURACY?", false, 4);
    ui_popup_add_header_divider(s_popup, 48);
    ui_popup_add_body(s_popup,
        "Runs at the CURRENT probe position.\n"
        "Home XYZ and position the probe over a clear bed area first.\n"
        "The probe will repeat and retract. Nothing is saved.", 28, 68, 604);
    ui_popup_add_body(s_popup, "SAMPLES", 28, 238, 160);
    s_samples = lv_roller_create(s_popup);
    lv_roller_set_options(s_samples, "5\n10\n20", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(s_samples, 3);
    lv_roller_set_selected(s_samples, 1, LV_ANIM_OFF);
    lv_obj_set_size(s_samples, 120, 100);
    lv_obj_set_pos(s_samples, 220, 236);
    ui_apply_surface_role(s_samples, UI_SURFACE_SECTION);
    ui_popup_add_standard_footer_divider(s_popup);
    ui_popup_add_footer_action(s_popup, UI_POPUP_ACTION_CANCEL, "BACK", 170,
        UI_POPUP_FOOTER_LEFT, close_cb, NULL, NULL);
    ui_popup_add_footer_action(s_popup, UI_POPUP_ACTION_CONFIRM, "RUN CHECK", 180,
        UI_POPUP_FOOTER_RIGHT, run_cb, NULL, NULL);
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
