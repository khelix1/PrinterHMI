#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_network_tools.h"
#include "ui_text.h"
#include "ui_button.h"
#include "ui_theme.h"
#include "ui_popup.h"
#include "network_wifi_scan.h"

/* Internally-owned WiFi popup objects. */
static lv_obj_t *s_wifi_scan_popup = NULL;
static lv_obj_t *s_wifi_scan_status_label = NULL;
static lv_obj_t *s_wifi_scan_list = NULL;
static lv_obj_t *s_wifi_selected_btn = NULL;
static lv_obj_t *s_wifi_password_popup = NULL;
static lv_obj_t *s_wifi_password_textarea = NULL;


void ui_network_tools_wifi_scan_show_owned(
    const char *status_text,
    lv_event_cb_t close_cb)
{
    ui_network_tools_show_wifi_scan_popup(
        &s_wifi_scan_popup,
        &s_wifi_scan_status_label,
        &s_wifi_scan_list,
        status_text,
        close_cb);
}

void ui_network_tools_wifi_scan_close_owned(void)
{
    ui_network_tools_close_wifi_scan_popup(
        &s_wifi_scan_popup,
        &s_wifi_scan_status_label,
        &s_wifi_scan_list,
        &s_wifi_selected_btn,
        &s_wifi_password_popup,
        &s_wifi_password_textarea);
}

void ui_network_tools_wifi_password_show_owned(
    const char *selected_ssid,
    lv_event_cb_t close_cb,
    lv_event_cb_t save_cb)
{
    ui_network_tools_show_wifi_password_popup_window(
        &s_wifi_password_popup,
        &s_wifi_password_textarea,
        selected_ssid,
        close_cb,
        save_cb);
}

void ui_network_tools_wifi_password_close_owned(void)
{
    ui_network_tools_close_wifi_password_popup(
        &s_wifi_password_popup,
        &s_wifi_password_textarea);
}

void ui_network_tools_wifi_password_copy_owned(
    char *password_buf,
    size_t password_buf_size)
{
    ui_network_tools_copy_wifi_password_text(
        s_wifi_password_textarea,
        password_buf,
        password_buf_size);
}

void ui_network_tools_wifi_select_owned(
    lv_obj_t *btn,
    const char *ssid,
    char *selected_ssid,
    size_t selected_ssid_size,
    char *status_buf,
    size_t status_buf_size)
{
    ui_network_tools_select_wifi_ssid(
        btn,
        &s_wifi_selected_btn,
        ssid,
        selected_ssid,
        selected_ssid_size,
        s_wifi_scan_status_label,
        status_buf,
        status_buf_size);
}

void ui_network_tools_wifi_scan_set_status(
    const char *status_text)
{
    if (s_wifi_scan_status_label) {
        lv_label_set_text(
            s_wifi_scan_status_label,
            status_text ? status_text : ui_text(""));
    }
}

void ui_network_tools_wifi_scan_render_owned(
    char *status_buf,
    size_t status_buf_size,
    const wifi_ap_record_t *aps,
    uint16_t count,
    unsigned total_count,
    lv_event_cb_t close_cb,
    lv_event_cb_t selected_cb)
{
    network_wifi_scan_render_results(
        &s_wifi_scan_popup,
        &s_wifi_scan_status_label,
        &s_wifi_scan_list,
        &s_wifi_selected_btn,
        status_buf,
        status_buf_size,
        aps,
        count,
        total_count,
        close_cb,
        selected_cb);
}

void ui_network_tools_wifi_popup_destroy_all(void)
{
    ui_network_tools_wifi_scan_close_owned();
}


void ui_network_tools_clear_wifi_popup_network_buttons(lv_obj_t *list, lv_obj_t **selected_btn)
{
    if (!list) return;

    lv_obj_clean(list);

    if (selected_btn) {
        *selected_btn = NULL;
    }
}

