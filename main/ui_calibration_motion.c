#include "ui_calibration_motion.h"
#include "ui_text.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "calibration_session_controller.h"
#include "console_controller.h"

#include "ui_button.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"

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
    bool accelerometer;
} ui_calibration_motion_state_t;

static ui_calibration_motion_state_t s_motion;


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
        s_motion.input_shaper_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_input_shaper_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_input_shaper_popup();
}


static void run_input_shaper_cb(
    lv_event_t *event)
{
    (void)event;

    if (!motion_ready(
            "Input Shaper calibration")) {
        return;
    }

    static const char command[] =
        "SHAPER_CALIBRATE";
    uint32_t start_sequence =
        console_controller_latest_sequence();

    calibration_session_controller_begin(
        CALIBRATION_SESSION_INPUT_SHAPER,
        start_sequence);
    console_controller_add_command(command);

    bool sent =
        s_motion.send_gcode &&
        s_motion.send_gcode(command);

    close_input_shaper_popup();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept SHAPER_CALIBRATE.");
    }

    show_results(
        "INPUT SHAPER CALIBRATION",
        "Klipper is measuring X and Y resonance. Keep the machine attended and do not touch it.\n\n"
        "Apply & Restart will appear only after Klipper reports a successful result requiring SAVE_CONFIG.");
}


static void input_shaper_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!motion_ready(
            "Input Shaper calibration")) {
        return;
    }

    if (!s_motion.input_shaper ||
        !s_motion.accelerometer) {
        ui_toast_show(
            UI_STATUS_WARNING,
            "INPUT SHAPER UNAVAILABLE",
            "This printer has not reported both Input Shaper and an accelerometer.");
        return;
    }

    close_input_shaper_popup();
    s_motion.input_shaper_popup =
        ui_popup_create(
            lv_layer_top(),
            650,
            410,
            UI_POPUP_STANDARD);

    if (!s_motion.input_shaper_popup) {
        return;
    }

    ui_popup_add_title(
        s_motion.input_shaper_popup,
        ui_text("RUN INPUT SHAPER CALIBRATION?"),
        false,
        4);
    ui_popup_add_header_divider(
        s_motion.input_shaper_popup,
        48);
    ui_popup_add_body(
        s_motion.input_shaper_popup,
        "Klipper will move and vibrate the toolhead across both axes. The machine may be loud.\n\n"
        "Clear the bed and motion area, make sure nothing can contact the printer, and keep the machine attended.",
        28,
        76,
        594);
    ui_popup_add_standard_footer_divider(
        s_motion.input_shaper_popup);
    ui_popup_add_footer_action(
        s_motion.input_shaper_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_input_shaper_popup_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_motion.input_shaper_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " RUN",
        170,
        UI_POPUP_FOOTER_RIGHT,
        run_input_shaper_cb,
        NULL,
        NULL);
}


static void close_resonance_test_popup(void)
{
    if (s_motion.resonance_test_popup) {
        lv_obj_t *popup =
            s_motion.resonance_test_popup;
        s_motion.resonance_test_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_resonance_test_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_resonance_test_popup();
}


static void run_resonance_test_cb(
    lv_event_t *event)
{
    if (!event ||
        !motion_ready(
            "Resonance testing")) {
        return;
    }

    const char *axis =
        (const char *)lv_event_get_user_data(event);

    if (!axis ||
        (strcmp(axis, "X") != 0 &&
         strcmp(axis, "Y") != 0)) {
        return;
    }

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

    uint32_t start_sequence =
        console_controller_latest_sequence();

    calibration_session_controller_begin(
        CALIBRATION_SESSION_RESONANCE_TEST,
        start_sequence);
    console_controller_add_command(command);

    bool sent =
        s_motion.send_gcode &&
        s_motion.send_gcode(command);

    close_resonance_test_popup();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept TEST_RESONANCES.");
    }

    char waiting[220];
    lv_snprintf(
        waiting,
        sizeof(waiting),
        "Klipper is measuring %s-axis resonance. Keep the machine attended and do not touch it.\n\n"
        "This diagnostic does not change the printer configuration or require SAVE_CONFIG.",
        axis);

    show_results(
        "RESONANCE TEST",
        waiting);
}


static void resonance_test_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!motion_ready(
            "Resonance testing")) {
        return;
    }

    if (!s_motion.input_shaper ||
        !s_motion.accelerometer) {
        ui_toast_show(
            UI_STATUS_WARNING,
            "RESONANCE TEST UNAVAILABLE",
            "This printer has not reported the required motion measurement stack.");
        return;
    }

    close_resonance_test_popup();
    s_motion.resonance_test_popup =
        ui_popup_create(
            lv_layer_top(),
            650,
            410,
            UI_POPUP_STANDARD);

    if (!s_motion.resonance_test_popup) {
        return;
    }

    ui_popup_add_title(
        s_motion.resonance_test_popup,
        ui_text("SELECT RESONANCE TEST AXIS"),
        false,
        4);
    ui_popup_add_header_divider(
        s_motion.resonance_test_popup,
        48);
    ui_popup_add_body(
        s_motion.resonance_test_popup,
        "Klipper will move and vibrate the selected axis. The machine may be loud.\n\n"
        "Clear the bed and motion area, then select an axis to begin.",
        28,
        76,
        594);
    ui_popup_add_action_at(
        s_motion.resonance_test_popup,
        UI_POPUP_ACTION_CONFIRM,
        ui_text("TEST X AXIS"),
        54,
        252,
        248,
        48,
        run_resonance_test_cb,
        (void *)ui_text("X"),
        NULL);
    ui_popup_add_action_at(
        s_motion.resonance_test_popup,
        UI_POPUP_ACTION_CONFIRM,
        ui_text("TEST Y AXIS"),
        348,
        252,
        248,
        48,
        run_resonance_test_cb,
        (void *)ui_text("Y"),
        NULL);
    ui_popup_add_standard_footer_divider(
        s_motion.resonance_test_popup);
    ui_popup_add_footer_action(
        s_motion.resonance_test_popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        170,
        UI_POPUP_FOOTER_RIGHT,
        close_resonance_test_popup_cb,
        NULL,
        NULL);
}


