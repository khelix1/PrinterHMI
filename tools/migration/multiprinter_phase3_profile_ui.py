from pathlib import Path

ui_h_path = Path("main/ui_printer_profiles.h")
ui_c_path = Path("main/ui_printer_profiles.c")
network_c_path = Path("main/ui_network_v32.c")
main_path = Path("main/main.c")
cmake_path = Path("main/CMakeLists.txt")

network = network_c_path.read_text()
main = main_path.read_text()
cmake = cmake_path.read_text()

ui_header = r'''#pragma once

#include "lvgl.h"

/*
 * Called only when the active printer endpoint changes.
 * The application owns clearing runtime state and restarting polling.
 */
typedef void (*ui_printer_profiles_active_changed_cb_t)(void);

void ui_printer_profiles_show(
    ui_printer_profiles_active_changed_cb_t active_changed_cb);

void ui_printer_profiles_close_all(void);
'''

ui_source = r'''#include "ui_printer_profiles.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "moonraker_config_controller.h"
#include "ui_popup.h"

static lv_obj_t *s_manager_popup = NULL;
static lv_obj_t *s_manager_list = NULL;
static lv_obj_t *s_manager_status = NULL;
static lv_obj_t *s_profile_rows[
    MOONRAKER_CONFIG_MAX_PROFILES];

static lv_obj_t *s_editor_popup = NULL;
static lv_obj_t *s_editor_name = NULL;
static lv_obj_t *s_editor_host = NULL;
static lv_obj_t *s_editor_port = NULL;
static lv_obj_t *s_editor_keyboard = NULL;
static lv_obj_t *s_editor_status = NULL;

static int s_selected_profile = 0;
static int s_editor_profile = -1;

static ui_printer_profiles_active_changed_cb_t
    s_active_changed_cb = NULL;


static void editor_close(void)
{
    if (s_editor_popup) {
        lv_obj_delete(s_editor_popup);
    }

    s_editor_popup = NULL;
    s_editor_name = NULL;
    s_editor_host = NULL;
    s_editor_port = NULL;
    s_editor_keyboard = NULL;
    s_editor_status = NULL;
    s_editor_profile = -1;
}


void ui_printer_profiles_close_all(void)
{
    editor_close();

    if (s_manager_popup) {
        lv_obj_delete(s_manager_popup);
    }

    s_manager_popup = NULL;
    s_manager_list = NULL;
    s_manager_status = NULL;

    memset(
        s_profile_rows,
        0,
        sizeof(s_profile_rows));

    s_active_changed_cb = NULL;
}


static void manager_close_cb(lv_event_t *event)
{
    (void)event;
    ui_printer_profiles_close_all();
}


static void editor_cancel_cb(lv_event_t *event)
{
    (void)event;
    editor_close();
}


static void manager_set_status(
    const char *text)
{
    if (s_manager_status) {
        lv_label_set_text(
            s_manager_status,
            text ? text : "");
    }
}


static void editor_set_status(
    const char *text)
{
    if (s_editor_status) {
        lv_label_set_text(
            s_editor_status,
            text ? text : "");
    }
}


static void manager_refresh_selection(void)
{
    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        if (s_profile_rows[index]) {
            ui_popup_set_selectable_row_selected(
                s_profile_rows[index],
                index == s_selected_profile);
        }
    }
}


static void profile_row_clicked_cb(
    lv_event_t *event)
{
    int profile_index =
        (int)(intptr_t)
            lv_event_get_user_data(event);

    if (profile_index < 0 ||
        profile_index >=
            MOONRAKER_CONFIG_MAX_PROFILES) {
        return;
    }

    s_selected_profile =
        profile_index;

    manager_refresh_selection();

    const moonraker_profile_t *profile =
        moonraker_config_profile(
            profile_index);

    char status[160];

    if (profile && profile->configured) {
        snprintf(
            status,
            sizeof(status),
            "Selected %s  |  %s:%d",
            profile->name,
            profile->host,
            profile->port);
    } else {
        snprintf(
            status,
            sizeof(status),
            "Printer %d is empty. Choose EDIT / ADD.",
            profile_index + 1);
    }

    manager_set_status(status);
}


static void manager_rebuild_rows(void)
{
    if (!s_manager_list) {
        return;
    }

    lv_obj_clean(s_manager_list);

    memset(
        s_profile_rows,
        0,
        sizeof(s_profile_rows));

    int active =
        moonraker_config_active_profile_index();

    for (int index = 0;
         index < MOONRAKER_CONFIG_MAX_PROFILES;
         ++index) {
        const moonraker_profile_t *profile =
            moonraker_config_profile(index);

        char row_text[180];

        if (profile && profile->configured) {
            snprintf(
                row_text,
                sizeof(row_text),
                "%s  %s   %s:%d",
                index == active
                    ? LV_SYMBOL_OK
                    : " ",
                profile->name,
                profile->host,
                profile->port);
        } else {
            snprintf(
                row_text,
                sizeof(row_text),
                "    PRINTER %d   EMPTY SLOT",
                index + 1);
        }

        s_profile_rows[index] =
            ui_popup_add_selectable_row(
                s_manager_list,
                row_text,
                0,
                index * 62,
                680,
                56,
                profile_row_clicked_cb,
                (void *)(intptr_t)index);
    }

    manager_refresh_selection();
}


static void manager_select_cb(
    lv_event_t *event)
{
    (void)event;

    const moonraker_profile_t *profile =
        moonraker_config_profile(
            s_selected_profile);

    if (!profile || !profile->configured) {
        manager_set_status(
            "The selected slot is empty. Choose EDIT / ADD first.");
        return;
    }

    int previous_active =
        moonraker_config_active_profile_index();

    if (!moonraker_config_select_profile(
            s_selected_profile)) {
        manager_set_status(
            "Unable to activate the selected printer.");
        return;
    }

    manager_rebuild_rows();

    char status[160];

    snprintf(
        status,
        sizeof(status),
        "Active printer: %s  |  %s:%d",
        moonraker_config_active_profile_name(),
        moonraker_config_host(),
        moonraker_config_port());

    manager_set_status(status);

    if (previous_active !=
            moonraker_config_active_profile_index() &&
        s_active_changed_cb) {
        s_active_changed_cb();
    }
}


static void editor_field_focused_cb(
    lv_event_t *event)
{
    if (!s_editor_keyboard) {
        return;
    }

    lv_obj_t *field =
        lv_event_get_target(event);

    if (!field) {
        return;
    }

    lv_keyboard_set_textarea(
        s_editor_keyboard,
        field);

    lv_keyboard_set_mode(
        s_editor_keyboard,
        field == s_editor_port
            ? LV_KEYBOARD_MODE_NUMBER
            : LV_KEYBOARD_MODE_TEXT_LOWER);
}


static void editor_save_cb(
    lv_event_t *event)
{
    (void)event;

    if (s_editor_profile < 0 ||
        !s_editor_name ||
        !s_editor_host ||
        !s_editor_port) {
        return;
    }

    const char *name =
        lv_textarea_get_text(
            s_editor_name);

    const char *host =
        lv_textarea_get_text(
            s_editor_host);

    const char *port_text =
        lv_textarea_get_text(
            s_editor_port);

    if (!host || !host[0]) {
        editor_set_status(
            "A Moonraker host or IP address is required.");
        return;
    }

    char *port_end = NULL;

    long port =
        strtol(
            port_text ? port_text : "",
            &port_end,
            10);

    if (!port_text ||
        !port_text[0] ||
        !port_end ||
        *port_end != '\0' ||
        port <= 0 ||
        port >= 65536) {
        editor_set_status(
            "Port must be between 1 and 65535.");
        return;
    }

    bool active_profile_edited =
        s_editor_profile ==
            moonraker_config_active_profile_index();

    if (!moonraker_config_save_profile(
            s_editor_profile,
            name,
            host,
            (int)port)) {
        editor_set_status(
            "Unable to save this printer profile.");
        return;
    }

    editor_close();
    manager_rebuild_rows();

    manager_set_status(
        active_profile_edited
            ? "Active printer updated. Reconnecting..."
            : "Printer profile saved.");

    if (active_profile_edited &&
        s_active_changed_cb) {
        s_active_changed_cb();
    }
}


static void manager_edit_cb(
    lv_event_t *event)
{
    (void)event;

    if (s_editor_popup) {
        lv_obj_move_foreground(
            s_editor_popup);
        return;
    }

    const moonraker_profile_t *profile =
        moonraker_config_profile(
            s_selected_profile);

    char default_name[
        MOONRAKER_CONFIG_NAME_LENGTH];

    snprintf(
        default_name,
        sizeof(default_name),
        "Printer %d",
        s_selected_profile + 1);

    const char *name =
        profile &&
        profile->configured &&
        profile->name[0]
            ? profile->name
            : default_name;

    const char *host =
        profile &&
        profile->configured
            ? profile->host
            : "";

    int port =
        profile &&
        profile->configured
            ? profile->port
            : 7125;

    char port_text[16];

    snprintf(
        port_text,
        sizeof(port_text),
        "%d",
        port);

    s_editor_profile =
        s_selected_profile;

    s_editor_popup =
        ui_popup_create(
            lv_screen_active(),
            800,
            560,
            UI_POPUP_STANDARD);

    if (!s_editor_popup) {
        s_editor_profile = -1;
        return;
    }

    ui_popup_add_title(
        s_editor_popup,
        profile && profile->configured
            ? "EDIT PRINTER"
            : "ADD PRINTER",
        false,
        0);

    ui_popup_add_header_divider(
        s_editor_popup,
        44);

    ui_popup_add_caption(
        s_editor_popup,
        "PRINTER NAME",
        28,
        68,
        150);

    s_editor_name =
        ui_popup_add_textarea(
            s_editor_popup,
            560,
            48,
            LV_ALIGN_TOP_LEFT,
            200,
            55,
            true,
            false,
            MOONRAKER_CONFIG_NAME_LENGTH - 1,
            "Printer name",
            name,
            NULL);

    ui_popup_add_caption(
        s_editor_popup,
        "MOONRAKER HOST",
        28,
        128,
        160);

    s_editor_host =
        ui_popup_add_textarea(
            s_editor_popup,
            560,
            48,
            LV_ALIGN_TOP_LEFT,
            200,
            115,
            true,
            false,
            MOONRAKER_CONFIG_HOST_LENGTH - 1,
            "IP address or hostname",
            host,
            NULL);

    ui_popup_add_caption(
        s_editor_popup,
        "MOONRAKER PORT",
        28,
        188,
        160);

    s_editor_port =
        ui_popup_add_textarea(
            s_editor_popup,
            220,
            48,
            LV_ALIGN_TOP_LEFT,
            200,
            175,
            true,
            false,
            5,
            "7125",
            port_text,
            "0123456789");

    s_editor_status =
        ui_popup_add_status_label(
            s_editor_popup,
            "Changes are saved to this printer profile.",
            440,
            179,
            320);

    s_editor_keyboard =
        ui_popup_add_keyboard(
            s_editor_popup,
            s_editor_name,
            720,
            235,
            LV_ALIGN_TOP_MID,
            0,
            235,
            LV_KEYBOARD_MODE_TEXT_LOWER);

    if (s_editor_name) {
        lv_obj_add_event_cb(
            s_editor_name,
            editor_field_focused_cb,
            LV_EVENT_FOCUSED,
            NULL);
    }

    if (s_editor_host) {
        lv_obj_add_event_cb(
            s_editor_host,
            editor_field_focused_cb,
            LV_EVENT_FOCUSED,
            NULL);
    }

    if (s_editor_port) {
        lv_obj_add_event_cb(
            s_editor_port,
            editor_field_focused_cb,
            LV_EVENT_FOCUSED,
            NULL);
    }

    ui_popup_add_footer_divider(
        s_editor_popup,
        490);

    ui_popup_add_action_at(
        s_editor_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        28,
        505,
        180,
        42,
        editor_cancel_cb,
        NULL,
        NULL);

    ui_popup_add_action_at(
        s_editor_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_SAVE " SAVE",
        590,
        505,
        180,
        42,
        editor_save_cb,
        NULL,
        NULL);

    lv_obj_move_foreground(
        s_editor_popup);
}


void ui_printer_profiles_show(
    ui_printer_profiles_active_changed_cb_t active_changed_cb)
{
    s_active_changed_cb =
        active_changed_cb;

    if (s_manager_popup) {
        lv_obj_move_foreground(
            s_manager_popup);
        return;
    }

    s_selected_profile =
        moonraker_config_active_profile_index();

    s_manager_popup =
        ui_popup_create(
            lv_screen_active(),
            760,
            460,
            UI_POPUP_STANDARD);

    if (!s_manager_popup) {
        return;
    }

    ui_popup_add_title(
        s_manager_popup,
        "PRINTER PROFILES",
        false,
        0);

    ui_popup_add_header_divider(
        s_manager_popup,
        44);

    s_manager_list =
        ui_popup_add_list(
            s_manager_popup,
            24,
            58,
            712,
            248);

    s_manager_status =
        ui_popup_add_status_label(
            s_manager_popup,
            "Select a printer profile.",
            24,
            316,
            712);

    ui_popup_add_footer_divider(
        s_manager_popup,
        350);

    ui_popup_add_action_at(
        s_manager_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        24,
        385,
        170,
        44,
        manager_close_cb,
        NULL,
        NULL);

    ui_popup_add_action_at(
        s_manager_popup,
        UI_POPUP_ACTION_SECONDARY,
        LV_SYMBOL_EDIT " EDIT / ADD",
        214,
        385,
        240,
        44,
        manager_edit_cb,
        NULL,
        NULL);

    ui_popup_add_action_at(
        s_manager_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_OK " USE PRINTER",
        474,
        385,
        262,
        44,
        manager_select_cb,
        NULL,
        NULL);

    manager_rebuild_rows();

    const moonraker_profile_t *active =
        moonraker_config_profile(
            moonraker_config_active_profile_index());

    if (active) {
        char status[160];

        snprintf(
            status,
            sizeof(status),
            "Active printer: %s  |  %s:%d",
            active->name,
            active->host,
            active->port);

        manager_set_status(status);
    }

    lv_obj_move_foreground(
        s_manager_popup);
}
'''

