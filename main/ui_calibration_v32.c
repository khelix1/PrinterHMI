#include "ui_calibration_v32.h"

#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "calibration_capability_controller.h"
#include "calibration_session_controller.h"
#include "ui_calibration_motion.h"
#include "ui_calibration_manual_probe.h"
#include "ui_calibration_pressure_advance.h"
#include "console_controller.h"
#include "device_catalog_controller.h"
#include "macro_controller.h"
#include "moonraker.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include "ui_button.h"
#include "ui_page_geometry_v32.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast_v32.h"
#include "ui_widgets.h"

typedef struct {
    lv_obj_t *summary;
    lv_obj_t *status;
} calibration_card_refs_t;

#define PID_HEATER_MAX 8
#define CUSTOM_CALIBRATION_MACRO_MAX 16

typedef struct {
    lv_obj_t *root;
    lv_obj_t *banner_status;
    lv_obj_t *bed_mesh_button;
    lv_obj_t *screws_tilt_button;
    lv_obj_t *gantry_level_button;
    lv_obj_t *axis_twist_button;
    lv_obj_t *pid_tune_button;
    lv_obj_t *probe_z_button;
    lv_obj_t *custom_calibration_button;
    lv_obj_t *axis_twist_popup;
    lv_obj_t *screws_popup;
    lv_obj_t *gantry_level_popup;
    lv_obj_t *pid_popup;
    lv_obj_t *pid_target_label;
    lv_obj_t *calibration_results_popup;
    lv_obj_t *calibration_results_label;
    lv_obj_t *apply_restart_button;
    lv_obj_t *save_confirm_popup;
    lv_obj_t *probe_popup;
    lv_obj_t *custom_popup;
    lv_obj_t *screws_results_label;
    calibration_card_refs_t bed;
    calibration_card_refs_t motion;
    calibration_card_refs_t thermal;
    calibration_card_refs_t probe;
    lv_timer_t *refresh_timer;
    ui_calibration_open_bed_mesh_cb_t open_bed_mesh;
    ui_calibration_send_gcode_cb_t send_gcode;
    bool screws_home_required;
    bool gantry_home_required;
    bool gantry_use_qgl;
    bool axis_twist_home_required;
    char pid_object_names[PID_HEATER_MAX]
                         [DEVICE_CATALOG_OBJECT_NAME_MAX];
    char pid_display_names[PID_HEATER_MAX]
                          [DEVICE_CATALOG_DISPLAY_NAME_MAX];
    size_t pid_heater_count;
    size_t pid_selected_index;
    int pid_target;
    int pid_target_min;
    int pid_target_max;
    bool probe_home_required;
    char custom_macro_names[CUSTOM_CALIBRATION_MACRO_MAX]
                           [MACRO_CONTROLLER_NAME_MAX];
    size_t custom_macro_count;
    size_t custom_macro_selected;
    calibration_session_snapshot_t session_snapshot;
    char screws_display[CALIBRATION_SESSION_RESULTS_MAX + 64];
    char calibration_display[CALIBRATION_SESSION_RESULTS_MAX + 96];
    uint32_t session_generation;
    uint32_t device_generation;
    uint32_t macro_generation;
    uint32_t command_generation;
} ui_calibration_state_t;

static const char TAG[] = "ui_calibration";
static ui_calibration_state_t *s_calibration;


static bool calibration_state_init(void)
{
    if (s_calibration) {
        return true;
    }

    s_calibration = heap_caps_calloc(
        1,
        sizeof(*s_calibration),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_calibration) {
        ESP_LOGI(
            TAG,
            "Calibration page state allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_calibration));
        return true;
    }

    s_calibration = heap_caps_calloc(
        1,
        sizeof(*s_calibration),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_calibration) {
        ESP_LOGE(TAG, "Unable to allocate Calibration page state");
        return false;
    }

    ESP_LOGW(TAG, "Calibration page state using internal RAM fallback");
    return true;
}


static lv_obj_t *calibration_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color,
    int x,
    int y,
    int width)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text ? text : "--");
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    ui_apply_custom_label_style(label, font, color);

    return label;
}


static lv_obj_t *calibration_card(
    lv_obj_t *parent,
    const char *title,
    int x,
    int y,
    calibration_card_refs_t *refs)
{
    lv_obj_t *card = ui_create_operator_card(
        parent,
        x,
        y,
        390,
        176);

    if (!card) {
        return NULL;
    }

    ui_create_operator_card_heading(card, title, 16, 14);
    ui_create_operator_card_divider(card, 16, 45, 358);

    if (refs) {
        refs->summary = calibration_label(
            card,
            "Waiting for active-printer discovery.",
            UI_FONT_BODY,
            UI_TEXT_DIM,
            16,
            58,
            358);

        refs->status = calibration_label(
            card,
            "AWAITING DISCOVERY",
            UI_FONT_CAPTION,
            UI_ACCENT_BRIGHT,
            16,
            142,
            190);
    }

    return card;
}


static void append_tool(
    char *output,
    size_t output_size,
    const char *tool)
{
    if (!output || output_size == 0 || !tool || !tool[0]) {
        return;
    }

    size_t used = strlen(output);

    if (used >= output_size - 1) {
        return;
    }

    lv_snprintf(
        output + used,
        output_size - used,
        "%s%s",
        used ? "  /  " : "",
        tool);
}


static void set_card(
    calibration_card_refs_t *refs,
    const char *summary,
    size_t count,
    size_t macro_count)
{
    if (!refs || !refs->summary || !refs->status) {
        return;
    }

    lv_label_set_text(
        refs->summary,
        summary && summary[0]
            ? summary
            : "No applicable tools reported by this printer.");

    char status[48];

    if (macro_count > 0) {
        lv_snprintf(
            status,
            sizeof(status),
            "%u TOOL%s + %u MACRO%s",
            (unsigned)count,
            count == 1 ? "" : "S",
            (unsigned)macro_count,
            macro_count == 1 ? "" : "S");
    } else if (count > 0) {
        lv_snprintf(
            status,
            sizeof(status),
            "%u TOOL%s DETECTED",
            (unsigned)count,
            count == 1 ? "" : "S");
    } else {
        lv_snprintf(
            status,
            sizeof(status),
            "NOT CONFIGURED");
    }

    lv_label_set_text(refs->status, status);

    if (count > 0 || macro_count > 0) {
        ui_apply_label_bright(refs->status);
    } else {
        ui_apply_label_dim(refs->status);
    }
}


static void set_action_label(
    lv_obj_t *button,
    const char *text)
{
    lv_obj_t *label = button
        ? lv_obj_get_child(button, 0)
        : NULL;

    if (label) {
        lv_label_set_text(label, text ? text : "");
    }
}


static void layout_bed_geometry_actions(
    const calibration_capabilities_t *capabilities)
{
    if (!s_calibration || !capabilities) {
        return;
    }

    lv_obj_t *buttons[] = {
        s_calibration->bed_mesh_button,
        s_calibration->screws_tilt_button,
        s_calibration->gantry_level_button,
        s_calibration->axis_twist_button,
    };
    bool visible[] = {
        capabilities->bed_mesh,
        capabilities->screws_tilt,
        capabilities->z_tilt ||
            capabilities->quad_gantry_level,
        capabilities->axis_twist,
    };
    lv_obj_t *visible_buttons[4] = {0};
    size_t count = 0;

    for (size_t index = 0;
         index < sizeof(buttons) / sizeof(buttons[0]);
         ++index) {
        if (buttons[index] && visible[index]) {
            visible_buttons[count++] = buttons[index];
        }
    }

    if (count == 0) {
        return;
    }

    set_action_label(
        s_calibration->bed_mesh_button,
        "BED MESH");

    for (size_t index = 0; index < count; ++index) {
        lv_obj_set_size(
            visible_buttons[index],
            110,
            38);
    }

    if (count == 1) {
        lv_obj_align(
            visible_buttons[0],
            LV_ALIGN_BOTTOM_LEFT,
            16,
            -12);
    } else if (count == 2) {
        lv_obj_align(
            visible_buttons[0],
            LV_ALIGN_BOTTOM_LEFT,
            16,
            -12);
        lv_obj_align(
            visible_buttons[1],
            LV_ALIGN_BOTTOM_RIGHT,
            -16,
            -12);
    } else if (count == 3) {
        lv_obj_align(
            visible_buttons[0],
            LV_ALIGN_BOTTOM_LEFT,
            16,
            -12);
        lv_obj_align(
            visible_buttons[1],
            LV_ALIGN_BOTTOM_MID,
            0,
            -12);
        lv_obj_align(
            visible_buttons[2],
            LV_ALIGN_BOTTOM_RIGHT,
            -16,
            -12);
    } else {
        if (s_calibration->bed.summary) {
            lv_label_set_text(
                s_calibration->bed.summary,
                "BED GEOMETRY TOOLS READY");
        }

        lv_obj_align(
            visible_buttons[0],
            LV_ALIGN_BOTTOM_LEFT,
            16,
            -58);
        lv_obj_align(
            visible_buttons[1],
            LV_ALIGN_BOTTOM_RIGHT,
            -16,
            -58);
        lv_obj_align(
            visible_buttons[2],
            LV_ALIGN_BOTTOM_LEFT,
            16,
            -12);
        lv_obj_align(
            visible_buttons[3],
            LV_ALIGN_BOTTOM_RIGHT,
            -16,
            -12);
    }
}


