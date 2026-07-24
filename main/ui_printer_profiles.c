#include "ui_printer_profiles.h"

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

    ui_popup_add_footer_action(
        s_editor_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        180,
        UI_POPUP_FOOTER_LEFT,
        editor_cancel_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_editor_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_SAVE " SAVE",
        180,
        UI_POPUP_FOOTER_RIGHT,
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
