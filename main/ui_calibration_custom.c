#include "ui_calibration_custom.h"
#include "ui_text.h"
#include <ctype.h>
#include <string.h>
#include "calibration_session_controller.h"
#include "console_controller.h"
#include "ui_popup.h"
#include "ui_calibration_dialog.h"
#include "ui_theme.h"
#include "ui_toast.h"
static ui_calibration_custom_context_t *s_custom;
static uint32_t s_custom_owner, s_custom_epoch;
static bool s_custom_dispatching;
static lv_obj_t *s_custom_feedback;

static void custom_retire_popup(void)
{
    ++s_custom_epoch;
    s_custom_feedback = NULL;
    if (s_custom) *s_custom->popup = NULL;
}

static void custom_popup_deleted(lv_event_t *event)
{
    if (s_custom && lv_event_get_target_obj(event) == *s_custom->popup)
        custom_retire_popup();
}

static void custom_register_popup(void)
{
    lv_obj_set_user_data(*s_custom->popup, (void *)(uintptr_t)s_custom_owner);
    lv_obj_add_event_cb(*s_custom->popup, custom_popup_deleted, LV_EVENT_DELETE, NULL);
}
static bool custom_macro_matches(
    const char *name)
{
    if (!name || !name[0]) {
        return false;
    }

    static const char *terms[] = {
        "CALIBRAT", "SCREW", "GANTRY", "Z_TILT",
        "BED_MESH", "SHAPER", "RESONANCE", "PID",
        "Z_OFFSET", "PRESSURE_ADVANCE",
    };

    for (size_t term_index = 0;
         term_index < sizeof(terms) / sizeof(terms[0]);
         ++term_index) {
        const char *needle = terms[term_index];
        size_t needle_length = strlen(needle);

        for (const char *start = name; *start; ++start) {
            size_t index = 0;
            while (index < needle_length &&
                   start[index] &&
                   toupper((unsigned char)start[index]) ==
                       toupper((unsigned char)needle[index])) {
                ++index;
            }
            if (index == needle_length) {
                return true;
            }
        }
    }

    return false;
}


static void close_custom_popup(void)
{
    if (!s_custom) {
        return;
    }

    if (*s_custom->popup) {
        lv_obj_t *popup = *s_custom->popup;
        custom_retire_popup();
        lv_obj_delete(popup);
    }
}


static void close_custom_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_custom_popup();
}


static void run_custom_macro_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_custom || !*s_custom->popup || s_custom_dispatching ||
        s_custom_owner != moonraker_config_generation() || !s_custom->ready ||
        *s_custom->selected >=
            *s_custom->count ||
        !s_custom->ready(
            "Custom calibration")) {
        return;
    }

    char command[MACRO_CONTROLLER_NAME_MAX];
    lv_snprintf(command, sizeof(command), "%s", s_custom->names[*s_custom->selected]);
    uint32_t start_sequence =
        console_controller_latest_sequence();
    calibration_session_controller_begin(
        CALIBRATION_SESSION_CUSTOM,
        start_sequence);
    console_controller_add_command(command);
    ui_calibration_custom_context_t *context = s_custom;
    lv_obj_t *popup = *context->popup;
    uint32_t epoch = s_custom_epoch;
    uint32_t owner = s_custom_owner;
    s_custom_dispatching = true;
    bool sent = context->send && context->send(command);
    s_custom_dispatching = false;
    if (s_custom != context || s_custom_epoch != epoch ||
        *context->popup != popup || owner != moonraker_config_generation()) return;
    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the custom calibration macro.");
        if (s_custom_feedback)
            lv_label_set_text(s_custom_feedback,
                "Command was not accepted. Your macro choice is retained. Retry when connected.");
        return;
    }
    close_custom_popup();
    s_custom->show_results(
        "Custom calibration",
        "The selected printer macro is running. Monitor Console for its printer-specific instructions.\n\n"
        "Apply & Restart will appear only if Klipper reports SAVE_CONFIG.");
    s_custom->refresh_results();
}