static void refresh_capabilities(void)
{
    if (!s_calibration || !s_calibration->root) {
        return;
    }

    calibration_capabilities_t capabilities;
    calibration_capability_controller_snapshot(
        &capabilities);

    if (!capabilities.discovered) {
        lv_label_set_text(
            s_calibration->banner_status,
            "WAITING FOR PRINTER");

        calibration_card_refs_t *cards[] = {
            &s_calibration->bed,
            &s_calibration->motion,
            &s_calibration->thermal,
            &s_calibration->probe,
        };

        for (size_t index = 0;
             index < sizeof(cards) / sizeof(cards[0]);
             ++index) {
            if (!cards[index]->summary ||
                !cards[index]->status) {
                continue;
            }

            lv_label_set_text(
                cards[index]->summary,
                "Waiting for active-printer discovery.");
            lv_label_set_text(
                cards[index]->status,
                "AWAITING DISCOVERY");
            ui_apply_label_dim(cards[index]->status);
        }

        if (s_calibration->bed_mesh_button) {
            lv_obj_add_flag(
                s_calibration->bed_mesh_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        if (s_calibration->screws_tilt_button) {
            lv_obj_add_flag(
                s_calibration->screws_tilt_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        if (s_calibration->gantry_level_button) {
            lv_obj_add_flag(
                s_calibration->gantry_level_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        if (s_calibration->axis_twist_button) {
            lv_obj_add_flag(
                s_calibration->axis_twist_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        if (s_calibration->pid_tune_button) {
            lv_obj_add_flag(
                s_calibration->pid_tune_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        ui_calibration_pressure_advance_refresh(
            false,
            false);

        ui_calibration_motion_refresh(
            false,
            false,
            false);

        if (s_calibration->probe_z_button) {
            lv_obj_add_flag(
                s_calibration->probe_z_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        if (s_calibration->custom_calibration_button) {
            lv_obj_add_flag(
                s_calibration->custom_calibration_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        if (s_calibration->bed.status) {
            lv_obj_clear_flag(
                s_calibration->bed.status,
                LV_OBJ_FLAG_HIDDEN);
        }

        s_calibration->device_generation =
            capabilities.device_generation;
        s_calibration->macro_generation =
            capabilities.macro_generation;
        s_calibration->command_generation =
            capabilities.command_generation;
        return;
    }

    char banner[64];
    lv_snprintf(
        banner,
        sizeof(banner),
        "%u TOOL%s DETECTED",
        (unsigned)capabilities.tool_count,
        capabilities.tool_count == 1 ? "" : "S");
    lv_label_set_text(
        s_calibration->banner_status,
        banner);

    char bed[176] = "";
    size_t bed_count = 0;

    if (capabilities.bed_mesh) {
        append_tool(bed, sizeof(bed), "BED MESH");
        ++bed_count;
    }
    if (capabilities.screws_tilt) {
        append_tool(bed, sizeof(bed), "SCREWS TILT");
        ++bed_count;
    }
    if (capabilities.quad_gantry_level) {
        append_tool(bed, sizeof(bed), "QGL");
        ++bed_count;
    }
    if (capabilities.z_tilt) {
        append_tool(bed, sizeof(bed), "Z TILT");
        ++bed_count;
    }
    if (capabilities.axis_twist) {
        append_tool(bed, sizeof(bed), "AXIS TWIST");
        ++bed_count;
    }

    set_card(
        &s_calibration->bed,
        bed,
        bed_count,
        0);

    if (s_calibration->bed_mesh_button) {
        if (capabilities.bed_mesh) {
            lv_obj_clear_flag(
                s_calibration->bed_mesh_button,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(
                s_calibration->bed_mesh_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_calibration->screws_tilt_button) {
        if (capabilities.screws_tilt) {
            lv_obj_clear_flag(
                s_calibration->screws_tilt_button,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(
                s_calibration->screws_tilt_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_calibration->gantry_level_button) {
        if (capabilities.z_tilt ||
            capabilities.quad_gantry_level) {
            lv_obj_clear_flag(
                s_calibration->gantry_level_button,
                LV_OBJ_FLAG_HIDDEN);

            lv_obj_t *label =
                lv_obj_get_child(
                    s_calibration->gantry_level_button,
                    0);
            if (label) {
                lv_label_set_text(
                    label,
                    capabilities.quad_gantry_level
                        ? "QGL"
                        : "Z TILT");
            }
        } else {
            lv_obj_add_flag(
                s_calibration->gantry_level_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_calibration->axis_twist_button) {
        if (capabilities.axis_twist) {
            lv_obj_clear_flag(
                s_calibration->axis_twist_button,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(
                s_calibration->axis_twist_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    layout_bed_geometry_actions(&capabilities);

    if (s_calibration->bed.status) {
        if (capabilities.bed_mesh ||
            capabilities.screws_tilt ||
            capabilities.z_tilt ||
            capabilities.quad_gantry_level ||
            capabilities.axis_twist) {
            lv_obj_add_flag(
                s_calibration->bed.status,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(
                s_calibration->bed.status,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    char motion[176] = "";
    size_t motion_count = 0;

    if (capabilities.input_shaper) {
        append_tool(
            motion,
            sizeof(motion),
            "INPUT SHAPER");
        ++motion_count;
    }
    if (capabilities.accelerometer) {
        append_tool(
            motion,
            sizeof(motion),
            "ACCELEROMETER");
        ++motion_count;
    }

    set_card(
        &s_calibration->motion,
        motion,
        motion_count,
        0);

    ui_calibration_motion_refresh(
        true,
        capabilities.input_shaper,
        capabilities.accelerometer);

    char thermal[176] = "";
    size_t thermal_count = 0;

    if (capabilities.hotend_pid) {
        append_tool(
            thermal,
            sizeof(thermal),
            "HOTEND PID");
        ++thermal_count;
    }
    if (capabilities.bed_pid) {
        append_tool(
            thermal,
            sizeof(thermal),
            "BED PID");
        ++thermal_count;
    }
    if (capabilities.generic_heater_pid) {
        append_tool(
            thermal,
            sizeof(thermal),
            "GENERIC HEATER PID");
        ++thermal_count;
    }
    if (capabilities.pressure_advance) {
        append_tool(
            thermal,
            sizeof(thermal),
            "PRESSURE ADVANCE");
        ++thermal_count;
    }

    set_card(
        &s_calibration->thermal,
        thermal,
        thermal_count,
        0);

    if (s_calibration->pid_tune_button) {
        if (capabilities.hotend_pid ||
            capabilities.bed_pid ||
            capabilities.generic_heater_pid) {
            lv_obj_clear_flag(
                s_calibration->pid_tune_button,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(
                s_calibration->pid_tune_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    ui_calibration_pressure_advance_refresh(
        true,
        capabilities.pressure_advance);

    if (s_calibration->thermal.status) {
        if (capabilities.hotend_pid ||
            capabilities.bed_pid ||
            capabilities.generic_heater_pid ||
            capabilities.pressure_advance) {
            lv_obj_add_flag(
                s_calibration->thermal.status,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(
                s_calibration->thermal.status,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    char probe[176] = "";
    size_t probe_count = 0;

    if (capabilities.probe) {
        append_tool(
            probe,
            sizeof(probe),
            capabilities.load_cell_probe
                ? "LOAD CELL PROBE / Z"
                : capabilities.bltouch
                    ? "BLTOUCH / Z OFFSET"
                    : "PROBE / Z OFFSET");
        ++probe_count;
    }

    if (capabilities.calibration_macro_count > 0) {
        char macros[48];
        lv_snprintf(
            macros,
            sizeof(macros),
            "CUSTOM CALIBRATION MACROS (%u)",
            (unsigned)capabilities.calibration_macro_count);
        append_tool(probe, sizeof(probe), macros);
    }

    set_card(
        &s_calibration->probe,
        probe,
        probe_count,
        capabilities.calibration_macro_count);

    if (s_calibration->probe_z_button) {
        if (capabilities.probe) {
            lv_obj_clear_flag(
                s_calibration->probe_z_button,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(
                s_calibration->probe_z_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_calibration->custom_calibration_button) {
        if (capabilities.calibration_macro_count > 0) {
            lv_obj_clear_flag(
                s_calibration->custom_calibration_button,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(
                s_calibration->custom_calibration_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_calibration->probe.status) {
        if (capabilities.probe ||
            capabilities.calibration_macro_count > 0) {
            lv_obj_add_flag(
                s_calibration->probe.status,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(
                s_calibration->probe.status,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    s_calibration->device_generation =
        capabilities.device_generation;
    s_calibration->macro_generation =
        capabilities.macro_generation;
    s_calibration->command_generation =
        capabilities.command_generation;
}


static void close_screws_popup(void)
{
    if (!s_calibration) {
        return;
    }

    if (s_calibration->screws_popup) {
        lv_obj_t *popup =
            s_calibration->screws_popup;
        s_calibration->screws_popup = NULL;
        s_calibration->screws_results_label = NULL;
        lv_obj_delete(popup);
    }
}


static void close_screws_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_screws_popup();
}


static void show_screws_results_popup(void)
{
    if (!s_calibration) {
        return;
    }

    close_screws_popup();

    s_calibration->screws_popup =
        ui_popup_create(
            lv_layer_top(),
            660,
            430,
            UI_POPUP_STANDARD);

    if (!s_calibration->screws_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->screws_popup,
        "SCREWS TILT RESULTS",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->screws_popup,
        48);

    s_calibration->screws_results_label =
        ui_popup_add_body(
            s_calibration->screws_popup,
            "Waiting for Klipper adjustment results...",
            30,
            76,
            600);

    ui_popup_add_standard_footer_divider(
        s_calibration->screws_popup);

    ui_popup_add_footer_action(
        s_calibration->screws_popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        180,
        UI_POPUP_FOOTER_RIGHT,
        close_screws_popup_cb,
        NULL,
        NULL);
}


static void refresh_screws_results(void)
{
    calibration_session_controller_poll();

    if (!s_calibration ||
        !s_calibration->screws_results_label) {
        return;
    }

    calibration_session_snapshot_t *session =
        &s_calibration->session_snapshot;
    calibration_session_controller_snapshot(
        session);

    if (session->generation ==
        s_calibration->session_generation) {
        return;
    }

    s_calibration->session_generation =
        session->generation;

    switch (session->status) {
    case CALIBRATION_SESSION_RESULTS:
        lv_label_set_text(
            s_calibration->screws_results_label,
            session->results[0]
                ? session->results
                : "Klipper completed without adjustment lines.");
        break;

    case CALIBRATION_SESSION_ERROR: {
        lv_snprintf(
            s_calibration->screws_display,
            sizeof(s_calibration->screws_display),
            "Klipper reported an error:\n\n%s",
            session->results[0]
                ? session->results
                : "Unknown calibration error.");
        lv_label_set_text(
            s_calibration->screws_results_label,
            s_calibration->screws_display);
        break;
    }

    case CALIBRATION_SESSION_WAITING:
        lv_label_set_text(
            s_calibration->screws_results_label,
            "Waiting for Klipper adjustment results...");
        break;

    case CALIBRATION_SESSION_IDLE:
    default:
        break;
    }
}


static void run_screws_tilt_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration) {
        return;
    }

    const char *command =
        s_calibration->screws_home_required
            ? "G28\nSCREWS_TILT_CALCULATE"
            : "SCREWS_TILT_CALCULATE";

    uint32_t start_sequence =
        console_controller_latest_sequence();

    calibration_session_controller_begin_screws_tilt(
        start_sequence);
    console_controller_add_command(command);

    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the calibration command.");
    }

    show_screws_results_popup();
    refresh_screws_results();
}


static void screws_tilt_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration) {
        return;
    }

    calibration_capabilities_t capabilities;
    calibration_capability_controller_snapshot(
        &capabilities);

    if (!capabilities.screws_tilt) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!state.moonraker_ok ||
        !state.live_data_ok) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION UNAVAILABLE",
            "The active printer is offline or not ready.");
        return;
    }

    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Screws Tilt cannot run during a print.");
        return;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Clear the printer error before calibration.");
        return;
    }

    s_calibration->screws_home_required =
        !strchr(state.homed_axes, 'x') ||
        !strchr(state.homed_axes, 'y') ||
        !strchr(state.homed_axes, 'z');

    close_screws_popup();

    s_calibration->screws_popup =
        ui_popup_create(
            lv_layer_top(),
            600,
            360,
            UI_POPUP_STANDARD);

    if (!s_calibration->screws_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->screws_popup,
        "RUN SCREWS TILT?",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->screws_popup,
        48);

    char body[420];
    lv_snprintf(
        body,
        sizeof(body),
        "The toolhead will move to every configured screw and probe the bed.%s\n\n"
        "Clear the bed and motion area before continuing.\n\n"
        "This reads adjustment guidance only. SAVE_CONFIG will not run.",
        s_calibration->screws_home_required
            ? " XYZ is not homed, so all axes will home first."
            : "");

    ui_popup_add_body(
        s_calibration->screws_popup,
        body,
        28,
        76,
        544);

    ui_popup_add_standard_footer_divider(
        s_calibration->screws_popup);

    ui_popup_add_footer_action(
        s_calibration->screws_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_screws_popup_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_calibration->screws_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " RUN",
        170,
        UI_POPUP_FOOTER_RIGHT,
        run_screws_tilt_cb,
        NULL,
        NULL);
}


static void close_gantry_level_popup(void)
{
    if (!s_calibration) {
        return;
    }

    if (s_calibration->gantry_level_popup) {
        lv_obj_t *popup =
            s_calibration->gantry_level_popup;
        s_calibration->gantry_level_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_gantry_level_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_gantry_level_popup();
}


static void run_gantry_level_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration) {
        return;
    }

    const char *command =
        s_calibration->gantry_use_qgl
            ? "QUAD_GANTRY_LEVEL"
            : "Z_TILT_ADJUST";

    /*
     * The shared action gateway prefers a detected printer macro. The macro
     * owns printer-specific homing and sequencing. If no macro exists, keep
     * the proven standard Klipper fallback and add G28 only when required.
     */
    char fallback[48];
    if (s_calibration->gantry_home_required) {
        lv_snprintf(
            fallback,
            sizeof(fallback),
            "G28\n%s",
            command);
        command = fallback;
    }

    console_controller_add_command(command);

    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    close_gantry_level_popup();

    if (!sent) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION FAILED",
            "Moonraker did not accept the gantry-level command.");
        return;
    }

    ui_toast_v32_show(
        UI_STATUS_OK,
        s_calibration->gantry_use_qgl
            ? "QGL STARTED"
            : "Z TILT STARTED",
        "Klipper is running the leveling workflow. Results remain available in Console.");
}


static void gantry_level_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration) {
        return;
    }

    calibration_capabilities_t capabilities;
    calibration_capability_controller_snapshot(
        &capabilities);

    if (!capabilities.z_tilt &&
        !capabilities.quad_gantry_level) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!state.moonraker_ok ||
        !state.live_data_ok) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION UNAVAILABLE",
            "The active printer is offline or not ready.");
        return;
    }

    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Gantry leveling cannot run during a print.");
        return;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Clear the printer error before calibration.");
        return;
    }

    s_calibration->gantry_use_qgl =
        capabilities.quad_gantry_level;
    s_calibration->gantry_home_required =
        !strchr(state.homed_axes, 'x') ||
        !strchr(state.homed_axes, 'y') ||
        !strchr(state.homed_axes, 'z');

    close_gantry_level_popup();

    s_calibration->gantry_level_popup =
        ui_popup_create(
            lv_layer_top(),
            620,
            370,
            UI_POPUP_STANDARD);

    if (!s_calibration->gantry_level_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->gantry_level_popup,
        s_calibration->gantry_use_qgl
            ? "RUN QUAD GANTRY LEVEL?"
            : "RUN Z TILT?",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->gantry_level_popup,
        48);

    char body[430];
    lv_snprintf(
        body,
        sizeof(body),
        "The toolhead will probe multiple bed positions and the gantry will move.%s\n\n"
        "Clear the bed and motion area before continuing.\n\n"
        "This runs the detected Klipper leveling command. SAVE_CONFIG will not run.",
        s_calibration->gantry_home_required
            ? " XYZ is not homed, so all axes will home first."
            : "");

    ui_popup_add_body(
        s_calibration->gantry_level_popup,
        body,
        28,
        76,
        564);

    ui_popup_add_standard_footer_divider(
        s_calibration->gantry_level_popup);

    ui_popup_add_footer_action(
        s_calibration->gantry_level_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_gantry_level_popup_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_calibration->gantry_level_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " RUN",
        170,
        UI_POPUP_FOOTER_RIGHT,
        run_gantry_level_cb,
        NULL,
        NULL);
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


static void close_pid_popup(void)
{
    if (!s_calibration) {
        return;
    }

    if (s_calibration->pid_popup) {
        lv_obj_t *popup =
            s_calibration->pid_popup;
        s_calibration->pid_popup = NULL;
        s_calibration->pid_target_label = NULL;
        lv_obj_delete(popup);
    }
}


static void close_pid_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_pid_popup();
}


static bool pid_printer_ready(void)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!state.moonraker_ok ||
        !state.live_data_ok) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "PID UNAVAILABLE",
            "The active printer is offline or not ready.");
        return false;
    }

    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "PID BLOCKED",
            "PID calibration cannot run during a print.");
        return false;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_toast_v32_show(
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
    if (!s_calibration || !object_name) {
        return;
    }

    if (strncmp(object_name, "extruder", 8) == 0) {
        s_calibration->pid_target = 200;
        s_calibration->pid_target_min = 150;
        s_calibration->pid_target_max = 300;
    } else if (strcmp(object_name, "heater_bed") == 0) {
        s_calibration->pid_target = 60;
        s_calibration->pid_target_min = 40;
        s_calibration->pid_target_max = 130;
    } else {
        s_calibration->pid_target = 60;
        s_calibration->pid_target_min = 20;
        s_calibration->pid_target_max = 120;
    }
}


static void refresh_pid_target_label(void)
{
    if (!s_calibration ||
        !s_calibration->pid_target_label ||
        s_calibration->pid_selected_index >=
            s_calibration->pid_heater_count) {
        return;
    }

    char text[240];
    lv_snprintf(
        text,
        sizeof(text),
        "%s\n\nTARGET: %d C\n"
        "Allowed range: %d-%d C\n\n"
        "The heater will cycle repeatedly. Keep the machine attended.",
        s_calibration->pid_display_names[
            s_calibration->pid_selected_index],
        s_calibration->pid_target,
        s_calibration->pid_target_min,
        s_calibration->pid_target_max);
    lv_label_set_text(
        s_calibration->pid_target_label,
        text);
}


static void pid_adjust_target_cb(
    lv_event_t *event)
{
    if (!s_calibration || !event) {
        return;
    }

    int delta =
        (int)(intptr_t)lv_event_get_user_data(
            event);
    int target =
        s_calibration->pid_target + delta;

    if (target < s_calibration->pid_target_min) {
        target = s_calibration->pid_target_min;
    }
    if (target > s_calibration->pid_target_max) {
        target = s_calibration->pid_target_max;
    }

    s_calibration->pid_target = target;
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


static void show_calibration_results_popup(
    const char *title,
    const char *waiting_text);
static void refresh_calibration_results(void);


static void run_pid_tune_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration ||
        s_calibration->pid_selected_index >=
            s_calibration->pid_heater_count ||
        !pid_printer_ready()) {
        return;
    }

    const char *object_name =
        s_calibration->pid_object_names[
            s_calibration->pid_selected_index];
    const char *heater =
        pid_heater_argument(object_name);

    char command[176];
    int written = lv_snprintf(
        command,
        sizeof(command),
        "PID_CALIBRATE HEATER=%s TARGET=%d",
        heater ? heater : "",
        s_calibration->pid_target);

    if (written <= 0 ||
        (size_t)written >= sizeof(command)) {
        ui_toast_v32_show(
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
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    close_pid_popup();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the PID command.");
    }

    show_calibration_results_popup(
        "PID CALIBRATION",
        "The heater is cycling. Keep the machine attended.\n\n"
        "Apply & Restart will appear only after Klipper reports a successful result requiring SAVE_CONFIG.");
    refresh_calibration_results();
}


static void pid_tune_button_cb(
    lv_event_t *event);


static void back_to_pid_heaters_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration) {
        return;
    }

    /*
     * With multiple heaters, rebuild the live detected-heater selector.
     * A single-heater printer has no prior selection screen, so Back closes.
     */
    if (s_calibration->pid_heater_count <= 1) {
        close_pid_popup();
        return;
    }

    pid_tune_button_cb(NULL);
}


static void show_pid_target_popup(void)
{
    if (!s_calibration ||
        s_calibration->pid_selected_index >=
            s_calibration->pid_heater_count) {
        return;
    }

    close_pid_popup();

    const char *object_name =
        s_calibration->pid_object_names[
            s_calibration->pid_selected_index];
    pid_set_defaults(object_name);

    s_calibration->pid_popup =
        ui_popup_create(
            lv_layer_top(),
            650,
            440,
            UI_POPUP_STANDARD);

    if (!s_calibration->pid_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->pid_popup,
        "CONFIRM PID CALIBRATION",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->pid_popup,
        48);

    s_calibration->pid_target_label =
        ui_popup_add_body(
            s_calibration->pid_popup,
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
            s_calibration->pid_popup,
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
        s_calibration->pid_popup);

    ui_popup_add_footer_action(
        s_calibration->pid_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        back_to_pid_heaters_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_calibration->pid_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " RUN",
        170,
        UI_POPUP_FOOTER_RIGHT,
        run_pid_tune_cb,
        NULL,
        NULL);
}


static void pid_heater_selected_cb(
    lv_event_t *event)
{
    if (!s_calibration || !event) {
        return;
    }

    size_t index =
        (size_t)(uintptr_t)lv_event_get_user_data(
            event);

    if (index >= s_calibration->pid_heater_count) {
        return;
    }

    s_calibration->pid_selected_index = index;
    show_pid_target_popup();
}


static void pid_tune_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration || !pid_printer_ready()) {
        return;
    }

    s_calibration->pid_heater_count = 0;
    memset(
        s_calibration->pid_object_names,
        0,
        sizeof(s_calibration->pid_object_names));
    memset(
        s_calibration->pid_display_names,
        0,
        sizeof(s_calibration->pid_display_names));

    device_catalog_status_t status;
    device_catalog_controller_status(&status);

    for (size_t index = 0;
         index < status.stored_count &&
         s_calibration->pid_heater_count <
             PID_HEATER_MAX;
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
            s_calibration->pid_heater_count++;
        lv_snprintf(
            s_calibration->pid_object_names[
                output_index],
            DEVICE_CATALOG_OBJECT_NAME_MAX,
            "%s",
            device.object_name);
        lv_snprintf(
            s_calibration->pid_display_names[
                output_index],
            DEVICE_CATALOG_DISPLAY_NAME_MAX,
            "%s",
            device.display_name);
    }

    if (s_calibration->pid_heater_count == 0) {
        ui_toast_v32_show(
            UI_STATUS_WARNING,
            "NO PID HEATERS",
            "No PID-capable heater objects are available.");
        return;
    }

    if (s_calibration->pid_heater_count == 1) {
        s_calibration->pid_selected_index = 0;
        show_pid_target_popup();
        return;
    }

    close_pid_popup();

    s_calibration->pid_popup =
        ui_popup_create(
            lv_layer_top(),
            650,
            440,
            UI_POPUP_STANDARD);

    if (!s_calibration->pid_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->pid_popup,
        "SELECT HEATER FOR PID",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->pid_popup,
        48);

    lv_obj_t *list =
        ui_popup_add_list(
            s_calibration->pid_popup,
            28,
            68,
            594,
            292);

    if (list) {
        for (size_t index = 0;
             index < s_calibration->pid_heater_count;
             ++index) {
            ui_popup_add_selectable_row(
                list,
                s_calibration->pid_display_names[index],
                8,
                8 + (int)index * 54,
                558,
                46,
                pid_heater_selected_cb,
                (void *)(uintptr_t)index);
        }
    }

    ui_popup_add_standard_footer_divider(
        s_calibration->pid_popup);

    ui_popup_add_footer_action(
        s_calibration->pid_popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        170,
        UI_POPUP_FOOTER_RIGHT,
        close_pid_popup_cb,
        NULL,
        NULL);
}


static void close_save_confirm_popup(void)
{
    if (!s_calibration) {
        return;
    }

    if (s_calibration->save_confirm_popup) {
        lv_obj_t *popup =
            s_calibration->save_confirm_popup;
        s_calibration->save_confirm_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_save_confirm_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_save_confirm_popup();
}


static void close_calibration_results_popup(void)
{
    if (!s_calibration) {
        return;
    }

    close_save_confirm_popup();

    if (s_calibration->calibration_results_popup) {
        lv_obj_t *popup =
            s_calibration->calibration_results_popup;
        s_calibration->calibration_results_popup = NULL;
        s_calibration->calibration_results_label = NULL;
        s_calibration->apply_restart_button = NULL;
        lv_obj_delete(popup);
    }
}


static void close_calibration_results_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_calibration_results_popup();
}


static void run_save_config_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration || !pid_printer_ready()) {
        return;
    }

    calibration_session_snapshot_t snapshot;
    calibration_session_controller_snapshot(
        &snapshot);

    if (!snapshot.completed ||
        !snapshot.save_available ||
        snapshot.status != CALIBRATION_SESSION_RESULTS) {
        close_save_confirm_popup();
        ui_toast_v32_show(
            UI_STATUS_WARNING,
            "SAVE NOT AVAILABLE",
            "Klipper has not reported a completed save-worthy calibration.");
        return;
    }

    static const char command[] = "SAVE_CONFIG";
    console_controller_add_command(command);

    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    close_calibration_results_popup();

    if (!sent) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "SAVE FAILED",
            "Moonraker did not accept SAVE_CONFIG.");
        return;
    }

    calibration_session_controller_reset();
    ui_toast_v32_show(
        UI_STATUS_OK,
        "APPLYING CONFIGURATION",
        "Klipper is saving the calibration and restarting. The printer will reconnect automatically.");
}


static void apply_restart_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration) {
        return;
    }

    calibration_session_snapshot_t snapshot;
    calibration_session_controller_snapshot(
        &snapshot);

    if (!snapshot.completed ||
        !snapshot.save_available) {
        return;
    }

    close_save_confirm_popup();

    s_calibration->save_confirm_popup =
        ui_popup_create(
            lv_layer_top(),
            620,
            370,
            UI_POPUP_DANGER);

    if (!s_calibration->save_confirm_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->save_confirm_popup,
        "APPLY CALIBRATION & RESTART?",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->save_confirm_popup,
        48);
    ui_popup_add_body(
        s_calibration->save_confirm_popup,
        "PrinterHMI will run SAVE_CONFIG. Klipper will write the reported calibration values and restart.\\n\\n"
        "The printer will disconnect temporarily. Do not start a print until it reconnects as Ready.",
        28,
        76,
        564);
    ui_popup_add_standard_footer_divider(
        s_calibration->save_confirm_popup);
    ui_popup_add_footer_action(
        s_calibration->save_confirm_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_save_confirm_popup_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_calibration->save_confirm_popup,
        UI_POPUP_ACTION_DANGER,
        "APPLY & RESTART",
        210,
        UI_POPUP_FOOTER_RIGHT,
        run_save_config_cb,
        NULL,
        NULL);
}


static void show_calibration_results_popup(
    const char *title,
    const char *waiting_text)
{
    if (!s_calibration) {
        return;
    }

    close_calibration_results_popup();

    s_calibration->calibration_results_popup =
        ui_popup_create(
            lv_layer_top(),
            680,
            460,
            UI_POPUP_STANDARD);

    if (!s_calibration->calibration_results_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->calibration_results_popup,
        title ? title : "CALIBRATION RESULTS",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->calibration_results_popup,
        48);
    s_calibration->calibration_results_label =
        ui_popup_add_body(
            s_calibration->calibration_results_popup,
            waiting_text ? waiting_text : "Waiting for Klipper...",
            28,
            72,
            624);
    ui_popup_add_standard_footer_divider(
        s_calibration->calibration_results_popup);
    ui_popup_add_footer_action(
        s_calibration->calibration_results_popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        150,
        UI_POPUP_FOOTER_LEFT,
        close_calibration_results_popup_cb,
        NULL,
        NULL);
    s_calibration->apply_restart_button =
        ui_popup_add_footer_action(
            s_calibration->calibration_results_popup,
            UI_POPUP_ACTION_DANGER,
            "APPLY & RESTART",
            210,
            UI_POPUP_FOOTER_RIGHT,
            apply_restart_button_cb,
            NULL,
            NULL);

    if (s_calibration->apply_restart_button) {
        lv_obj_add_flag(
            s_calibration->apply_restart_button,
            LV_OBJ_FLAG_HIDDEN);
    }
}


static void refresh_calibration_results(void)
{
    calibration_session_controller_poll();

    if (!s_calibration ||
        !s_calibration->calibration_results_label) {
        return;
    }

    calibration_session_snapshot_t *snapshot =
        &s_calibration->session_snapshot;
    calibration_session_controller_snapshot(
        snapshot);

    if (snapshot->generation ==
        s_calibration->session_generation) {
        return;
    }

    s_calibration->session_generation =
        snapshot->generation;

    if (snapshot->status == CALIBRATION_SESSION_ERROR) {
        lv_snprintf(
            s_calibration->calibration_display,
            sizeof(s_calibration->calibration_display),
            "Calibration failed:\\n\\n%s",
            snapshot->results[0]
                ? snapshot->results
                : "Unknown Klipper error.");
        lv_label_set_text(
            s_calibration->calibration_results_label,
            s_calibration->calibration_display);
    } else if (snapshot->status ==
                   CALIBRATION_SESSION_RESULTS &&
               snapshot->results[0]) {
        lv_snprintf(
            s_calibration->calibration_display,
            sizeof(s_calibration->calibration_display),
            "%s%s",
            snapshot->results,
            snapshot->save_available
                ? "\\n\\nCalibration complete. Review the result, then Apply & Restart to save it."
                : "");
        lv_label_set_text(
            s_calibration->calibration_results_label,
            s_calibration->calibration_display);
    }

    if (s_calibration->apply_restart_button) {
        if (snapshot->status ==
                CALIBRATION_SESSION_RESULTS &&
            snapshot->completed &&
            snapshot->save_available) {
            lv_obj_clear_flag(
                s_calibration->apply_restart_button,
                LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(
                s_calibration->apply_restart_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }
}


static bool calibration_action_ready(
    const char *workflow)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!state.moonraker_ok ||
        !state.live_data_ok) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION UNAVAILABLE",
            "The active printer is offline or not ready.");
        return false;
    }

    if (strcmp(state.printer_state, "printing") == 0 ||
        strcmp(state.printer_state, "paused") == 0) {
        char detail[150];
        lv_snprintf(
            detail,
            sizeof(detail),
            "%s cannot run during a print.",
            workflow ? workflow : "Calibration");
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            detail);
        return false;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            "Clear the printer error before calibration.");
        return false;
    }

    return true;
}


static void close_probe_popup(void)
{
    if (!s_calibration) {
        return;
    }

    if (s_calibration->probe_popup) {
        lv_obj_t *popup = s_calibration->probe_popup;
        s_calibration->probe_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_probe_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_probe_popup();
}


static void send_probe_step_cb(
    lv_event_t *event)
{
    if (!s_calibration || !event) {
        return;
    }

    const char *command =
        (const char *)lv_event_get_user_data(event);

    if (!command || !command[0] ||
        !calibration_action_ready("Probe/Z calibration")) {
        return;
    }

    console_controller_add_command(command);
    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the TESTZ command.");
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "PROBE MOVE FAILED",
            "Moonraker did not accept the TESTZ command.");
    }
}


static void abort_probe_calibration_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration) {
        return;
    }

    static const char command[] = "ABORT";
    console_controller_add_command(command);
    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);
    ui_calibration_manual_probe_hide();
    close_calibration_results_popup();
    calibration_session_controller_reset();

    ui_toast_v32_show(
        sent ? UI_STATUS_INFO : UI_STATUS_DANGER,
        sent ? "PROBE CALIBRATION ABORTED" : "ABORT FAILED",
        sent
            ? "No calibration values were saved."
            : "Moonraker did not accept ABORT.");
}


static void accept_probe_calibration_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration ||
        !calibration_action_ready(
            "Probe/Z calibration")) {
        return;
    }

    static const char command[] = "ACCEPT";
    console_controller_add_command(command);
    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    ui_calibration_manual_probe_hide();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept ACCEPT.");
    }

    show_calibration_results_popup(
        "PROBE / Z RESULTS",
        "Waiting for Klipper to accept the measured Z offset.\\n\\n"
        "Apply & Restart will appear only if Klipper reports SAVE_CONFIG.");
    refresh_calibration_results();
}


static void show_probe_adjustment_popup(void)
{
    ui_calibration_manual_probe_show(
        "PROBE / Z OFFSET",
        "Use TESTZ steps to move the nozzle toward (-) or away from (+) the bed.\n"
        "Use a paper test, then ACCEPT only when the offset is correct.",
        "ACCEPT",
        send_probe_step_cb,
        abort_probe_calibration_cb,
        accept_probe_calibration_cb);
}


static void run_probe_calibration_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration ||
        !calibration_action_ready(
            "Probe/Z calibration")) {
        return;
    }

    const char *command =
        s_calibration->probe_home_required
            ? "G28\nPROBE_CALIBRATE"
            : "PROBE_CALIBRATE";
    uint32_t start_sequence =
        console_controller_latest_sequence();
    calibration_session_controller_begin(
        CALIBRATION_SESSION_PROBE_Z,
        start_sequence);
    console_controller_add_command(command);
    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    close_probe_popup();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept PROBE_CALIBRATE.");
        show_calibration_results_popup(
            "PROBE / Z RESULTS",
            "Probe/Z calibration could not be started.");
        refresh_calibration_results();
        return;
    }

    show_probe_adjustment_popup();
}


static void probe_z_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration ||
        !calibration_action_ready(
            "Probe/Z calibration")) {
        return;
    }

    calibration_capabilities_t capabilities;
    calibration_capability_controller_snapshot(
        &capabilities);
    if (!capabilities.probe) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    s_calibration->probe_home_required =
        !strchr(state.homed_axes, 'x') ||
        !strchr(state.homed_axes, 'y') ||
        !strchr(state.homed_axes, 'z');

    close_probe_popup();
    s_calibration->probe_popup =
        ui_popup_create(
            lv_layer_top(),
            630,
            390,
            UI_POPUP_STANDARD);

    if (!s_calibration->probe_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->probe_popup,
        "START PROBE / Z CALIBRATION?",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->probe_popup,
        48);

    char body[440];
    lv_snprintf(
        body,
        sizeof(body),
        "The toolhead will move to the configured probe position.%s\\n\\n"
        "Clear the bed and prepare a paper gauge. You will manually adjust TESTZ before accepting.\\n\\n"
        "Nothing is saved until Apply & Restart is confirmed.",
        s_calibration->probe_home_required
            ? " XYZ is not homed, so the printer's homing workflow will run first."
            : "");
    ui_popup_add_body(
        s_calibration->probe_popup,
        body,
        28,
        76,
        574);
    ui_popup_add_standard_footer_divider(
        s_calibration->probe_popup);
    ui_popup_add_footer_action(
        s_calibration->probe_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_probe_popup_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_calibration->probe_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " START",
        180,
        UI_POPUP_FOOTER_RIGHT,
        run_probe_calibration_cb,
        NULL,
        NULL);
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
    if (!s_calibration) {
        return;
    }

    if (s_calibration->custom_popup) {
        lv_obj_t *popup = s_calibration->custom_popup;
        s_calibration->custom_popup = NULL;
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

    if (!s_calibration ||
        s_calibration->custom_macro_selected >=
            s_calibration->custom_macro_count ||
        !calibration_action_ready(
            "Custom calibration")) {
        return;
    }

    const char *command =
        s_calibration->custom_macro_names[
            s_calibration->custom_macro_selected];
    uint32_t start_sequence =
        console_controller_latest_sequence();
    calibration_session_controller_begin(
        CALIBRATION_SESSION_CUSTOM,
        start_sequence);
    console_controller_add_command(command);
    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    close_custom_popup();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the custom calibration macro.");
    }

    show_calibration_results_popup(
        "CUSTOM CALIBRATION",
        sent
            ? "The selected printer macro is running. Monitor Console for its printer-specific instructions.\\n\\n"
              "Apply & Restart will appear only if Klipper reports SAVE_CONFIG."
            : "The selected custom calibration macro could not be started.");
    refresh_calibration_results();
}


