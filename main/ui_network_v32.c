#include "ui_network_v32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_page_title.h"
#include "ui_button.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_status_banner_v32.h"
#include "ui_page_geometry_v32.h"

/*
 * Temporary application bridges retained in main.c.
 *
 * Network transport, Wi-Fi management, Moonraker probing, NVS ownership,
 * and popup behavior remain outside this page module.
 */
void ui_network_v32_create(void);
void ui_network_v32_destroy(void);

typedef struct {
    lv_obj_t *root;

    lv_obj_t *banner;

    lv_obj_t *wifi_card;
    lv_obj_t *wifi_name;
    lv_obj_t *wifi_state;
    lv_obj_t *wifi_ip;

    lv_obj_t *moonraker_card;
    lv_obj_t *moonraker_host;
    lv_obj_t *moonraker_port;
    lv_obj_t *moonraker_state;
    lv_obj_t *moonraker_http;

    lv_obj_t *networks_card;
    lv_obj_t *networks_status;
    lv_obj_t *networks_hint;
    lv_obj_t *networks_list;

    lv_obj_t *actions_card;
} network_page_ctx_t;

static network_page_ctx_t s_network;

/* ------------------------------------------------------------------------- */
/* Local shared-layout helpers                                                */
/* ------------------------------------------------------------------------- */

static lv_obj_t *network_make_value_label(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y,
    int width,
    lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);

    if (!label) {
        return NULL;
    }

    lv_label_set_text(label, text ? text : "--");
    lv_obj_set_size(label, width, 24);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);

    ui_apply_text_body(label);

    ui_apply_label_primary(label);

    lv_obj_set_style_text_align(
        label,
        align,
        0);

    lv_obj_set_pos(label, x, y);

    return label;
}

static lv_obj_t *network_make_row(
    lv_obj_t *card,
    const char *name,
    const char *value,
    int y,
    lv_obj_t **value_out)
{
    lv_obj_t *name_label =
        network_make_value_label(
            card,
            name,
            18,
            y,
            150,
            LV_TEXT_ALIGN_LEFT);

    if (name_label) {
        ui_apply_label_dim(name_label);
    }

    lv_obj_t *value_label =
        network_make_value_label(
            card,
            value,
            168,
            y,
            190,
            LV_TEXT_ALIGN_RIGHT);

    if (value_out) {
        *value_out = value_label;
    }

    return value_label;
}

static void network_scan_ssid_data_delete_cb(lv_event_t *e)
{
    char *ssid =
        (char *)lv_event_get_user_data(e);

    free(ssid);
}


/* ------------------------------------------------------------------------- */
/* Page ownership                                                             */
/* ------------------------------------------------------------------------- */

void ui_network_v32_show(void)
{
    ui_network_v32_create();
}

void ui_network_v32_hide(void)
{
    ui_network_v32_destroy();
}

void ui_network_v32_refresh(void)
{
}