void ui_network_tools_add_wifi_ssid_button(lv_obj_t *parent,
                                           const char *ssid,
                                           int rssi,
                                           int y,
                                           lv_event_cb_t selected_cb)
{
    if (!parent || !ssid || !ssid[0]) return;

    char row[96];
    snprintf(row, sizeof(row), "%s    RSSI %d", ssid, rssi);

    char *ssid_copy = strdup(ssid);

    lv_obj_t *btn =
        ui_popup_add_selectable_row(
            parent,
            row,
            8,
            y,
            780,
            52,
            selected_cb,
            ssid_copy);

    if (!btn) {
        free(ssid_copy);
    }
}

void ui_network_tools_show_wifi_scan_popup(lv_obj_t **popup,
                                           lv_obj_t **status_label,
                                           lv_obj_t **list,
                                           const char *status_text,
                                           lv_event_cb_t close_cb)
{
    if (!popup || !status_label || !list) return;

    if (*popup) {
        lv_obj_move_foreground(*popup);
        if (*status_label) {
            lv_label_set_text(*status_label, status_text ? status_text : ui_text(""));
        }
        return;
    }

    *popup = ui_popup_create(
        lv_screen_active(),
        860,
        540,
        UI_POPUP_STANDARD);

    if (!*popup) return;

    lv_obj_t *title =
        ui_popup_add_title(
            *popup,
            ui_text("WIFI NETWORKS"),
            false,
            4);

    if (title) {
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 25, 4);
    }

    ui_popup_add_header_divider(
        *popup,
        43);

    *status_label =
        ui_popup_add_status_label(
            *popup,
            status_text ? status_text : ui_text(""),
            20,
            52,
            700);

    *list = ui_popup_add_list(
        *popup,
        20,
        105,
        800,
        350);

    ui_popup_add_standard_footer_divider(*popup);

    ui_popup_add_footer_action(
        *popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_CENTER,
        close_cb,
        NULL,
        NULL);
}

void ui_network_tools_close_wifi_scan_popup(lv_obj_t **scan_popup,
                                            lv_obj_t **scan_label,
                                            lv_obj_t **scan_list,
                                            lv_obj_t **selected_btn,
                                            lv_obj_t **password_popup,
                                            lv_obj_t **password_textarea)
{
    if (password_popup && *password_popup) {
        lv_obj_delete(*password_popup);
        *password_popup = NULL;
    }

    if (password_textarea) {
        *password_textarea = NULL;
    }

    if (scan_popup && *scan_popup) {
        lv_obj_delete(*scan_popup);
        *scan_popup = NULL;
    }

    if (scan_label) *scan_label = NULL;
    if (scan_list) *scan_list = NULL;
    if (selected_btn) *selected_btn = NULL;
}


void ui_network_tools_close_wifi_password_popup(lv_obj_t **password_popup,
                                                lv_obj_t **password_textarea)
{
    if (password_popup && *password_popup) {
        lv_obj_delete(*password_popup);
        *password_popup = NULL;
    }

    if (password_textarea) {
        *password_textarea = NULL;
    }
}