static void close_accelerometer_check_popup(void)
{
    if (s_motion.accelerometer_check_popup) {
        lv_obj_t *popup =
            s_motion.accelerometer_check_popup;
        s_motion.accelerometer_check_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_accelerometer_check_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_accelerometer_check_popup();
}


static void run_accelerometer_check_cb(
    lv_event_t *event)
{
    (void)event;

    if (!motion_ready(
            "Accelerometer checking")) {
        return;
    }

    static const char command[] =
        "MEASURE_AXES_NOISE";
    uint32_t start_sequence =
        console_controller_latest_sequence();

    calibration_session_controller_begin(
        CALIBRATION_SESSION_ACCELEROMETER_CHECK,
        start_sequence);
    console_controller_add_command(command);

    bool sent =
        s_motion.send_gcode &&
        s_motion.send_gcode(command);

    close_accelerometer_check_popup();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept MEASURE_AXES_NOISE.");
    }

    show_results(
        "ACCELEROMETER CHECK",
        "Klipper is sampling accelerometer noise.\n\n"
        "Keep the printer completely still until the axis values appear.");
}


static void accelerometer_check_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!motion_ready(
            "Accelerometer checking")) {
        return;
    }

    if (!s_motion.accelerometer) {
        ui_toast_show(
            UI_STATUS_WARNING,
            "SENSOR CHECK UNAVAILABLE",
            "This printer has not reported a supported accelerometer.");
        return;
    }

    close_accelerometer_check_popup();
    s_motion.accelerometer_check_popup =
        ui_popup_create(
            lv_layer_top(),
            620,
            370,
            UI_POPUP_STANDARD);

    if (!s_motion.accelerometer_check_popup) {
        return;
    }

    ui_popup_add_title(
        s_motion.accelerometer_check_popup,
        ui_text("CHECK ACCELEROMETER?"),
        false,
        4);
    ui_popup_add_header_divider(
        s_motion.accelerometer_check_popup,
        48);
    ui_popup_add_body(
        s_motion.accelerometer_check_popup,
        "Klipper will sample the idle noise on every enabled accelerometer axis.\n\n"
        "Keep the printer completely still during the measurement.",
        28,
        76,
        564);
    ui_popup_add_standard_footer_divider(
        s_motion.accelerometer_check_popup);
    ui_popup_add_footer_action(
        s_motion.accelerometer_check_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_accelerometer_check_popup_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_motion.accelerometer_check_popup,
        UI_POPUP_ACTION_CONFIRM,
        "RUN CHECK",
        170,
        UI_POPUP_FOOTER_RIGHT,
        run_accelerometer_check_cb,
        NULL,
        NULL);
}


static lv_obj_t *make_action_button(
    lv_obj_t *card,
    const char *label,
    lv_align_t align,
    int x,
    lv_event_cb_t callback)
{
    lv_obj_t *button = ui_button_create(
        card,
        UI_BUTTON_OUTLINED,
        label);

    if (!button) {
        return NULL;
    }

    lv_obj_set_size(button, 110, 38);
    lv_obj_align(button, align, x, -12);
    lv_obj_add_event_cb(
        button,
        callback,
        LV_EVENT_CLICKED,
        NULL);
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

    s_motion.send_gcode = send_gcode_cb;
    s_motion.ready = ready_cb;
    s_motion.show_results = show_results_cb;
    s_motion.refresh_results = refresh_results_cb;

    s_motion.input_shaper_button =
        make_action_button(
            card,
            "INPUT SHAPER",
            LV_ALIGN_BOTTOM_LEFT,
            16,
            input_shaper_button_cb);

    s_motion.resonance_test_button =
        make_action_button(
            card,
            "RESONANCE",
            LV_ALIGN_BOTTOM_MID,
            0,
            resonance_test_button_cb);

    s_motion.accelerometer_check_button =
        make_action_button(
            card,
            "SENSOR CHECK",
            LV_ALIGN_BOTTOM_RIGHT,
            -16,
            accelerometer_check_button_cb);
}


void ui_calibration_motion_refresh(
    bool discovered,
    bool input_shaper,
    bool accelerometer)
{
    s_motion.discovered = discovered;
    s_motion.input_shaper = input_shaper;
    s_motion.accelerometer = accelerometer;

    bool complete_stack =
        discovered &&
        input_shaper &&
        accelerometer;

    set_visible(
        s_motion.input_shaper_button,
        complete_stack);
    set_visible(
        s_motion.resonance_test_button,
        complete_stack);
    set_visible(
        s_motion.accelerometer_check_button,
        discovered && accelerometer);
}


void ui_calibration_motion_hide(void)
{
    close_input_shaper_popup();
    close_resonance_test_popup();
    close_accelerometer_check_popup();

    memset(&s_motion, 0, sizeof(s_motion));
}