void ui_network_v32_create_objects(
    const char *banner_text,
    int moonraker_port,
    ui_network_v32_make_info_cb_t make_info_cb,
    lv_event_cb_t wifi_clicked_cb,
    lv_event_cb_t host_clicked_cb,
    lv_event_cb_t port_clicked_cb)
{
    /*
     * Compatibility parameter retained while main.c still uses the old
     * create bridge. The new page uses shared Operator components directly.
     */
    (void)make_info_cb;

    if (s_network.root) {
        lv_obj_move_foreground(s_network.root);
        return;
    }

    /*
     * Full Network page owner.
     *
     * Geometry matches the existing 170 px sidebar and 72 px top bar.
     */
    s_network.root = lv_obj_create(lv_screen_active());

    if (!s_network.root) {
        return;
    }

    lv_obj_set_size(s_network.root,
                    UI_PAGE_ROOT_WIDTH,
                    UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_network.root,
                   UI_PAGE_ROOT_X,
                   UI_PAGE_ROOT_Y);

    lv_obj_clear_flag(
        s_network.root,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_bg_color(
        s_network.root,
        UI_BG,
        0);

    lv_obj_set_style_bg_opa(
        s_network.root,
        LV_OPA_COVER,
        0);

    lv_obj_set_style_border_width(
        s_network.root,
        0,
        0);

    lv_obj_set_style_radius(
        s_network.root,
        0,
        0);

    lv_obj_set_style_pad_all(
        s_network.root,
        0,
        0);

    ui_page_title_create(
        s_network.root,
        LV_SYMBOL_WIFI " NETWORK",
        "Connectivity and remote access");

    /*
     * Network-state banner.
     */
    s_network.banner =
        ui_status_banner_v32_create(
            s_network.root,
            UI_STATUS_BAR_X,
            UI_STATUS_BAR_Y,
            UI_STATUS_BAR_WIDTH,
            UI_STATUS_BAR_HEIGHT);

    if (!s_network.banner) {
        ui_network_v32_destroy_objects(NULL, NULL, NULL);
        return;
    }

    ui_status_banner_v32_set_simple(
        s_network.banner,
        "OFFLINE",
        banner_text ? banner_text : "NETWORK OFFLINE");

    /*
     * Wi-Fi status card.
     */
    s_network.wifi_card =
        ui_create_operator_card(
            s_network.root,
            16,
            130,
            402,
            190);

    if (s_network.wifi_card) {
        ui_create_operator_card_heading(
            s_network.wifi_card,
            LV_SYMBOL_WIFI " WIFI STATUS",
            18,
            14);

        network_make_row(
            s_network.wifi_card,
            "SSID",
            "--",
            52,
            &s_network.wifi_name);

        ui_create_operator_card_divider(
            s_network.wifi_card,
            18,
            80,
            366);

        network_make_row(
            s_network.wifi_card,
            "Connection",
            "OFFLINE",
            88,
            &s_network.wifi_state);

        ui_create_operator_card_divider(
            s_network.wifi_card,
            18,
            116,
            366);

        network_make_row(
            s_network.wifi_card,
            "IP Address",
            "--",
            124,
            &s_network.wifi_ip);

    }

    /*
     * Moonraker status card.
     */
    s_network.moonraker_card =
        ui_create_operator_card(
            s_network.root,
            436,
            130,
            402,
            190);

    if (s_network.moonraker_card) {
        ui_create_operator_card_heading(
            s_network.moonraker_card,
            LV_SYMBOL_UPLOAD " MOONRAKER",
            18,
            14);

        network_make_row(
            s_network.moonraker_card,
            "Host",
            "--",
            48,
            &s_network.moonraker_host);

        ui_create_operator_card_divider(
            s_network.moonraker_card,
            18,
            74,
            366);

        char port_buf[16];
        snprintf(
            port_buf,
            sizeof(port_buf),
            "%d",
            moonraker_port);

        network_make_row(
            s_network.moonraker_card,
            "Port",
            port_buf,
            80,
            &s_network.moonraker_port);

        ui_create_operator_card_divider(
            s_network.moonraker_card,
            18,
            106,
            366);

        network_make_row(
            s_network.moonraker_card,
            "Connection",
            "DISCONNECTED",
            112,
            &s_network.moonraker_state);

        ui_create_operator_card_divider(
            s_network.moonraker_card,
            18,
            138,
            366);

        network_make_row(
            s_network.moonraker_card,
            "Last HTTP",
            "--",
            144,
            &s_network.moonraker_http);
    }

    /*
     * Available Networks.
     *
     * Wi-Fi scan results are rendered directly in this card. Password entry
     * remains popup-owned because it requires the on-screen keyboard.
     */
    s_network.networks_card =
        ui_create_operator_card(
            s_network.root,
            16,
            332,
            474,
            180);

    if (s_network.networks_card) {
        ui_create_operator_card_heading(
            s_network.networks_card,
            LV_SYMBOL_WIFI " AVAILABLE NETWORKS",
            18,
            14);

        s_network.networks_status =
            lv_label_create(s_network.networks_card);

        if (s_network.networks_status) {
            lv_label_set_text(
                s_network.networks_status,
                "READY TO SCAN");

            lv_obj_set_size(
                s_network.networks_status,
                430,
                22);

            lv_label_set_long_mode(
                s_network.networks_status,
                LV_LABEL_LONG_DOT);

            ui_apply_text_caption(
                s_network.networks_status);

            ui_apply_label_dim(
                s_network.networks_status);

            lv_obj_set_pos(
                s_network.networks_status,
                18,
                45);
        }

        ui_create_operator_card_divider(
            s_network.networks_card,
            18,
            70,
            438);

        s_network.networks_list =
            lv_obj_create(s_network.networks_card);

        if (s_network.networks_list) {
            lv_obj_set_size(
                s_network.networks_list,
                438,
                92);

            lv_obj_set_pos(
                s_network.networks_list,
                18,
                77);

            lv_obj_set_style_bg_opa(
                s_network.networks_list,
                LV_OPA_TRANSP,
                0);

            lv_obj_set_style_border_width(
                s_network.networks_list,
                0,
                0);

            lv_obj_set_style_radius(
                s_network.networks_list,
                0,
                0);

            lv_obj_set_style_pad_all(
                s_network.networks_list,
                0,
                0);

            lv_obj_set_style_pad_row(
                s_network.networks_list,
                ui_theme_density_metric(2, 4, 6),
                0);

            lv_obj_set_flex_flow(
                s_network.networks_list,
                LV_FLEX_FLOW_COLUMN);

            lv_obj_set_flex_align(
                s_network.networks_list,
                LV_FLEX_ALIGN_START,
                LV_FLEX_ALIGN_START,
                LV_FLEX_ALIGN_START);

            lv_obj_set_scroll_dir(
                s_network.networks_list,
                LV_DIR_VER);

            lv_obj_set_scrollbar_mode(
                s_network.networks_list,
                LV_SCROLLBAR_MODE_AUTO);
        }
    }

    /*
     * Quick actions.
     *
     * All page actions use the shared Operator button component.
     */
    s_network.actions_card =
        ui_create_operator_card(
            s_network.root,
            508,
            332,
            330,
            180);

    if (s_network.actions_card) {
        ui_create_operator_card_heading(
            s_network.actions_card,
            LV_SYMBOL_SETTINGS " QUICK ACTIONS",
            18,
            14);

        lv_obj_t *scan_button =
            ui_button_create_icon(
                s_network.actions_card,
                UI_BUTTON_OUTLINED,
                LV_SYMBOL_REFRESH,
                "SCAN NETWORKS",
                UI_ACCENT_CYAN,
                UI_BUTTON_ICON_HORIZONTAL);

        if (scan_button) {
            lv_obj_set_size(scan_button, 294, 38);
            lv_obj_set_pos(scan_button, 18, 48);

            if (wifi_clicked_cb) {
                lv_obj_add_event_cb(
                    scan_button,
                    wifi_clicked_cb,
                    LV_EVENT_CLICKED,
                    NULL);
            }
        }

        /*
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
    }

}

void ui_network_v32_refresh_objects(
    const char *banner_text,
    const char *wifi_text,
    const char *ip_text,
    bool moonraker_connected,
    const char *host_text,
    int http_code,
    const char *scan_status_text)
{
    if (!s_network.root) {
        return;
    }

    const bool wifi_connected =
        ip_text &&
        ip_text[0] &&
        ip_text[0] != '-';

    if (s_network.banner) {
        ui_status_banner_v32_set_simple(
            s_network.banner,
            moonraker_connected
                ? "CONNECTED"
                : (wifi_connected ? "WIFI" : "OFFLINE"),
            banner_text ? banner_text : "NETWORK OFFLINE");
    }

    if (s_network.wifi_name) {
        lv_label_set_text(
            s_network.wifi_name,
            wifi_text ? wifi_text : "OFFLINE");
    }

    if (s_network.wifi_state) {
        lv_label_set_text(
            s_network.wifi_state,
            wifi_connected
                ? "CONNECTED"
                : "OFFLINE");

        lv_obj_set_style_text_color(
            s_network.wifi_state,
            wifi_connected
                ? UI_OK
                : UI_TEXT_DIM,
            0);
    }

    if (s_network.wifi_ip) {
        lv_label_set_text(
            s_network.wifi_ip,
            ip_text ? ip_text : "--");
    }

    if (s_network.moonraker_host) {
        lv_label_set_text(
            s_network.moonraker_host,
            host_text ? host_text : "--");
    }

    if (s_network.moonraker_state) {
        lv_label_set_text(
            s_network.moonraker_state,
            moonraker_connected
                ? "CONNECTED"
                : "DISCONNECTED");

        lv_obj_set_style_text_color(
            s_network.moonraker_state,
            moonraker_connected
                ? UI_OK
                : UI_TEXT_DIM,
            0);
    }

    if (s_network.moonraker_http) {
        char http_buf[24];

        if (http_code > 0) {
            snprintf(
                http_buf,
                sizeof(http_buf),
                "%d",
                http_code);
        } else {
            snprintf(
                http_buf,
                sizeof(http_buf),
                "--");
        }

        lv_label_set_text(
            s_network.moonraker_http,
            http_buf);
    }

    if (s_network.networks_status) {
        if (scan_status_text && scan_status_text[0]) {
            lv_label_set_text(
                s_network.networks_status,
                scan_status_text);
        } else {
            lv_label_set_text(
                s_network.networks_status,
                "READY TO SCAN");
        }
    }

}

void ui_network_v32_set_scan_status(
    const char *status_text)
{
    if (!s_network.networks_status) {
        return;
    }

    lv_label_set_text(
        s_network.networks_status,
        status_text && status_text[0]
            ? status_text
            : "READY TO SCAN");
}


void ui_network_v32_render_scan_results(
    const wifi_ap_record_t *aps,
    uint16_t count,
    unsigned total_count,
    lv_event_cb_t selected_cb)
{
    if (!s_network.networks_list) {
        return;
    }

    lv_obj_clean(s_network.networks_list);

    if (!aps || count == 0 || !selected_cb) {
        ui_network_v32_set_scan_status(
            "NO NETWORKS FOUND");

        return;
    }

    char status[72];

    if (total_count > count) {
        snprintf(
            status,
            sizeof(status),
            "%u FOUND - SHOWING %u",
            total_count,
            (unsigned)count);
    } else {
        snprintf(
            status,
            sizeof(status),
            "%u NETWORK%s FOUND - TAP TO CONNECT",
            total_count,
            total_count == 1 ? "" : "S");
    }

    ui_network_v32_set_scan_status(status);

    for (uint16_t i = 0; i < count; i++) {
        const char *ssid =
            aps[i].ssid[0]
                ? (const char *)aps[i].ssid
                : "(hidden network)";

        lv_obj_t *button =
            ui_button_create_empty(
                s_network.networks_list,
                UI_BUTTON_OUTLINED);

        if (!button) {
            continue;
        }

        lv_obj_set_width(button, 418);
        lv_obj_set_height(button, 40);

        lv_obj_set_flex_grow(button, 0);

        lv_obj_set_style_pad_left(
            button,
            UI_PAD_CARD,
            0);

        lv_obj_set_style_pad_right(
            button,
            UI_PAD_CARD,
            0);

        char *ssid_copy =
            malloc(strlen(ssid) + 1);

        if (ssid_copy) {
            strcpy(ssid_copy, ssid);

            lv_obj_add_event_cb(
                button,
                selected_cb,
                LV_EVENT_CLICKED,
                ssid_copy);

            lv_obj_add_event_cb(
                button,
                network_scan_ssid_data_delete_cb,
                LV_EVENT_DELETE,
                ssid_copy);
        }

        lv_obj_t *name =
            lv_label_create(button);

        if (name) {
            lv_label_set_text(name, ssid);
            lv_obj_set_width(name, 286);

            lv_label_set_long_mode(
                name,
                LV_LABEL_LONG_DOT);

            ui_apply_text_body(name);

            ui_apply_label_primary(name);

            lv_obj_align(
                name,
                LV_ALIGN_LEFT_MID,
                0,
                0);
        }

        lv_obj_t *signal =
            lv_label_create(button);

        if (signal) {
            char rssi[24];

            snprintf(
                rssi,
                sizeof(rssi),
                "%d dBm",
                aps[i].rssi);

            lv_label_set_text(signal, rssi);
            lv_obj_set_width(signal, 88);

            ui_apply_text_caption(signal);

            lv_obj_set_style_text_color(
                signal,
                UI_ACCENT_CYAN,
                0);

            lv_obj_set_style_text_align(
                signal,
                LV_TEXT_ALIGN_RIGHT,
                0);

            lv_obj_align(
                signal,
                LV_ALIGN_RIGHT_MID,
                0,
                0);
        }
    }

    lv_obj_scroll_to_y(
        s_network.networks_list,
        0,
        LV_ANIM_OFF);
}


void ui_network_v32_set_port(int port)
{
    if (!s_network.moonraker_port) {
        return;
    }

    char port_buf[16];

    snprintf(
        port_buf,
        sizeof(port_buf),
        "%d",
        port);

    lv_label_set_text(
        s_network.moonraker_port,
        port_buf);
}

void ui_network_v32_destroy_objects(
    lv_obj_t **network_selected_ssid_label,
    lv_obj_t **network_password_ta,
    lv_obj_t **network_keyboard)
{
    if (s_network.root) {
        lv_obj_delete(s_network.root);
    }

    s_network = (network_page_ctx_t){0};

    if (network_selected_ssid_label) {
        *network_selected_ssid_label = NULL;
    }

    if (network_password_ta) {
        *network_password_ta = NULL;
    }

    if (network_keyboard) {
        *network_keyboard = NULL;
    }
}