static void custom_macro_selected_cb(
    lv_event_t *event)
{
    if (!s_calibration || !event) {
        return;
    }

    size_t index =
        (size_t)(uintptr_t)lv_event_get_user_data(
            event);
    if (index >= s_calibration->custom_macro_count) {
        return;
    }

    s_calibration->custom_macro_selected = index;
    close_custom_popup();
    s_calibration->custom_popup =
        ui_popup_create(
            lv_layer_top(),
            620,
            370,
            UI_POPUP_STANDARD);

    if (!s_calibration->custom_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->custom_popup,
        "RUN CUSTOM CALIBRATION?",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->custom_popup,
        48);

    char body[400];
    lv_snprintf(
        body,
        sizeof(body),
        "Macro: %s\\n\\n"
        "This is printer-defined behavior. It may move hardware or heat components. Keep the machine attended.\\n\\n"
        "PrinterHMI will not save automatically.",
        s_calibration->custom_macro_names[index]);
    ui_popup_add_body(
        s_calibration->custom_popup,
        body,
        28,
        76,
        564);
    ui_popup_add_standard_footer_divider(
        s_calibration->custom_popup);
    ui_popup_add_footer_action(
        s_calibration->custom_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_custom_popup_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_calibration->custom_popup,
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

    if (!s_calibration ||
        !calibration_action_ready(
            "Custom calibration")) {
        return;
    }

    s_calibration->custom_macro_count = 0;
    memset(
        s_calibration->custom_macro_names,
        0,
        sizeof(s_calibration->custom_macro_names));

    macro_controller_status_t status;
    macro_controller_status(&status);

    for (size_t index = 0;
         index < status.count &&
         s_calibration->custom_macro_count <
             CUSTOM_CALIBRATION_MACRO_MAX;
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
            s_calibration->custom_macro_names[
                s_calibration->custom_macro_count++],
            MACRO_CONTROLLER_NAME_MAX,
            "%s",
            name);
    }

    if (s_calibration->custom_macro_count == 0) {
        ui_toast_v32_show(
            UI_STATUS_WARNING,
            "NO CUSTOM CALIBRATIONS",
            "No matching public calibration macros are available.");
        return;
    }

    close_custom_popup();
    s_calibration->custom_popup =
        ui_popup_create(
            lv_layer_top(),
            650,
            450,
            UI_POPUP_STANDARD);

    if (!s_calibration->custom_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->custom_popup,
        "CUSTOM CALIBRATION MACROS",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->custom_popup,
        48);
    lv_obj_t *list =
        ui_popup_add_list(
            s_calibration->custom_popup,
            28,
            68,
            594,
            302);

    if (list) {
        for (size_t index = 0;
             index < s_calibration->custom_macro_count;
             ++index) {
            ui_popup_add_selectable_row(
                list,
                s_calibration->custom_macro_names[index],
                8,
                8 + (int)index * 54,
                558,
                46,
                custom_macro_selected_cb,
                (void *)(uintptr_t)index);
        }
    }

    ui_popup_add_standard_footer_divider(
        s_calibration->custom_popup);
    ui_popup_add_footer_action(
        s_calibration->custom_popup,
        UI_POPUP_ACTION_CLOSE,
        "CLOSE",
        170,
        UI_POPUP_FOOTER_RIGHT,
        close_custom_popup_cb,
        NULL,
        NULL);
}




