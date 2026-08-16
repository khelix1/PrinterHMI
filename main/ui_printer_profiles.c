#include "ui_printer_profiles.h"

#include <stdint.h>
#include <dirent.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "moonraker_config_controller.h"
#include "moonraker_transport_security_controller.h"
#include "moonraker_endpoint_test.h"
#include "camera_discovery_controller.h"
#include "camera_catalog_controller.h"
#include "ui_camera.h"
#include "printer_preview_cache.h"
#include "printer_preview_store.h"
#include "printer_profile_health.h"
#include "ui_popup.h"
#include "ui_printer_chooser.h"

static lv_obj_t *s_manager_popup = NULL;
static lv_obj_t *s_delete_popup = NULL;
static lv_obj_t *s_manager_list = NULL;
static lv_obj_t *s_manager_status = NULL;
static lv_obj_t *s_profile_rows[
    MOONRAKER_CONFIG_MAX_PROFILES];

static lv_obj_t *s_editor_popup = NULL;
static lv_obj_t *s_editor_name = NULL;
static lv_obj_t *s_editor_host = NULL;
static lv_obj_t *s_editor_port = NULL;
static lv_obj_t *s_editor_auth_popup = NULL;
static lv_obj_t *s_editor_auth_key = NULL;
static lv_obj_t *s_editor_auth_keyboard = NULL;
static lv_obj_t *s_editor_keyboard = NULL;
static lv_obj_t *s_editor_keyboard_popup = NULL;
static lv_obj_t *s_editor_keyboard_value = NULL;
static lv_obj_t *s_editor_keyboard_target = NULL;
static char s_editor_api_key[MOONRAKER_CONFIG_API_KEY_LENGTH];
static lv_obj_t *s_editor_camera_popup = NULL;
static lv_obj_t *s_editor_camera_remove_popup = NULL;
static lv_obj_t *s_editor_camera_stream = NULL;
static lv_obj_t *s_editor_camera_name = NULL;
static char s_editor_camera_names[CAMERA_CATALOG_MAX_CAMERAS][CAMERA_CATALOG_NAME_LENGTH];
static lv_obj_t *s_editor_camera_keyboard = NULL;
static char s_editor_camera_url[MOONRAKER_CONFIG_CAMERA_URL_LENGTH];
static lv_obj_t *s_editor_camera_status = NULL;
static lv_timer_t *s_editor_camera_discovery_timer = NULL;
static size_t s_editor_camera_slot = 0;
static lv_obj_t *s_editor_camera_slot_buttons[CAMERA_CATALOG_MAX_CAMERAS];
static lv_obj_t *s_editor_status = NULL;
static lv_timer_t *s_editor_test_timer = NULL;

static int s_selected_profile = 0;
static int s_editor_profile = -1;
static bool s_editor_secure = false;
static lv_obj_t *s_editor_security_popup = NULL;
#define EDITOR_PEM_MAX_FILES 6
static lv_obj_t *s_editor_pem_picker = NULL;
static char s_editor_pem_paths[EDITOR_PEM_MAX_FILES][128];

static ui_printer_profiles_active_changed_cb_t
    s_active_changed_cb = NULL;

static ui_printer_profiles_discover_cb_t
    s_discover_cb = NULL;

static void editor_security_close(void);
static void editor_set_status(const char *text);
static void editor_camera_set_status(const char *text);
static void editor_auth_open_cb(lv_event_t *event);
static void editor_camera_open_cb(lv_event_t *event);
static void editor_camera_discover_cb(lv_event_t *event);

static void editor_close(void)
{
    if (s_editor_keyboard_popup) lv_obj_delete(s_editor_keyboard_popup);
    s_editor_keyboard_popup = NULL;
    if (s_editor_popup) lv_obj_delete(s_editor_popup);
    s_editor_popup = NULL;
    s_editor_name = NULL;
    s_editor_host = NULL;
    s_editor_port = NULL;
    if (s_editor_auth_popup) lv_obj_delete(s_editor_auth_popup);
    s_editor_auth_popup = NULL;
    s_editor_auth_key = NULL;
    s_editor_auth_keyboard = NULL;
    memset(s_editor_api_key, 0, sizeof(s_editor_api_key));
    s_editor_keyboard = NULL;
    if (s_editor_test_timer) {
        lv_timer_delete(s_editor_test_timer);
        s_editor_test_timer = NULL;
    }
    s_editor_status = NULL;
    s_editor_profile = -1;
    s_editor_secure = false;
    editor_security_close();
}

