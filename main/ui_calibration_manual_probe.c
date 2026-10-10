#include "ui_calibration_manual_probe.h"
#include "ui_text.h"

#include "ui_popup.h"
#include "ui_calibration_dialog.h"

typedef struct {
    lv_obj_t *popup;
    lv_obj_t *status_label;
} ui_calibration_manual_probe_state_t;

static ui_calibration_manual_probe_state_t s_probe;


void ui_calibration_manual_probe_hide(void)
{
    if (!s_probe.popup) {
        return;
    }

    lv_obj_t *popup = s_probe.popup;
    s_probe.popup = NULL;
    s_probe.status_label = NULL;
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
    if (!step_cb || !abort_cb || !accept_cb) {
        return false;
    }

    ui_calibration_manual_probe_hide();

    s_probe.popup =
        ui_cal_dialog_create(lv_layer_top(), 700, 480, UI_POPUP_STANDARD);

    if (!s_probe.popup) {
        return false;
    }

    ui_cal_dialog_title(s_probe.popup, title ? title : ui_text("MANUAL PROBE"));

    s_probe.status_label =
        ui_cal_dialog_text(ui_cal_dialog_body(s_probe.popup), instructions
                ? instructions
                : "Adjust the nozzle height, then accept the point.");

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
        ui_cal_dialog_choice(s_probe.popup, UI_POPUP_ACTION_CHOICE, labels[index], 120, step_cb, (void *)commands[index], NULL);
    }


    ui_cal_dialog_action(s_probe.popup, UI_POPUP_ACTION_DANGER, "ABORT", abort_cb, NULL, NULL);
    ui_cal_dialog_action(s_probe.popup, UI_POPUP_ACTION_CONFIRM, accept_label ? accept_label : "ACCEPT", accept_cb, NULL, NULL);

    return true;
}


void ui_calibration_manual_probe_set_status(
    const char *status)
{
    if (s_probe.status_label && status) {
        lv_label_set_text(
            s_probe.status_label,
            status);
    }
}


bool ui_calibration_manual_probe_is_visible(void)
{
    return s_probe.popup != NULL;
}