static void close_axis_twist_popup(void)
{
    if (!s_calibration) {
        return;
    }

    if (s_calibration->axis_twist_popup) {
        lv_obj_t *popup =
            s_calibration->axis_twist_popup;
        s_calibration->axis_twist_popup = NULL;
        lv_obj_delete(popup);
    }
}


static void close_axis_twist_popup_cb(
    lv_event_t *event)
{
    (void)event;
    close_axis_twist_popup();
}


static void send_axis_twist_step_cb(
    lv_event_t *event)
{
    if (!s_calibration || !event) {
        return;
    }

    const char *command =
        (const char *)lv_event_get_user_data(event);

    if (!command || !command[0] ||
        !calibration_action_ready(
            "Axis Twist calibration")) {
        return;
    }

    console_controller_add_command(command);
    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the Axis Twist TESTZ command.");
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "AXIS TWIST MOVE FAILED",
            "Moonraker did not accept the TESTZ command.");
    }
}


static void abort_axis_twist_calibration_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration) {
        return;
    }

    static const char command[] = "ABORT";
    console_controller_add_command(command);
    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    ui_calibration_manual_probe_hide();
    close_calibration_results_popup();
    calibration_session_controller_reset();

    ui_toast_v32_show(
        sent ? UI_STATUS_INFO : UI_STATUS_DANGER,
        sent ? "AXIS TWIST ABORTED" : "ABORT FAILED",
        sent
            ? "No Axis Twist values were saved."
            : "Moonraker did not accept ABORT.");
}


