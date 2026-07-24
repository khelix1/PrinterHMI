#pragma once

#include "esp_err.h"
#include "esp_wifi.h"
#include "lvgl.h"
#include <stddef.h>

/*
 * network_wifi_scan
 *
 * Owns WiFi scan workflow/state.
 *
 * Current phase:
 * - Boundary file only.
 *
 * Future ownership:
 * - Start WiFi scan
 * - Collect AP records
 * - Format scan status
 * - Provide scan results to UI
 */

esp_err_t network_wifi_scan_init(void);


void network_wifi_scan_format_found_status(char *buf,
                                           size_t buf_size,
                                           unsigned ap_count);

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
                                      lv_event_cb_t selected_cb);
