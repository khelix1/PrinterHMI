#include "ui_calibration_motion.h"
#include "ui_motion_diagnostics.h"
#include "ui_calibration_layout.h"
#include "ui_text.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "calibration_session_controller.h"
#include "console_controller.h"

#include "ui_button.h"
#include "ui_popup.h"
#include "ui_calibration_dialog.h"
#include "ui_theme.h"
#include "ui_toast.h"
#include "ui_text_fit.h"
#include "ui_selector_style.h"
#include "ui_value_update.h"

typedef struct {
    lv_obj_t *input_shaper_button;
    lv_obj_t *input_shaper_popup;
    lv_obj_t *resonance_test_button;
    lv_obj_t *resonance_test_popup;
    lv_obj_t *accelerometer_check_button;
    lv_obj_t *accelerometer_check_popup;

    ui_calibration_motion_send_gcode_cb_t send_gcode;
    ui_calibration_motion_ready_cb_t ready;
    ui_calibration_motion_show_results_cb_t show_results;
    ui_calibration_motion_refresh_results_cb_t refresh_results;

    bool discovered;
    bool input_shaper;
    bool shaper_calibrate;
    bool accelerometer;
} ui_calibration_motion_state_t;

static ui_calibration_motion_state_t s_motion;
static uint32_t s_motion_epoch;
static bool s_motion_dispatching;
static lv_obj_t *s_shaper_axis, *s_shaper_run, *s_sensor_run, *s_resonance_run[2];
static lv_obj_t *s_shaper_feedback, *s_resonance_feedback, *s_sensor_feedback;

static void motion_deleted(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == s_shaper_feedback) s_shaper_feedback = NULL;
    if (target == s_resonance_feedback) s_resonance_feedback = NULL;
    if (target == s_sensor_feedback) s_sensor_feedback = NULL;
    if (target == s_sensor_run || target == s_motion.accelerometer_check_popup) s_sensor_run = NULL;
    for (unsigned i=0;i<2;i++)
        if (target == s_resonance_run[i] || target == s_motion.resonance_test_popup) s_resonance_run[i] = NULL;
    if (target == s_shaper_axis) s_shaper_axis = NULL;
    if (target == s_shaper_run) s_shaper_run = NULL;
    if (target == s_motion.input_shaper_popup) s_shaper_axis = s_shaper_run = NULL;
    lv_obj_t **slots[] = {&s_motion.input_shaper_popup, &s_motion.resonance_test_popup,
                         &s_motion.accelerometer_check_popup};
    lv_obj_t **feedback[] = {&s_shaper_feedback, &s_resonance_feedback, &s_sensor_feedback};
    for (unsigned i = 0; i < 3; ++i) if (target == *slots[i]) {
        ++s_motion_epoch;
        *slots[i] = NULL;
        *feedback[i] = NULL;
    }
    lv_obj_t **buttons[] = {&s_motion.input_shaper_button, &s_motion.resonance_test_button,
                           &s_motion.accelerometer_check_button};
    for (unsigned i = 0; i < 3; ++i) if (target == *buttons[i]) *buttons[i] = NULL;
}

static void motion_register(lv_obj_t *popup)
{
    if (popup) lv_obj_add_event_cb(popup, motion_deleted, LV_EVENT_DELETE, NULL);
}


static void set_visible(
    lv_obj_t *object,
    bool visible)
{
    if (!object) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(
            object,
            LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(
            object,
            LV_OBJ_FLAG_HIDDEN);
    }
}


static bool motion_ready(
    const char *workflow)
{
    return s_motion.ready &&
        s_motion.ready(workflow);
}


static bool motion_action(lv_event_t *event, lv_obj_t *button)
{
    return button && !lv_obj_has_state(button, LV_STATE_DISABLED) &&
        (!event || (lv_event_get_code(event) == LV_EVENT_CLICKED &&
        lv_event_get_target_obj(event) == button &&
        lv_event_get_current_target_obj(event) == button));
}