static void accept_axis_twist_point_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration ||
        !calibration_action_ready(
            "Axis Twist calibration")) {
        return;
    }

    static const char command[] = "ACCEPT";
    console_controller_add_command(command);
    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept the Axis Twist sample.");
        return;
    }

    ui_calibration_manual_probe_set_status(
        "Point accepted. Klipper is moving to the next sample.\n"
        "When movement stops, repeat the paper test and adjust TESTZ.");
}


static void show_axis_twist_adjustment_popup(void)
{
    ui_calibration_manual_probe_show(
        "AXIS TWIST CALIBRATION",
        "At each Klipper sample point, use a paper test and TESTZ to set the nozzle height.\n"
        "Press ACCEPT for every point. The controls remain available until Klipper completes the sequence.",
        "ACCEPT POINT",
        send_axis_twist_step_cb,
        abort_axis_twist_calibration_cb,
        accept_axis_twist_point_cb);
}


static void run_axis_twist_calibration_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration ||
        !calibration_action_ready(
            "Axis Twist calibration")) {
        return;
    }

    const char *command =
        s_calibration->axis_twist_home_required
            ? "G28\nAXIS_TWIST_COMPENSATION_CALIBRATE"
            : "AXIS_TWIST_COMPENSATION_CALIBRATE";
    uint32_t start_sequence =
        console_controller_latest_sequence();

    calibration_session_controller_begin(
        CALIBRATION_SESSION_AXIS_TWIST,
        start_sequence);
    console_controller_add_command(command);
    bool sent =
        s_calibration->send_gcode &&
        s_calibration->send_gcode(command);

    close_axis_twist_popup();

    if (!sent) {
        calibration_session_controller_mark_error(
            "Moonraker did not accept AXIS_TWIST_COMPENSATION_CALIBRATE.");
        show_calibration_results_popup(
            "AXIS TWIST RESULTS",
            "Axis Twist calibration could not be started.");
        refresh_calibration_results();
        return;
    }

    show_axis_twist_adjustment_popup();
}


