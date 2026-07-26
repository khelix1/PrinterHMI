#include "ui_printer_live_status.h"

#include "printer_controller.h"
#include "ui_theme.h"

#include <stdint.h>
#include <stdio.h>

typedef enum {
    PRINTER_TUNING_SPEED = 0,
    PRINTER_TUNING_FLOW
} printer_tuning_kind_t;

static ui_printer_live_status_send_gcode_cb_t
    s_send_gcode_cb = NULL;

static lv_obj_t *s_speed_factor_slider = NULL;
static lv_obj_t *s_flow_factor_slider = NULL;
static lv_obj_t *s_speed_factor_value = NULL;
static lv_obj_t *s_flow_factor_value = NULL;

static int s_pending_speed_factor = -1;
static int s_pending_flow_factor = -1;


static int clamp_factor(int value)
{
    if (value < 50) {
        return 50;
    }

    if (value > 150) {
        return 150;
    }

    return value;
}


static void set_factor_value_text(
    lv_obj_t *label,
    int value)
{
    if (!label) {
        return;
    }

    char text[16];

    lv_snprintf(
        text,
        sizeof(text),
        "%d%%",
        value);

    lv_label_set_text(label, text);
}


static void tuning_slider_event_cb(lv_event_t *event)
{
    if (!event) {
        return;
    }

    lv_obj_t *slider =
        lv_event_get_target(event);

    if (!slider) {
        return;
    }

    printer_tuning_kind_t kind =
        (printer_tuning_kind_t)(intptr_t)
            lv_event_get_user_data(event);

    int value =
        lv_slider_get_value(slider);

    lv_obj_t *value_label =
        kind == PRINTER_TUNING_SPEED
            ? s_speed_factor_value
            : s_flow_factor_value;

    set_factor_value_text(
        value_label,
        value);

    if (lv_event_get_code(event) !=
        LV_EVENT_RELEASED) {
        return;
    }

    char command[24];

    lv_snprintf(
        command,
        sizeof(command),
        kind == PRINTER_TUNING_SPEED
            ? "M220 S%d"
            : "M221 S%d",
        value);

    if (!s_send_gcode_cb ||
        !s_send_gcode_cb(command)) {
        return;
    }

    if (kind == PRINTER_TUNING_SPEED) {
        s_pending_speed_factor = value;
    } else {
        s_pending_flow_factor = value;
    }
}


