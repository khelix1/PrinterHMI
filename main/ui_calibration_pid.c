#include "ui_calibration_pid.h"
#include "ui_text.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "calibration_session_controller.h"
#include "console_controller.h"
#include "moonraker.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"

static ui_calibration_pid_context_t *s_context;

void ui_calibration_pid_init(ui_calibration_pid_context_t *context)
{
    s_context = context;
}

static bool pid_object_supported(
    const char *object_name)
{
    return object_name &&
        (strcmp(object_name, "heater_bed") == 0 ||
         strncmp(object_name, "extruder", 8) == 0 ||
         strncmp(
             object_name,
             "heater_generic ",
             strlen("heater_generic ")) == 0);
}


void ui_calibration_pid_close(void)
{
    if (!s_context) {
        return;
    }

    if (*s_context->popup) {
        lv_obj_t *popup =
            *s_context->popup;
        *s_context->popup = NULL;
        *s_context->target_label = NULL;
        lv_obj_delete(popup);
    }
}


static void close_pid_popup_cb(
    lv_event_t *event)
{
    (void)event;
    ui_calibration_pid_close();
}


bool ui_calibration_pid_printer_ready(void)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!state.moonraker_ok ||
        !state.live_data_ok) {
        ui_toast_show(
            UI_STATUS_DANGER,
            "PID UNAVAILABLE",
            "The active printer is offline or not ready.");
        return false;
    }

    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        ui_toast_show(
            UI_STATUS_DANGER,
            "PID BLOCKED",
            "PID calibration cannot run during a print.");
        return false;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_toast_show(
            UI_STATUS_DANGER,
            "PID BLOCKED",
            "Clear the printer error before calibration.");
        return false;
    }

    return true;
}


static void pid_set_defaults(
    const char *object_name)
{
    if (!s_context || !object_name) {
        return;
    }

    if (strncmp(object_name, "extruder", 8) == 0) {
        *s_context->target = 200;
        *s_context->target_min = 150;
        *s_context->target_max = 300;
    } else if (strcmp(object_name, "heater_bed") == 0) {
        *s_context->target = 60;
        *s_context->target_min = 40;
        *s_context->target_max = 130;
    } else {
        *s_context->target = 60;
        *s_context->target_min = 20;
        *s_context->target_max = 120;
    }
}


static void refresh_pid_target_label(void)
{
    if (!s_context ||
        !*s_context->target_label ||
        *s_context->selected_index >=
            *s_context->heater_count) {
        return;
    }

    char text[240];
    lv_snprintf(
        text,
        sizeof(text),
        "%s\n\nTARGET: %d C\n"
        "Allowed range: %d-%d C\n\n"
        "The heater will cycle repeatedly. Keep the machine attended.",
        s_context->display_names[
            *s_context->selected_index],
        *s_context->target,
        *s_context->target_min,
        *s_context->target_max);
    lv_label_set_text(
        *s_context->target_label,
        text);
}


static void pid_adjust_target_cb(
    lv_event_t *event)
{
    if (!s_context || !event) {
        return;
    }

    int delta =
        (int)(intptr_t)lv_event_get_user_data(
            event);
    int target =
        *s_context->target + delta;

    if (target < *s_context->target_min) {
        target = *s_context->target_min;
    }
    if (target > *s_context->target_max) {
        target = *s_context->target_max;
    }

    *s_context->target = target;
    refresh_pid_target_label();
}


static const char *pid_heater_argument(
    const char *object_name)
{
    static const char GENERIC_PREFIX[] =
        "heater_generic ";

    if (object_name &&
        strncmp(
            object_name,
            GENERIC_PREFIX,
            sizeof(GENERIC_PREFIX) - 1) == 0) {
        return object_name +
            sizeof(GENERIC_PREFIX) - 1;
    }

    return object_name;
}



