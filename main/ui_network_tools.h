#pragma once

/*
 * Network tools module.
 *
 * Future owner of Network-related popups/actions:
 * - WiFi scan popup
 * - WiFi password popup
 * - Host editor
 * - Port editor
 * - Moonraker test/scan popup
 *
 * For now this file establishes the module boundary only.
 */

#include "lvgl.h"
#include "esp_wifi.h"

/*
 * Internally-owned WiFi popup workflow.
 *
 * The module owns all scan/password popup LVGL object references.
 * Application credential buffers and connection logic remain external.
 */
void ui_network_tools_wifi_scan_show_owned(
    const char *status_text,
    lv_event_cb_t close_cb);

void ui_network_tools_wifi_scan_close_owned(void);

void ui_network_tools_wifi_password_show_owned(
    const char *selected_ssid,
    lv_event_cb_t close_cb,
    lv_event_cb_t save_cb);

void ui_network_tools_wifi_password_close_owned(void);

void ui_network_tools_wifi_password_copy_owned(
    char *password_buf,
    size_t password_buf_size);

void ui_network_tools_wifi_select_owned(
    lv_obj_t *btn,
    const char *ssid,
    char *selected_ssid,
    size_t selected_ssid_size,
    char *status_buf,
    size_t status_buf_size);

void ui_network_tools_wifi_scan_set_status(
    const char *status_text);

void ui_network_tools_wifi_scan_render_owned(
    char *status_buf,
    size_t status_buf_size,
    const wifi_ap_record_t *aps,
    uint16_t count,
    unsigned total_count,
    lv_event_cb_t close_cb,
    lv_event_cb_t selected_cb);

void ui_network_tools_wifi_popup_destroy_all(void);

/* WiFi scan list helpers */
void ui_network_tools_clear_wifi_popup_network_buttons(lv_obj_t *list, lv_obj_t **selected_btn);
void ui_network_tools_add_wifi_ssid_button(lv_obj_t *parent,
                                           const char *ssid,
                                           int rssi,
                                           int y,
                                           lv_event_cb_t selected_cb);

/* WiFi scan popup */
void ui_network_tools_show_wifi_scan_popup(lv_obj_t **popup,
                                           lv_obj_t **status_label,
                                           lv_obj_t **list,
                                           const char *status_text,
                                           lv_event_cb_t close_cb);

void ui_network_tools_close_wifi_scan_popup(lv_obj_t **scan_popup,
                                            lv_obj_t **scan_label,
                                            lv_obj_t **scan_list,
                                            lv_obj_t **selected_btn,
                                            lv_obj_t **password_popup,
                                            lv_obj_t **password_textarea);


/* WiFi password popup */
void ui_network_tools_copy_wifi_password_text(lv_obj_t *password_textarea,
                                               char *password_buf,
                                               size_t password_buf_size);


void ui_network_tools_close_wifi_password_popup(lv_obj_t **password_popup,
                                                lv_obj_t **password_textarea);

void ui_network_tools_show_wifi_password_popup_window(lv_obj_t **password_popup,
                                                       lv_obj_t **password_textarea,
                                                       const char *selected_ssid,
                                                       lv_event_cb_t close_cb,
                                                       lv_event_cb_t save_cb);


void ui_network_tools_select_wifi_ssid(lv_obj_t *btn,
                                      lv_obj_t **selected_btn,
                                      const char *ssid,
                                      char *selected_ssid,
                                      size_t selected_ssid_size,
                                      lv_obj_t *status_label,
                                      char *status_buf,
                                      size_t status_buf_size);


void ui_network_tools_populate_wifi_scan_buttons(lv_obj_t *list,
                                                lv_obj_t *fallback_parent,
                                                const wifi_ap_record_t *aps,
                                                uint16_t count,
                                                lv_event_cb_t selected_cb);

typedef void (*ui_network_port_save_cb_t)(int port);

void ui_network_port_popup_show(int current_port,
                                ui_network_port_save_cb_t save_cb);
void ui_network_port_popup_close(void);

void ui_network_tools_show_test_moonraker_popup(const char *title_txt, const char *body_txt, bool ok);