static bool motion_send(lv_obj_t **slot, lv_obj_t **feedback,
    const char *workflow, const char *command, calibration_session_kind_t kind)
{
    if (!*slot || s_motion_dispatching) return false;
    lv_obj_t *popup = *slot;
    uint32_t owner = (uint32_t)(uintptr_t)lv_obj_get_user_data(popup);
    if (owner != moonraker_config_generation()) return false;
    bool object_stack=s_motion.discovered && s_motion.input_shaper && s_motion.accelerometer;
    bool supported=kind == CALIBRATION_SESSION_INPUT_SHAPER
        ? (s_motion.shaper_calibrate || object_stack)
        : kind == CALIBRATION_SESSION_ACCELEROMETER_CHECK
            ? (s_motion.discovered && s_motion.accelerometer) : object_stack;
    if (!supported) {
        ui_value_set_text(*feedback, "Required measurement capabilities are no longer available.");
        return false;
    }
    uint32_t epoch = s_motion_epoch;
    s_motion_dispatching = true;
    bool sent = false;
    if (!motion_ready(workflow) || s_motion_epoch != epoch || *slot != popup ||
        owner != moonraker_config_generation()) goto finished;
    calibration_session_controller_begin(kind, console_controller_latest_sequence());
    console_controller_add_command(command);
    if (s_motion_epoch != epoch || *slot != popup ||
        owner != moonraker_config_generation()) goto finished;
    sent = s_motion.send_gcode && s_motion.send_gcode(command);
    s_motion_dispatching = false;
    if (s_motion_epoch != epoch || *slot != popup || owner != moonraker_config_generation()) return false;
    if (!sent) {
        calibration_session_controller_mark_error("Moonraker did not accept the measurement command.");
        ui_value_set_text(*feedback, "Command was not accepted. Your choices are retained. Retry when connected.");
        return false;
    }
    return true;
finished:
    s_motion_dispatching = false;
    return false;
}

static void show_results(
    const char *title,
    const char *waiting_text)
{
    if (s_motion.show_results) {
        s_motion.show_results(
            title,
            waiting_text);
    }

    if (s_motion.refresh_results) {
        s_motion.refresh_results();
    }
}


static void close_input_shaper_popup(void)
{
    if (s_motion.input_shaper_popup) {
        lv_obj_t *popup =
            s_motion.input_shaper_popup;
        ++s_motion_epoch;
        s_motion.input_shaper_popup = NULL;
        s_shaper_feedback = s_shaper_axis = s_shaper_run = NULL;
        lv_obj_delete(popup);
    }
}


static void close_input_shaper_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_input_shaper_popup();
}


static void run_input_shaper_cb(lv_event_t *event)
{
    if (!s_motion.input_shaper_popup || !s_shaper_axis ||
        !motion_action(event,s_shaper_run)) return;

    static const char *const commands[] = {
        "SHAPER_CALIBRATE", "SHAPER_CALIBRATE AXIS=X", "SHAPER_CALIBRATE AXIS=Y"
    };
    static const char *const axes[] = {"X and Y", "X-axis", "Y-axis"};
    unsigned axis = lv_dropdown_get_selected(s_shaper_axis);
    if (axis >= sizeof(commands)/sizeof(commands[0])) return;
    if (!motion_send(&s_motion.input_shaper_popup, &s_shaper_feedback,
            "Input Shaper calibration", commands[axis], CALIBRATION_SESSION_INPUT_SHAPER)) return;
    close_input_shaper_popup();

    char waiting[240];
    snprintf(waiting, sizeof(waiting),
        "Klipper is measuring %s resonance. Keep the machine attended and do not touch it.\n\n"
        "Apply & Restart will appear only after Klipper reports a successful result requiring SAVE_CONFIG.",
        axes[axis]);
    show_results("Input Shaper calibration", waiting);
}