static void run_pid_tune_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_context ||
        *s_context->selected_index >=
            *s_context->heater_count ||
        !ui_calibration_pid_printer_ready()) {
        return;
    }

    const char *object_name =
        s_context->object_names[
            *s_context->selected_index];
    const char *heater =
        pid_heater_argument(object_name);

    char command[176];
    int written = lv_snprintf(
        command,
        sizeof(command),
        "PID_CALIBRATE HEATER=%s TARGET=%d",
        heater ? heater : "",
        *s_context->target);

    if (written <= 0 ||
        (size_t)written >= sizeof(command)) {
        ui_toast_show(
            UI_STATUS_DANGER,
            "PID FAILED",
            "The selected heater command is too long.");
        return;
    }

    uint32_t start_sequence =
        console_controller_latest_sequence();
    calibration_session_controller_begin(
        CALIBRATION_SESSION_PID,
        start_sequence);
    console_controller_add_command(command);

    bool sent =
        s_context->send_gcode &&
        s_context->send_gcode(command);

    ui_calibration_pid_close();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the PID command.");
    }

    s_context->show_results(
        "PID CALIBRATION",
        "The heater is cycling. Keep the machine attended.\n\n"
        "Apply & Restart will appear only after Klipper reports a successful result requiring SAVE_CONFIG.");
    s_context->refresh_results();
}


void ui_calibration_pid_event(
    lv_event_t *event);


static void back_to_pid_heaters_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_context) {
        return;
    }

    /*
     * With multiple heaters, rebuild the live detected-heater selector.
     * A single-heater printer has no prior selection screen, so Back closes.
     */
    if (*s_context->heater_count <= 1) {
        ui_calibration_pid_close();
        return;
    }

    ui_calibration_pid_event(NULL);
}


static void pid_start_warning_cb(lv_event_t *event);
static void show_pid_target_popup(void)
{
    if (!s_context ||
        *s_context->selected_index >=
            *s_context->heater_count) {
        return;
    }

    ui_calibration_pid_close();

    const char *object_name =
        s_context->object_names[
            *s_context->selected_index];
    pid_set_defaults(object_name);

    *s_context->popup =
        ui_popup_create(
            lv_layer_top(),
            650,
            440,
            UI_POPUP_STANDARD);

    if (!*s_context->popup) {
        return;
    }

    ui_popup_add_title(
        *s_context->popup,
        ui_text("CONFIRM PID CALIBRATION"),
        false,
        4);
    ui_popup_add_header_divider(
        *s_context->popup,
        48);

    *s_context->target_label =
        ui_popup_add_body(
            *s_context->popup,
            "",
            28,
            72,
            594);
    refresh_pid_target_label();

    static const int deltas[] =
        {-10, -5, 5, 10};
    static const char *labels[] =
        {"-10", "-5", "+5", "+10"};

    for (size_t index = 0;
         index < sizeof(deltas) / sizeof(deltas[0]);
         ++index) {
        ui_popup_add_action_at(
            *s_context->popup,
            UI_POPUP_ACTION_CHOICE,
            labels[index],
            45 + (int)index * 145,
            260,
            120,
            46,
            pid_adjust_target_cb,
            (void *)(intptr_t)deltas[index],
            NULL);
    }

    ui_popup_add_standard_footer_divider(
        *s_context->popup);

    ui_popup_add_footer_action(
        *s_context->popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        back_to_pid_heaters_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        *s_context->popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " RUN",
        170,
        UI_POPUP_FOOTER_RIGHT,
        pid_start_warning_cb,
        NULL,
        NULL);
}


static void back_to_pid_target_cb(lv_event_t *event)
{
    (void)event;
    show_pid_target_popup();
}