# ------------------------------------------------------------
# Network page: replace separate host/port buttons.
# ------------------------------------------------------------

old_action_block = r'''        lv_obj_t *host_button =
            ui_button_create_icon(
                s_network.actions_card,
                UI_BUTTON_OUTLINED,
                LV_SYMBOL_EDIT,
                "EDIT HOST",
                UI_TEXT,
                UI_BUTTON_ICON_HORIZONTAL);

        if (host_button) {
            lv_obj_set_size(host_button, 142, 38);
            lv_obj_set_pos(host_button, 18, 98);

            if (host_clicked_cb) {
                lv_obj_add_event_cb(
                    host_button,
                    host_clicked_cb,
                    LV_EVENT_CLICKED,
                    NULL);
            }
        }

        lv_obj_t *port_button =
            ui_button_create_icon(
                s_network.actions_card,
                UI_BUTTON_OUTLINED,
                LV_SYMBOL_SETTINGS,
                "EDIT PORT",
                UI_TEXT,
                UI_BUTTON_ICON_HORIZONTAL);

        if (port_button) {
            lv_obj_set_size(port_button, 142, 38);
            lv_obj_set_pos(port_button, 170, 98);

            if (port_clicked_cb) {
                lv_obj_add_event_cb(
                    port_button,
                    port_clicked_cb,
                    LV_EVENT_CLICKED,
                    NULL);
            }
        }
'''