void ui_printer_profiles_close_all(void)
{
    editor_close();
    if (s_delete_popup) lv_obj_delete(s_delete_popup);
    s_delete_popup = NULL;
    if (s_manager_popup) lv_obj_delete(s_manager_popup);
    s_manager_popup = NULL;
    s_manager_list = NULL;
    s_manager_status = NULL;
    memset(s_profile_rows, 0, sizeof(s_profile_rows));
    s_active_changed_cb = NULL;
    s_discover_cb = NULL;
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

static void editor_pem_picker_close(void)
{
    if (s_editor_pem_picker) lv_obj_delete(s_editor_pem_picker);
    s_editor_pem_picker = NULL;
}

static void editor_pem_picker_close_cb(lv_event_t *event)
{
    (void)event;
    editor_pem_picker_close();
}

static void editor_security_close(void)
{
    editor_pem_picker_close();
    if (s_editor_security_popup) lv_obj_delete(s_editor_security_popup);
    s_editor_security_popup = NULL;
}

static void editor_security_standard_cb(lv_event_t *event)
{
    (void)event;
    s_editor_secure = false;
    editor_security_close();
    editor_set_status("Standard HTTP selected. API-key behavior is unchanged.");
}

static bool editor_pem_name(const char *name)
{
    size_t length = name ? strlen(name) : 0;
    return length > 4 && strcasecmp(name + length - 4, ".pem") == 0;
}

static void editor_security_pem_selected_cb(lv_event_t *event)
{
    const char *path = lv_event_get_user_data(event);
    if (!path || s_editor_profile < 0 ||
        !moonraker_transport_security_import_ca_file_for_profile(s_editor_profile, path)) {
        editor_set_status("CA import failed for selected PEM file.");
        return;
    }
    s_editor_secure = true;
    if (s_editor_port && atoi(lv_textarea_get_text(s_editor_port)) == 7125) {
        lv_textarea_set_text(s_editor_port, "443");
    }
    editor_security_close();
    editor_set_status("Secure HTTPS/WSS selected. Set the TLS proxy port (443-446).");
}

static void editor_security_pem_picker_open_cb(lv_event_t *event)
{
    (void)event;
    if (s_editor_pem_picker) { lv_obj_move_foreground(s_editor_pem_picker); return; }
    s_editor_pem_picker = ui_popup_create(lv_layer_top(), 700, 500, UI_POPUP_STANDARD);
    if (!s_editor_pem_picker) return;
    ui_popup_add_title(s_editor_pem_picker, "SELECT CA CERTIFICATE", false, 4);
    ui_popup_add_header_divider(s_editor_pem_picker, 48);
    ui_popup_add_body(s_editor_pem_picker, "SD-card root .pem files only", 28, 70, 644);
    DIR *directory = opendir("/sdcard");
    size_t count = 0;
    struct dirent *entry;
    while (directory && count < EDITOR_PEM_MAX_FILES && (entry = readdir(directory))) {
        if (!editor_pem_name(entry->d_name)) continue;
        snprintf(s_editor_pem_paths[count], sizeof(s_editor_pem_paths[count]), "/sdcard/%s", entry->d_name);
        ui_popup_add_action_at(s_editor_pem_picker, UI_POPUP_ACTION_SECONDARY,
            entry->d_name, 28, 105 + (int)count * 48, 644, 42,
            editor_security_pem_selected_cb, s_editor_pem_paths[count], NULL);
        ++count;
    }
    if (directory) closedir(directory);
    if (count == 0) ui_popup_add_body(s_editor_pem_picker, "No .pem files found in SD-card root.", 28, 130, 644);
    ui_popup_add_standard_footer_divider(s_editor_pem_picker);
    ui_popup_add_footer_action(s_editor_pem_picker, UI_POPUP_ACTION_CANCEL,
        "CLOSE", 230, UI_POPUP_FOOTER_RIGHT, editor_pem_picker_close_cb, NULL, NULL);
}

static void editor_security_open_cb(lv_event_t *event)
{
    (void)event;
    if (s_editor_security_popup) { lv_obj_move_foreground(s_editor_security_popup); return; }
    s_editor_security_popup = ui_popup_create(lv_layer_top(), 700, 350, UI_POPUP_STANDARD);
    if (!s_editor_security_popup) return;
    ui_popup_add_title(s_editor_security_popup, "CONNECTION SECURITY", false, 4);
    ui_popup_add_header_divider(s_editor_security_popup, 48);
    ui_popup_add_body(s_editor_security_popup,
        "Standard keeps current HTTP behavior. Secure lets this printer profile use an approved SD-card PEM certificate.",
        28, 76, 644);
    ui_popup_add_standard_footer_divider(s_editor_security_popup);
    ui_popup_add_footer_action(s_editor_security_popup, UI_POPUP_ACTION_SECONDARY,
        "STANDARD HTTP", 230, UI_POPUP_FOOTER_LEFT, editor_security_standard_cb, NULL, NULL);
    ui_popup_add_footer_action(s_editor_security_popup, UI_POPUP_ACTION_CONFIRM,
        "SELECT .PEM", 270, UI_POPUP_FOOTER_RIGHT, editor_security_pem_picker_open_cb, NULL, NULL);
}

static void editor_discover_cb(lv_event_t *event)
{
    (void)event;

    if (s_discover_cb) {
        s_discover_cb();
    }
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


static void delete_close(void)
{
    if (s_delete_popup) {
        lv_obj_delete(s_delete_popup);
    }
    s_delete_popup = NULL;
}


static void delete_cancel_cb(lv_event_t *event)
{
    (void)event;
    delete_close();
}


static void delete_confirm_cb(lv_event_t *event)
{
    (void)event;

    const moonraker_profile_t *profile =
        moonraker_config_profile(s_selected_profile);

    if (!profile || !profile->configured) {
        delete_close();
        manager_set_status("The selected printer is already empty.");
        return;
    }

    bool active_removed =
        s_selected_profile ==
            moonraker_config_active_profile_index();

    char removed_name[MOONRAKER_CONFIG_NAME_LENGTH];
    strlcpy(removed_name, profile->name, sizeof(removed_name));

    if (!moonraker_config_delete_profile(s_selected_profile)) {
        delete_close();
        manager_set_status("Unable to remove the selected printer.");
        return;
    }

    printer_profile_health_set(s_selected_profile, true, false);
    printer_preview_cache_invalidate(s_selected_profile);
    printer_preview_store_invalidate(s_selected_profile);

    s_selected_profile =
        moonraker_config_active_profile_index();

    delete_close();
    manager_rebuild_rows();

    if (active_removed && s_active_changed_cb) {
        s_active_changed_cb();
    } else {
        ui_printer_chooser_refresh();
    }

    char status[160];
    snprintf(
        status,
        sizeof(status),
        active_removed
            ? "Removed %s. Active printer changed to %s."
            : "Removed printer profile: %s.",
        removed_name,
        active_removed
            ? moonraker_config_active_profile_name()
            : "");
    manager_set_status(status);
}


static void manager_delete_cb(lv_event_t *event)
{
    (void)event;

    const moonraker_profile_t *profile =
        moonraker_config_profile(s_selected_profile);

    if (!profile || !profile->configured) {
        manager_set_status("Select a configured printer to remove.");
        return;
    }

    if (moonraker_config_profile_count() <= 1) {
        manager_set_status(
            "At least one profile is required. Edit this profile instead.");
        return;
    }

    if (s_delete_popup) {
        lv_obj_move_foreground(s_delete_popup);
        return;
    }

    s_delete_popup = ui_popup_create(
        lv_screen_active(),
        640,
        300,
        UI_POPUP_DANGER);

    if (!s_delete_popup) {
        return;
    }

    ui_popup_add_title(
        s_delete_popup,
        "REMOVE PRINTER?",
        true,
        0);
    ui_popup_add_header_divider(s_delete_popup, 44);

    char message[256];
    snprintf(
        message,
        sizeof(message),
        "Remove %s from this panel?\n\nIts saved endpoint, health state and cached preview will be deleted.",
        profile->name);

    ui_popup_add_body(
        s_delete_popup,
        message,
        28,
        70,
        584);

    ui_popup_add_standard_footer_divider(s_delete_popup);
    ui_popup_add_footer_action(
        s_delete_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        260,
        UI_POPUP_FOOTER_LEFT,
        delete_cancel_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_delete_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_TRASH " REMOVE",
        260,
        UI_POPUP_FOOTER_RIGHT,
        delete_confirm_cb,
        NULL,
        NULL);
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


static void editor_keyboard_popup_close(void)
{
    if (s_editor_keyboard_target && s_editor_keyboard_value) {
        lv_textarea_set_text(
            s_editor_keyboard_target,
            lv_textarea_get_text(s_editor_keyboard_value));
    }
    if (s_editor_keyboard_popup) lv_obj_delete(s_editor_keyboard_popup);
    s_editor_keyboard_popup = NULL;
    s_editor_keyboard = NULL;
    s_editor_keyboard_value = NULL;
    s_editor_keyboard_target = NULL;
}



static void editor_keyboard_popup_done_cb(lv_event_t *event)
{
    (void)event;
    editor_keyboard_popup_close();
}


static void editor_field_focused_cb(lv_event_t *event)
{
    lv_obj_t *field = lv_event_get_target(event);
    if (!field) return;
    if (s_editor_keyboard_popup && s_editor_keyboard && s_editor_keyboard_value) {
        s_editor_keyboard_target = field;
        lv_textarea_set_text(s_editor_keyboard_value, lv_textarea_get_text(field));
        lv_keyboard_set_textarea(s_editor_keyboard, s_editor_keyboard_value);
        lv_obj_move_foreground(s_editor_keyboard_popup);
        return;
    }

    s_editor_keyboard_target = field;
    s_editor_keyboard_popup = ui_popup_create(
        lv_screen_active(), 800, 350, UI_POPUP_STANDARD);
    if (!s_editor_keyboard_popup) {
        s_editor_keyboard_target = NULL;
        return;
    }
    ui_popup_add_title(s_editor_keyboard_popup, "EDIT VALUE", false, 0);
    ui_popup_add_header_divider(s_editor_keyboard_popup, 44);
    s_editor_keyboard_value = ui_popup_add_textarea(
        s_editor_keyboard_popup, 720, 48, LV_ALIGN_TOP_MID, 0, 56,
        true, false, 127, "", lv_textarea_get_text(field), NULL);
    s_editor_keyboard = ui_popup_add_keyboard(
        s_editor_keyboard_popup, s_editor_keyboard_value, 720, 178,
        LV_ALIGN_TOP_MID, 0, 112, LV_KEYBOARD_MODE_TEXT_LOWER);
    ui_popup_add_standard_footer_divider(s_editor_keyboard_popup);
    ui_popup_add_action_at(
        s_editor_keyboard_popup, UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_OK " DONE", 508, 296, 260, 44,
        editor_keyboard_popup_done_cb, NULL, NULL);
    lv_obj_move_foreground(s_editor_keyboard_popup);
}




static void editor_apply_identity_suggestion(
    const char *identity)
{
    /*
     * A verified identity is a suggestion for a fresh profile only. It never
     * overwrites an operator's existing custom profile name.
     */
    if (!identity || !identity[0] || !s_editor_name) {
        return;
    }

    char default_name[MOONRAKER_CONFIG_NAME_LENGTH];
    snprintf(
        default_name,
        sizeof(default_name),
        "Printer %d",
        s_editor_profile + 1);

    const char *current_name =
        lv_textarea_get_text(s_editor_name);

    if (!current_name || !current_name[0] ||
        strcmp(current_name, default_name) == 0) {
        lv_textarea_set_text(s_editor_name, identity);
    }
}


static void editor_test_poll_cb(lv_timer_t *timer)
{
    moonraker_probe_result_t result = {0};
    bool verified = false;

    if (!moonraker_endpoint_test_take_result(&result, &verified)) {
        return;
    }

    if (timer) {
        lv_timer_delete(timer);
    }
    s_editor_test_timer = NULL;

    if (!s_editor_popup) {
        return;
    }

    if (!verified) {
        editor_set_status(
            "No verified Moonraker response. Check the host, port and network.");
        return;
    }

    editor_apply_identity_suggestion(result.identity);

    char status[192];
    snprintf(
        status,
        sizeof(status),
        result.klippy_ready
            ? "Verified %s. Klipper is READY. Press SAVE to keep this profile."
            : "Verified %s. Moonraker is reachable; Klipper is not ready.",
        result.identity[0] ? result.identity : "Moonraker");
    editor_set_status(status);
}


static void editor_auth_close(void)
{
    if (s_editor_auth_popup) {
        lv_obj_delete(s_editor_auth_popup);
    }
    s_editor_auth_popup = NULL;
    s_editor_auth_key = NULL;
    s_editor_auth_keyboard = NULL;
}


static void editor_auth_cancel_cb(lv_event_t *event)
{
    (void)event;
    editor_auth_close();
}


static void editor_auth_save_cb(lv_event_t *event)
{
    (void)event;
    if (!s_editor_auth_key) {
        return;
    }

    strlcpy(
        s_editor_api_key,
        lv_textarea_get_text(s_editor_auth_key),
        sizeof(s_editor_api_key));
    editor_auth_close();
    editor_set_status(
        s_editor_api_key[0]
            ? "Moonraker API key ready. Use TEST, then SAVE."
            : "Moonraker API key cleared. Trusted-client access remains enabled.");
}


static void editor_auth_open_cb(lv_event_t *event)
{
    (void)event;
    if (s_editor_auth_popup) {
        lv_obj_move_foreground(s_editor_auth_popup);
        return;
    }

    s_editor_auth_popup = ui_popup_create(
        lv_screen_active(), 800, 560, UI_POPUP_STANDARD);
    if (!s_editor_auth_popup) {
        return;
    }

    ui_popup_add_title(s_editor_auth_popup, "MOONRAKER AUTHENTICATION", false, 0);
    ui_popup_add_header_divider(s_editor_auth_popup, 44);
    ui_popup_add_caption(s_editor_auth_popup, "API KEY (OPTIONAL)", 28, 70, 220);
    ui_popup_add_status_label(
        s_editor_auth_popup,
        "Stored only on this panel; it is never shown in backups or logs.",
        28, 112, 720);

    s_editor_auth_key = ui_popup_add_textarea(
        s_editor_auth_popup, 720, 48, LV_ALIGN_TOP_MID, 0, 145,
        true, true, MOONRAKER_CONFIG_API_KEY_LENGTH - 1,
        "Leave blank for trusted Moonraker clients", s_editor_api_key, NULL);

    s_editor_auth_keyboard = ui_popup_add_keyboard(
        s_editor_auth_popup, s_editor_auth_key, 720, 230,
        LV_ALIGN_TOP_MID, 0, 210, LV_KEYBOARD_MODE_TEXT_LOWER);

    ui_popup_add_standard_footer_divider(s_editor_auth_popup);
    ui_popup_add_action_at(
        s_editor_auth_popup, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " CANCEL",
        32, 500, 260, 48, editor_auth_cancel_cb, NULL, NULL);
    ui_popup_add_action_at(
        s_editor_auth_popup, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_SAVE " USE KEY",
        508, 500, 260, 48, editor_auth_save_cb, NULL, NULL);
    lv_obj_move_foreground(s_editor_auth_popup);
}


static void editor_test_cb(lv_event_t *event)
{
    (void)event;

    if (!s_editor_host || !s_editor_port) {
        return;
    }

    const char *host = lv_textarea_get_text(s_editor_host);
    const char *port_text = lv_textarea_get_text(s_editor_port);

    if (!host || !host[0]) {
        editor_set_status("Enter a Moonraker host or IP address first.");
        return;
    }

    char *port_end = NULL;
    long port = strtol(port_text ? port_text : "", &port_end, 10);
    if (!port_text || !port_text[0] || !port_end || *port_end != '\0' ||
        port <= 0 || port >= 65536) {
        editor_set_status("Port must be between 1 and 65535.");
        return;
    }

    if (!moonraker_endpoint_test_start(
            host, (int)port, s_editor_api_key)) {
        editor_set_status(
            moonraker_endpoint_test_busy()
                ? "A connection test is already running."
                : "Unable to start the connection test.");
        return;
    }

    editor_set_status("Testing Moonraker connection...");
    s_editor_test_timer = lv_timer_create(editor_test_poll_cb, 80, NULL);

    if (!s_editor_test_timer) {
        editor_set_status("Connection test is running; wait for it to finish.");
    }
}


static void editor_camera_close(void)
{
    if (s_editor_camera_remove_popup) {
        lv_obj_delete(s_editor_camera_remove_popup);
        s_editor_camera_remove_popup = NULL;
    }
    if (s_editor_camera_discovery_timer) {
        lv_timer_delete(s_editor_camera_discovery_timer);
        s_editor_camera_discovery_timer = NULL;
    }
    if (s_editor_camera_popup) lv_obj_delete(s_editor_camera_popup);
    s_editor_camera_popup = NULL;
    s_editor_camera_stream = NULL;
    s_editor_camera_name = NULL;
    memset(s_editor_camera_slot_buttons, 0, sizeof(s_editor_camera_slot_buttons));
    s_editor_camera_keyboard = NULL;
    s_editor_camera_status = NULL;
}

static void editor_camera_cancel_cb(lv_event_t *event)
{
    (void)event;
    editor_camera_close();
}

static bool editor_camera_store_current(void);
static void editor_camera_load_slot(void);

static void editor_camera_update_slot_buttons(void)
{
    int profile = s_editor_profile;
    size_t default_slot = camera_catalog_default(profile);

    for (size_t index = 0; index < CAMERA_CATALOG_MAX_CAMERAS; ++index) {
        lv_obj_t *button = s_editor_camera_slot_buttons[index];
        if (!button) continue;

        camera_catalog_entry_t entry = {0};
        bool configured = camera_catalog_get(profile, index, &entry) &&
                          entry.configured;
        if (index == 0 && s_editor_camera_url[0]) configured = true;

        const char *name = s_editor_camera_names[index][0]
            ? s_editor_camera_names[index]
            : (entry.name[0] ? entry.name : "");
        char label[56];
        if (configured) {
            snprintf(label, sizeof(label), "%u: %.16s%s",
                     (unsigned)(index + 1),
                     name[0] ? name : "Camera",
                     default_slot == index ? "  DEFAULT" : "");
        } else {
            snprintf(label, sizeof(label), "+ CAMERA %u", (unsigned)(index + 1));
        }

        lv_obj_t *button_label = lv_obj_get_child(button, 0);
        if (button_label) lv_label_set_text(button_label, label);
        if (index == s_editor_camera_slot) {
            lv_obj_add_state(button, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(button, LV_STATE_CHECKED);
        }
    }
}

static void editor_camera_select_slot_cb(lv_event_t *event)
{
    size_t slot = (size_t)(uintptr_t)lv_event_get_user_data(event);
    if (slot >= CAMERA_CATALOG_MAX_CAMERAS || slot == s_editor_camera_slot) return;
    if (!editor_camera_store_current()) {
        editor_camera_set_status("Camera URL must be empty or start with http:// or https://.");
        return;
    }
    s_editor_camera_slot = slot;
    editor_camera_load_slot();
}

static bool editor_camera_store_current(void)
{
    const char *url = s_editor_camera_stream
        ? lv_textarea_get_text(s_editor_camera_stream) : "";
    const char *name = s_editor_camera_name
        ? lv_textarea_get_text(s_editor_camera_name) : "";
    strlcpy(s_editor_camera_names[s_editor_camera_slot],
            name && name[0] ? name : "Camera",
            sizeof(s_editor_camera_names[s_editor_camera_slot]));
    if (s_editor_camera_slot == 0) {
        strlcpy(s_editor_camera_url, url ? url : "", sizeof(s_editor_camera_url));
        return true;
    }
    return camera_catalog_set(s_editor_profile, s_editor_camera_slot,
                              s_editor_camera_names[s_editor_camera_slot],
                              url ? url : "");
}

static void editor_camera_load_slot(void)
{
    char url[MOONRAKER_CONFIG_CAMERA_URL_LENGTH] = "";
    camera_catalog_entry_t entry = {0};
    if (camera_catalog_get(s_editor_profile, s_editor_camera_slot, &entry) &&
        entry.name[0]) {
        strlcpy(s_editor_camera_names[s_editor_camera_slot], entry.name,
                sizeof(s_editor_camera_names[s_editor_camera_slot]));
    }
    if (!s_editor_camera_names[s_editor_camera_slot][0]) {
        snprintf(s_editor_camera_names[s_editor_camera_slot],
                 sizeof(s_editor_camera_names[s_editor_camera_slot]),
                 "Camera %u", (unsigned)(s_editor_camera_slot + 1));
    }
    if (s_editor_camera_slot == 0) {
        strlcpy(url, s_editor_camera_url, sizeof(url));
    } else if (entry.configured) {
        strlcpy(url, entry.stream_url, sizeof(url));
    }
    if (s_editor_camera_name) {
        lv_textarea_set_text(s_editor_camera_name,
                             s_editor_camera_names[s_editor_camera_slot]);
    }
    if (s_editor_camera_stream) lv_textarea_set_text(s_editor_camera_stream, url);

    char status[128];
    snprintf(status, sizeof(status),
             "Editing Camera %u of %u. Enter its stream or search Moonraker.",
             (unsigned)(s_editor_camera_slot + 1),
             (unsigned)CAMERA_CATALOG_MAX_CAMERAS);
    editor_camera_set_status(status);
    editor_camera_update_slot_buttons();
}

static void editor_camera_next_slot_cb(lv_event_t *event)
{
    (void)event;
    if (!editor_camera_store_current()) {
        editor_camera_set_status("Camera URL must be empty or start with http:// or https://.");
        return;
    }
    s_editor_camera_slot =
        (s_editor_camera_slot + 1) % CAMERA_CATALOG_MAX_CAMERAS;
    editor_camera_load_slot();
}

static bool editor_camera_apply_primary_slot(void)
{
    if (s_editor_camera_slot != 0) return true;
    return camera_catalog_set(s_editor_profile, 0,
                              s_editor_camera_names[0],
                              s_editor_camera_url);
}

static void editor_camera_make_default_cb(lv_event_t *event)
{
    (void)event;
    if (!editor_camera_store_current() || !editor_camera_apply_primary_slot()) {
        editor_camera_set_status("Enter a valid camera stream before making it default.");
        return;
    }
    if (!camera_catalog_set_default(s_editor_profile, s_editor_camera_slot)) {
        editor_camera_set_status("Configure this camera before making it default.");
        return;
    }
    ui_camera_refresh_catalog();
    editor_camera_update_slot_buttons();
    editor_camera_set_status("This is now the default camera for this printer.");
}

static void editor_camera_remove_cancel_cb(lv_event_t *event)
{
    (void)event;
    if (s_editor_camera_remove_popup) {
        lv_obj_delete(s_editor_camera_remove_popup);
        s_editor_camera_remove_popup = NULL;
    }
}

static void editor_camera_remove_apply_cb(lv_event_t *event)
{
    (void)event;
    editor_camera_remove_cancel_cb(NULL);
    if (!camera_catalog_clear(s_editor_profile, s_editor_camera_slot)) {
        editor_camera_set_status("Unable to remove this camera.");
        return;
    }
    if (s_editor_camera_slot == 0) {
        s_editor_camera_url[0] = '\0';
    }
    snprintf(s_editor_camera_names[s_editor_camera_slot],
             sizeof(s_editor_camera_names[s_editor_camera_slot]),
             "Camera %u", (unsigned)(s_editor_camera_slot + 1));
    ui_camera_refresh_catalog();
    editor_camera_load_slot();
    editor_camera_update_slot_buttons();
    editor_camera_set_status("Camera removed.");
}

static void editor_camera_remove_cb(lv_event_t *event)
{
    (void)event;
    if (s_editor_camera_remove_popup) {
        lv_obj_move_foreground(s_editor_camera_remove_popup);
        return;
    }

    const char *name = s_editor_camera_names[s_editor_camera_slot][0]
        ? s_editor_camera_names[s_editor_camera_slot] : "this camera";
    char prompt[160];
    snprintf(prompt, sizeof(prompt),
             "Remove %s from this printer? Its saved stream and view settings will be cleared.",
             name);

    s_editor_camera_remove_popup = ui_popup_create(
        lv_screen_active(), 620, 260, UI_POPUP_STANDARD);
    if (!s_editor_camera_remove_popup) return;
    ui_popup_add_title(s_editor_camera_remove_popup, "REMOVE CAMERA?", false, 0);
    ui_popup_add_header_divider(s_editor_camera_remove_popup, 44);
    ui_popup_add_status_label(s_editor_camera_remove_popup, prompt, 28, 78, 564);
    ui_popup_add_status_label(s_editor_camera_remove_popup,
                              "This cannot be undone from the panel.",
                              28, 126, 564);
    ui_popup_add_standard_footer_divider(s_editor_camera_remove_popup);
    ui_popup_add_action_at(s_editor_camera_remove_popup, UI_POPUP_ACTION_CANCEL,
                           LV_SYMBOL_CLOSE " KEEP CAMERA",
                           28, 200, 270, 44,
                           editor_camera_remove_cancel_cb, NULL, NULL);
    ui_popup_add_action_at(s_editor_camera_remove_popup, UI_POPUP_ACTION_DANGER,
                           LV_SYMBOL_TRASH " REMOVE",
                           322, 200, 270, 44,
                           editor_camera_remove_apply_cb, NULL, NULL);
    lv_obj_move_foreground(s_editor_camera_remove_popup);
}

static void editor_camera_save_cb(lv_event_t *event)
{
    (void)event;
    if (!editor_camera_store_current()) {
        editor_camera_set_status("Camera URL must be empty or start with http:// or https://.");
        return;
    }

    /* USE CAMERA is an apply action.  Persist Camera 1 now, just as the
     * additional slots already persist when they are advanced.  This makes
     * the visible selector and stream change without a page round-trip. */
    if (s_editor_camera_slot == 0 &&
        !camera_catalog_set(s_editor_profile, 0,
                            s_editor_camera_names[0],
                            s_editor_camera_url)) {
        editor_camera_set_status("Unable to apply this camera.");
        return;
    }
    ui_camera_refresh_catalog();

    bool configured = s_editor_camera_stream &&
        lv_textarea_get_text(s_editor_camera_stream)[0];
    editor_camera_close();
    editor_set_status(configured
        ? "Camera source applied."
        : "Camera source cleared.");
}


static void editor_camera_set_status(const char *text)
{
    if (s_editor_camera_status) {
        lv_label_set_text(s_editor_camera_status, text ? text : "");
    }
}


static void editor_camera_discovery_poll_cb(lv_timer_t *timer)
{
    moonraker_webcam_t webcam = {0};
    bool found = false;
    size_t s_discovered_camera_count = 0;

    if (!camera_discovery_take_result(&webcam, &found,
                                      &s_discovered_camera_count)) {
        return;
    }

    if (timer) {
        lv_timer_delete(timer);
    }
    s_editor_camera_discovery_timer = NULL;

    if (!s_editor_camera_popup) {
        return;
    }
    if (!found) {
        /* A manually configured stream remains valid when Moonraker has no
         * additional webcam entry to import.  Do not call this a missing
         * camera: it is the normal result for a single already-configured
         * camera or a legacy webcam configuration. */
        editor_camera_set_status(
            "Moonraker did not return another camera. Enter this stream URL manually.");
        return;
    }

    /* The worker has already imported every returned webcam into the
     * numbered catalog slots.  Camera 1 also mirrors the legacy profile URL,
     * so keep its editor-local staged value synchronized before the eventual
     * profile SAVE can write it back. */
    if (s_editor_camera_slot == 0) {
        strlcpy(s_editor_camera_url, webcam.stream_url,
                sizeof(s_editor_camera_url));
        strlcpy(s_editor_camera_names[0],
                webcam.name[0] ? webcam.name : "Camera 1",
                sizeof(s_editor_camera_names[0]));
    }
    editor_camera_load_slot();

    char status[192];
    snprintf(
        status,
        sizeof(status),
        "%u camera%s found and imported. Camera %u is selected.",
        (unsigned)s_discovered_camera_count,
        s_discovered_camera_count == 1 ? "" : "s",
        (unsigned)(s_editor_camera_slot + 1));
    editor_camera_set_status(status);
}


static void editor_camera_discover_cb(lv_event_t *event)
{
    (void)event;
    if (!s_editor_host || !s_editor_port) {
        return;
    }

    const char *host = lv_textarea_get_text(s_editor_host);
    const char *port_text = lv_textarea_get_text(s_editor_port);
    char *port_end = NULL;
    long port = strtol(port_text ? port_text : "", &port_end, 10);
    if (!host || !host[0] || !port_text || !port_text[0] ||
        !port_end || *port_end != '\0' || port <= 0 || port >= 65536) {
        editor_camera_set_status(
            "Enter a valid Moonraker host and port before searching.");
        return;
    }
    if (!camera_discovery_start(host, (int)port, s_editor_api_key)) {
        editor_camera_set_status(
            camera_discovery_busy()
                ? "Camera search is already running."
                : "Unable to start the camera search.");
        return;
    }

    editor_camera_set_status("Searching Moonraker for configured cameras...");
    s_editor_camera_discovery_timer = lv_timer_create(
        editor_camera_discovery_poll_cb, 80, NULL);
    if (!s_editor_camera_discovery_timer) {
        editor_camera_set_status("Camera search is running; wait for it to finish.");
    }
}


static void editor_camera_field_focused_cb(lv_event_t *event)
{
    if (!s_editor_camera_keyboard) {
        return;
    }

    lv_obj_t *field = lv_event_get_target(event);
    if (field) {
        lv_keyboard_set_textarea(s_editor_camera_keyboard, field);
        lv_keyboard_set_mode(
            s_editor_camera_keyboard,
            LV_KEYBOARD_MODE_TEXT_LOWER);
    }
}

static void editor_camera_open_cb(lv_event_t *event)
{
    (void)event;
    if (s_editor_camera_popup) { lv_obj_move_foreground(s_editor_camera_popup); return; }
    s_editor_camera_popup = ui_popup_create(lv_screen_active(), 800, 560, UI_POPUP_STANDARD);
    if (!s_editor_camera_popup) return;
    ui_popup_add_title(s_editor_camera_popup, "CAMERA SETUP", false, 0);
    ui_popup_add_header_divider(s_editor_camera_popup, 44);

    for (size_t index = 0; index < CAMERA_CATALOG_MAX_CAMERAS; ++index) {
        s_editor_camera_slot_buttons[index] = ui_popup_add_action_at(
            s_editor_camera_popup, UI_POPUP_ACTION_SECONDARY, "+ CAMERA",
            28 + (int)index * 180, 54, 170, 32,
            editor_camera_select_slot_cb, (void *)(uintptr_t)index, NULL);
        if (s_editor_camera_slot_buttons[index]) {
            lv_obj_add_flag(s_editor_camera_slot_buttons[index], LV_OBJ_FLAG_CHECKABLE);
        }
    }

    ui_popup_add_caption(s_editor_camera_popup, "CAMERA NAME", 28, 94, 220);
    s_editor_camera_name = ui_popup_add_textarea(s_editor_camera_popup, 720, 38, LV_ALIGN_TOP_MID, 0, 108, true, false, CAMERA_CATALOG_NAME_LENGTH - 1, "Camera name", "", NULL);
    ui_popup_add_caption(s_editor_camera_popup, "MJPEG / HTTP STREAM URL (OPTIONAL)", 28, 154, 420);
    s_editor_camera_stream = ui_popup_add_textarea(s_editor_camera_popup, 720, 40, LV_ALIGN_TOP_MID, 0, 168, true, false, MOONRAKER_CONFIG_CAMERA_URL_LENGTH - 1, "http://...", s_editor_camera_url, NULL);
    ui_popup_add_action_at(s_editor_camera_popup, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_REFRESH " FIND", 28, 218, 170, 38, editor_camera_discover_cb, NULL, NULL);
    ui_popup_add_action_at(s_editor_camera_popup, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_RIGHT " NEXT", 208, 218, 170, 38, editor_camera_next_slot_cb, NULL, NULL);
    ui_popup_add_action_at(s_editor_camera_popup, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_OK " DEFAULT", 388, 218, 170, 38, editor_camera_make_default_cb, NULL, NULL);
    ui_popup_add_action_at(s_editor_camera_popup, UI_POPUP_ACTION_DANGER, LV_SYMBOL_TRASH " REMOVE", 568, 218, 180, 38, editor_camera_remove_cb, NULL, NULL);
    s_editor_camera_status = ui_popup_add_status_label(s_editor_camera_popup, "Select a camera slot, then name, find, or edit its stream.", 28, 266, 720);
    s_editor_camera_keyboard = ui_popup_add_keyboard(s_editor_camera_popup, s_editor_camera_stream, 720, 164, LV_ALIGN_TOP_MID, 0, 286, LV_KEYBOARD_MODE_TEXT_LOWER);
    if (s_editor_camera_name) lv_obj_add_event_cb(s_editor_camera_name, editor_camera_field_focused_cb, LV_EVENT_CLICKED, NULL);
    if (s_editor_camera_stream) lv_obj_add_event_cb(s_editor_camera_stream, editor_camera_field_focused_cb, LV_EVENT_CLICKED, NULL);
    ui_popup_add_standard_footer_divider(s_editor_camera_popup);
    ui_popup_add_action_at(s_editor_camera_popup, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " CANCEL", 32, 500, 260, 48, editor_camera_cancel_cb, NULL, NULL);
    ui_popup_add_action_at(s_editor_camera_popup, UI_POPUP_ACTION_CONFIRM, LV_SYMBOL_SAVE " USE CAMERA", 508, 500, 260, 48, editor_camera_save_cb, NULL, NULL);
    editor_camera_load_slot();
    lv_obj_move_foreground(s_editor_camera_popup);
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

    if (s_editor_secure && !moonraker_transport_security_ca_pem_for_profile(s_editor_profile)) {
        editor_set_status("Secure mode requires an approved CA certificate.");
        return;
    }

    bool active_profile_edited =
        s_editor_profile ==
            moonraker_config_active_profile_index();

    if (!moonraker_config_save_profile(
            s_editor_profile,
            name,
            host,
            (int)port,
            s_editor_api_key)) {
        editor_set_status(
            "Use a hostname or IPv4 address only; omit http://, paths and spaces.");
        return;
    }

    if (!moonraker_config_set_camera_stream_url(s_editor_profile, s_editor_camera_url)) {
        editor_set_status("Camera URL must be empty or start with http:// or https://.");
        return;
    }

    if (!camera_catalog_set(s_editor_profile, 0,
                            s_editor_camera_names[0],
                            s_editor_camera_url)) {
        editor_set_status("Unable to save camera name.");
        return;
    }
    ui_camera_refresh_catalog();

    if (!moonraker_config_set_transport_security(s_editor_profile, s_editor_secure)) {
        editor_set_status("Unable to save connection-security setting.");
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


void ui_printer_profiles_set_discovered_endpoint(
    const char *host,
    int port,
    const char *identity)
{
    if (!s_editor_popup ||
        !s_editor_host ||
        !s_editor_port ||
        !host ||
        !host[0] ||
        port <= 0 ||
        port >= 65536) {
        return;
    }

    char port_text[16];

    snprintf(
        port_text,
        sizeof(port_text),
        "%d",
        port);

    lv_textarea_set_text(
        s_editor_host,
        host);

    lv_textarea_set_text(
        s_editor_port,
        port_text);

    editor_apply_identity_suggestion(identity);

    editor_set_status(
        "Verified Moonraker endpoint selected. Review the name and press SAVE.");
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

    strlcpy(
        s_editor_api_key,
        profile && profile->configured ? profile->api_key : "",
        sizeof(s_editor_api_key));
    strlcpy(s_editor_camera_url, profile && profile->configured ? profile->camera_stream_url : "", sizeof(s_editor_camera_url));
    s_editor_camera_slot = 0;

    char port_text[16];

    snprintf(
        port_text,
        sizeof(port_text),
        "%d",
        port);

    s_editor_profile =
        s_selected_profile;
    s_editor_secure = profile && profile->secure_transport;

    s_editor_popup = ui_popup_create(lv_screen_active(), 800, 560, UI_POPUP_STANDARD);
    if (!s_editor_popup) { s_editor_profile = -1; return; }
    ui_popup_add_title(s_editor_popup, profile && profile->configured ? "EDIT PRINTER" : "ADD PRINTER", false, 0);
    ui_popup_add_header_divider(s_editor_popup, 44);
    ui_popup_add_caption(s_editor_popup, "IDENTITY", 28, 68, 160);
    ui_popup_add_caption(s_editor_popup, "PRINTER NAME", 28, 96, 160);
    s_editor_name = ui_popup_add_textarea(s_editor_popup, 520, 48, LV_ALIGN_TOP_LEFT, 240, 83, true, false, MOONRAKER_CONFIG_NAME_LENGTH - 1, "Printer name", name, NULL);
    ui_popup_add_caption(s_editor_popup, "MOONRAKER CONNECTION", 28, 151, 240);
    ui_popup_add_caption(s_editor_popup, "HOST", 28, 181, 160);
    s_editor_host = ui_popup_add_textarea(s_editor_popup, 520, 48, LV_ALIGN_TOP_LEFT, 240, 168, true, false, MOONRAKER_CONFIG_HOST_LENGTH - 1, "IP address or hostname", host, NULL);
    ui_popup_add_caption(s_editor_popup, "PORT", 28, 237, 160);
    s_editor_port = ui_popup_add_textarea(s_editor_popup, 160, 48, LV_ALIGN_TOP_LEFT, 240, 224, true, false, 5, "7125", port_text, "0123456789");
    ui_popup_add_action_at(s_editor_popup, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_REFRESH " DISCOVER", 420, 224, 340, 48, editor_discover_cb, NULL, NULL);
    ui_popup_add_caption(s_editor_popup, "SECURITY & ACCESS", 28, 278, 240);
    ui_popup_add_action_at(s_editor_popup, UI_POPUP_ACTION_SECONDARY, s_editor_secure ? LV_SYMBOL_SETTINGS " SECURE HTTPS/WSS" : LV_SYMBOL_SETTINGS " STANDARD HTTP", 28, 302, 356, 48, editor_security_open_cb, NULL, NULL);
    ui_popup_add_action_at(s_editor_popup, UI_POPUP_ACTION_SECONDARY, LV_SYMBOL_SETTINGS " AUTHENTICATION", 404, 302, 356, 48, editor_auth_open_cb, NULL, NULL);
    s_editor_status = ui_popup_add_status_label(s_editor_popup, "Configure the printer connection, then test and save.", 28, 358, 720);
    s_editor_keyboard = ui_popup_add_keyboard(s_editor_popup, s_editor_name, 0, 0, LV_ALIGN_TOP_MID, 0, 0, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_flag(s_editor_keyboard, LV_OBJ_FLAG_HIDDEN);
    if (s_editor_name) {
        lv_obj_add_event_cb(
            s_editor_name,
            editor_field_focused_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    if (s_editor_host) {
        lv_obj_add_event_cb(
            s_editor_host,
            editor_field_focused_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    if (s_editor_port) {
        lv_obj_add_event_cb(
            s_editor_port,
            editor_field_focused_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    ui_popup_add_standard_footer_divider(s_editor_popup);

    ui_popup_add_action_at(
        s_editor_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        32,
        508,
        172,
        44,
        editor_cancel_cb,
        NULL,
        NULL);


    ui_popup_add_action_at(
        s_editor_popup,
        UI_POPUP_ACTION_SECONDARY,
        LV_SYMBOL_IMAGE " CAMERA",
        220,
        508,
        172,
        44,
        editor_camera_open_cb,
        NULL,
        NULL);

    ui_popup_add_action_at(
        s_editor_popup,
        UI_POPUP_ACTION_SECONDARY,
        LV_SYMBOL_PLAY " TEST",
        408,
        508,
        172,
        44,
        editor_test_cb,
        NULL,
        NULL);

    ui_popup_add_action_at(
        s_editor_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_SAVE " SAVE",
        596,
        508,
        172,
        44,
        editor_save_cb,
        NULL,
        NULL);

    lv_obj_move_foreground(
        s_editor_popup);
}


void ui_printer_profiles_show(
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb)
{
    s_active_changed_cb =
        active_changed_cb;

    s_discover_cb =
        discover_cb;

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
            520,
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
            440);

    ui_popup_add_action_at(
        s_manager_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_TRASH " REMOVE SELECTED",
        492,
        328,
        220,
        48,
        manager_delete_cb,
        NULL,
        NULL);

    ui_popup_add_standard_footer_divider(s_manager_popup);

    ui_popup_add_footer_action(
        s_manager_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        220,
        UI_POPUP_FOOTER_LEFT,
        manager_close_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_manager_popup,
        UI_POPUP_ACTION_SECONDARY,
        LV_SYMBOL_EDIT " EDIT / ADD",
        220,
        UI_POPUP_FOOTER_CENTER,
        manager_edit_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_manager_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_OK " USE PRINTER",
        220,
        UI_POPUP_FOOTER_RIGHT,
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


void ui_printer_profiles_show_for_slot(
    int profile_index,
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb)
{
    ui_printer_profiles_show(active_changed_cb, discover_cb);

    if (!s_manager_popup) {
        return;
    }

    if (profile_index < 0 ||
        profile_index >= MOONRAKER_CONFIG_MAX_PROFILES) {
        return;
    }

    s_selected_profile = profile_index;
    manager_rebuild_rows();

    const moonraker_profile_t *profile =
        moonraker_config_profile(profile_index);

    if (!profile || !profile->configured) {
        /* Empty chooser cards should not make an operator select the slot
         * a second time before entering the profile details. */
        manager_edit_cb(NULL);
        return;
    }

    char status[160];
    snprintf(
        status,
        sizeof(status),
        "Selected %s  |  %s:%d",
        profile->name,
        profile->host,
        profile->port);
    manager_set_status(status);
}


void ui_printer_profiles_open_camera_setup(
    int profile_index,
    ui_printer_profiles_active_changed_cb_t active_changed_cb,
    ui_printer_profiles_discover_cb_t discover_cb)
{
    ui_printer_profiles_show_for_slot(
        profile_index, active_changed_cb, discover_cb);
    if (!s_manager_popup) return;
    manager_edit_cb(NULL);
    if (s_editor_popup) editor_camera_open_cb(NULL);
}
