#include "ui_calibration.h"
#include "ui_text.h"

#include <stdbool.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "calibration_capability_controller.h"
#include "calibration_session_controller.h"
#include "ui_calibration_motion.h"
#include "ui_calibration_layout.h"
#include "ui_calibration_geometry.h"
#include "ui_calibration_custom.h"
#include "ui_calibration_manual_probe.h"
#include "ui_calibration_pressure_advance.h"
#include "ui_calibration_pid.h"
#include "ui_calibration_results.h"
#include "console_controller.h"
#include "device_catalog_controller.h"
#include "macro_controller.h"
#include "moonraker.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include "ui_button.h"
#include "ui_page_geometry.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"
#include "ui_widgets.h"

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
    ui_calibration_card_refs_t bed;
    ui_calibration_card_refs_t motion;
    ui_calibration_card_refs_t thermal;
    ui_calibration_card_refs_t probe;
    lv_timer_t *refresh_timer;
    ui_calibration_open_bed_mesh_cb_t open_bed_mesh;
    ui_calibration_send_gcode_cb_t send_gcode;
    bool screws_home_required;
    bool gantry_home_required;
    bool gantry_use_qgl;
    bool axis_twist_home_required;
    char pid_object_names[UI_CALIBRATION_PID_HEATER_MAX]
                         [DEVICE_CATALOG_OBJECT_NAME_MAX];
    char pid_display_names[UI_CALIBRATION_PID_HEATER_MAX]
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
    ui_calibration_geometry_context_t geometry;
    ui_calibration_custom_context_t custom;
    ui_calibration_pid_context_t pid;
    ui_calibration_results_context_t results;
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

    ui_calibration_layout_set_action_label(
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
                ui_text("BED GEOMETRY TOOLS READY"));
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
            ui_text("WAITING FOR PRINTER"));

        ui_calibration_card_refs_t *cards[] = {
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
                ui_text("Waiting for active-printer discovery."));
            lv_label_set_text(
                cards[index]->status,
                ui_text("AWAITING DISCOVERY"));
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
        ui_calibration_layout_append_tool(bed, sizeof(bed), "BED MESH");
        ++bed_count;
    }
    if (capabilities.screws_tilt) {
        ui_calibration_layout_append_tool(bed, sizeof(bed), "SCREWS TILT");
        ++bed_count;
    }
    if (capabilities.quad_gantry_level) {
        ui_calibration_layout_append_tool(bed, sizeof(bed), "QGL");
        ++bed_count;
    }
    if (capabilities.z_tilt) {
        ui_calibration_layout_append_tool(bed, sizeof(bed), "Z TILT");
        ++bed_count;
    }
    if (capabilities.axis_twist) {
        ui_calibration_layout_append_tool(bed, sizeof(bed), "AXIS TWIST");
        ++bed_count;
    }

    ui_calibration_layout_set_card(
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
                        ? ui_text("QGL")
                        : ui_text("Z TILT"));
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
        ui_calibration_layout_append_tool(
            motion,
            sizeof(motion),
            "INPUT SHAPER");
        ++motion_count;
    }
    if (capabilities.accelerometer) {
        ui_calibration_layout_append_tool(
            motion,
            sizeof(motion),
            "ACCELEROMETER");
        ++motion_count;
    }

    ui_calibration_layout_set_card(
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
        ui_calibration_layout_append_tool(
            thermal,
            sizeof(thermal),
            "HOTEND PID");
        ++thermal_count;
    }
    if (capabilities.bed_pid) {
        ui_calibration_layout_append_tool(
            thermal,
            sizeof(thermal),
            "BED PID");
        ++thermal_count;
    }
    if (capabilities.generic_heater_pid) {
        ui_calibration_layout_append_tool(
            thermal,
            sizeof(thermal),
            "GENERIC HEATER PID");
        ++thermal_count;
    }
    if (capabilities.pressure_advance) {
        ui_calibration_layout_append_tool(
            thermal,
            sizeof(thermal),
            "PRESSURE ADVANCE");
        ++thermal_count;
    }

    ui_calibration_layout_set_card(
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
        ui_calibration_layout_append_tool(
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
        ui_calibration_layout_append_tool(probe, sizeof(probe), macros);
    }

    ui_calibration_layout_set_card(
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


static bool calibration_action_ready(
    const char *workflow)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!state.moonraker_ok ||
        !state.live_data_ok) {
        ui_toast_show(
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
        ui_toast_show(
            UI_STATUS_DANGER,
            "CALIBRATION BLOCKED",
            detail);
        return false;
    }

    if (strcmp(state.printer_state, "error") == 0 ||
        strcmp(state.printer_state, "shutdown") == 0) {
        ui_toast_show(
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
        ui_toast_show(
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
    ui_calibration_results_close();
    calibration_session_controller_reset();

    ui_toast_show(
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

    ui_calibration_results_show(
        "PROBE / Z RESULTS",
        "Waiting for Klipper to accept the measured Z offset.\\n\\n"
        "Apply & Restart will appear only if Klipper reports SAVE_CONFIG.");
    ui_calibration_results_refresh();
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
        ui_calibration_results_show(
            "PROBE / Z RESULTS",
            "Probe/Z calibration could not be started.");
        ui_calibration_results_refresh();
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
        ui_text("START PROBE / Z CALIBRATION?"),
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
        ui_toast_show(
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
    ui_calibration_results_close();
    calibration_session_controller_reset();

    ui_toast_show(
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
        ui_calibration_results_show(
            "AXIS TWIST RESULTS",
            "Axis Twist calibration could not be started.");
        ui_calibration_results_refresh();
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
        ui_text("START AXIS TWIST CALIBRATION?"),
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
        ui_calibration_results_show(
            "AXIS TWIST RESULTS",
            "Klipper reported an Axis Twist calibration error.");
        ui_calibration_results_refresh();
        return;
    }

    if (snapshot.status ==
            CALIBRATION_SESSION_RESULTS &&
        snapshot.completed &&
        snapshot.save_available) {
        ui_calibration_manual_probe_hide();
        ui_calibration_results_show(
            "AXIS TWIST RESULTS",
            "Klipper completed the Axis Twist calibration.");
        ui_calibration_results_refresh();
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

    ui_calibration_geometry_refresh();
    refresh_axis_twist_session();
    ui_calibration_results_refresh();
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


void ui_calibration_show(
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
    s_calibration->geometry = (ui_calibration_geometry_context_t){
        .screws_popup = &s_calibration->screws_popup,
        .gantry_popup = &s_calibration->gantry_level_popup,
        .screws_results_label = &s_calibration->screws_results_label,
        .screws_home_required = &s_calibration->screws_home_required,
        .gantry_home_required = &s_calibration->gantry_home_required,
        .gantry_use_qgl = &s_calibration->gantry_use_qgl,
        .screws_display = s_calibration->screws_display,
        .screws_display_size = sizeof(s_calibration->screws_display),
        .session_snapshot = &s_calibration->session_snapshot,
        .session_generation = &s_calibration->session_generation,
        .send_gcode = s_calibration->send_gcode,
    };
    ui_calibration_geometry_init(&s_calibration->geometry);
    s_calibration->custom = (ui_calibration_custom_context_t){
        .popup = &s_calibration->custom_popup,
        .names = s_calibration->custom_macro_names,
        .names_capacity = CUSTOM_CALIBRATION_MACRO_MAX,
        .count = &s_calibration->custom_macro_count,
        .selected = &s_calibration->custom_macro_selected,
        .send = s_calibration->send_gcode,
        .ready = calibration_action_ready,
        .show_results = ui_calibration_results_show,
        .refresh_results = ui_calibration_results_refresh,
    };
    ui_calibration_custom_init(&s_calibration->custom);
    s_calibration->results = (ui_calibration_results_context_t){
        .save_confirm_popup = &s_calibration->save_confirm_popup,
        .results_popup = &s_calibration->calibration_results_popup,
        .results_label = &s_calibration->calibration_results_label,
        .apply_restart_button = &s_calibration->apply_restart_button,
        .session_snapshot = &s_calibration->session_snapshot,
        .session_generation = &s_calibration->session_generation,
        .display = s_calibration->calibration_display,
        .display_size = sizeof(s_calibration->calibration_display),
        .send_gcode = s_calibration->send_gcode,
    };
    ui_calibration_results_init(&s_calibration->results);
    s_calibration->pid = (ui_calibration_pid_context_t){
        .popup = &s_calibration->pid_popup,
        .target_label = &s_calibration->pid_target_label,
        .object_names = s_calibration->pid_object_names,
        .display_names = s_calibration->pid_display_names,
        .heater_capacity = UI_CALIBRATION_PID_HEATER_MAX,
        .heater_count = &s_calibration->pid_heater_count,
        .selected_index = &s_calibration->pid_selected_index,
        .target = &s_calibration->pid_target,
        .target_min = &s_calibration->pid_target_min,
        .target_max = &s_calibration->pid_target_max,
        .send_gcode = s_calibration->send_gcode,
        .show_results = ui_calibration_results_show,
        .refresh_results = ui_calibration_results_refresh,
    };
    ui_calibration_pid_init(&s_calibration->pid);

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

    ui_calibration_layout_label(
        banner,
        "CALIBRATION",
        UI_FONT_TITLE,
        UI_TEXT_BRIGHT,
        20,
        15,
        520);

    ui_calibration_layout_label(
        banner,
        "Available workflows from the active printer's capabilities",
        UI_FONT_CAPTION,
        UI_TEXT_DIM,
        20,
        50,
        620);

    s_calibration->banner_status = ui_calibration_layout_label(
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

    lv_obj_t *bed = ui_calibration_layout_card(
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
                ui_calibration_geometry_screws_event,
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
                ui_calibration_geometry_gantry_event,
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

    lv_obj_t *motion = ui_calibration_layout_card(
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
            ui_calibration_results_show,
            ui_calibration_results_refresh);
    }

    lv_obj_t *thermal = ui_calibration_layout_card(
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
                ui_calibration_pid_event,
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

    lv_obj_t *probe = ui_calibration_layout_card(
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
                ui_calibration_custom_event,
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


void ui_calibration_hide(void)
{
    if (!s_calibration) {
        return;
    }

    if (s_calibration->refresh_timer) {
        lv_timer_delete(
            s_calibration->refresh_timer);
        s_calibration->refresh_timer = NULL;
    }

    ui_calibration_geometry_close();
    ui_calibration_pid_close();
    ui_calibration_pressure_advance_hide();
    close_probe_popup();
    ui_calibration_custom_close();
    ui_calibration_motion_hide();
    close_axis_twist_popup();
    ui_calibration_manual_probe_hide();
    ui_calibration_results_close();

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