static lv_obj_t *create_tuning_slider(
    lv_obj_t *parent,
    int x,
    int y,
    lv_color_t accent,
    printer_tuning_kind_t kind)
{
    lv_obj_t *slider =
        lv_slider_create(parent);

    if (!slider) {
        return NULL;
    }

    lv_obj_set_size(
        slider,
        200,
        12);

    lv_obj_set_pos(
        slider,
        x,
        y);

    lv_slider_set_range(
        slider,
        50,
        150);

    lv_slider_set_value(
        slider,
        100,
        LV_ANIM_OFF);

    lv_obj_set_style_radius(
        slider,
        LV_RADIUS_CIRCLE,
        LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        slider,
        UI_BG_DEEP,
        LV_PART_MAIN);

    lv_obj_set_style_bg_opa(
        slider,
        LV_OPA_COVER,
        LV_PART_MAIN);

    lv_obj_set_style_border_color(
        slider,
        UI_BORDER_SOFT,
        LV_PART_MAIN);

    lv_obj_set_style_border_width(
        slider,
        UI_BORDER_THIN,
        LV_PART_MAIN);

    lv_obj_set_style_radius(
        slider,
        LV_RADIUS_CIRCLE,
        LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(
        slider,
        accent,
        LV_PART_INDICATOR);

    lv_obj_set_style_bg_opa(
        slider,
        LV_OPA_COVER,
        LV_PART_INDICATOR);

    lv_obj_set_style_radius(
        slider,
        LV_RADIUS_CIRCLE,
        LV_PART_KNOB);

    lv_obj_set_style_bg_color(
        slider,
        UI_TEXT_BRIGHT,
        LV_PART_KNOB);

    lv_obj_set_style_bg_opa(
        slider,
        LV_OPA_COVER,
        LV_PART_KNOB);

    lv_obj_set_style_border_color(
        slider,
        accent,
        LV_PART_KNOB);

    lv_obj_set_style_border_width(
        slider,
        UI_BORDER_STRONG,
        LV_PART_KNOB);

    lv_obj_set_style_pad_all(
        slider,
        ui_theme_density_metric(4, 6, 8),
        LV_PART_KNOB);

    lv_obj_add_event_cb(
        slider,
        tuning_slider_event_cb,
        LV_EVENT_VALUE_CHANGED,
        (void *)(intptr_t)kind);

    lv_obj_add_event_cb(
        slider,
        tuning_slider_event_cb,
        LV_EVENT_RELEASED,
        (void *)(intptr_t)kind);

    return slider;
}


static void refresh_filament_label(
    lv_obj_t *label,
    bool moonraker_online,
    const moonraker_filament_state_t *state)
{
    if (!label) return;

    size_t present = 0;
    size_t enabled = 0;

    moonraker_filament_status_t status =
        moonraker_filament_state_status(
            state,
            &present,
            &enabled);

    char text[40];

    if (state &&
        state->discovered &&
        state->total_count > 0 &&
        !moonraker_online) {
        snprintf(text, sizeof(text), "FILAMENT\nOFFLINE");
        ui_apply_label_error(label);
    } else {
        switch (status) {
            case MOONRAKER_FILAMENT_ABSENT:
                snprintf(text, sizeof(text), "FILAMENT\nN/A");
                ui_apply_label_dim(label);
                break;

            case MOONRAKER_FILAMENT_CHECKING:
                snprintf(text, sizeof(text), "FILAMENT\nCHECKING");
                ui_apply_label_warning(label);
                break;

            case MOONRAKER_FILAMENT_READY:
                if (enabled <= 1) {
                    snprintf(
                        text,
                        sizeof(text),
                        "FILAMENT\nPRESENT");
                } else {
                    snprintf(
                        text,
                        sizeof(text),
                        "FILAMENT\n%u/%u PRESENT",
                        (unsigned)present,
                        (unsigned)enabled);
                }
                ui_apply_label_success(label);
                break;

            case MOONRAKER_FILAMENT_RUNOUT:
                if (enabled <= 1) {
                    snprintf(
                        text,
                        sizeof(text),
                        "FILAMENT\nRUNOUT");
                } else {
                    snprintf(
                        text,
                        sizeof(text),
                        "FILAMENT\n%u/%u RUNOUT",
                        (unsigned)present,
                        (unsigned)enabled);
                }
                ui_apply_label_error(label);
                break;

            case MOONRAKER_FILAMENT_DISABLED:
                snprintf(text, sizeof(text), "FILAMENT\nDISABLED");
                ui_apply_label_dim(label);
                break;

            case MOONRAKER_FILAMENT_UNKNOWN:
            default:
                snprintf(text, sizeof(text), "FILAMENT\n--");
                ui_apply_label_dim(label);
                break;
        }
    }

    /*
     * The Printer-page filament value owns the control entry point.
     * Show an affordance only when a real sensor is available.
     */
    if (state &&
        state->discovered &&
        state->total_count > 0) {
        size_t used = strlen(text);

        if (used < sizeof(text)) {
            snprintf(
                text + used,
                sizeof(text) - used,
                " " LV_SYMBOL_RIGHT);
        }
    }

    lv_label_set_text(label, text);
}


static void refresh_factor_slider(
    lv_obj_t *slider,
    lv_obj_t *value_label,
    double reported_factor,
    int *pending_factor)
{
    if (!slider ||
        !value_label ||
        !pending_factor) {
        return;
    }

    int reported =
        clamp_factor(
            (int)(reported_factor + 0.5));

    int displayed = reported;

    if (*pending_factor >= 0) {
        int difference =
            reported - *pending_factor;

        if (difference < 0) {
            difference = -difference;
        }

        if (difference <= 1) {
            *pending_factor = -1;
        } else {
            displayed = *pending_factor;
        }
    }

    if (lv_obj_has_state(
            slider,
            LV_STATE_PRESSED)) {
        return;
    }

    lv_slider_set_value(
        slider,
        displayed,
        LV_ANIM_OFF);

    set_factor_value_text(
        value_label,
        displayed);
}


void ui_printer_live_status_create(
    lv_obj_t *parent,
    lv_obj_t **active_file_label,
    lv_obj_t **speed_label,
    lv_obj_t **flow_label,
    lv_obj_t **layer_label,
    lv_obj_t **filament_label,
    lv_event_cb_t filament_event_cb,
    double initial_speed_factor,
    double initial_flow_factor,
    ui_printer_live_status_send_gcode_cb_t send_gcode_cb)
{
    if (!parent) {
        return;
    }

    /*
     * Pending values deliberately survive page destruction/recreation.
     * Moonraker remains authoritative and clears them when its reported
     * factor matches the requested value.
     */
    s_send_gcode_cb = send_gcode_cb;

    /*
     * parent is the 800x220 Active Print panel created by
     * ui_printer_layout_v32.
     */
    lv_obj_t *card = parent;

    lv_obj_set_style_pad_all(card, 0, 0);

    lv_obj_t *title =
        lv_label_create(card);

    lv_label_set_text(
        title,
        "ACTIVE PRINT");

    ui_apply_text_body_large(title);
    ui_apply_label_dim(title);

    lv_obj_set_pos(title, 330, 12);

    if (active_file_label) {
        *active_file_label =
            lv_label_create(card);

        lv_label_set_text(
            *active_file_label,
            "FILE: --");

        lv_obj_set_width(
            *active_file_label,
            300);

        lv_label_set_long_mode(
            *active_file_label,
            LV_LABEL_LONG_DOT);

        ui_apply_text_body(
            *active_file_label);

        ui_apply_label_bright(
            *active_file_label);

        lv_obj_set_pos(
            *active_file_label,
            470,
            14);
    }

    lv_obj_t *divider =
        lv_obj_create(card);

    lv_obj_clear_flag(
        divider,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_size(divider, 1, 156);
    lv_obj_set_pos(divider, 310, 48);

    lv_obj_set_style_bg_color(
        divider,
        UI_BORDER_SOFT,
        0);

    lv_obj_set_style_bg_opa(
        divider,
        LV_OPA_COVER,
        0);

    lv_obj_set_style_border_width(
        divider,
        0,
        0);

    lv_obj_set_style_pad_all(
        divider,
        0,
        0);

    if (layer_label) {
        *layer_label =
            lv_label_create(card);

        lv_label_set_text(
            *layer_label,
            LV_SYMBOL_LIST " LAYER\n-- / --");

        lv_obj_set_width(
            *layer_label,
            105);

        ui_apply_text_body(
            *layer_label);

        ui_apply_label_primary(
            *layer_label);

        lv_obj_set_pos(
            *layer_label,
            330,
            62);
    }

    if (speed_label) {
        *speed_label =
            lv_label_create(card);

        lv_label_set_text(
            *speed_label,
            "SPEED\n-- mm/s");

        lv_obj_set_width(
            *speed_label,
            130);

        ui_apply_text_body(
            *speed_label);

        ui_apply_label_primary(
            *speed_label);

        lv_obj_set_width(
            *speed_label,
            105);

        lv_obj_set_pos(
            *speed_label,
            445,
            62);
    }

    if (flow_label) {
        *flow_label =
            lv_label_create(card);

        lv_label_set_text(
            *flow_label,
            "FLOW\n-- mm3/s");

        lv_obj_set_width(
            *flow_label,
            105);

        ui_apply_text_body(
            *flow_label);

        ui_apply_label_primary(
            *flow_label);

        lv_obj_set_pos(
            *flow_label,
            560,
            62);
    }

    if (filament_label) {
        *filament_label =
            lv_label_create(card);

        lv_label_set_text(
            *filament_label,
            "FILAMENT\n--");

        lv_obj_set_width(
            *filament_label,
            105);

        ui_apply_text_body(
            *filament_label);

        ui_apply_label_dim(
            *filament_label);

        lv_obj_set_pos(
            *filament_label,
            675,
            62);

        lv_obj_add_flag(
            *filament_label,
            LV_OBJ_FLAG_CLICKABLE);

        if (filament_event_cb) {
            lv_obj_add_event_cb(
                *filament_label,
                filament_event_cb,
                LV_EVENT_CLICKED,
                NULL);
        }
    }

    lv_obj_t *speed_title =
        lv_label_create(card);

    lv_label_set_text(
        speed_title,
        "SPEED FACTOR");

    ui_apply_text_caption(speed_title);
    ui_apply_label_dim(speed_title);

    lv_obj_set_pos(
        speed_title,
        330,
        126);

    s_speed_factor_value =
        lv_label_create(card);

    ui_apply_text_body(
        s_speed_factor_value);

    ui_apply_label_bright(
        s_speed_factor_value);

    lv_obj_set_width(
        s_speed_factor_value,
        60);

    lv_obj_set_style_text_align(
        s_speed_factor_value,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_set_pos(
        s_speed_factor_value,
        470,
        124);

    set_factor_value_text(
        s_speed_factor_value,
        100);

    s_speed_factor_slider =
        create_tuning_slider(
            card,
            330,
            174,
            UI_ACCENT_CYAN,
            PRINTER_TUNING_SPEED);

    lv_obj_t *flow_title =
        lv_label_create(card);

    lv_label_set_text(
        flow_title,
        "FLOW FACTOR");

    ui_apply_text_caption(flow_title);
    ui_apply_label_dim(flow_title);

    lv_obj_set_pos(
        flow_title,
        575,
        126);

    s_flow_factor_value =
        lv_label_create(card);

    ui_apply_text_body(
        s_flow_factor_value);

    ui_apply_label_bright(
        s_flow_factor_value);

    lv_obj_set_width(
        s_flow_factor_value,
        60);

    lv_obj_set_style_text_align(
        s_flow_factor_value,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_set_pos(
        s_flow_factor_value,
        715,
        124);

    set_factor_value_text(
        s_flow_factor_value,
        100);

    s_flow_factor_slider =
        create_tuning_slider(
            card,
            575,
            174,
            UI_OK,
            PRINTER_TUNING_FLOW);

    /*
     * Do not flash/reset to 100% when returning to the Printer page.
     * Initialize immediately from the latest Moonraker state or from a
     * command that is still awaiting confirmation.
     */
    refresh_factor_slider(
        s_speed_factor_slider,
        s_speed_factor_value,
        initial_speed_factor,
        &s_pending_speed_factor);

    refresh_factor_slider(
        s_flow_factor_slider,
        s_flow_factor_value,
        initial_flow_factor,
        &s_pending_flow_factor);
}


void ui_printer_live_status_refresh(
    lv_obj_t *printer_panel,
    lv_obj_t *active_file_box,
    lv_obj_t *active_file_label,
    lv_obj_t *speed_label,
    lv_obj_t *flow_label,
    lv_obj_t *layer_label,
    lv_obj_t *filament_label,
    const char *printer_state,
    const char *printer_file,
    double printer_live_velocity,
    double printer_live_flow,
    double printer_speed_factor,
    double printer_flow_factor,
    int printer_current_layer,
    int printer_total_layer,
    double printer_meta_object_height,
    double printer_meta_layer_height,
    double printer_progress,
    bool moonraker_online,
    const moonraker_filament_state_t *filament_state)
{
    if (printer_panel &&
        active_file_box &&
        active_file_label) {
        bool active_job =
            printer_controller_has_active_job(
                printer_state,
                printer_file);

        if (active_job) {
            char file_text[300];

            snprintf(
                file_text,
                sizeof(file_text),
                "FILE: %s",
                printer_file);

            lv_label_set_text(
                active_file_label,
                file_text);

            lv_obj_remove_flag(
                active_file_box,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(
                active_file_box,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (printer_panel && speed_label) {
        char text[48];

        snprintf(
            text,
            sizeof(text),
            "SPEED\n%.0f mm/s",
            printer_live_velocity);

        lv_label_set_text(
            speed_label,
            text);
    }

    if (printer_panel && flow_label) {
        char text[48];

        snprintf(
            text,
            sizeof(text),
            "FLOW\n%.1f mm3/s",
            printer_live_flow);

        lv_label_set_text(
            flow_label,
            text);
    }

    if (printer_panel && layer_label) {
        char text[48];

        int total =
            printer_total_layer;

        int current =
            printer_current_layer;


        if (current >= 0 && total > 0) {
            snprintf(
                text,
                sizeof(text),
                "LAYER\n%d / %d",
                current,
                total);
        } else {
            snprintf(
                text,
                sizeof(text),
                "LAYER\n-- / --");
        }

        lv_label_set_text(
            layer_label,
            text);
    }

    if (printer_panel) {
        refresh_filament_label(
            filament_label,
            moonraker_online,
            filament_state);

        refresh_factor_slider(
            s_speed_factor_slider,
            s_speed_factor_value,
            printer_speed_factor,
            &s_pending_speed_factor);

        refresh_factor_slider(
            s_flow_factor_slider,
            s_flow_factor_value,
            printer_flow_factor,
            &s_pending_flow_factor);
    }
}