new_action_block = r'''        /*
         * Host and port editing now belong to the modular printer-profile
         * manager. Keep the legacy callback parameter temporarily so this
         * page API remains compatible during migration.
         */
        (void)port_clicked_cb;

        lv_obj_t *profiles_button =
            ui_button_create_icon(
                s_network.actions_card,
                UI_BUTTON_OUTLINED,
                LV_SYMBOL_LIST,
                "MANAGE PRINTERS",
                UI_ACCENT_CYAN,
                UI_BUTTON_ICON_HORIZONTAL);

        if (profiles_button) {
            lv_obj_set_size(
                profiles_button,
                294,
                38);

            lv_obj_set_pos(
                profiles_button,
                18,
                98);

            if (host_clicked_cb) {
                lv_obj_add_event_cb(
                    profiles_button,
                    host_clicked_cb,
                    LV_EVENT_CLICKED,
                    NULL);
            }
        }
'''

if network.count(old_action_block) != 1:
    raise RuntimeError(
        "could not locate Network host/port action block")

network = network.replace(
    old_action_block,
    new_action_block,
    1)

# ------------------------------------------------------------
# Main application integration and atomic active-state reset.
# ------------------------------------------------------------

network_include = '#include "ui_network_v32.h"\n'

if main.count(network_include) != 1:
    raise RuntimeError("could not locate Network include")

