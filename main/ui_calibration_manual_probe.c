#include "ui_calibration_manual_probe.h"
#include "ui_text.h"

#include "ui_popup.h"
#include "ui_calibration_dialog.h"
#include "ui_value_update.h"

typedef struct {
    lv_obj_t *popup;
    lv_obj_t *status_label;
    uint32_t owner;
    lv_event_cb_t step_cb;
    lv_event_cb_t abort_cb;
    lv_event_cb_t accept_cb;
} ui_calibration_manual_probe_state_t;

static ui_calibration_manual_probe_state_t s_probe;
/* A callback can retire/reopen the dialog; keep dispatch ownership separate. */
static bool s_probe_dispatching;

static void manual_probe_retire(void)
{
    s_probe = (ui_calibration_manual_probe_state_t){0};
}

static void manual_probe_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_probe.popup) manual_probe_retire();
}

static void manual_probe_dispatch(lv_event_t *event, lv_event_cb_t callback)
{
    if (!event || !s_probe.popup || !callback || s_probe_dispatching ||
        s_probe.owner != moonraker_config_generation()) return;
    s_probe_dispatching = true;
    callback(event); /* Preserve original event and TESTZ command user data. */
    s_probe_dispatching = false;
}

static void manual_probe_step(lv_event_t *event)
{
    manual_probe_dispatch(event, s_probe.step_cb);
}

static void manual_probe_abort(lv_event_t *event)
{
    manual_probe_dispatch(event, s_probe.abort_cb);
}

static void manual_probe_accept(lv_event_t *event)
{
    manual_probe_dispatch(event, s_probe.accept_cb);
}


void ui_calibration_manual_probe_hide(void)
{
    if (!s_probe.popup) {
        return;
    }

    lv_obj_t *popup = s_probe.popup;
    manual_probe_retire();
    lv_obj_delete(popup);
}


bool ui_calibration_manual_probe_show(
    const char *title,
    const char *instructions,
    const char *accept_label,
    lv_event_cb_t step_cb,
    lv_event_cb_t abort_cb,
    lv_event_cb_t accept_cb)
{
    if (!step_cb || !abort_cb || !accept_cb || s_probe_dispatching) {
        return false;
    }

    if (s_probe.popup && s_probe.owner == moonraker_config_generation() &&
        s_probe.step_cb == step_cb && s_probe.abort_cb == abort_cb &&
        s_probe.accept_cb == accept_cb) {
        lv_obj_move_foreground(s_probe.popup);
        return true;
    }
    ui_calibration_manual_probe_hide();

    s_probe.popup =
        ui_cal_dialog_create(lv_layer_top(), 700, 480, UI_POPUP_STANDARD);

    if (!s_probe.popup) {
        return false;
    }

    s_probe.owner = moonraker_config_generation();
    s_probe.step_cb = step_cb;
    s_probe.abort_cb = abort_cb;
    s_probe.accept_cb = accept_cb;
    lv_obj_add_event_cb(s_probe.popup, manual_probe_deleted, LV_EVENT_DELETE, NULL);
    ui_cal_dialog_title_divided(s_probe.popup, title ? title : ui_text("Manual probe"));

    s_probe.status_label =
        ui_cal_dialog_text(ui_cal_dialog_body(s_probe.popup), instructions
                ? instructions
                : "Adjust the nozzle height, then accept the point.");

    lv_obj_set_style_text_line_space(s_probe.status_label,4,0);
    ui_cal_dialog_caption(ui_cal_dialog_body(s_probe.popup),
        "Nozzle movement (mm): negative lowers, positive raises.");

    /* Shared by Probe/Z and Axis Twist: coarse travel first, then
     * 0.01 mm or 0.005 mm TESTZ steps for the final paper-contact pass. */
    static const char *labels[] = {
        "-1.00", "-0.10", "-0.05", "-0.01", "-0.005",
        "+0.005", "+0.01", "+0.05", "+0.10", "+1.00",
    };
    static const char *commands[] = {
        "TESTZ Z=-1.0",
        "TESTZ Z=-0.1",
        "TESTZ Z=-0.05",
        "TESTZ Z=-0.01",
        "TESTZ Z=-0.005",
        "TESTZ Z=0.005",
        "TESTZ Z=0.01",
        "TESTZ Z=0.05",
        "TESTZ Z=0.1",
        "TESTZ Z=1.0",
    };

    for (size_t index = 0;
         index < sizeof(labels) / sizeof(labels[0]);
         ++index) {
        ui_cal_dialog_choice(s_probe.popup, UI_POPUP_ACTION_CHOICE, labels[index], 120, manual_probe_step, (void *)commands[index], NULL);
    }


    ui_cal_dialog_action(s_probe.popup, UI_POPUP_ACTION_DANGER, "Abort", manual_probe_abort, NULL, NULL);
    ui_cal_dialog_action(s_probe.popup, UI_POPUP_ACTION_CONFIRM, accept_label ? accept_label : "Accept", manual_probe_accept, NULL, NULL);

    return true;
}


void ui_calibration_manual_probe_set_status(
    const char *status)
{
    if (s_probe.popup && s_probe.status_label && status &&
        s_probe.owner == moonraker_config_generation()) {
        ui_value_set_text(
            s_probe.status_label,
            status);
    }
}


bool ui_calibration_manual_probe_is_visible(void)
{
    return s_probe.popup != NULL;
}