static void pid_start_warning_cb(lv_event_t *event)
{
    (void)event;
    if (!s_context) return;
    ui_calibration_pid_close();
    *s_context->popup = ui_popup_create(lv_layer_top(), 650, 420, UI_POPUP_DANGER);
    if (!*s_context->popup) return;
    ui_popup_add_title(*s_context->popup, ui_text("START PID HEAT CYCLE?"), false, 4);
    ui_popup_add_header_divider(*s_context->popup, 48);
    ui_popup_add_body(*s_context->popup,
        "The selected heater will cycle repeatedly at the chosen target. Keep the printer attended, clear the area, and do not start a print until calibration completes.",
        28, 76, 594);
    ui_popup_add_standard_footer_divider(*s_context->popup);
    ui_popup_add_footer_action(*s_context->popup, UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK", 170, UI_POPUP_FOOTER_LEFT,
        back_to_pid_target_cb, NULL, NULL);
    ui_popup_add_footer_action(*s_context->popup, UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_PLAY " START HEAT", 210, UI_POPUP_FOOTER_RIGHT,
        run_pid_tune_cb, NULL, NULL);
}


static void pid_heater_selected_cb(
    lv_event_t *event)
{
    if (!s_context || !event) {
        return;
    }

    size_t index =
        (size_t)(uintptr_t)lv_event_get_user_data(
            event);

    if (index >= *s_context->heater_count) {
        return;
    }

    *s_context->selected_index = index;
    show_pid_target_popup();
}


void ui_calibration_pid_event(
    lv_event_t *event)
{
    (void)event;

    if (!s_context || !ui_calibration_pid_printer_ready()) {
        return;
    }

    *s_context->heater_count = 0;
    memset(
        s_context->object_names,
        0,
        s_context->heater_capacity *
            sizeof(*s_context->object_names));
    memset(
        s_context->display_names,
        0,
        s_context->heater_capacity *
            sizeof(*s_context->display_names));

    device_catalog_status_t status;
    device_catalog_controller_status(&status);

    for (size_t index = 0;
         index < status.stored_count &&
         *s_context->heater_count <
             UI_CALIBRATION_PID_HEATER_MAX;
         ++index) {
        device_descriptor_t device;

        if (!device_catalog_controller_get(
                index,
                &device) ||
            !pid_object_supported(
                device.object_name)) {
            continue;
        }

        size_t output_index =
            (*s_context->heater_count)++;
        lv_snprintf(
            s_context->object_names[
                output_index],
            DEVICE_CATALOG_OBJECT_NAME_MAX,
            "%s",
            device.object_name);
        lv_snprintf(
            s_context->display_names[
                output_index],
            DEVICE_CATALOG_DISPLAY_NAME_MAX,
            "%s",
            device.display_name);
    }

    if (*s_context->heater_count == 0) {
        ui_toast_show(
            UI_STATUS_WARNING,
            "NO PID HEATERS",
            "No PID-capable heater objects are available.");
        return;
    }

    if (*s_context->heater_count == 1) {
        *s_context->selected_index = 0;
        show_pid_target_popup();
        return;
    }

    ui_calibration_pid_close();

    *s_context->popup =
        ui_popup_create(
            lv_layer_top(),
            650,
            440,
            UI_POPUP_STANDARD);

    if (!*s_context->popup) {
        return;
    }

    ui_popup_add_title(
        *s_context->popup,
        ui_text("SELECT HEATER FOR PID"),
        false,
        4);
    ui_popup_add_header_divider(
        *s_context->popup,
        48);

    lv_obj_t *list =
        ui_popup_add_list(
            *s_context->popup,
            28,
            68,
            594,
            292);

    if (list) {
        for (size_t index = 0;
             index < *s_context->heater_count;
             ++index) {
            ui_popup_add_selectable_row(
                list,
                s_context->display_names[index],
                8,
                8 + (int)index * 54,
                558,
                46,
                pid_heater_selected_cb,
                (void *)(uintptr_t)index);
        }
    }

    ui_popup_add_standard_footer_divider(
        *s_context->popup);

    ui_popup_add_footer_action(
        *s_context->popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        170,
        UI_POPUP_FOOTER_RIGHT,
        close_pid_popup_cb,
        NULL,
        NULL);
}