static void input_shaper_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (s_motion_dispatching) return;
    if (s_motion.input_shaper_popup) {
        lv_obj_move_foreground(s_motion.input_shaper_popup);
        return;
    }
    if (!motion_ready(
            "Input Shaper calibration")) {
        return;
    }

    if (!s_motion.shaper_calibrate &&
        !(s_motion.discovered && s_motion.input_shaper && s_motion.accelerometer)) {
        ui_cal_dialog_notice(
            UI_STATUS_WARNING,
            "Input Shaper unavailable",
            "This printer has not reported Input Shaper calibration support.");
        return;
    }

    close_input_shaper_popup();
    s_motion.input_shaper_popup =
        ui_cal_dialog_create(lv_layer_top(), 650, 460, UI_POPUP_STANDARD);

    if (!s_motion.input_shaper_popup) {
        return;
    }

    motion_register(s_motion.input_shaper_popup);
    ui_cal_dialog_title_divided(s_motion.input_shaper_popup, ui_text("Run input shaper calibration?"));

    ui_cal_dialog_text(ui_cal_dialog_body(s_motion.input_shaper_popup), "Klipper will move and vibrate the toolhead on the selected axes. The machine may be loud.");
    ui_cal_dialog_caption(ui_cal_dialog_body(s_motion.input_shaper_popup),
        "Clear the bed and motion area. Keep objects away from the printer and keep it attended.");

    lv_obj_t *body = ui_cal_dialog_body(s_motion.input_shaper_popup);
    ui_cal_dialog_caption(body, ui_text("Axes to calibrate"));
    s_shaper_axis = lv_dropdown_create(body);
    lv_obj_set_size(s_shaper_axis, LV_PCT(100), 54);
    ui_selector_style(s_shaper_axis, UI_FONT_BODY);
    lv_dropdown_set_options(s_shaper_axis, "Both (X and Y)\nX only\nY only");
    motion_register(s_shaper_axis);

    s_shaper_feedback = ui_cal_dialog_caption(ui_cal_dialog_body(s_motion.input_shaper_popup), "");
    motion_register(s_shaper_feedback);
    ui_cal_dialog_action(s_motion.input_shaper_popup, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_LEFT " Back", close_input_shaper_popup_cb, NULL, NULL);
    s_shaper_run = ui_cal_dialog_action(s_motion.input_shaper_popup, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_PLAY " Run", run_input_shaper_cb, NULL, NULL);
    if (s_shaper_run) motion_register(s_shaper_run);
}


static void close_resonance_test_popup(void)
{
    if (s_motion.resonance_test_popup) {
        lv_obj_t *popup =
            s_motion.resonance_test_popup;
        ++s_motion_epoch;
        s_motion.resonance_test_popup = NULL;
        s_resonance_feedback = NULL;
        lv_obj_delete(popup);
    }
}


static void close_resonance_test_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_resonance_test_popup();
}


static void run_resonance_test_cb(lv_event_t *event)
{
    if (!event || !s_motion.resonance_test_popup || s_motion_dispatching) return;
    const char *axis = motion_action(event,s_resonance_run[0]) ? "X" :
        motion_action(event,s_resonance_run[1]) ? "Y" : NULL;
    if (!axis) return;

    char command[64];
    int written = lv_snprintf(
        command,
        sizeof(command),
        "TEST_RESONANCES AXIS=%s",
        axis);

    if (written <= 0 ||
        (size_t)written >= sizeof(command)) {
        return;
    }

    if (!motion_send(&s_motion.resonance_test_popup, &s_resonance_feedback,
            "Resonance testing", command, CALIBRATION_SESSION_RESONANCE_TEST)) return;
    close_resonance_test_popup();

    char waiting[220];
    lv_snprintf(
        waiting,
        sizeof(waiting),
        "Klipper is measuring %s-axis resonance. Keep the machine attended and do not touch it.\n\n"
        "This diagnostic does not change the printer configuration or require SAVE_CONFIG.",
        axis);

    show_results(
        "Resonance test",
        waiting);
}