static void custom_macro_selected_cb(
    lv_event_t *event)
{
    if (!s_custom || !*s_custom->popup || !event || s_custom_dispatching ||
        s_custom_owner != moonraker_config_generation()) {
        return;
    }

    size_t index =
        (size_t)(uintptr_t)lv_event_get_user_data(
            event);
    if (index >= *s_custom->count) {
        return;
    }

    *s_custom->selected = index;
    close_custom_popup();
    *s_custom->popup =
        ui_cal_dialog_create(lv_layer_top(), 650, 410, UI_POPUP_STANDARD);

    if (!*s_custom->popup) {
        return;
    }

    custom_register_popup();
    ui_cal_dialog_title_divided(*s_custom->popup, ui_text("Run custom calibration?"));


    lv_obj_t *body=ui_cal_dialog_body(*s_custom->popup);
    ui_cal_dialog_caption(body,"Selected macro");
    lv_obj_t *name=ui_cal_dialog_text(body,s_custom->names[index]);
    ui_apply_custom_label_style(name,UI_FONT_BODY_LARGE,UI_TEXT_BRIGHT);
    ui_cal_dialog_text(body,
        "Printer-defined behavior may move hardware or heat components. Keep the printer attended.");
    ui_cal_dialog_caption(body,"PrinterHMI will not save automatically.");
    s_custom_feedback = ui_cal_dialog_caption(body, "");

    ui_cal_dialog_action(*s_custom->popup, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_LEFT " Back", close_custom_popup_cb, NULL, NULL);
    ui_cal_dialog_action(*s_custom->popup, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_PLAY " Run", run_custom_macro_cb, NULL, NULL);
}


static void custom_calibration_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_custom || s_custom_dispatching || !s_custom->ready ||
        !s_custom->ready(
            "Custom calibration")) {
        return;
    }

    if (*s_custom->popup) {
        lv_obj_move_foreground(*s_custom->popup);
        return;
    }
    s_custom_owner = moonraker_config_generation();
    *s_custom->count = 0;
    memset(
        s_custom->names,
        0,
        s_custom->names_capacity * sizeof(*s_custom->names));

    macro_controller_status_t status;
    macro_controller_status(&status);

    for (size_t index = 0;
         index < status.count &&
         *s_custom->count <
             s_custom->names_capacity;
         ++index) {
        char name[MACRO_CONTROLLER_NAME_MAX];
        if (!macro_controller_get(
                index,
                name,
                sizeof(name)) ||
            !custom_macro_matches(name)) {
            continue;
        }

        lv_snprintf(
            s_custom->names[
                (*s_custom->count)++],
            MACRO_CONTROLLER_NAME_MAX,
            "%s",
            name);
    }

    if (*s_custom->count == 0) {
        ui_cal_dialog_notice(
            UI_STATUS_WARNING,
            "No custom calibrations",
            "No matching public calibration macros are available.");
        return;
    }

    close_custom_popup();
    *s_custom->popup =
        ui_cal_dialog_create(lv_layer_top(), 650, 450, UI_POPUP_STANDARD);

    if (!*s_custom->popup) {
        return;
    }

    custom_register_popup();
    ui_cal_dialog_title_divided(*s_custom->popup, ui_text("Custom calibration macros"));

    lv_obj_t *list =
        ui_cal_dialog_body(*s_custom->popup);

    if (list) {
        for (size_t index = 0;
             index < *s_custom->count;
             ++index) {
            ui_cal_dialog_select(list, s_custom->names[index], custom_macro_selected_cb, (void *)(uintptr_t)index);
        }
    }


    ui_cal_dialog_action(*s_custom->popup, UI_POPUP_ACTION_CLOSE, "Close", close_custom_popup_cb, NULL, NULL);
}





void ui_calibration_custom_init(ui_calibration_custom_context_t *context){s_custom=context;}
void ui_calibration_custom_event(lv_event_t *event){custom_calibration_button_cb(event);}
void ui_calibration_custom_close(void){close_custom_popup();}
