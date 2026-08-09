#include "ui_calibration_custom.h"
#include <ctype.h>
#include <string.h>
#include "calibration_session_controller.h"
#include "console_controller.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"
static ui_calibration_custom_context_t *s_custom;
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
        *s_custom->popup = NULL;
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

    if (!s_custom ||
        *s_custom->selected >=
            *s_custom->count ||
        !s_custom->ready(
            "Custom calibration")) {
        return;
    }

    const char *command =
        s_custom->names[
            *s_custom->selected];
    uint32_t start_sequence =
        console_controller_latest_sequence();
    calibration_session_controller_begin(
        CALIBRATION_SESSION_CUSTOM,
        start_sequence);
    console_controller_add_command(command);
    bool sent =
        s_custom->send &&
        s_custom->send(command);

    close_custom_popup();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the custom calibration macro.");
    }

    s_custom->show_results(
        "CUSTOM CALIBRATION",
        sent
            ? "The selected printer macro is running. Monitor Console for its printer-specific instructions.\\n\\n"
              "Apply & Restart will appear only if Klipper reports SAVE_CONFIG."
            : "The selected custom calibration macro could not be started.");
    s_custom->refresh_results();
}


static void custom_macro_selected_cb(
    lv_event_t *event)
{
    if (!s_custom || !event) {
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
        ui_popup_create(
            lv_layer_top(),
            620,
            370,
            UI_POPUP_STANDARD);

    if (!*s_custom->popup) {
        return;
    }

    ui_popup_add_title(
        *s_custom->popup,
        "RUN CUSTOM CALIBRATION?",
        false,
        4);
    ui_popup_add_header_divider(
        *s_custom->popup,
        48);

    char body[400];
    lv_snprintf(
        body,
        sizeof(body),
        "Macro: %s\\n\\n"
        "This is printer-defined behavior. It may move hardware or heat components. Keep the machine attended.\\n\\n"
        "PrinterHMI will not save automatically.",
        s_custom->names[index]);
    ui_popup_add_body(
        *s_custom->popup,
        body,
        28,
        76,
        564);
    ui_popup_add_standard_footer_divider(
        *s_custom->popup);
    ui_popup_add_footer_action(
        *s_custom->popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_custom_popup_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        *s_custom->popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " RUN",
        170,
        UI_POPUP_FOOTER_RIGHT,
        run_custom_macro_cb,
        NULL,
        NULL);
}


static void custom_calibration_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_custom ||
        !s_custom->ready(
            "Custom calibration")) {
        return;
    }

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
                *s_custom->count++],
            MACRO_CONTROLLER_NAME_MAX,
            "%s",
            name);
    }

    if (*s_custom->count == 0) {
        ui_toast_show(
            UI_STATUS_WARNING,
            "NO CUSTOM CALIBRATIONS",
            "No matching public calibration macros are available.");
        return;
    }

    close_custom_popup();
    *s_custom->popup =
        ui_popup_create(
            lv_layer_top(),
            650,
            450,
            UI_POPUP_STANDARD);

    if (!*s_custom->popup) {
        return;
    }

    ui_popup_add_title(
        *s_custom->popup,
        "CUSTOM CALIBRATION MACROS",
        false,
        4);
    ui_popup_add_header_divider(
        *s_custom->popup,
        48);
    lv_obj_t *list =
        ui_popup_add_list(
            *s_custom->popup,
            28,
            68,
            594,
            302);

    if (list) {
        for (size_t index = 0;
             index < *s_custom->count;
             ++index) {
            ui_popup_add_selectable_row(
                list,
                s_custom->names[index],
                8,
                8 + (int)index * 54,
                558,
                46,
                custom_macro_selected_cb,
                (void *)(uintptr_t)index);
        }
    }

    ui_popup_add_standard_footer_divider(
        *s_custom->popup);
    ui_popup_add_footer_action(
        *s_custom->popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        170,
        UI_POPUP_FOOTER_RIGHT,
        close_custom_popup_cb,
        NULL,
        NULL);
}





void ui_calibration_custom_init(ui_calibration_custom_context_t *context){s_custom=context;}
void ui_calibration_custom_event(lv_event_t *event){custom_calibration_button_cb(event);}
void ui_calibration_custom_close(void){close_custom_popup();}