static void resonance_test_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (s_motion_dispatching) return;
    if (s_motion.resonance_test_popup) {
        lv_obj_move_foreground(s_motion.resonance_test_popup);
        return;
    }
    if (!motion_ready(
            "Resonance testing")) {
        return;
    }

    if (!s_motion.input_shaper ||
        !s_motion.accelerometer) {
        ui_cal_dialog_notice(
            UI_STATUS_WARNING,
            "Resonance test unavailable",
            "This printer has not reported the required motion measurement stack.");
        return;
    }

    close_resonance_test_popup();
    s_motion.resonance_test_popup =
        ui_cal_dialog_create(lv_layer_top(), 650, 410, UI_POPUP_STANDARD);

    if (!s_motion.resonance_test_popup) {
        return;
    }

    motion_register(s_motion.resonance_test_popup);
    ui_cal_dialog_title_divided(s_motion.resonance_test_popup, ui_text("Select resonance test axis"));

    ui_cal_dialog_text(ui_cal_dialog_body(s_motion.resonance_test_popup), "Klipper will move and vibrate the selected axis. The machine may be loud.");
    ui_cal_dialog_caption(ui_cal_dialog_body(s_motion.resonance_test_popup),
        "Clear the bed and motion area, then select an axis to begin.");
    s_resonance_feedback = ui_cal_dialog_caption(ui_cal_dialog_body(s_motion.resonance_test_popup), "");
    motion_register(s_resonance_feedback);
    s_resonance_run[0] = ui_cal_dialog_choice(s_motion.resonance_test_popup, UI_POPUP_ACTION_CONFIRM, ui_text("Test X axis"), 248, run_resonance_test_cb, NULL, NULL);
    motion_register(s_resonance_run[0]);
    s_resonance_run[1] = ui_cal_dialog_choice(s_motion.resonance_test_popup, UI_POPUP_ACTION_CONFIRM, ui_text("Test Y axis"), 248, run_resonance_test_cb, NULL, NULL);
    motion_register(s_resonance_run[1]);

    ui_cal_dialog_action(s_motion.resonance_test_popup, UI_POPUP_ACTION_CLOSE, "Close", close_resonance_test_popup_cb, NULL, NULL);
}


static void close_accelerometer_check_popup(void)
{
    if (s_motion.accelerometer_check_popup) {
        lv_obj_t *popup =
            s_motion.accelerometer_check_popup;
        ++s_motion_epoch;
        s_motion.accelerometer_check_popup = NULL;
        s_sensor_feedback = NULL;
        lv_obj_delete(popup);
    }
}


static void close_accelerometer_check_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_accelerometer_check_popup();
}


static void run_accelerometer_check_cb(lv_event_t *event)
{
    if (!motion_action(event,s_sensor_run)) return;
    if (!motion_send(&s_motion.accelerometer_check_popup, &s_sensor_feedback,
            "Accelerometer checking", "MEASURE_AXES_NOISE", CALIBRATION_SESSION_ACCELEROMETER_CHECK)) return;
    close_accelerometer_check_popup();

    show_results(
        "Accelerometer check",
        "Klipper is sampling accelerometer noise.\n\n"
        "Keep the printer completely still until the axis values appear.");
}