void ui_network_tools_show_wifi_password_popup_window(lv_obj_t **password_popup,
                                                       lv_obj_t **password_textarea,
                                                       const char *selected_ssid,
                                                       lv_event_cb_t close_cb,
                                                       lv_event_cb_t save_cb)
{
    if (!password_popup || !password_textarea || !selected_ssid || !selected_ssid[0]) return;

    if (*password_popup) {
        lv_obj_move_foreground(*password_popup);
        return;
    }

    *password_popup =
        ui_popup_create(
            lv_screen_active(),
            960,
            570,
            UI_POPUP_STANDARD);

    if (!*password_popup) return;

    lv_obj_t *title =
        ui_popup_add_title(
            *password_popup,
            ui_text("WIFI PASSWORD"),
            false,
            2);

    if (title) {
        lv_obj_set_pos(title, 30, 2);
    }

    ui_popup_add_header_divider(
        *password_popup,
        42);

    char sbuf[96];
    snprintf(sbuf, sizeof(sbuf), "SSID: %s", selected_ssid);

    lv_obj_t *ssid_label =
        ui_popup_add_status_label(
            *password_popup,
            sbuf,
            40,
            50,
            860);

    if (ssid_label) {
        lv_label_set_long_mode(
            ssid_label,
            LV_LABEL_LONG_DOT);
    }

    *password_textarea =
        ui_popup_add_textarea(
            *password_popup,
            860,
            56,
            LV_ALIGN_TOP_MID,
            0,
            92,
            true,
            true,
            64,
            ui_text("Enter WiFi password"),
            NULL,
            NULL);

    if (!*password_textarea) {
        lv_obj_delete(*password_popup);
        *password_popup = NULL;
        return;
    }

    ui_popup_add_keyboard(
        *password_popup,
        *password_textarea,
        900,
        320,
        LV_ALIGN_TOP_MID,
        0,
        150,
        LV_KEYBOARD_MODE_TEXT_LOWER);

    ui_popup_add_standard_footer_divider(*password_popup);

    lv_obj_t *back_label = NULL;

    ui_popup_add_footer_action(
        *password_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        160,
        UI_POPUP_FOOTER_LEFT,
        close_cb,
        NULL,
        &back_label);

    lv_obj_t *save_label = NULL;

    ui_popup_add_footer_action(
        *password_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_WIFI " CONNECT",
        180,
        UI_POPUP_FOOTER_RIGHT,
        save_cb,
        NULL,
        &save_label);

}


void ui_network_tools_copy_wifi_password_text(lv_obj_t *password_textarea,
                                               char *password_buf,
                                               size_t password_buf_size)
{
    if (!password_buf || password_buf_size == 0) return;

    const char *pw = "";
    if (password_textarea) {
        const char *txt = lv_textarea_get_text(password_textarea);
        if (txt) pw = txt;
    }

    snprintf(password_buf, password_buf_size, "%s", pw);
}


void ui_network_tools_select_wifi_ssid(lv_obj_t *btn,
                                      lv_obj_t **selected_btn,
                                      const char *ssid,
                                      char *selected_ssid,
                                      size_t selected_ssid_size,
                                      lv_obj_t *status_label,
                                      char *status_buf,
                                      size_t status_buf_size)
{
    if (!ssid || !selected_ssid || selected_ssid_size == 0) return;

    snprintf(selected_ssid, selected_ssid_size, "%s", ssid);

    if (selected_btn && *selected_btn) {
        ui_popup_set_selectable_row_selected(
            *selected_btn,
            false);
    }

    if (selected_btn) {
        *selected_btn = btn;
    }

    if (selected_btn && *selected_btn) {
        ui_popup_set_selectable_row_selected(
            *selected_btn,
            true);
    }

    if (status_buf && status_buf_size > 0) {
        snprintf(status_buf, status_buf_size,
                 "Selected WiFi:\n%s\n\nEnter password to connect.",
                 selected_ssid);
    }

    if (status_label && status_buf) {
        lv_label_set_text(status_label, status_buf);
    }
}


void ui_network_tools_populate_wifi_scan_buttons(lv_obj_t *list,
                                                lv_obj_t *fallback_parent,
                                                const wifi_ap_record_t *aps,
                                                uint16_t count,
                                                lv_event_cb_t selected_cb)
{
    if (!aps || !selected_cb) return;

    lv_obj_t *parent = list ? list : fallback_parent;
    if (!parent) return;

    int y = 0;
    for (uint16_t i = 0; i < count; i++) {
        const char *ssid = aps[i].ssid[0] ? (char *)aps[i].ssid : "(hidden)";
        if (y < 900) {
            ui_network_tools_add_wifi_ssid_button(parent, ssid, aps[i].rssi, y, selected_cb);
            y += 58;
        }
    }
}

/* Moonraker port edit popup */
static lv_obj_t *s_moon_port_popup = NULL;
static lv_obj_t *s_moon_port_ta = NULL;
static ui_network_port_save_cb_t s_save_cb = NULL;