main = main.replace(
    network_include,
    network_include +
    '#include "ui_printer_profiles.h"\n',
    1)

telemetry_include = '#include "ui_telemetry_v32.h"\n'

if '#include "telemetry_history.h"' not in main:
    if main.count(telemetry_include) != 1:
        raise RuntimeError(
            "could not locate telemetry include")

    main = main.replace(
        telemetry_include,
        telemetry_include +
        '#include "telemetry_history.h"\n',
        1)

config_change_anchor = """static void moonraker_configuration_changed(
"""

runtime_reset = r'''static void reset_active_printer_runtime_state(void)
{
    /*
     * Never clear s_moonraker_objects here. A previous synchronous HTTP
     * transaction may still be writing it. The profile-generation guard
     * discards that response before parsing.
     */
    moonraker_poll_reset();
    moonraker_state_reset();
    telemetry_history_reset();

    s_moonraker_code = 0;
    s_moonraker_ok = false;
    s_last_moonraker_ok_us = 0;
    s_live_data_ok = false;

    live_chamber_temp = -999.0;
    live_air_temp = -999.0;
    live_humidity = -999.0;
    live_heater_target = -999.0;
    live_fan_speed = -999.0;
    live_heater_power = false;

    printer_part_fan_speed = -1.0;
    printer_speed_factor = 100.0;
    printer_flow_factor = 100.0;
    printer_live_velocity = 0.0;
    printer_live_flow = 0.0;

    printer_nozzle_temp = -999.0;
    printer_nozzle_target = -999.0;
    printer_bed_temp = -999.0;
    printer_bed_target = -999.0;

    printer_progress = -1.0;
    printer_print_duration = 0.0;
    printer_current_layer = -1;
    printer_total_layer = -1;
    printer_meta_object_height = 0.0;
    printer_meta_layer_height = 0.0;

    s_drybox_selected_program =
        UI_DRYBOX_PROGRAM_NONE;

    s_drybox_active_program =
        UI_DRYBOX_PROGRAM_NONE;

    safe_copy(
        printer_state,
        sizeof(printer_state),
        "--");

    safe_copy(
        printer_file,
        sizeof(printer_file),
        "No file");

    last_dashboard_print_state[0] = '\0';

    thumbnail_session_v32_clear_selected_file();
    thumbnail_session_v32_clear_thumbnail_path();
}


'''

