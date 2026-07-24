from pathlib import Path

source_path = Path("main/ui_printer_live_status.c")
header_path = Path("main/ui_printer_live_status.h")
main_path = Path("main/main.c")

old_source = source_path.read_text()
old_header = header_path.read_text()
main = main_path.read_text()

source_checks = [
    "void ui_printer_live_status_create(",
    "void ui_printer_live_status_refresh(",
    '"ACTIVE PRINT"',
    '"Speed\\n%.0f mm/s"',
    '"Flow\\n%.1f mm3/s"',
]

for marker in source_checks:
    if old_source.count(marker) != 1:
        raise RuntimeError(
            f"unexpected live-status source marker: {marker!r}")

if old_header.count(
        "void ui_printer_live_status_create(") != 1:
    raise RuntimeError("unexpected live-status header")

old_create_call = """    ui_printer_live_status_create(printer_layout.active_panel,
                                  &printer_active_file_label,
                                  &printer_fan_label,
                                  &printer_speed_label,
                                  &printer_flow_label);
"""

new_create_call = """    ui_printer_live_status_create(
        printer_layout.active_panel,
        &printer_active_file_label,
        &printer_fan_label,
        &printer_speed_label,
        &printer_flow_label,
        moonraker_send_gcode);
"""

old_refresh_values = """                                   printer_live_velocity,
                                   printer_live_flow,
                                   printer_current_layer,
"""

new_refresh_values = """                                   printer_live_velocity,
                                   printer_live_flow,
                                   printer_speed_factor,
                                   printer_flow_factor,
                                   printer_current_layer,
"""

for text, anchor, description in [
    (main, old_create_call, "Printer live-status creation"),
    (main, old_refresh_values, "Printer live-status refresh"),
]:
    count = text.count(anchor)
    if count != 1:
        raise RuntimeError(
            f"expected one {description}, found {count}")

new_header = r'''#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef bool (*ui_printer_live_status_send_gcode_cb_t)(
    const char *command);

void ui_printer_live_status_create(
    lv_obj_t *parent,
    lv_obj_t **active_file_label,
    lv_obj_t **speed_label,
    lv_obj_t **flow_label,
    lv_obj_t **layer_label,
    ui_printer_live_status_send_gcode_cb_t send_gcode_cb);

void ui_printer_live_status_refresh(
    lv_obj_t *printer_panel,
    lv_obj_t *active_file_box,
    lv_obj_t *active_file_label,
    lv_obj_t *speed_label,
    lv_obj_t *flow_label,
    lv_obj_t *layer_label,
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
    double printer_progress);
'''

new_source = r'''#include "ui_printer_live_status.h"

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
        1,
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
        2,
        LV_PART_KNOB);

    lv_obj_set_style_pad_all(
        slider,
        6,
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
    ui_printer_live_status_send_gcode_cb_t send_gcode_cb)
{
    if (!parent) {
        return;
    }

    s_send_gcode_cb = send_gcode_cb;
    s_pending_speed_factor = -1;
    s_pending_flow_factor = -1;

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
            130);

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

        lv_obj_set_pos(
            *speed_label,
            485,
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
            140);

        ui_apply_text_body(
            *flow_label);

        ui_apply_label_primary(
            *flow_label);

        lv_obj_set_pos(
            *flow_label,
            640,
            62);
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
}


void ui_printer_live_status_refresh(
    lv_obj_t *printer_panel,
    lv_obj_t *active_file_box,
    lv_obj_t *active_file_label,
    lv_obj_t *speed_label,
    lv_obj_t *flow_label,
    lv_obj_t *layer_label,
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
    double printer_progress)
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

        if ((current < 0 || total <= 0) &&
            printer_meta_object_height > 0.0 &&
            printer_meta_layer_height > 0.0 &&
            printer_progress >= 0.0) {
            total =
                (int)(
                    (printer_meta_object_height /
                     printer_meta_layer_height) +
                    0.5);

            current =
                (int)(
                    (printer_progress * total) +
                    0.5);

            if (current < 1 &&
                printer_progress > 0.0) {
                current = 1;
            }

            if (current > total) {
                current = total;
            }
        }

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
'''

main = main.replace(
    old_create_call,
    new_create_call,
    1)

main = main.replace(
    old_refresh_values,
    new_refresh_values,
    1)

source_path.write_text(new_source)
header_path.write_text(new_header)
main_path.write_text(main)

print("Installed Printer-page speed and flow factor sliders.")
print("Range: 50–150%")
print("Speed command: M220")
print("Flow command: M221")
print("Commands send only on slider release.")