static void axis_twist_button_cb(
    lv_event_t *event)
{
    (void)event;

    if (!s_calibration ||
        !calibration_action_ready(
            "Axis Twist calibration")) {
        return;
    }

    calibration_capabilities_t capabilities;
    calibration_capability_controller_snapshot(
        &capabilities);
    if (!capabilities.axis_twist) {
        return;
    }

    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    s_calibration->axis_twist_home_required =
        !strchr(state.homed_axes, 'x') ||
        !strchr(state.homed_axes, 'y') ||
        !strchr(state.homed_axes, 'z');

    close_axis_twist_popup();
    s_calibration->axis_twist_popup =
        ui_popup_create(
            lv_layer_top(),
            630,
            400,
            UI_POPUP_STANDARD);

    if (!s_calibration->axis_twist_popup) {
        return;
    }

    ui_popup_add_title(
        s_calibration->axis_twist_popup,
        "START AXIS TWIST CALIBRATION?",
        false,
        4);
    ui_popup_add_header_divider(
        s_calibration->axis_twist_popup,
        48);

    char body[440];
    lv_snprintf(
        body,
        sizeof(body),
        "Klipper will probe and request a manual paper measurement at several points across the X axis.%s\n\n"
        "Clear the bed and motion area. Nothing is saved until Apply & Restart is confirmed.",
        s_calibration->axis_twist_home_required
            ? " XYZ is not homed, so homing will run first."
            : "");
    ui_popup_add_body(
        s_calibration->axis_twist_popup,
        body,
        28,
        76,
        574);
    ui_popup_add_standard_footer_divider(
        s_calibration->axis_twist_popup);
    ui_popup_add_footer_action(
        s_calibration->axis_twist_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_axis_twist_popup_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_calibration->axis_twist_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_PLAY " START",
        180,
        UI_POPUP_FOOTER_RIGHT,
        run_axis_twist_calibration_cb,
        NULL,
        NULL);
}