if main.count(config_change_anchor) != 1:
    raise RuntimeError(
        "could not locate configuration-change function")

main = main.replace(
    config_change_anchor,
    runtime_reset + config_change_anchor,
    1)

wifi_callback_anchor = """static void ui_network_tools_open_wifi_scan_cb(lv_event_t *e)
{
    (void)e;
    ui_network_tools_wifi_scan_now();
}

"""

profile_bridges = r'''static void printer_profiles_active_changed_bridge(void)
{
    reset_active_printer_runtime_state();

    char status[128];

    snprintf(
        status,
        sizeof(status),
        "Moonraker: switching to %s",
        moonraker_config_active_profile_name());

    moonraker_configuration_changed(
        status,
        false);
}


static void ui_network_tools_open_printer_profiles_cb(
    lv_event_t *event)
{
    (void)event;

    ui_printer_profiles_show(
        printer_profiles_active_changed_bridge);
}


'''

if main.count(wifi_callback_anchor) != 1:
    raise RuntimeError(
        "could not locate Network callback anchor")

main = main.replace(
    wifi_callback_anchor,
    wifi_callback_anchor + profile_bridges,
    1)

old_network_create = """        make_printer_info,
        ui_network_tools_open_wifi_scan_cb,
        ui_network_tools_open_host_edit_cb,
        ui_network_tools_open_port_edit_cb);
"""

new_network_create = """        make_printer_info,
        ui_network_tools_open_wifi_scan_cb,
        ui_network_tools_open_printer_profiles_cb,
        ui_network_tools_open_port_edit_cb);
"""

if main.count(old_network_create) != 1:
    raise RuntimeError(
        "could not locate Network create callbacks")

main = main.replace(
    old_network_create,
    new_network_create,
    1)

old_network_destroy = """void ui_network_v32_destroy(void)
{
    ui_network_tools_wifi_popup_destroy_all();
"""

new_network_destroy = """void ui_network_v32_destroy(void)
{
    ui_printer_profiles_close_all();
    ui_network_tools_wifi_popup_destroy_all();
"""

if main.count(old_network_destroy) != 1:
    raise RuntimeError(
        "could not locate Network destroy function")

main = main.replace(
    old_network_destroy,
    new_network_destroy,
    1)

# ------------------------------------------------------------
# Register the new module.
# ------------------------------------------------------------

cmake_anchor = """        "ui_network_v32.c"
        "ui_network_tools.c"
"""

cmake_replacement = """        "ui_network_v32.c"
        "ui_network_tools.c"
        "ui_printer_profiles.c"
"""

if cmake.count(cmake_anchor) != 1:
    raise RuntimeError(
        "could not locate Network CMake registration")

cmake = cmake.replace(
    cmake_anchor,
    cmake_replacement,
    1)

ui_h_path.write_text(ui_header)
ui_c_path.write_text(ui_source)
network_c_path.write_text(network)
main_path.write_text(main)
cmake_path.write_text(cmake)

print("Installed modular Theme B printer-profile manager.")
print("Network action: MANAGE PRINTERS")
print("Supports four profiles, add/edit, and active selection.")
print("Profile switches clear shared state, telemetry, and thumbnails.")