void ui_network_port_popup_close(void)
{
    if (s_moon_port_popup) {
        lv_obj_delete(s_moon_port_popup);
        s_moon_port_popup = NULL;
        s_moon_port_ta = NULL;
        s_save_cb = NULL;
    }
}

static void close_cb(lv_event_t *e)
{
    (void)e;
    ui_network_port_popup_close();
}

static void save_cb(lv_event_t *e)
{
    (void)e;

    if (!s_moon_port_ta) return;

    const char *txt = lv_textarea_get_text(s_moon_port_ta);
    int port = txt ? atoi(txt) : 0;

    if (port > 0 && port < 65536 && s_save_cb) {
        s_save_cb(port);
    }

    ui_network_port_popup_close();
}

void ui_network_port_popup_show(int current_port,
                                ui_network_port_save_cb_t save_cb_fn)
{
    if (s_moon_port_popup) {
        lv_obj_move_foreground(s_moon_port_popup);
        return;
    }

    s_save_cb = save_cb_fn;

    s_moon_port_popup =
        ui_popup_create(
            lv_screen_active(),
            620,
            500,
            UI_POPUP_STANDARD);

    if (!s_moon_port_popup) {
        s_save_cb = NULL;
        return;
    }

    lv_obj_t *title =
        ui_popup_add_title(
            s_moon_port_popup,
            ui_text("MOONRAKER PORT"),
            false,
            2);

    if (title) {
        lv_obj_align(
            title,
            LV_ALIGN_TOP_LEFT,
            6,
            2);
    }

    ui_popup_add_header_divider(
        s_moon_port_popup,
        43);

    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%d", current_port);

    s_moon_port_ta =
        ui_popup_add_textarea(
            s_moon_port_popup,
            500,
            60,
            LV_ALIGN_TOP_MID,
            0,
            82,
            true,
            false,
            5,
            NULL,
            pbuf,
            ui_text("0123456789"));

    if (!s_moon_port_ta) {
        ui_network_port_popup_close();
        return;
    }

    ui_popup_add_keyboard(
        s_moon_port_popup,
        s_moon_port_ta,
        560,
        230,
        LV_ALIGN_TOP_MID,
        0,
        165,
        LV_KEYBOARD_MODE_NUMBER);

    ui_popup_add_standard_footer_divider(s_moon_port_popup);

    lv_obj_t *cancel_label = NULL;

    ui_popup_add_footer_action(
        s_moon_port_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        160,
        UI_POPUP_FOOTER_LEFT,
        close_cb,
        NULL,
        &cancel_label);

    lv_obj_t *save_label = NULL;

    ui_popup_add_footer_action(
        s_moon_port_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_SAVE " SAVE",
        160,
        UI_POPUP_FOOTER_RIGHT,
        save_cb,
        NULL,
        &save_label);

}

/* Moonraker test result popup */
static void close_test_popup_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *popup = lv_obj_get_parent(obj);
    if (popup) lv_obj_delete(popup);
}

void ui_network_tools_show_test_moonraker_popup(const char *title_txt, const char *body_txt, bool ok)
{
    lv_obj_t *popup =
        ui_popup_create(
            lv_screen_active(),
            620,
            300,
            ok ? UI_POPUP_STANDARD : UI_POPUP_DANGER);

    if (!popup) return;

    lv_obj_t *title =
        ui_popup_add_title(
            popup,
            title_txt,
            !ok,
            4);

    if (title) {
        if (ok) {
            ui_apply_label_success(title);
        }

        lv_obj_align(
            title,
            LV_ALIGN_TOP_LEFT,
            25,
            4);
    }

    ui_popup_add_header_divider(
        popup,
        44);

    ui_popup_add_body(
        popup,
        body_txt,
        18,
        60,
        560);

    ui_popup_add_standard_footer_divider(popup);

    ui_popup_add_footer_action(
        popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_CENTER,
        close_test_popup_cb,
        NULL,
        NULL);
}