static void refresh_axis_twist_session(void)
{
    if (!s_calibration ||
        !ui_calibration_manual_probe_is_visible()) {
        return;
    }

    calibration_session_snapshot_t snapshot;
    calibration_session_controller_snapshot(
        &snapshot);

    if (snapshot.kind !=
            CALIBRATION_SESSION_AXIS_TWIST) {
        return;
    }

    if (snapshot.status == CALIBRATION_SESSION_ERROR) {
        ui_calibration_manual_probe_hide();
        show_calibration_results_popup(
            "AXIS TWIST RESULTS",
            "Klipper reported an Axis Twist calibration error.");
        refresh_calibration_results();
        return;
    }

    if (snapshot.status ==
            CALIBRATION_SESSION_RESULTS &&
        snapshot.completed &&
        snapshot.save_available) {
        ui_calibration_manual_probe_hide();
        show_calibration_results_popup(
            "AXIS TWIST RESULTS",
            "Klipper completed the Axis Twist calibration.");
        refresh_calibration_results();
    }
}


static void calibration_refresh_timer_cb(
    lv_timer_t *timer)
{
    (void)timer;

    if (!s_calibration || !s_calibration->root) {
        return;
    }

    calibration_capabilities_t capabilities;
    calibration_capability_controller_snapshot(
        &capabilities);

    if (capabilities.device_generation !=
            s_calibration->device_generation ||
        capabilities.macro_generation !=
            s_calibration->macro_generation ||
        capabilities.command_generation !=
            s_calibration->command_generation) {
        refresh_capabilities();
    }

    refresh_screws_results();
    refresh_axis_twist_session();
    refresh_calibration_results();
}


static void calibration_open_bed_mesh_event_cb(
    lv_event_t *event)
{
    (void)event;

    if (s_calibration &&
        s_calibration->open_bed_mesh) {
        s_calibration->open_bed_mesh();
    }
}