static void accelerometer_check_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (s_motion_dispatching) return;
    if (s_motion.accelerometer_check_popup) {
        lv_obj_move_foreground(s_motion.accelerometer_check_popup);
        return;
    }
    if (!motion_ready(
            "Accelerometer checking")) {
        return;
    }

    if (!s_motion.accelerometer) {
        ui_cal_dialog_notice(
            UI_STATUS_WARNING,
            "Sensor check unavailable",
            "This printer has not reported a supported accelerometer.");
        return;
    }

    close_accelerometer_check_popup();
    s_motion.accelerometer_check_popup =
        ui_cal_dialog_create(lv_layer_top(), 620, 370, UI_POPUP_STANDARD);

    if (!s_motion.accelerometer_check_popup) {
        return;
    }

    motion_register(s_motion.accelerometer_check_popup);
    ui_cal_dialog_title_divided(s_motion.accelerometer_check_popup, ui_text("Check accelerometer?"));

    ui_cal_dialog_text(ui_cal_dialog_body(s_motion.accelerometer_check_popup), "Klipper will sample idle noise on every enabled accelerometer axis.");
    ui_cal_dialog_caption(ui_cal_dialog_body(s_motion.accelerometer_check_popup),
        "Keep the printer completely still during the measurement.");

    s_sensor_feedback = ui_cal_dialog_caption(ui_cal_dialog_body(s_motion.accelerometer_check_popup), "");
    motion_register(s_sensor_feedback);
    ui_cal_dialog_action(s_motion.accelerometer_check_popup, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_LEFT " Back", close_accelerometer_check_popup_cb, NULL, NULL);
    s_sensor_run=ui_cal_dialog_action(s_motion.accelerometer_check_popup, UI_POPUP_ACTION_CONFIRM, "Run check", run_accelerometer_check_cb, NULL, NULL);
    motion_register(s_sensor_run);
}


static lv_obj_t *make_action_button(
    lv_obj_t *card,
    const char *label,
    unsigned column,
    lv_event_cb_t callback)
{
    lv_obj_t *button = ui_button_create(
        card,
        UI_BUTTON_OUTLINED,
        label);

    if (!button) {
        return NULL;
    }

    ui_calibration_layout_motion_action(button,column,false);
    lv_obj_set_style_pad_hor(button, 1, 0);
    lv_label_set_long_mode(lv_obj_get_child(button, 0), LV_LABEL_LONG_CLIP);
    ui_text_fit_single_line(lv_obj_get_child(button, 0), UI_FONT_BODY_LARGE);
    lv_label_set_long_mode(lv_obj_get_child(button, 0), LV_LABEL_LONG_CLIP);
    lv_obj_add_event_cb(
        button,
        callback,
        LV_EVENT_CLICKED,
        NULL);
    lv_obj_add_event_cb(button, motion_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_add_flag(
        button,
        LV_OBJ_FLAG_HIDDEN);
    return button;
}


void ui_calibration_motion_create(
    lv_obj_t *card,
    ui_calibration_motion_send_gcode_cb_t send_gcode_cb,
    ui_calibration_motion_ready_cb_t ready_cb,
    ui_calibration_motion_show_results_cb_t show_results_cb,
    ui_calibration_motion_refresh_results_cb_t refresh_results_cb)
{
    if (!card) {
        return;
    }

    ui_motion_diagnostics_create(card);
    s_motion.send_gcode = send_gcode_cb;
    s_motion.ready = ready_cb;
    s_motion.show_results = show_results_cb;
    s_motion.refresh_results = refresh_results_cb;

    s_motion.input_shaper_button =
        make_action_button(
            card,
            "Shaper",
            0,
            input_shaper_button_cb);

    s_motion.resonance_test_button =
        make_action_button(
            card,
            "Resonance",
            1,
            resonance_test_button_cb);

    s_motion.accelerometer_check_button =
        make_action_button(
            card,
            "Sensor",
            2,
            accelerometer_check_button_cb);
}


void ui_calibration_motion_refresh(
    bool discovered,
    bool input_shaper,
    bool accelerometer,
    bool shaper_calibrate)
{
    s_motion.discovered = discovered;
    s_motion.input_shaper = input_shaper;
    s_motion.accelerometer = accelerometer;
    s_motion.shaper_calibrate = shaper_calibrate;

    bool complete_stack =
        discovered &&
        input_shaper &&
        accelerometer;

    set_visible(
        s_motion.input_shaper_button,
        shaper_calibrate || complete_stack);
    set_visible(
        s_motion.resonance_test_button,
        complete_stack);
    set_visible(
        s_motion.accelerometer_check_button,
        discovered && accelerometer);
}


void ui_calibration_motion_hide(void)
{
    ui_motion_diagnostics_hide();
    close_input_shaper_popup();
    close_resonance_test_popup();
    close_accelerometer_check_popup();

    memset(&s_motion, 0, sizeof(s_motion));
}
