#include "network_wifi_scan.h"
#include "ui_network_tools.h"
#include <stdio.h>

esp_err_t network_wifi_scan_init(void)
{
    return ESP_OK;
}


void network_wifi_scan_format_found_status(char *buf,
                                           size_t buf_size,
                                           unsigned ap_count)
{
    if (!buf || buf_size == 0) return;

    snprintf(buf, buf_size,
             "WiFi scan: %u found\nTap a network to select it.",
             ap_count);
}

void network_wifi_scan_render_results(lv_obj_t **scan_popup,
                                      lv_obj_t **scan_label,
                                      lv_obj_t **scan_list,
                                      lv_obj_t **selected_btn,
                                      char *status_buf,
                                      size_t status_buf_size,
                                      const wifi_ap_record_t *aps,
                                      uint16_t visible_count,
                                      unsigned total_count,
                                      lv_event_cb_t close_cb,
                                      lv_event_cb_t selected_cb)
{
    if (!status_buf || status_buf_size == 0) return;

    network_wifi_scan_format_found_status(status_buf, status_buf_size, total_count);

    ui_network_tools_show_wifi_scan_popup(scan_popup,
                                          scan_label,
                                          scan_list,
                                          status_buf,
                                          close_cb);

    if (scan_label && *scan_label) {
        lv_label_set_text(*scan_label, status_buf);
    }

    ui_network_tools_clear_wifi_popup_network_buttons(scan_list ? *scan_list : NULL,
                                                      selected_btn);

    ui_network_tools_populate_wifi_scan_buttons(scan_list ? *scan_list : NULL,
                                                scan_popup ? *scan_popup : NULL,
                                                aps,
                                                visible_count,
                                                selected_cb);
}