void ui_calibration_v32_show(
    ui_calibration_open_bed_mesh_cb_t open_bed_mesh_cb,
    ui_calibration_send_gcode_cb_t send_gcode_cb)
{
    if (!calibration_state_init()) {
        return;
    }

    s_calibration->open_bed_mesh =
        open_bed_mesh_cb;
    s_calibration->send_gcode =
        send_gcode_cb;

    if (s_calibration->root) {
        lv_obj_move_foreground(
            s_calibration->root);
        refresh_capabilities();
        return;
    }

    s_calibration->root = lv_obj_create(
        lv_screen_active());
    lv_obj_set_size(
        s_calibration->root,
        UI_PAGE_ROOT_WIDTH,
        UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(
        s_calibration->root,
        UI_PAGE_ROOT_X,
        UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(
        s_calibration->root,
        LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(
        s_calibration->root,
        UI_SURFACE_PAGE_DEEP);

    lv_obj_t *banner = ui_create_operator_banner(
        s_calibration->root,
        20,
        20,
        800,
        86,
        UI_STATUS_INFO);

    calibration_label(
        banner,
        "CALIBRATION",
        UI_FONT_TITLE,
        UI_TEXT_BRIGHT,
        20,
        15,
        520);

    calibration_label(
        banner,
        "Available workflows from the active printer's capabilities",
        UI_FONT_CAPTION,
        UI_TEXT_DIM,
        20,
        50,
        620);

    s_calibration->banner_status = calibration_label(
        banner,
        "WAITING FOR PRINTER",
        UI_FONT_CAPTION,
        UI_ACCENT_BRIGHT,
        590,
        32,
        190);

    lv_obj_set_style_text_align(
        s_calibration->banner_status,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_t *bed = calibration_card(
        s_calibration->root,
        "BED GEOMETRY",
        20,
        126,
        &s_calibration->bed);

    if (bed) {
        s_calibration->bed_mesh_button =
            ui_button_create(
                bed,
                UI_BUTTON_OUTLINED,
                "BED MESH");

        if (s_calibration->bed_mesh_button) {
            lv_obj_set_size(
                s_calibration->bed_mesh_button,
                82,
                38);
            lv_obj_align(
                s_calibration->bed_mesh_button,
                LV_ALIGN_BOTTOM_LEFT,
                16,
                -12);
            lv_obj_add_event_cb(
                s_calibration->bed_mesh_button,
                calibration_open_bed_mesh_event_cb,
                LV_EVENT_CLICKED,
                NULL);
            lv_obj_add_flag(
                s_calibration->bed_mesh_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        s_calibration->screws_tilt_button =
            ui_button_create(
                bed,
                UI_BUTTON_OUTLINED,
                "SCREWS");

        if (s_calibration->screws_tilt_button) {
            lv_obj_set_size(
                s_calibration->screws_tilt_button,
                82,
                38);
            lv_obj_align(
                s_calibration->screws_tilt_button,
                LV_ALIGN_BOTTOM_LEFT,
                108,
                -12);
            lv_obj_add_event_cb(
                s_calibration->screws_tilt_button,
                screws_tilt_button_cb,
                LV_EVENT_CLICKED,
                NULL);
            lv_obj_add_flag(
                s_calibration->screws_tilt_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        s_calibration->gantry_level_button =
            ui_button_create(
                bed,
                UI_BUTTON_OUTLINED,
                "LEVEL");

        if (s_calibration->gantry_level_button) {
            lv_obj_set_size(
                s_calibration->gantry_level_button,
                82,
                38);
            lv_obj_align(
                s_calibration->gantry_level_button,
                LV_ALIGN_BOTTOM_LEFT,
                200,
                -12);
            lv_obj_add_event_cb(
                s_calibration->gantry_level_button,
                gantry_level_button_cb,
                LV_EVENT_CLICKED,
                NULL);
            lv_obj_add_flag(
                s_calibration->gantry_level_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        s_calibration->axis_twist_button =
            ui_button_create(
                bed,
                UI_BUTTON_OUTLINED,
                "TWIST");

        if (s_calibration->axis_twist_button) {
            lv_obj_set_size(
                s_calibration->axis_twist_button,
                82,
                38);
            lv_obj_align(
                s_calibration->axis_twist_button,
                LV_ALIGN_BOTTOM_LEFT,
                292,
                -12);
            lv_obj_add_event_cb(
                s_calibration->axis_twist_button,
                axis_twist_button_cb,
                LV_EVENT_CLICKED,
                NULL);
            lv_obj_add_flag(
                s_calibration->axis_twist_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_t *motion = calibration_card(
        s_calibration->root,
        "MOTION",
        430,
        126,
        &s_calibration->motion);

    if (motion) {
        ui_calibration_motion_create(
            motion,
            s_calibration->send_gcode,
            calibration_action_ready,
            show_calibration_results_popup,
            refresh_calibration_results);
    }

    lv_obj_t *thermal = calibration_card(
        s_calibration->root,
        "TEMPERATURE & EXTRUSION",
        20,
        322,
        &s_calibration->thermal);

    if (thermal) {
        s_calibration->pid_tune_button =
            ui_button_create(
                thermal,
                UI_BUTTON_OUTLINED,
                "PID TUNE");

        if (s_calibration->pid_tune_button) {
            lv_obj_set_size(
                s_calibration->pid_tune_button,
                150,
                38);
            lv_obj_align(
                s_calibration->pid_tune_button,
                LV_ALIGN_BOTTOM_RIGHT,
                -16,
                -12);
            lv_obj_add_event_cb(
                s_calibration->pid_tune_button,
                pid_tune_button_cb,
                LV_EVENT_CLICKED,
                NULL);
            lv_obj_add_flag(
                s_calibration->pid_tune_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        ui_calibration_pressure_advance_create(
            thermal,
            s_calibration->send_gcode,
            calibration_action_ready);
    }

    lv_obj_t *probe = calibration_card(
        s_calibration->root,
        "PROBE, Z & CUSTOM",
        430,
        322,
        &s_calibration->probe);

    if (probe) {
        s_calibration->probe_z_button =
            ui_button_create(
                probe,
                UI_BUTTON_OUTLINED,
                "PROBE / Z");

        if (s_calibration->probe_z_button) {
            lv_obj_set_size(
                s_calibration->probe_z_button,
                166,
                38);
            lv_obj_align(
                s_calibration->probe_z_button,
                LV_ALIGN_BOTTOM_LEFT,
                16,
                -12);
            lv_obj_add_event_cb(
                s_calibration->probe_z_button,
                probe_z_button_cb,
                LV_EVENT_CLICKED,
                NULL);
            lv_obj_add_flag(
                s_calibration->probe_z_button,
                LV_OBJ_FLAG_HIDDEN);
        }

        s_calibration->custom_calibration_button =
            ui_button_create(
                probe,
                UI_BUTTON_OUTLINED,
                "CUSTOM");

        if (s_calibration->custom_calibration_button) {
            lv_obj_set_size(
                s_calibration->custom_calibration_button,
                166,
                38);
            lv_obj_align(
                s_calibration->custom_calibration_button,
                LV_ALIGN_BOTTOM_RIGHT,
                -16,
                -12);
            lv_obj_add_event_cb(
                s_calibration->custom_calibration_button,
                custom_calibration_button_cb,
                LV_EVENT_CLICKED,
                NULL);
            lv_obj_add_flag(
                s_calibration->custom_calibration_button,
                LV_OBJ_FLAG_HIDDEN);
        }
    }

    s_calibration->device_generation = UINT32_MAX;
    s_calibration->macro_generation = UINT32_MAX;
    refresh_capabilities();

    s_calibration->refresh_timer = lv_timer_create(
        calibration_refresh_timer_cb,
        500,
        NULL);
}


void ui_calibration_v32_hide(void)
{
    if (!s_calibration) {
        return;
    }

    if (s_calibration->refresh_timer) {
        lv_timer_delete(
            s_calibration->refresh_timer);
        s_calibration->refresh_timer = NULL;
    }

    close_screws_popup();
    close_gantry_level_popup();
    close_pid_popup();
    ui_calibration_pressure_advance_hide();
    close_probe_popup();
    close_custom_popup();
    ui_calibration_motion_hide();
    close_axis_twist_popup();
    ui_calibration_manual_probe_hide();
    close_calibration_results_popup();

    if (s_calibration->root) {
        lv_obj_delete(s_calibration->root);
    }

    s_calibration->root = NULL;
    s_calibration->banner_status = NULL;
    s_calibration->bed_mesh_button = NULL;
    s_calibration->screws_tilt_button = NULL;
    s_calibration->gantry_level_button = NULL;
    s_calibration->axis_twist_button = NULL;
    s_calibration->pid_tune_button = NULL;
    s_calibration->probe_z_button = NULL;
    s_calibration->custom_calibration_button = NULL;
    s_calibration->axis_twist_popup = NULL;
    s_calibration->send_gcode = NULL;
    s_calibration->screws_home_required = false;
    s_calibration->gantry_home_required = false;
    s_calibration->gantry_use_qgl = false;
    s_calibration->axis_twist_home_required = false;
    s_calibration->pid_heater_count = 0;
    s_calibration->pid_selected_index = 0;
    s_calibration->pid_target = 0;
    s_calibration->pid_target_min = 0;
    s_calibration->pid_target_max = 0;
    s_calibration->probe_home_required = false;
    s_calibration->custom_macro_count = 0;
    s_calibration->custom_macro_selected = 0;
    memset(
        s_calibration->custom_macro_names,
        0,
        sizeof(s_calibration->custom_macro_names));
    memset(
        s_calibration->pid_object_names,
        0,
        sizeof(s_calibration->pid_object_names));
    memset(
        s_calibration->pid_display_names,
        0,
        sizeof(s_calibration->pid_display_names));
    memset(
        &s_calibration->session_snapshot,
        0,
        sizeof(s_calibration->session_snapshot));
    memset(
        s_calibration->screws_display,
        0,
        sizeof(s_calibration->screws_display));
    memset(
        s_calibration->calibration_display,
        0,
        sizeof(s_calibration->calibration_display));
    s_calibration->session_generation = 0;
    memset(
        &s_calibration->bed,
        0,
        sizeof(s_calibration->bed));
    memset(
        &s_calibration->motion,
        0,
        sizeof(s_calibration->motion));
    memset(
        &s_calibration->thermal,
        0,
        sizeof(s_calibration->thermal));
    memset(
        &s_calibration->probe,
        0,
        sizeof(s_calibration->probe));
    s_calibration->device_generation = 0;
    s_calibration->macro_generation = 0;
}
