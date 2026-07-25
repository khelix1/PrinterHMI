#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <stdbool.h>
#include "ui_button.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_app_desc.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_sntp.h"
#include <time.h>
#include <sys/time.h>
#include "esp_hosted.h"
#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "ui_dashboard_v32.h"
#include "ui_dashboard_status_v32.h"
#include "ui_command_bar_v32.h"
#include "ui_ota_popup.h"
#include "ota_manager.h"
#include "operator_event_log.h"

/*
 * PrinterHMI v4.0.0 Multi-Printer Release
 * Board: JC1060P470C / ESP32-P4 1024x600 MIPI-DSI + GT911 touch
 *
 * WiFi engine: copied from the proven P4+C6 hosted WiFi build.
 *
 * Important rule:
 * Keep the vendor/BSP display + touch init path untouched.
 */

#define MAXIMUM_RETRY 8
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "PrinterHMI";

static lv_obj_t *wifi_label = NULL;
static lv_obj_t *clock_label = NULL;
static lv_obj_t *topbar_eta_label = NULL;
static lv_obj_t *topbar_wifi_bars[4] = {0};


void app_files_reload(void);
void ui_files_v32_refresh(void);
static void files_refresh_bridge(void);
static void files_select_bridge(const char *path);
static void files_preview_bridge(const char *path);


    


static void sntp_wait_task(void *arg)
{
    (void)arg;

    time_t now = 0;
    struct tm timeinfo = {0};

    for (int i = 0; i < 30; i++) {
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year >= (2024 - 1900)) {
            char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S %p", &timeinfo);
            ESP_LOGI(TAG, "SNTP time set: %s", buf);
            // clock_timer_cb(NULL);  // unsafe from SNTP task
            vTaskDelete(NULL);
            return;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGW(TAG, "SNTP did not set time yet");
    vTaskDelete(NULL);
}

static void start_sntp_time_sync(void)
{
    if (esp_sntp_enabled()) {
        return;
    }

    ESP_LOGI(TAG, "Starting SNTP time sync");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();
    xTaskCreate(sntp_wait_task, "sntp_wait", 4096, NULL, 5, NULL);
}

static void start_sntp_time_sync(void);
static void sntp_wait_task(void *arg);

#include "ui_theme.h"
#include "theme_manager.h"
#include "ui_cards.h"
#include "ui_page_title.h"
#include "ui_page_geometry_v32.h"
#include "ui_widgets.h"
#include "ui_settings.h"
#include "timezone_config.h"
#include "ui_splash_v32.h"
#include "ui_shell.h"
#include "ui_telemetry_v32.h"
#include "telemetry_history.h"
#include "ui_printer_v32.h"
#include "ui_printer_motion.h"
#include "ui_printer_popups.h"
#include "ui_printer_live_status.h"
#include "ui_printer_layout_v32.h"
#include "ui_printer_info_cards.h"
#include "ui_printer_actions.h"
#include "ui_printer_banner.h"
#include "printer_controller.h"
#include "printer_files.h"
#include "thumbnail_manager_v32.h"
#include "thumbnail_render_v32.h"
#include "ui_network_v32.h"
#include "ui_printer_profiles.h"
#include "ui_printer_chooser_v32.h"
#include "printer_profile_health.h"
#include "printer_preview_cache_v32.h"
#include "printer_profile_preview_worker_v32.h"
#include "printer_preview_store_v32.h"
#include "network_status_controller.h"
#include "ui_network_tools.h"
#include "moonraker_discovery.h"
#include "moonraker_probe.h"
#include "network_wifi_scan.h"
#include "ui_drybox_v32.h"
#include "ui_drybox_page_v32.h"
#include "ui_files_v32.h"
#include "ui_thumbnail_v32.h"
#include "ui_toast_v32.h"
#include "file_detail_loader_v32.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "moonraker_live_transport.h"
#include "moonraker_poll.h"
#include "moonraker_live_websocket.h"


#include "thumbnail_preview_coordinator_v32.h"

#include "thumbnail_session_v32.h"

#include "printer_ui_controller.h"
#include "printer_file_controller.h"

#include "files_page_controller.h"
void ui_shell_raise(void)
{
    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}


static bool sd_card_ok = false;
static bool sd_mount_attempted = false;
static char sd_status[128] = "SD: not mounted";

static char wifi_status[128] = "";

#define FW_NAME "PrinterHMI"
#define FW_VERSION "v4.0.0"
#define FW_BUILD_NAME "Multi-Printer Release"
#define FW_BUILD_STAMP __DATE__ " " __TIME__

static char moonraker_status[160] = "Moonraker: waiting for WiFi...";
static lv_obj_t *printer_panel = NULL;

static ui_printer_layout_v32_t printer_layout = {0};
static lv_obj_t *printer_thumb_box = NULL;
static ui_thumbnail_v32_t *printer_thumb_view = NULL;

#define THUMB_TARGET_LIVE  0
#define THUMB_TARGET_POPUP 1
static volatile int printer_thumb_target = THUMB_TARGET_LIVE;

static lv_timer_t *thumb_poll_timer = NULL;

static lv_obj_t *drybox_panel = NULL;


static lv_obj_t *network_selected_ssid_label = NULL;
static lv_obj_t *network_password_ta = NULL;
static lv_obj_t *network_keyboard = NULL;
static char ui_network_tools_selected_wifi_password[65] = "";
static char saved_wifi_ssid[33] = "";
static char saved_wifi_password[65] = "";
static char ui_network_tools_network_scan_status[512] = "WiFi scan: not run";
static char ui_network_tools_selected_wifi_ssid[33] = "";
static lv_obj_t *drybox_banner_label = NULL;
static lv_obj_t *drybox_air_label = NULL;
static lv_obj_t *drybox_center_label = NULL;
static lv_obj_t *drybox_humidity_label = NULL;
static lv_obj_t *drybox_target_label = NULL;
static lv_obj_t *drybox_heater_label = NULL;
static lv_obj_t *drybox_fan_label = NULL;


/*
 * Drybox program selection is application state, not inferred from
 * heater activity or banner text.
 *
 * selected_program remembers the material profile so RESUME can
 * return from HOLD to PLA or PETG.
 */
static ui_drybox_program_v32_t s_drybox_selected_program =
    UI_DRYBOX_PROGRAM_NONE;

static ui_drybox_program_v32_t s_drybox_active_program =
    UI_DRYBOX_PROGRAM_NONE;
static lv_obj_t *printer_state_label = NULL;
static lv_obj_t *printer_file_label = NULL;
static lv_obj_t *printer_active_file_box = NULL;
static lv_obj_t *printer_active_file_label = NULL;
static char printer_state[32] = "--";
static char printer_file[160] = "--";
static lv_obj_t *printer_progress_label = NULL;
static lv_obj_t *printer_tuning_label = NULL;
static lv_obj_t *printer_fan_label = NULL;
static lv_obj_t *printer_speed_label = NULL;
static lv_obj_t *printer_flow_label = NULL;
static double printer_progress = -1.0;
static double printer_print_duration = 0.0;
static double printer_jog_step = 10.0;
static char temp_popup_status[96] = "";
static lv_obj_t *motion_step1_btn = NULL;
static lv_obj_t *motion_step10_btn = NULL;
static lv_obj_t *motion_step50_btn = NULL;
static lv_obj_t *printer_nozzle_label = NULL;
static lv_obj_t *printer_bed_label = NULL;
static lv_obj_t *printer_part_fan_label = NULL;
static lv_obj_t *printer_eta_label = NULL;
static lv_obj_t *printer_banner_label = NULL;
static lv_obj_t *printer_elapsed_label = NULL;
static lv_obj_t *printer_remaining_label = NULL;
static ui_printer_info_cards_t printer_info_cards = {0};
static lv_obj_t *printer_home_btn = NULL;
static lv_obj_t *printer_pause_btn = NULL;
static lv_obj_t *printer_resume_btn = NULL;
static lv_obj_t *printer_object_btn = NULL;
static lv_obj_t *printer_cancel_btn = NULL;
static ui_printer_actions_t printer_actions = {0};
static double printer_nozzle_temp = -999.0;
static double printer_nozzle_target = -999.0;
static double printer_bed_temp = -999.0;
static double printer_bed_target = -999.0;
static lv_obj_t *card_chamber_temp = NULL;
static lv_obj_t *card_humidity = NULL;
static lv_obj_t *card_target_rh = NULL;
static lv_obj_t *card_heater = NULL;
static lv_obj_t *card_fan = NULL;
static lv_obj_t *card_moonraker = NULL;
static lv_obj_t *dash_nozzle_label = NULL;
static lv_obj_t *dash_bed_label = NULL;
static lv_obj_t *dash_thumb_img = NULL;
#define dash_thumb_canvas (*ui_dashboard_v32_thumb_canvas_ref())
#define dash_thumb_canvas_buf (*ui_dashboard_v32_thumb_canvas_buf_ref())
static volatile bool dash_thumb_render_running = false;
static volatile bool dash_thumb_render_ready = false;
static volatile bool dash_thumb_render_failed = false;
static lv_timer_t *dash_thumb_render_timer = NULL;
static char dash_thumb_render_file[160] = "";
static int dash_thumb_render_profile_index = -1;
static uint32_t dash_thumb_render_generation = 0;
#define DASH_THUMB_CANVAS_W 286
#define DASH_THUMB_CANVAS_H 215
static char last_dashboard_print_state[32] = "";

#define MOONRAKER_API_KEY ""

static int s_moonraker_code = 0;
static bool s_moonraker_ok = false;
/* P4 polling normalized against CYD v2.3.11 behavior */
#define MOONRAKER_LIVE_POLL_INTERVAL_US 2500000LL
#define MOONRAKER_OBJECTS_CAPACITY 4096
static int64_t s_last_moonraker_ok_us = 0;
static char *s_moonraker_objects = NULL;

static double live_chamber_temp = -999.0;
static double live_air_temp = -999.0;
static double live_humidity = -999.0;
static double live_heater_target = -999.0;
static double live_fan_speed = -999.0;
static double printer_part_fan_speed = -1.0;
static double printer_speed_factor = 100.0;
static double printer_flow_factor = 100.0;
static double printer_live_velocity = 0.0;
static double printer_live_flow = 0.0;
static int printer_current_layer = -1;
static int printer_total_layer = -1;
static double printer_meta_object_height = 0.0;
static double printer_meta_layer_height = 0.0;
static bool live_heater_power = false;
static bool s_live_data_ok = false;
static volatile bool s_moonraker_http_busy = false;


static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_retry_num = 0;
static esp_netif_t *s_sta_netif = NULL;
static esp_ip4_addr_t s_ip = {0};
static bool s_got_ip = false;
static bool s_wifi_credentials_configured = false;
static bool s_wifi_transport_ready = false;
static char connected_wifi_ssid[33] = "";


static void ota_confirm_running_app_valid(void)
{
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;

    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        ESP_LOGI(TAG, "OTA: running partition=%s state=%d",
                 running ? running->label : "unknown", state);

        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "OTA: image marked valid, rollback cancelled");
            } else {
                ESP_LOGE(TAG, "OTA: mark valid failed: %s", esp_err_to_name(err));
            }
        }
    }
#endif
}


static const char *network_banner_text(void)
{
    if (!s_got_ip) return "NETWORK OFFLINE";
    if (s_moonraker_ok) return "NETWORK LINKED";
    return "WIFI CONNECTED";
}

static const char *drybox_banner_text(void)
{
    if (!s_live_data_ok) return "DRYBOX OFFLINE";
    if (live_heater_power) return "DRYBOX HEATING";
    if (live_humidity > 0.0 && live_humidity < 20.0) return "DRYBOX READY";
    return "DRYBOX MONITORING";
}

static const char *printer_banner_text(void);


static void test_moonraker_now(void);
static void safe_copy(char *dst, size_t dst_len, const char *src);
static void dashboard_restore_active_profile_preview(void);


static void reset_preview_state_for_host_change(void)
{
    printer_thumb_target = THUMB_TARGET_LIVE;
    thumbnail_manager_v32_set_force_refresh(true);
    thumbnail_manager_v32_mark_pending();

    thumbnail_session_v32_selected_thumbnail_path()[0] = 0;
    ui_dashboard_v32_thumb_canvas_file()[0] = 0;
    ui_printer_v32_preview_reset();
    thumbnail_preview_coordinator_v32_reset();

    thumbnail_session_v32_free_thumbnail();
}


static void reset_active_printer_runtime_state(void)
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


static void moonraker_configuration_changed(
    const char *status,
    bool retest_now)
{
    reset_preview_state_for_host_change();

    ui_network_v32_set_port(
        moonraker_config_port());

    s_moonraker_ok = false;

    safe_copy(
        moonraker_status,
        sizeof(moonraker_status),
        status ? status : "Moonraker: configuration changed");

    if (retest_now) {
        test_moonraker_now();
    }
}


static void safe_copy(char *dst, size_t dst_len, const char *src)
{
    if (!dst || dst_len == 0) return;
    if (!src) src = "";
    strlcpy(dst, src, dst_len);
}


static const char *wifi_reason_name(uint8_t reason)
{
    switch (reason) {
        case WIFI_REASON_AUTH_EXPIRE: return "AUTH_EXPIRE";
        case WIFI_REASON_AUTH_LEAVE: return "AUTH_LEAVE";
        case WIFI_REASON_ASSOC_EXPIRE: return "ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";
        case WIFI_REASON_NOT_AUTHED: return "NOT_AUTHED";
        case WIFI_REASON_NOT_ASSOCED: return "NOT_ASSOCED";
        case WIFI_REASON_ASSOC_LEAVE: return "ASSOC_LEAVE";
        case WIFI_REASON_ASSOC_NOT_AUTHED: return "ASSOC_NOT_AUTHED";
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: return "4WAY_HANDSHAKE_TIMEOUT";
        case WIFI_REASON_HANDSHAKE_TIMEOUT: return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_BEACON_TIMEOUT: return "BEACON_TIMEOUT";
        case WIFI_REASON_NO_AP_FOUND: return "NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL: return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL: return "ASSOC_FAIL";
        case WIFI_REASON_CONNECTION_FAIL: return "CONNECTION_FAIL";
        default: return "UNKNOWN";
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_wifi_credentials_configured) {
            ESP_LOGI(TAG, "STA_START: connecting with saved credentials");
            safe_copy(wifi_status, sizeof(wifi_status), "Network: connecting...");
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "STA_START: provisioning required");
            safe_copy(
                wifi_status,
                sizeof(wifi_status),
                "Network: setup required");
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
        ESP_LOGI(TAG, "STA_CONNECTED: channel=%d authmode=%d",
                 event->channel, event->authmode);
        if (event->ssid_len > 0) {
            size_t n = event->ssid_len;
            if (n >= sizeof(connected_wifi_ssid)) n = sizeof(connected_wifi_ssid) - 1;
            memcpy(connected_wifi_ssid, event->ssid, n);
            connected_wifi_ssid[n] = 0;
        }
        safe_copy(wifi_status, sizeof(wifi_status), "Network: associated, waiting for IP...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "STA_DISCONNECTED: reason=%u (%s), retry=%d/%d",
                 event->reason, wifi_reason_name(event->reason), s_retry_num, MAXIMUM_RETRY);
        s_got_ip = false;

        if (!s_wifi_credentials_configured) {
            safe_copy(
                wifi_status,
                sizeof(wifi_status),
                "Network: setup required");
            return;
        }

        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            snprintf(wifi_status, sizeof(wifi_status), "Network: retry %d/%d", s_retry_num, MAXIMUM_RETRY);
        } else {
            safe_copy(wifi_status, sizeof(wifi_status), "Network: WiFi failed");
            if (s_wifi_event_group) xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_ip = event->ip_info.ip;
        s_got_ip = true;
        s_retry_num = 0;
        wifi_status[0] = '\0';
        ESP_LOGI(TAG, "GOT_IP");
        start_sntp_time_sync();
        if (s_wifi_event_group) xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        s_got_ip = false;
        safe_copy(wifi_status, sizeof(wifi_status), "Network: lost IP");
        ESP_LOGW(TAG, "LOST_IP");
    }
}


static bool load_wifi_credentials_from_nvs(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("netcfg", NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS WiFi load: no netcfg namespace");
        return false;
    }

    size_t ssid_len = sizeof(saved_wifi_ssid);
    size_t pass_len = sizeof(saved_wifi_password);

    err = nvs_get_str(h, "ssid", saved_wifi_ssid, &ssid_len);
    if (err != ESP_OK || saved_wifi_ssid[0] == 0) {
        nvs_close(h);
        ESP_LOGW(TAG, "NVS WiFi load: no saved SSID");
        return false;
    }

    err = nvs_get_str(h, "pass", saved_wifi_password, &pass_len);
    if (err != ESP_OK) {
        saved_wifi_password[0] = 0;
    }

    nvs_close(h);
    ESP_LOGI(TAG, "NVS WiFi load: saved credentials available");
    return true;
}

static bool save_wifi_credentials_to_nvs(const char *ssid, const char *pass)
{
    if (!ssid || !ssid[0]) return false;
    if (!pass) pass = "";

    nvs_handle_t h;
    esp_err_t err = nvs_open("netcfg", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS WiFi save open failed: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "pass", pass);
    if (err == ESP_OK) err = nvs_commit(h);

    nvs_close(h);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS WiFi save failed: %s", esp_err_to_name(err));
        return false;
    }

    safe_copy(saved_wifi_ssid, sizeof(saved_wifi_ssid), ssid);
    safe_copy(saved_wifi_password, sizeof(saved_wifi_password), pass);

    ESP_LOGI(TAG, "NVS WiFi saved");
    return true;
}


static void wifi_prepare_transport(void)
{
    if (s_wifi_transport_ready) {
        return;
    }

    bool have_saved_wifi = load_wifi_credentials_from_nvs();
    s_wifi_credentials_configured = have_saved_wifi;
    moonraker_config_load();

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(s_wifi_event_group ? ESP_OK : ESP_FAIL);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(s_sta_netif ? ESP_OK : ESP_FAIL);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_LOGI(TAG, "Using WIFI_INIT_CONFIG_DEFAULT P4+C6 hosted path");
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    s_wifi_transport_ready = true;
    ESP_LOGI(TAG, "ESP-Hosted transport ready before display startup");
}


static void wifi_init_sta(void)
{
    ESP_LOGI(TAG, "wifi_init_sta ENTER");

    wifi_prepare_transport();

    /*
     * wifi_init_sta() runs on the main task while LVGL renders on CPU 1.
     * Updating this label without the display mutex can corrupt LVGL's
     * invalid-area list and leave the main task spinning in lv_inv_area.
     */
    if (bsp_display_lock(0)) {
        ui_shell_set_active_printer_name(
            moonraker_config_active_profile_name());
        bsp_display_unlock();
    } else {
        ESP_LOGE(TAG, "Failed to lock display for active printer label");
    }

    wifi_config_t wifi_config = { 0 };

    if (s_wifi_credentials_configured) {
        strlcpy((char *)wifi_config.sta.ssid, saved_wifi_ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, saved_wifi_password, sizeof(wifi_config.sta.password));
        snprintf(wifi_status, sizeof(wifi_status), "Network: using saved WiFi %.24s", saved_wifi_ssid);
    } else {
        safe_copy(
            wifi_status,
            sizeof(wifi_status),
            "Network: setup required");
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    if (s_wifi_credentials_configured) {
        ESP_ERROR_CHECK(
            esp_wifi_set_config(
                WIFI_IF_STA,
                &wifi_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    if (!s_wifi_credentials_configured) {
        ESP_LOGW(
            TAG,
            "No saved WiFi credentials; use Network page to provision");
        return;
    }

    ESP_LOGI(TAG, "Waiting for WiFi/DHCP before display start...");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "CONNECTED + GOT_IP: " IPSTR, IP2STR(&s_ip));
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGW(TAG, "WiFi failed; continuing to display anyway");
    } else {
        ESP_LOGW(TAG, "WiFi timeout; continuing to display anyway");
        safe_copy(wifi_status, sizeof(wifi_status), "Network: timeout, UI still running");
    }

}


static bool moonraker_get_live_objects(void)
{
    if (!s_got_ip || !s_moonraker_objects) {
        return false;
    }

    int live_http_status = 0;

    uint32_t request_generation =
        moonraker_config_generation();

    char request_host[
        MOONRAKER_CONFIG_HOST_LENGTH];

    safe_copy(
        request_host,
        sizeof(request_host),
        moonraker_config_host());

    int request_port =
        moonraker_config_port();

    bool transport_ok = moonraker_live_transport_fetch(
        request_host,
        request_port,
        MOONRAKER_API_KEY,
        s_moonraker_objects,
        MOONRAKER_OBJECTS_CAPACITY,
        &live_http_status);

    if (request_generation !=
        moonraker_config_generation()) {
        ESP_LOGW(
            TAG,
            "Discarding stale Moonraker response from %s:%d",
            request_host,
            request_port);

        return false;
    }

    s_moonraker_code = live_http_status;

    if (!transport_ok) {
        s_live_data_ok = false;
        moonraker_state_set_connection(false, false);
        return false;
    }

    double val = 0.0;

    if (json_find_number_after(s_moonraker_objects, "\"temperature_sensor drybox_center\"", "temperature", &val)) {
        live_chamber_temp = val;
    }

    if (json_find_number_after(s_moonraker_objects, "\"sht3x drybox_env\"", "temperature", &val)) {
        live_air_temp = val;
    }

    if (json_find_number_after(s_moonraker_objects, "\"sht3x drybox_env\"", "humidity", &val)) {
        live_humidity = val;
    }

    if (json_find_number_after(s_moonraker_objects, "\"heater_generic drybox_heater\"", "target", &val)) {
        live_heater_target = val;
    }

    if (json_find_number_after(s_moonraker_objects, "\"heater_generic drybox_heater\"", "power", &val)) {
        live_heater_power = val > 0.01;
    }

    
    if (json_find_number_after(s_moonraker_objects, "\"fan\"", "speed", &val)) {
        printer_part_fan_speed = val * 100.0;
    }

    if (json_find_number_after(s_moonraker_objects, "\"gcode_move\"", "speed_factor", &val)) {
        printer_speed_factor = val * 100.0;
    }

    if (json_find_number_after(s_moonraker_objects, "\"gcode_move\"", "extrude_factor", &val)) {
        printer_flow_factor = val * 100.0;
    }

    if (json_find_number_after(s_moonraker_objects, "\"fan_generic drybox_fan\"", "speed", &val)) {
        live_fan_speed = val * 100.0;
    }

    /*
     * Klipper is the authoritative Drybox program source.
     *
     * Codes exposed by gcode_macro DRYBOX_VARS:
     *   0 = none
     *   1 = PLA
     *   2 = PETG
     *   3 = hold
     */
    if (json_find_number_after(
            s_moonraker_objects,
            "\"gcode_macro DRYBOX_VARS\"",
            "selected_program",
            &val)) {
        int program_code = (int)val;

        switch (program_code) {
            case 1:
                s_drybox_selected_program =
                    UI_DRYBOX_PROGRAM_PLA;
                break;

            case 2:
                s_drybox_selected_program =
                    UI_DRYBOX_PROGRAM_PETG;
                break;

            case 0:
            default:
                s_drybox_selected_program =
                    UI_DRYBOX_PROGRAM_NONE;
                break;
        }
    }

    if (json_find_number_after(
            s_moonraker_objects,
            "\"gcode_macro DRYBOX_VARS\"",
            "active_program",
            &val)) {
        int program_code = (int)val;

        switch (program_code) {
            case 1:
                s_drybox_active_program =
                    UI_DRYBOX_PROGRAM_PLA;
                break;

            case 2:
                s_drybox_active_program =
                    UI_DRYBOX_PROGRAM_PETG;
                break;

            case 3:
                s_drybox_active_program =
                    UI_DRYBOX_PROGRAM_HOLD;
                break;

            case 0:
            default:
                s_drybox_active_program =
                    UI_DRYBOX_PROGRAM_NONE;
                break;
        }
    }

//             live_chamber_temp, live_humidity, live_heater_target,
//             live_fan_speed, live_heater_power ? 1 : 0);

    json_find_string(strstr(s_moonraker_objects, "\"print_stats\"") ? strstr(s_moonraker_objects, "\"print_stats\"") : s_moonraker_objects,
                     "state", printer_state, sizeof(printer_state));

    json_find_string(strstr(s_moonraker_objects, "\"print_stats\"") ? strstr(s_moonraker_objects, "\"print_stats\"") : s_moonraker_objects,
                     "filename", printer_file, sizeof(printer_file));

    if (strlen(printer_state) == 0) {
        safe_copy(printer_state, sizeof(printer_state), "--");
    }

    if (strlen(printer_file) == 0) {
        safe_copy(printer_file, sizeof(printer_file), "No file");
    }
    json_find_number_after(s_moonraker_objects, "\"display_status\"", "progress", &printer_progress);
    json_find_number_after(s_moonraker_objects, "\"print_stats\"", "print_duration", &printer_print_duration);

    double lv = 0.0;
    if (json_find_number_after(s_moonraker_objects, "\"motion_report\"", "live_velocity", &lv)) {
        printer_live_velocity = lv;
    }

    double lf = 0.0;
    if (json_find_number_after(s_moonraker_objects, "\"motion_report\"", "live_extruder_velocity", &lf)) {
        printer_live_flow = lf;
    }

    double layer_val = 0.0;
    if (json_find_number_after(s_moonraker_objects, "\"info\"", "current_layer", &layer_val)) {
        printer_current_layer = (int)(layer_val + 0.5);
    }

    if (json_find_number_after(s_moonraker_objects, "\"info\"", "total_layer", &layer_val)) {
        printer_total_layer = (int)(layer_val + 0.5);
    }

    json_find_number_after(s_moonraker_objects, "\"extruder\"", "temperature", &printer_nozzle_temp);
    json_find_number_after(s_moonraker_objects, "\"extruder\"", "target", &printer_nozzle_target);
    json_find_number_after(s_moonraker_objects, "\"heater_bed\"", "temperature", &printer_bed_temp);
    json_find_number_after(s_moonraker_objects, "\"heater_bed\"", "target", &printer_bed_target);

    s_live_data_ok = true;
    
    moonraker_state_update_from_legacy(
        live_chamber_temp,
        live_air_temp,
        live_humidity,
        live_heater_target,
        live_heater_power,
        live_fan_speed,
        printer_part_fan_speed,
        printer_speed_factor,
        printer_flow_factor,
        printer_live_velocity,
        printer_live_flow,
        printer_nozzle_temp,
        printer_nozzle_target,
        printer_bed_temp,
        printer_bed_target,
        printer_progress,
        printer_print_duration,
        printer_current_layer,
        printer_total_layer,
        s_live_data_ok,
        true,
        printer_state,
        printer_file);

    moonraker_state_set_drybox_programs(
        (int)s_drybox_selected_program,
        (int)s_drybox_active_program);

return true;
}

static const char *dash_print_state_text(void)
{
    moonraker_state_t mr_state_snapshot;
    moonraker_state_snapshot(&mr_state_snapshot);
    const moonraker_state_t *mr_state = &mr_state_snapshot;

    const char *txt =
        printer_controller_status_text(
            mr_state->printer_state);

    if (txt && strcmp(txt, "--") != 0) {
        return txt;
    }

    return s_moonraker_ok ? "CONNECTED" : "OFFLINE";
}


static void printer_thumb_start_delayed(void);
static void printer_build_metadata_text(const char *file, char *out, size_t out_sz);
static lv_color_t temp_value_color(double temp, double target)
{
    if (temp < -100.0) return UI_TEXT;
    if (target <= 0.0) return UI_TEXT;

    double diff = target - temp;
    if (diff > 10.0) return UI_DANGER_BRIGHT;   // far below target
    if (diff > 2.0)  return UI_WARN;   // heating
    if (diff < -5.0) return UI_DANGER_BRIGHT;   // overshoot
    return UI_OK_BRIGHT;                    // at/near target
}

static void update_dashboard_status_cards(
    const moonraker_state_t *mr_state)
{
    char buf[64];
    char layer[32];
    char elapsed[32];
    char remaining[32];

    if (dash_nozzle_label) {
        if (mr_state->nozzle_temp > -100.0) {
            snprintf(buf,
                     sizeof(buf),
                     "%.1f / %.1f C",
                     mr_state->nozzle_temp,
                     mr_state->nozzle_target);
        } else {
            snprintf(buf, sizeof(buf), "-- / -- C");
        }

        lv_label_set_text(dash_nozzle_label, buf);
        lv_obj_set_style_text_color(
            dash_nozzle_label,
            temp_value_color(mr_state->nozzle_temp,
                             mr_state->nozzle_target),
            0);
    }

    if (dash_bed_label) {
        if (mr_state->bed_temp > -100.0) {
            snprintf(buf,
                     sizeof(buf),
                     "%.1f / %.1f C",
                     mr_state->bed_temp,
                     mr_state->bed_target);
        } else {
            snprintf(buf, sizeof(buf), "-- / -- C");
        }

        lv_label_set_text(dash_bed_label, buf);
        lv_obj_set_style_text_color(
            dash_bed_label,
            temp_value_color(mr_state->bed_temp,
                             mr_state->bed_target),
            0);
    }

    ui_dashboard_status_v32_refresh(mr_state->progress,
                                    mr_state->print_duration);

    if (mr_state->current_layer > 0 || mr_state->total_layer > 0) {
        snprintf(layer,
                 sizeof(layer),
                 "%d/%d",
                 mr_state->current_layer,
                 mr_state->total_layer);
    } else {
        snprintf(layer, sizeof(layer), "--/--");
    }

    printer_controller_format_hhmm(elapsed,
                                   sizeof(elapsed),
                                   mr_state->print_duration);
    printer_controller_format_remaining(remaining,
                                        sizeof(remaining),
                                        mr_state->progress,
                                        mr_state->print_duration);
    ui_dashboard_v32_set_active_print(layer, elapsed, remaining);

    ui_dashboard_v32_set_active_print_file(mr_state->printer_file);

    ui_dashboard_status_v32_set_print_state(
        dash_print_state_text());

}

static void thumbnail_preview_coordinator_v32_set_live_target(void)
{
    printer_thumb_target = THUMB_TARGET_LIVE;
}

static void update_dashboard_live_preview(
    const moonraker_state_t *mr_state)
{
    thumbnail_preview_coordinator_v32_context_t context = {
        .printer_state = mr_state->printer_state,
        .printer_file = mr_state->printer_file,

        .moonraker_host = moonraker_config_host(),
        .moonraker_port = moonraker_config_port(),

        .selected_print_file = thumbnail_session_v32_selected_file(),
        .selected_print_file_size = thumbnail_session_v32_selected_file_size(),

        .selected_thumbnail_path = thumbnail_session_v32_selected_thumbnail_path(),

        .dashboard_canvas_file =
            ui_dashboard_v32_thumb_canvas_file(),
        .printer_canvas_file =
            ui_printer_v32_preview_canvas_file(),

        .dashboard_canvas = &dash_thumb_canvas,
        .dashboard_image = &dash_thumb_img,
        .printer_canvas = ui_printer_v32_preview_canvas_ref(),
        .printer_image = ui_printer_v32_preview_image_ref(),

        .metadata_info = thumbnail_session_v32_metadata_info(),
        .metadata_info_size = thumbnail_session_v32_metadata_info_size(),

        .set_live_target =
            thumbnail_preview_coordinator_v32_set_live_target,
        .free_thumbnail = thumbnail_session_v32_free_thumbnail,
        .build_metadata = printer_build_metadata_text,
        .start_delayed = printer_thumb_start_delayed,
    };

    thumbnail_preview_coordinator_v32_update(&context);
}

static void update_dashboard_print_complete_cleanup(void)
{
    const char *now_state = dash_print_state_text();

    if ((strcmp(last_dashboard_print_state, "PRINTING") == 0 ||
         strcmp(last_dashboard_print_state, "PAUSED") == 0) &&
        (strcmp(now_state, "READY") == 0 ||
         strcmp(now_state, "CONNECTED") == 0)) {

        ui_dashboard_v32_set_active_print_file("No active file");
        ui_dashboard_status_v32_set_progress("-- %", 0, UI_TEXT);
        ui_dashboard_status_v32_set_times("--:--",
                                          "--:--",
                                          "--:--");

        if (dash_thumb_img) {
            lv_obj_delete(dash_thumb_img);
            dash_thumb_img = NULL;
        }

        ui_dashboard_v32_thumb_set_placeholder(
            "PRINT\nTHUMBNAIL");

        thumbnail_session_v32_selected_file()[0] = 0;
        thumbnail_session_v32_selected_thumbnail_path()[0] = 0;
        thumbnail_session_v32_free_thumbnail();

        ESP_LOGI(TAG, "DASH_PRINT_COMPLETE cleanup");
    }

    safe_copy(last_dashboard_print_state,
              sizeof(last_dashboard_print_state),
              now_state);
}

static void update_environment_cards(void)
{
    char buf[64];

    if (card_chamber_temp) {
        snprintf(buf, sizeof(buf), "%.1f C", live_air_temp);
        lv_label_set_text(card_chamber_temp, buf);
    }

    if (card_humidity) {
        snprintf(buf, sizeof(buf), "%.1f %%RH", live_humidity);
        lv_label_set_text(card_humidity, buf);
    }

    if (card_target_rh) {
        snprintf(buf,
                 sizeof(buf),
                 "Heat %.0f C",
                 live_heater_target);
        lv_label_set_text(card_target_rh, buf);
    }

    if (card_heater) {
        lv_label_set_text(card_heater,
                          live_heater_power ? "ON" : "OFF");
    }

    if (card_fan) {
        snprintf(buf, sizeof(buf), "%.0f %%", live_fan_speed);
        lv_label_set_text(card_fan, buf);
    }

    if (card_moonraker) {
        lv_label_set_text(card_moonraker,
                          s_live_data_ok
                              ? "linked"
                              : "not linked");
    }
}

static void update_live_cards(void)
{
    moonraker_state_t mr_state_snapshot;
    moonraker_state_snapshot(&mr_state_snapshot);
    const moonraker_state_t *mr_state = &mr_state_snapshot;

    update_dashboard_status_cards(mr_state);
    update_dashboard_live_preview(mr_state);
    update_dashboard_print_complete_cleanup();
    update_environment_cards();
}

static void moonraker_live_poll_tasklet(void)
{
    /* WebSocket is authoritative only while subscribed status is fresh.
     * The proven HTTP poller automatically resumes after three seconds.
     */
    if (!s_got_ip) {
        moonraker_state_set_connection(false, false);
    }

    static bool websocket_was_authoritative = false;
    bool websocket_is_fresh =
        moonraker_live_websocket_fresh(3000000LL);

    if (websocket_is_fresh) {
        if (!websocket_was_authoritative) {
            ESP_LOGI(TAG, "LIVE_TRANSPORT websocket authoritative");
        }
        websocket_was_authoritative = true;
        s_moonraker_code = 200;
        s_moonraker_ok = true;
        s_live_data_ok = true;
        return;
    }

    if (websocket_was_authoritative) {
        ESP_LOGW(TAG, "LIVE_TRANSPORT HTTP fallback");
    }
    websocket_was_authoritative = false;

    moonraker_poll_result_t result =
        moonraker_poll_run(
            esp_timer_get_time(),
            s_got_ip,
            s_moonraker_http_busy,
            moonraker_get_live_objects);

    switch (result) {
    case MOONRAKER_POLL_NOT_DUE:
    case MOONRAKER_POLL_BUSY:
        return;

    case MOONRAKER_POLL_OK:
        s_moonraker_code = 200;
        s_moonraker_ok = true;
        s_last_moonraker_ok_us = esp_timer_get_time();

        safe_copy(
            moonraker_status,
            sizeof(moonraker_status),
            "Moonraker: linked");
        return;

    case MOONRAKER_POLL_NO_WIFI:
        s_moonraker_ok = false;

        safe_copy(
            moonraker_status,
            sizeof(moonraker_status),
            "Moonraker: waiting for WiFi...");
        return;

    case MOONRAKER_POLL_FAILED:
    default:
        s_moonraker_ok = false;

        safe_copy(
            moonraker_status,
            sizeof(moonraker_status),
            "Moonraker: live poll timeout");
        return;
    }
}

static bool moonraker_send_gcode(const char *cmd)
{
    if (!s_got_ip || !cmd || !cmd[0]) {
        safe_copy(moonraker_status,
                  sizeof(moonraker_status),
                  "Moonraker: no WiFi for command");
        ui_toast_v32_show(
            UI_STATUS_DANGER,
            "COMMAND NOT SENT",
            "Moonraker is offline.");
        return false;
    }

    int http_code = 0;
    esp_err_t err = ESP_FAIL;

    bool ok = moonraker_send_gcode_script(
        moonraker_config_host(),
        moonraker_config_port(),
        MOONRAKER_API_KEY,
        cmd,
        &http_code,
        &err);

    if (ok) {
        snprintf(moonraker_status,
                 sizeof(moonraker_status),
                 "Sent: %s",
                 cmd);

        const char *excluded = strstr(cmd, "EXCLUDE_OBJECT NAME=");
        if (excluded) {
            char notice[128];
            snprintf(notice,
                     sizeof(notice),
                     "Excluded object: %.96s",
                     excluded + strlen("EXCLUDE_OBJECT NAME="));
            ui_printer_banner_show_notice(notice,
                                           UI_STATUS_OK,
                                           3200);
        }
        return true;
    }

    snprintf(moonraker_status,
             sizeof(moonraker_status),
             "Command failed: %s %d",
             cmd,
             http_code);

    char toast_detail[160];
    snprintf(toast_detail,
             sizeof(toast_detail),
             "HTTP %d  %.110s",
             http_code,
             cmd);
    ui_toast_v32_show(
        UI_STATUS_DANGER,
        "COMMAND FAILED",
        toast_detail);

    return false;
}

void ui_printer_v32_create(void);
/* BEGIN LEGACY PRINTER PAGE BLOCK */
void ui_printer_v32_destroy(void);
void legacy_create_drybox_tab(void);
void legacy_cleanup_drybox_tab(void);
void legacy_refresh_drybox_tab(void);

static void ui_network_tools_wifi_scan_now(void);
static void scan_moonraker_now(void);
static void moonraker_discovery_selected_bridge(const char *host);

static void ui_network_tools_open_wifi_scan_cb(lv_event_t *e)
{
    (void)e;
    ui_network_tools_wifi_scan_now();
}

static void printer_profiles_active_changed_bridge(void)
{
    reset_preview_state_for_host_change();
    reset_active_printer_runtime_state();
    dashboard_restore_active_profile_preview();

    ui_shell_set_active_printer_name(
        moonraker_config_active_profile_name());

    printer_profile_health_reset();
    printer_profile_preview_worker_v32_reset();
    printer_preview_store_v32_reset_restore();
    ui_printer_chooser_v32_refresh();

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
        printer_profiles_active_changed_bridge,
        scan_moonraker_now);
}


static void scan_moonraker_now(void)
{
    if (!s_got_ip) {
        moonraker_discovery_set_status("WiFi is not connected.");
        moonraker_discovery_show(
            NULL, NULL, moonraker_discovery_selected_bridge);
        return;
    }

    if (moonraker_discovery_is_running()) {
        moonraker_discovery_show(
            NULL, NULL, moonraker_discovery_selected_bridge);
        return;
    }

    moonraker_discovery_show(
        "Starting Moonraker discovery...\nClose cancels discovery.",
        NULL,
        moonraker_discovery_selected_bridge);

    if (!moonraker_discovery_start(&s_ip)) {
        moonraker_discovery_set_status(
            "Unable to start Moonraker discovery task.");
    }
}

static void moon_port_popup_save_bridge(int port)
{
    if (!moonraker_config_select_port(port)) {
        return;
    }

    moonraker_configuration_changed(
        "Moonraker: port changed",
        true);
}

static void ui_network_tools_open_port_edit_cb(lv_event_t *e)
{
    (void)e;
    ui_network_port_popup_show(moonraker_config_port(),
                           moon_port_popup_save_bridge);
}

static lv_obj_t *make_printer_info(
    lv_obj_t *parent,
    const char *title,
    const char *value,
    int x,
    int y)
{
    return ui_info_card_create(
        parent,
        title,
        value,
        x,
        y,
        250,
        98);
}

void ui_network_v32_create(void)
{
    ui_network_v32_create_objects(
        network_banner_text(),
        moonraker_config_port(),
        make_printer_info,
        ui_network_tools_open_wifi_scan_cb,
        ui_network_tools_open_printer_profiles_cb,
        ui_network_tools_open_port_edit_cb);
}

static void ui_network_v32_refresh_bridge(void)
{
    char ip_buf[32];

    if (s_got_ip) {
        snprintf(
            ip_buf,
            sizeof(ip_buf),
            IPSTR,
            IP2STR(&s_ip));
    } else {
        snprintf(
            ip_buf,
            sizeof(ip_buf),
            "--");
    }

    network_status_controller_refresh(
        network_banner_text(),
        s_got_ip,
        connected_wifi_ssid,
        ui_network_tools_selected_wifi_ssid,
        ip_buf,
        s_moonraker_ok,
        moonraker_config_host(),
        s_moonraker_code,
        ui_network_tools_network_scan_status);
}

static void ota_test_btn_cb(lv_event_t *e)
{
    (void)e;

    if (ota_manager_is_running()) {
        ESP_LOGW(TAG, "OTA: update already running");
        return;
    }

    if (!ota_manager_start(ota_manager_get_url())) {
        ESP_LOGE(TAG, "OTA: unable to start update");
    }
}

static void ota_popup_start_bridge(const char *url)
{
    if (!url || !url[0]) {
        return;
    }

    if (ota_manager_is_running()) {
        ESP_LOGW(TAG, "OTA: update already running");
        return;
    }

    /*
     * ota_manager_start() copies the URL, creates the progress popup and
     * flushes it before returning. Persist the URL afterward so an NVS write
     * cannot leave the keyboard visible during the transition.
     */
    if (!ota_manager_start(url)) {
        ESP_LOGE(TAG, "OTA: unable to start update");
        return;
    }

    ota_manager_set_url(url);
}

static void ota_popup_remote_bridge(void)
{
    ui_ota_remote_builds_placeholder_show();
}

static void ota_open_popup_cb(lv_event_t *e)
{
    (void)e;

    const esp_app_desc_t *app = esp_app_get_description();
    const esp_partition_t *running = esp_ota_get_running_partition();

    char ota_info[320];
    snprintf(ota_info, sizeof(ota_info),
             "Current firmware v2: %s  |  Built: %s %s  |  Slot: %s",
             app ? app->project_name : "unknown",
             app ? app->date : __DATE__,
             app ? app->time : __TIME__,
             running ? running->label : "unknown");

    ui_ota_popup_show(ota_manager_get_url(),
                      ota_info,
                      ota_manager_url_capacity() - 1,
                      ota_popup_start_bridge,
                      ota_popup_remote_bridge);
}


void ui_files_v32_destroy(void);
static void dashboard_dry_status_event_cb(lv_event_t *e);

static void printer_popup_send_gcode_bridge(const char *cmd)
{
    (void)moonraker_send_gcode(cmd);
}


void ui_command_bar_v32_action(const char *action)
{
    if (!action) return;

    if (strcmp(action, "CANCEL_OBJECT") == 0) {
        ui_printer_popups_show_cancel_object(
            printer_popup_send_gcode_bridge);
        return;
    }

    if (strcmp(action, "CANCEL_PRINT") == 0) {
        ui_printer_popups_show_cancel(
            printer_popup_send_gcode_bridge);
        return;
    }

    if (strcmp(action, "FILES") == 0) {
    ui_files_v32_set_callbacks(
        files_refresh_bridge,
        files_select_bridge,
        files_preview_bridge);
    ui_files_v32_show();
    app_files_reload();
return;
    }

    if (strcmp(action, "DRYBOX") == 0) {
        ui_files_v32_hide();
        ui_printer_v32_hide();
        ui_network_v32_hide();
        hide_settings_tab();
        ui_drybox_v32_show();
        ui_shell_set_active_nav(1);
        return;
    }

    if (strcmp(action, "STATUS") == 0) {
        dashboard_dry_status_event_cb(NULL);
        return;
    }

    moonraker_send_gcode(action);
}


static void ui_dashboard_v32_push_live_banner_data(void)
{
    moonraker_state_t mr_state_snapshot;
    moonraker_state_snapshot(&mr_state_snapshot);
    const moonraker_state_t *mr_state = &mr_state_snapshot;

    char state[48];
    char file[160];
    char progress[24];
    char eta[48];

    printer_controller_format_status_symbol_text(
        state,
        sizeof(state),
        mr_state->printer_state,
        s_moonraker_ok,
        mr_state->live_data_ok);

    if (mr_state->printer_file[0] &&
        strcmp(mr_state->printer_file, "No file") != 0) {
        snprintf(
            file,
            sizeof(file),
            "%.*s",
            (int)sizeof(file) - 1,
            mr_state->printer_file);
    } else {
        snprintf(
            file,
            sizeof(file),
            "No active print");
    }

    if (mr_state->progress >= 0.0) {
        snprintf(
            progress,
            sizeof(progress),
            "%.0f%%",
            mr_state->progress * 100.0);
    } else {
        snprintf(
            progress,
            sizeof(progress),
            "--%%");
    }

    if (printer_eta_label) {
        const char *txt =
            lv_label_get_text(printer_eta_label);

        if (txt && txt[0]) {
            snprintf(
                eta,
                sizeof(eta),
                "%s",
                txt);
        } else {
            snprintf(
                eta,
                sizeof(eta),
                "ETA --:--");
        }
    } else {
        snprintf(
            eta,
            sizeof(eta),
            "ETA --:--");
    }

    ui_dashboard_v32_set_banner(
        state,
        file,
        eta,
        progress);
}


static void ui_dashboard_v32_push_live_machine_data(void)
{
    moonraker_state_t mr_state_snapshot;
    moonraker_state_snapshot(&mr_state_snapshot);
    const moonraker_state_t *mr_state = &mr_state_snapshot;

    char nozzle[32];
    char hotend_name[32];
    char bed[32];
    char chamber[32];
    char humidity[32];
    char speed[24];
    char flow[24];
    char fan[24];

    if (mr_state->nozzle_temp > -100.0) {
        snprintf(
            nozzle,
            sizeof(nozzle),
            "%.1f / %.1f C",
            mr_state->nozzle_temp,
            mr_state->nozzle_target);
    } else {
        snprintf(
            nozzle,
            sizeof(nozzle),
            "-- / -- C");
    }

    snprintf(
        hotend_name,
        sizeof(hotend_name),
        "NOZZLE");

    if (mr_state->hotend_count > 1) {
        for (size_t i = 0;
             i < mr_state->hotend_count;
             ++i) {
            if (!mr_state->hotends[i].active) continue;

            snprintf(
                hotend_name,
                sizeof(hotend_name),
                "T%u ACTIVE",
                (unsigned)i);
            break;
        }
    }

    if (mr_state->bed_temp > -100.0) {
        snprintf(
            bed,
            sizeof(bed),
            "%.1f / %.1f C",
            mr_state->bed_temp,
            mr_state->bed_target);
    } else {
        snprintf(
            bed,
            sizeof(bed),
            "-- / -- C");
    }

    /*
     * The existing dashboard chamber field represents the drybox
     * environmental air temperature, not the center probe.
     */
    if (mr_state->air_temp > -100.0) {
        snprintf(
            chamber,
            sizeof(chamber),
            "%.1f C",
            mr_state->air_temp);
    } else {
        snprintf(
            chamber,
            sizeof(chamber),
            "-- C");
    }

    if (mr_state->humidity > -100.0) {
        snprintf(
            humidity,
            sizeof(humidity),
            "%.1f %%RH",
            mr_state->humidity);
    } else {
        snprintf(
            humidity,
            sizeof(humidity),
            "-- %%RH");
    }

    snprintf(
        speed,
        sizeof(speed),
        "%.0f%%",
        mr_state->speed_factor);

    snprintf(
        flow,
        sizeof(flow),
        "%.0f%%",
        mr_state->flow_factor);

    if (mr_state->part_fan_speed >= 0.0) {
        snprintf(
            fan,
            sizeof(fan),
            "%.0f%%",
            mr_state->part_fan_speed);
    } else {
        snprintf(
            fan,
            sizeof(fan),
            "--%%");
    }

    ui_dashboard_v32_set_machine(
        nozzle,
        bed,
        chamber,
        humidity,
        speed,
        flow,
        fan);

    ui_dashboard_v32_set_active_hotend(
        hotend_name,
        nozzle);
}

static void open_v32_dashboard_cb(lv_event_t *e)
{
    (void)e;
    hide_settings_tab();
    ui_dashboard_v32_create();
}

static void show_settings_tab(void);
static void app_theme_changed(void);


static void printer_chooser_select_bridge(int profile_index)
{
    int previous = moonraker_config_active_profile_index();

    if (!moonraker_config_select_profile(profile_index)) {
        return;
    }

    if (previous != moonraker_config_active_profile_index()) {
        printer_profiles_active_changed_bridge();
    } else {
        ui_shell_set_active_printer_name(
            moonraker_config_active_profile_name());
    }

    ui_printer_chooser_v32_hide();
    ui_dashboard_v32_create();
    dashboard_restore_active_profile_preview();
    ui_shell_set_active_nav(UI_SHELL_PAGE_DASHBOARD);
}


static void printer_chooser_manage_bridge(lv_event_t *event)
{
    (void)event;

    ui_printer_profiles_show(
        printer_profiles_active_changed_bridge,
        scan_moonraker_now);
}



static void printer_chooser_open_from_topbar(void)
{
    ui_telemetry_v32_hide();
    ui_files_v32_hide();
    ui_printer_v32_hide();
    ui_drybox_v32_hide();
    ui_network_v32_hide();
    hide_settings_tab();

    ui_printer_chooser_v32_show(
        printer_chooser_select_bridge,
        printer_chooser_manage_bridge);

    /* The chooser is a destination, not the Dashboard nav selection. */
    ui_shell_set_active_nav(-1);
}


void ui_shell_page_action(ui_shell_page_t page)
{
    /* Every sidebar destination closes the explicit printer chooser. */
    ui_printer_chooser_v32_hide();

    switch (page) {
    case UI_SHELL_PAGE_DASHBOARD:
        ui_telemetry_v32_hide();
        ui_files_v32_hide();
        ui_printer_v32_hide();
        ui_drybox_v32_hide();
        ui_network_v32_hide();
        hide_settings_tab();

        ui_dashboard_v32_create();
        dashboard_restore_active_profile_preview();
        return;

    case UI_SHELL_PAGE_DRYBOX:
        ui_telemetry_v32_hide();
        ui_files_v32_hide();
        ui_printer_v32_hide();
        ui_network_v32_hide();
        hide_settings_tab();
        ui_drybox_v32_show();
        return;

    case UI_SHELL_PAGE_PRINTER:
        ui_telemetry_v32_hide();
        ui_files_v32_hide();
        ui_drybox_v32_hide();
        ui_network_v32_hide();
        hide_settings_tab();
        ui_printer_v32_show();
        return;

    case UI_SHELL_PAGE_FILES:
        ui_telemetry_v32_hide();
        ui_printer_v32_hide();
        ui_drybox_v32_hide();
        ui_network_v32_hide();
        hide_settings_tab();
    ui_files_v32_set_callbacks(
        files_refresh_bridge,
        files_select_bridge,
        files_preview_bridge);
    ui_files_v32_show();
    app_files_reload();
return;

    case UI_SHELL_PAGE_NETWORK:
        ui_telemetry_v32_hide();
        ui_files_v32_hide();
        ui_printer_v32_hide();
        ui_drybox_v32_hide();
        hide_settings_tab();
        ui_network_v32_show();
        return;

    case UI_SHELL_PAGE_SETTINGS:
        ui_telemetry_v32_hide();
        ui_files_v32_hide();
        ui_printer_v32_hide();
        ui_drybox_v32_hide();
        ui_network_v32_hide();
        show_settings_tab();
        return;


    case UI_SHELL_PAGE_TELEMETRY:
        ui_files_v32_hide();
        ui_printer_v32_hide();
        ui_drybox_v32_hide();
        ui_network_v32_hide();
        hide_settings_tab();
        ui_telemetry_v32_show();
        return;

    default:
        return;
    }
}


static void printer_thumb_cleanup_for_popup_close(void)
{
    thumbnail_manager_v32_set_task_running(false);
    thumbnail_manager_v32_mark_pending();

    thumb_poll_timer = NULL;

    dash_thumb_img = NULL;
    printer_thumb_box = NULL;
    printer_thumb_view = NULL;
}

/* END LEGACY DRYBOX BLOCK */

static void close_printer_file_detail_popup(void);
void ui_files_v32_destroy(void)
{
    close_printer_file_detail_popup();

}


static void moonraker_sync_legacy_from_state(void)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    live_chamber_temp = state.chamber_temp;
    live_air_temp = state.air_temp;
    live_humidity = state.humidity;
    live_heater_target = state.heater_target;
    live_heater_power = state.heater_on;
    live_fan_speed = state.drybox_fan_speed;
    printer_part_fan_speed = state.part_fan_speed;
    printer_speed_factor = state.speed_factor;
    printer_flow_factor = state.flow_factor;
    printer_live_velocity = state.live_velocity;
    printer_live_flow = state.live_flow;
    printer_nozzle_temp = state.nozzle_temp;
    printer_nozzle_target = state.nozzle_target;
    printer_bed_temp = state.bed_temp;
    printer_bed_target = state.bed_target;
    printer_progress = state.progress;
    printer_print_duration = state.print_duration;
    printer_current_layer = state.current_layer;
    printer_total_layer = state.total_layer;
    s_live_data_ok = state.live_data_ok;
    s_moonraker_ok = state.moonraker_ok;

    s_drybox_selected_program =
        (ui_drybox_program_v32_t)state.drybox_selected_program;
    s_drybox_active_program =
        (ui_drybox_program_v32_t)state.drybox_active_program;

    safe_copy(printer_state, sizeof(printer_state), state.printer_state);
    safe_copy(printer_file, sizeof(printer_file), state.printer_file);
}


/* MOONRAKER_WEBSOCKET_PUSH_EVENTS
 * Moonraker keeps file payload transport on HTTP. WebSocket only invalidates
 * the active profile's visible Files page, coalescing bursts into one reload.
 */
static void moonraker_process_filelist_notification(void)
{
    if (!moonraker_live_websocket_file_change_pending()) return;

    if (!ui_files_v32_get_popup()) {
        /* Opening Files always performs a fresh HTTP reload. */
        (void)moonraker_live_websocket_take_file_change();
        return;
    }

    if (ui_files_v32_detail_is_open()) {
        /* Preserve the confirmation popup; consume after it closes. */
        return;
    }

    if (!moonraker_live_websocket_take_file_change()) return;

    ESP_LOGI(TAG, "WS_FILELIST_REFRESH visible Files page");
    ui_files_v32_refresh();
}


static void ota_ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ota_manager_pump_ui();
}


static void ui_refresh_timer_cb(lv_timer_t *timer)
{
    int64_t ui_refresh_t0 = esp_timer_get_time();

    moonraker_sync_legacy_from_state();
    moonraker_process_filelist_notification();

    (void)timer;

    moonraker_state_t telemetry_state;
    moonraker_state_snapshot(&telemetry_state);

    ui_telemetry_v32_refresh(
        &telemetry_state,
        esp_timer_get_time());
    if (wifi_label) lv_label_set_text(wifi_label, wifi_status);
    ui_printer_banner_refresh(printer_panel,
                              printer_banner_label,
                              printer_state_label,
                              printer_banner_text(),
                              printer_state);
    ui_printer_live_status_refresh(printer_panel,
                                   printer_active_file_box,
                                   printer_active_file_label,
                                   printer_fan_label,
                                   printer_speed_label,
                                   printer_flow_label,
                                   printer_state,
                                   printer_file,
                                   printer_live_velocity,
                                   printer_live_flow,
                                   printer_speed_factor,
                                   printer_flow_factor,
                                   printer_current_layer,
                                   printer_total_layer,
                                   printer_meta_object_height,
                                   printer_meta_layer_height,
                                   printer_progress);

    ui_printer_info_cards_refresh_live(
        printer_panel,
        &printer_info_cards,
        printer_progress,
        printer_nozzle_temp,
        printer_nozzle_target,
        printer_bed_temp,
        printer_bed_target,
        printer_part_fan_speed,
        printer_print_duration,
        s_moonraker_ok);
    if (printer_panel && printer_file_label) {
        lv_label_set_text(printer_file_label, printer_file);
    }

    if (topbar_eta_label) {
        char printer_remaining_buf[32];
        char tbuf[40];

        printer_controller_format_remaining(
            printer_remaining_buf,
            sizeof(printer_remaining_buf),
            printer_progress,
            printer_print_duration);

        printer_controller_format_topbar_eta(
            tbuf,
            sizeof(tbuf),
            printer_progress,
            printer_print_duration,
            printer_remaining_buf,
            s_moonraker_ok);

        lv_label_set_text(topbar_eta_label, tbuf);
    }

ui_drybox_v32_refresh();

    ui_network_v32_refresh_bridge();
    ui_settings_refresh();

    printer_ui_controller_update_action_buttons(
        printer_home_btn,
        printer_pause_btn,
        printer_resume_btn,
        printer_object_btn,
        printer_cancel_btn,
        moonraker_exclude_objects_available(),
        printer_state);


    ui_dashboard_v32_push_live_banner_data();
    ui_dashboard_v32_push_live_machine_data();
    ui_command_bar_v32_update(
        printer_state,
        moonraker_exclude_objects_available());

    int64_t ui_refresh_dt = esp_timer_get_time() - ui_refresh_t0;
    if (ui_refresh_dt > 50000) {
        ESP_LOGW(TAG, "UI_REFRESH_SLOW %lld us", (long long)ui_refresh_dt);
    }
}


static void dashboard_dry_status_event_cb(lv_event_t *e)
{
    (void)e;

    char body[512];
    snprintf(body, sizeof(body),
             "DRYBOX LIVE STATUS\n\n"
             "Air Temp:    %.1f C\n"
             "Center Temp: %.1f C\n"
             "Humidity:    %.1f %%RH\n"
             "Target Temp: %.1f C\n"
             "Fan:         %.0f %%\n"
             "Heater:      %s\n"
             "Moonraker:   %s\n"
             "IP:          " IPSTR,
             live_air_temp,
             live_chamber_temp,
             live_humidity,
             live_heater_target,
             live_fan_speed,
             live_heater_power ? "ON" : "OFF",
             s_live_data_ok ? "linked" : "not linked",
             IP2STR(&s_ip));

    ui_dashboard_v32_status_popup_show("DRYBOX LIVE STATUS", body);
}


static void show_dashboard_printer_status_popup(void)
{
    char elapsed[32];
    char remaining[32];
    char progress[24];

    printer_controller_format_hhmm(
        elapsed,
        sizeof(elapsed),
        printer_print_duration);

    printer_controller_format_remaining(
        remaining,
        sizeof(remaining),
        printer_progress,
        printer_print_duration);

    if (printer_progress >= 0.0) {
        snprintf(progress,
                 sizeof(progress),
                 "%.0f %%",
                 printer_progress * 100.0);
    } else {
        snprintf(progress, sizeof(progress), "-- %%");
    }

    ui_printer_popups_show_printer_status(
        dash_print_state_text(),
        printer_file,
        progress,
        elapsed,
        remaining,
        printer_nozzle_temp,
        printer_nozzle_target,
        printer_bed_temp,
        printer_bed_target,
        s_moonraker_ok);
}


static const char *printer_banner_text(void)
{
    return printer_controller_machine_banner_text(printer_state, s_moonraker_ok);
}


static void part_fan_card_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    ui_printer_popups_show_part_fan(
        printer_popup_send_gcode_bridge,
        printer_part_fan_speed);
}

static void nozzle_card_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (state.hotend_count > 1) {
        ui_printer_popups_show_hotends(
            printer_popup_send_gcode_bridge,
            &state);
    } else {
        ui_printer_popups_show_nozzle(
            printer_popup_send_gcode_bridge,
            printer_nozzle_temp,
            printer_nozzle_target);
    }
}

static void bed_card_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    ui_printer_popups_show_bed(
        printer_popup_send_gcode_bridge,
        printer_bed_temp,
        printer_bed_target);
}


static void printer_motion_send_gcode_bridge(const char *cmd)
{
    if (cmd) {
        moonraker_send_gcode(cmd);
    }
}

static void show_motion_popup(void)
{
    ui_printer_motion_show(&motion_step1_btn,
                           &motion_step10_btn,
                           &motion_step50_btn,
                           &printer_jog_step,
                           printer_motion_send_gcode_bridge);
}


/* END LEGACY PRINTER CONTROLS BLOCK */


static void printer_ui_controller_show_cancel_bridge(void)
{
    ui_printer_popups_show_cancel(
        printer_popup_send_gcode_bridge);
}

static void printer_ui_controller_show_object_bridge(void)
{
    ui_printer_popups_show_cancel_object(
        printer_popup_send_gcode_bridge);
}

/* Printer information cards are constructed by ui_cards. */

/* BEGIN LEGACY PRINTER PAGE BLOCK */
void ui_printer_v32_destroy(void)
{

    if (printer_panel) {
        lv_obj_delete(printer_panel);
        printer_panel = NULL;
    }

    printer_home_btn = NULL;
    printer_pause_btn = NULL;
    printer_resume_btn = NULL;
    printer_object_btn = NULL;
    printer_cancel_btn = NULL;
    printer_state_label = NULL;
    printer_file_label = NULL;
    printer_progress_label = NULL;
    printer_tuning_label = NULL;
    printer_active_file_label = NULL;
    ui_printer_v32_preview_destroy_refs();
    printer_nozzle_label = NULL;
    printer_bed_label = NULL;
    printer_eta_label = NULL;
    printer_banner_label = NULL;
    printer_elapsed_label = NULL;
    printer_remaining_label = NULL;
    printer_fan_label = NULL;
    printer_speed_label = NULL;
    printer_flow_label = NULL;

    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}


static void ui_network_tools_close_wifi_password_popup_cb(lv_event_t *e)
{
    (void)e;
    ui_network_tools_wifi_password_close_owned();
}


static void connect_selected_wifi(void)
{
    if (!ui_network_tools_selected_wifi_ssid[0]) {
        safe_copy(ui_network_tools_network_scan_status, sizeof(ui_network_tools_network_scan_status),
                  "No WiFi selected");
        return;
    }

    wifi_config_t cfg = {0};

    strlcpy((char *)cfg.sta.ssid,
            ui_network_tools_selected_wifi_ssid,
            sizeof(cfg.sta.ssid));

    strlcpy((char *)cfg.sta.password,
            ui_network_tools_selected_wifi_password,
            sizeof(cfg.sta.password));

    safe_copy(wifi_status, sizeof(wifi_status), "Network: connecting...");
    snprintf(ui_network_tools_network_scan_status,
             sizeof(ui_network_tools_network_scan_status),
             "Connecting to:\n%s",
             ui_network_tools_selected_wifi_ssid);

    ESP_LOGI(TAG, "CONNECT_WIFI requested");

    esp_err_t err;

    s_wifi_credentials_configured = false;

    err = esp_wifi_disconnect();
    ESP_LOGI(TAG, "CONNECT_WIFI disconnect result=%s", esp_err_to_name(err));

    err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    ESP_LOGI(TAG, "CONNECT_WIFI set_config result=%s", esp_err_to_name(err));

    if (err != ESP_OK) {
        snprintf(
            ui_network_tools_network_scan_status,
            sizeof(ui_network_tools_network_scan_status),
            "Connect failed applying WiFi configuration:\n%s",
            esp_err_to_name(err));
        return;
    }

    s_wifi_credentials_configured = true;
    s_retry_num = 0;

    err = esp_wifi_connect();
    ESP_LOGI(TAG, "CONNECT_WIFI connect result=%s", esp_err_to_name(err));

    if (err != ESP_OK) {
        snprintf(ui_network_tools_network_scan_status, sizeof(ui_network_tools_network_scan_status),
                 "Connect failed starting WiFi:\n%s", esp_err_to_name(err));
    }
}


static void ui_network_tools_save_wifi_password_only_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "WIFI POPUP CONNECT pressed");

    ui_network_tools_wifi_password_copy_owned(
        ui_network_tools_selected_wifi_password,
        sizeof(ui_network_tools_selected_wifi_password));

    save_wifi_credentials_to_nvs(ui_network_tools_selected_wifi_ssid, ui_network_tools_selected_wifi_password);
    connect_selected_wifi();

    ui_network_tools_wifi_scan_set_status(
        ui_network_tools_network_scan_status);

    ui_network_tools_wifi_scan_close_owned();
}

static void ui_network_tools_show_wifi_password_popup(void)
{
    ui_network_tools_wifi_password_show_owned(
        ui_network_tools_selected_wifi_ssid,
        ui_network_tools_close_wifi_password_popup_cb,
        ui_network_tools_save_wifi_password_only_cb);
}


static void ui_network_tools_wifi_ssid_selected_cb(lv_event_t *e)
{
    const char *ssid = (const char *)lv_event_get_user_data(e);
    if (!ssid) return;

    lv_obj_t *btn = lv_event_get_target(e);

    ui_network_tools_wifi_select_owned(
        btn,
        ssid,
        ui_network_tools_selected_wifi_ssid,
        sizeof(ui_network_tools_selected_wifi_ssid),
        ui_network_tools_network_scan_status,
        sizeof(ui_network_tools_network_scan_status));

    ui_network_tools_show_wifi_password_popup();
}


static void ui_network_tools_close_wifi_scan_popup_cb(lv_event_t *e)
{
    (void)e;

    ui_network_tools_wifi_scan_close_owned();
}


/* BEGIN LEGACY NETWORK BLOCK */
static void ui_network_tools_wifi_scan_now(void)
{
    safe_copy(
        ui_network_tools_network_scan_status,
        sizeof(ui_network_tools_network_scan_status),
        "SCANNING...");

    ui_network_v32_set_scan_status(
        ui_network_tools_network_scan_status);

    /*
     * Draw the in-page scan state before starting the blocking scan.
     */
    lv_refr_now(NULL);
    vTaskDelay(pdMS_TO_TICKS(250));

    wifi_scan_config_t scan_config = {0};

    esp_err_t err =
        esp_wifi_scan_start(
            &scan_config,
            true);

    if (err != ESP_OK) {
        snprintf(
            ui_network_tools_network_scan_status,
            sizeof(ui_network_tools_network_scan_status),
            "SCAN FAILED: %s",
            esp_err_to_name(err));

        ui_network_v32_set_scan_status(
            ui_network_tools_network_scan_status);

        return;
    }

    uint16_t ap_count = 0;

    err = esp_wifi_scan_get_ap_num(&ap_count);

    if (err != ESP_OK) {
        snprintf(
            ui_network_tools_network_scan_status,
            sizeof(ui_network_tools_network_scan_status),
            "COUNT FAILED: %s",
            esp_err_to_name(err));

        ui_network_v32_set_scan_status(
            ui_network_tools_network_scan_status);

        return;
    }

    if (ap_count == 0) {
        safe_copy(
            ui_network_tools_network_scan_status,
            sizeof(ui_network_tools_network_scan_status),
            "NO NETWORKS FOUND");

        ui_network_v32_set_scan_status(
            ui_network_tools_network_scan_status);

        return;
    }

    wifi_ap_record_t aps[8];

    uint16_t visible_count = ap_count;

    if (visible_count > 8) {
        visible_count = 8;
    }

    err =
        esp_wifi_scan_get_ap_records(
            &visible_count,
            aps);

    if (err != ESP_OK) {
        snprintf(
            ui_network_tools_network_scan_status,
            sizeof(ui_network_tools_network_scan_status),
            "RESULT FAILED: %s",
            esp_err_to_name(err));

        ui_network_v32_set_scan_status(
            ui_network_tools_network_scan_status);

        return;
    }

    ui_network_v32_render_scan_results(
        aps,
        visible_count,
        (unsigned)ap_count,
        ui_network_tools_wifi_ssid_selected_cb);
}


static void moonraker_discovery_selected_bridge(
    const char *host)
{
    if (!host || !host[0]) {
        return;
    }

    /*
     * Discovery edits the open profile draft only. The operator remains
     * responsible for reviewing the result and pressing SAVE.
     */
    ui_printer_profiles_set_discovered_endpoint(
        host,
        7125);

    moonraker_discovery_close();
}


static void test_moonraker_now(void)
{
    char msg[384];
    int code = 0;
    esp_err_t err = ESP_FAIL;

    bool ok = moonraker_test_connection(
        moonraker_config_host(),
        moonraker_config_port(),
        &code,
        &err);

    if (ok) {
        snprintf(msg, sizeof(msg),
                 "Host: %s:%d\nHTTP: %d\nStatus: connected\n\nMoonraker responded to /server/info.",
                 moonraker_config_host(), moonraker_config_port(), code);
        ui_network_tools_show_test_moonraker_popup(
            "MOONRAKER CONNECTED",
            msg,
            true);
    } else {
        snprintf(msg, sizeof(msg),
                 "Host: %s:%d\nHTTP: %d\nError: %s\n\nCheck IP, port, WiFi, or Moonraker service.",
                 moonraker_config_host(),
                 moonraker_config_port(),
                 code,
                 esp_err_to_name(err));
        ui_network_tools_show_test_moonraker_popup(
            "MOONRAKER FAILED",
            msg,
            false);
    }
}

void ui_network_v32_destroy(void)
{
    ui_printer_profiles_close_all();
    ui_network_tools_wifi_popup_destroy_all();

    ui_network_v32_destroy_objects(
        &network_selected_ssid_label,
        &network_password_ta,
        &network_keyboard);

    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}



static void settings_format_storage_text(
    char *output,
    size_t output_size)
{
    if (!output || output_size == 0) {
        return;
    }

    if (!sd_card_ok) {
        snprintf(
            output,
            output_size,
            "Not mounted");
        return;
    }

    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;

    esp_err_t err =
        esp_vfs_fat_info(
            "/sdcard",
            &total_bytes,
            &free_bytes);

    if (err != ESP_OK || total_bytes == 0) {
        snprintf(
            output,
            output_size,
            "Mounted - capacity unavailable");

        ESP_LOGW(
            TAG,
            "SD capacity query failed: %s",
            esp_err_to_name(err));
        return;
    }

    const double bytes_per_gb =
        1024.0 * 1024.0 * 1024.0;

    double total_gb =
        (double)total_bytes / bytes_per_gb;

    double free_gb =
        (double)free_bytes / bytes_per_gb;

    snprintf(
        output,
        output_size,
        "%.1f GB total / %.1f GB free",
        total_gb,
        free_gb);
}


static void show_settings_tab(void)
{
    char storage_text[96];

    settings_format_storage_text(
        storage_text,
        sizeof(storage_text));

    ui_settings_show_page(
        sd_card_ok ? "Mounted" : "Not mounted",
        storage_text,
        ota_open_popup_cb,
        app_theme_changed);
}

static void app_create_wifi_status_label(void)
{
    if (wifi_label) {
        lv_obj_delete(wifi_label);
        wifi_label = NULL;
    }

    wifi_label = lv_label_create(lv_screen_active());

    if (!wifi_label) {
        return;
    }

    lv_label_set_text(wifi_label, wifi_status);
    ui_apply_text_body(wifi_label);
    ui_apply_label_dim(wifi_label);
    lv_obj_set_width(wifi_label, 430);
    lv_label_set_long_mode(wifi_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(
        wifi_label,
        LV_TEXT_ALIGN_CENTER,
        0);
    lv_obj_set_pos(wifi_label, 380, 555);
}

static void app_theme_changed(void)
{
    /*
     * Existing LVGL objects retain their old local styles. Recreate every
     * persistent surface that can still be alive behind Settings, then
     * return the operator to the same page under the newly selected theme.
     */
    hide_settings_tab();
    ui_dashboard_v32_status_popup_close();
    ui_dashboard_v32_destroy();

    if (wifi_label) {
        lv_obj_delete(wifi_label);
        wifi_label = NULL;
    }

    ui_shell_destroy();
    ui_apply_root_style(lv_screen_active());

    ui_shell_create();
    ui_shell_set_printer_switch_callback(
        printer_chooser_open_from_topbar);
    ui_shell_create_nav();
    ui_shell_set_active_printer_name(
        moonraker_config_active_profile_name());

    ui_dashboard_v32_create();
    dashboard_restore_active_profile_preview();
    app_create_wifi_status_label();

    show_settings_tab();
    ui_shell_set_active_nav(UI_SHELL_PAGE_SETTINGS);
    ui_shell_raise();
    ui_shell_update_status_icons();
}


/* END LEGACY NETWORK BLOCK */

/* BEGIN LEGACY DRYBOX BLOCK */
static lv_obj_t *drybox_info_factory_bridge(
    lv_obj_t *parent,
    const char *title,
    const char *value,
    int x,
    int y)
{
    return make_printer_info(
        parent,
        title,
        value,
        x,
        y);
}

static void drybox_page_action_bridge(
    const char *command,
    lv_event_t *event)
{
    if (!command) {
        return;
    }

    if (strcmp(command, "DRY_STATUS") == 0) {
        dashboard_dry_status_event_cb(event);
        return;
    }

    /*
     * Update the UI state immediately when the operator chooses a
     * program. Moonraker telemetry continues to own heater, fan,
     * temperature, humidity, and online status.
     */
    if (strcmp(command, "DRY_PLA") == 0) {
        s_drybox_selected_program =
            UI_DRYBOX_PROGRAM_PLA;
        s_drybox_active_program =
            UI_DRYBOX_PROGRAM_PLA;
    } else if (strcmp(command, "DRY_PETG") == 0) {
        s_drybox_selected_program =
            UI_DRYBOX_PROGRAM_PETG;
        s_drybox_active_program =
            UI_DRYBOX_PROGRAM_PETG;
    } else if (strcmp(command, "DRY_HOLD") == 0) {
        s_drybox_active_program =
            UI_DRYBOX_PROGRAM_HOLD;
    } else if (strcmp(command, "DRY_RESUME") == 0) {
        s_drybox_active_program =
            s_drybox_selected_program;
    } else if (strcmp(command, "DRY_STOP") == 0) {
        s_drybox_active_program =
            UI_DRYBOX_PROGRAM_NONE;
    }

    moonraker_send_gcode(command);

    /*
     * Refresh immediately so button highlighting responds to the tap
     * instead of waiting for the next live-data refresh.
     */
    ui_drybox_v32_refresh();
}


void legacy_refresh_drybox_tab(void)
{
    ui_drybox_page_v32_t page = {
        .panel = drybox_panel,
        .banner_label = drybox_banner_label,
        .air_label = drybox_air_label,
        .center_label = drybox_center_label,
        .humidity_label = drybox_humidity_label,
        .target_label = drybox_target_label,
        .heater_label = drybox_heater_label,
        .fan_label = drybox_fan_label,
    };

    ui_drybox_page_v32_state_t state = {
        .banner_text = drybox_banner_text(),
        .air_temp = live_air_temp,
        .center_temp = live_chamber_temp,
        .humidity = live_humidity,
        .heater_target = live_heater_target,
        .heater_on = live_heater_power,
        .fan_speed = live_fan_speed,
        .active_program = s_drybox_active_program,
    };

    ui_drybox_page_v32_refresh(
        &page,
        &state);
}


void legacy_cleanup_drybox_tab(void)
{
    ui_drybox_page_v32_t page = {
        .panel = drybox_panel,
        .banner_label = drybox_banner_label,
        .air_label = drybox_air_label,
        .center_label = drybox_center_label,
        .humidity_label = drybox_humidity_label,
        .target_label = drybox_target_label,
        .heater_label = drybox_heater_label,
        .fan_label = drybox_fan_label,
    };

    ui_drybox_page_v32_cleanup(&page);

    drybox_panel = page.panel;
    drybox_banner_label = page.banner_label;
    drybox_air_label = page.air_label;
    drybox_center_label = page.center_label;
    drybox_humidity_label = page.humidity_label;
    drybox_target_label = page.target_label;
    drybox_heater_label = page.heater_label;
    drybox_fan_label = page.fan_label;

    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}


void legacy_create_drybox_tab(void)
{
    ui_drybox_page_v32_t page = {
        .panel = drybox_panel,
        .banner_label = drybox_banner_label,
        .air_label = drybox_air_label,
        .center_label = drybox_center_label,
        .humidity_label = drybox_humidity_label,
        .target_label = drybox_target_label,
        .heater_label = drybox_heater_label,
        .fan_label = drybox_fan_label,
    };

    if (!ui_drybox_page_v32_create(
            &page,
            drybox_info_factory_bridge,
            drybox_page_action_bridge,
            drybox_banner_text)) {
        ESP_LOGE(TAG, "Drybox page creation failed");
        return;
    }

    drybox_panel = page.panel;
    drybox_banner_label = page.banner_label;
    drybox_air_label = page.air_label;
    drybox_center_label = page.center_label;
    drybox_humidity_label = page.humidity_label;
    drybox_target_label = page.target_label;
    drybox_heater_label = page.heater_label;
    drybox_fan_label = page.fan_label;

    ui_drybox_v32_refresh();
}


/* END LEGACY DRYBOX BLOCK */

static void close_printer_file_detail_popup(void)
{
    file_detail_loader_v32_cancel();
    printer_thumb_cleanup_for_popup_close();
    ui_files_v32_close_detail_popup();

    printer_thumb_box = NULL;
    printer_thumb_view = NULL;
}


static bool printer_start_print_file_bridge(const char *filename)
{
    return printer_file_controller_start_file(
        s_got_ip,
        moonraker_config_host(),
        moonraker_config_port(),
        MOONRAKER_API_KEY,
        filename,
        moonraker_status,
        sizeof(moonraker_status));
}


static void printer_file_detail_start_bridge(void)
{
    (void)printer_file_controller_start_selected_file(
        s_got_ip,
        moonraker_config_host(),
        moonraker_config_port(),
        MOONRAKER_API_KEY,
        moonraker_status,
        sizeof(moonraker_status),
        close_printer_file_detail_popup);
}


static void printer_build_metadata_text(
    const char *file,
    char *out,
    size_t out_size)
{
    printer_meta_object_height = 0.0;
    printer_meta_layer_height = 0.0;

    bool metadata_ok = thumbnail_session_v32_build_metadata(
        moonraker_config_host(),
        moonraker_config_port(),
        MOONRAKER_API_KEY,
        file,
        out,
        out_size);

    if (metadata_ok) {
        (void)thumbnail_session_v32_get_layer_metadata(
            &printer_meta_object_height,
            &printer_meta_layer_height);
    }
}

static void printer_thumb_set_label(const char *txt)
{
    if (!printer_thumb_view) {
        return;
    }

    ui_thumbnail_v32_set_placeholder(
        printer_thumb_view,
        txt ? txt : "NO THUMBNAIL");
}

static void dashboard_show_loaded_thumbnail(void);


static void dashboard_show_loaded_thumbnail(void)
{
    if (!ui_dashboard_v32_thumb_ready() ||
        !thumbnail_manager_v32_has_png()) {
        return;
    }

    if (dash_thumb_canvas &&
        thumbnail_session_v32_selected_file()[0] &&
        strcmp(ui_dashboard_v32_thumb_canvas_file(),
               thumbnail_session_v32_selected_file()) == 0) {
        return;
    }

    if (!dash_thumb_canvas_buf) {
        dash_thumb_canvas_buf = heap_caps_malloc(
            DASH_THUMB_CANVAS_W *
                DASH_THUMB_CANVAS_H *
                sizeof(uint16_t),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!dash_thumb_canvas_buf) {
            dash_thumb_canvas_buf = heap_caps_malloc(
                DASH_THUMB_CANVAS_W *
                    DASH_THUMB_CANVAS_H *
                    sizeof(uint16_t),
                MALLOC_CAP_8BIT);
        }
    }

    if (!dash_thumb_canvas_buf) {
        ESP_LOGW(TAG, "DASH_CANVAS malloc failed");
        return;
    }

    if (!thumbnail_render_v32_to_rgb565(
            thumbnail_manager_v32_image_dsc(),
            dash_thumb_canvas_buf,
            DASH_THUMB_CANVAS_W,
            DASH_THUMB_CANVAS_H)) {
        ESP_LOGW(TAG, "DASH_CANVAS shared render failed");
        return;
    }


    /* CACHE_PUBLISH_SYNC: PREVIEW_PROFILE_OWNERSHIP_COMPLETE */
    const char *cache_file =
        thumbnail_session_v32_selected_file();

    if (cache_file && cache_file[0]) {
        bool cache_published =
            printer_preview_cache_v32_publish_active(
                cache_file,
                dash_thumb_canvas_buf,
                DASH_THUMB_CANVAS_W,
                DASH_THUMB_CANVAS_H);

        if (cache_published &&
            thumbnail_manager_v32_has_png()) {
            printer_preview_store_v32_store_active(
                cache_file,
                thumbnail_manager_v32_png_data(),
                thumbnail_manager_v32_png_size());
        }
    }

    if (dash_thumb_img) {
        lv_obj_delete(dash_thumb_img);
        dash_thumb_img = NULL;
    }

    if (!dash_thumb_canvas) {
        dash_thumb_canvas =
            lv_canvas_create(ui_dashboard_v32_thumb_box());
    }

    lv_canvas_set_buffer(
        dash_thumb_canvas,
        dash_thumb_canvas_buf,
        DASH_THUMB_CANVAS_W,
        DASH_THUMB_CANVAS_H,
        LV_COLOR_FORMAT_RGB565);

    lv_obj_center(dash_thumb_canvas);
    lv_obj_move_foreground(dash_thumb_canvas);

    ui_dashboard_v32_thumb_clear_placeholder();

    safe_copy(
        ui_dashboard_v32_thumb_canvas_file(),
        ui_dashboard_v32_thumb_canvas_file_size(),
        thumbnail_session_v32_selected_file());

}


static void dashboard_apply_rendered_thumbnail(void)
{
    if (!ui_dashboard_v32_thumb_ready() || !dash_thumb_canvas_buf) return;

    /*
     * Minimal LVGL apply:
     * - Do not delete labels/images here.
     * - Do not recreate canvas if it already exists.
     * - Worker already rendered pixels into dash_thumb_canvas_buf.
     */
    if (!dash_thumb_canvas) {
        dash_thumb_canvas = lv_canvas_create(ui_dashboard_v32_thumb_box());
        lv_canvas_set_buffer(dash_thumb_canvas,
                             dash_thumb_canvas_buf,
                             DASH_THUMB_CANVAS_W,
                             DASH_THUMB_CANVAS_H,
                             LV_COLOR_FORMAT_RGB565);
        lv_obj_center(dash_thumb_canvas);
        lv_obj_move_foreground(dash_thumb_canvas);
    } else {
        lv_obj_invalidate(dash_thumb_canvas);
    }

    ui_dashboard_v32_thumb_clear_placeholder();

    if (dash_thumb_img) {
        lv_obj_add_flag(dash_thumb_img, LV_OBJ_FLAG_HIDDEN);
    }

    safe_copy(ui_dashboard_v32_thumb_canvas_file(), ui_dashboard_v32_thumb_canvas_file_size(), dash_thumb_render_file);

}



static void dashboard_restore_active_profile_preview(void)
{
    const char *file = NULL;
    uint32_t revision = 0;

    const lv_image_dsc_t *image =
        printer_preview_cache_v32_image(
            moonraker_config_active_profile_index(),
            &file,
            &revision);

    (void)revision;

    if (!image ||
        !file || !file[0] ||
        image->header.cf != LV_COLOR_FORMAT_RGB565 ||
        image->header.w != DASH_THUMB_CANVAS_W ||
        image->header.h != DASH_THUMB_CANVAS_H ||
        image->data_size <
            DASH_THUMB_CANVAS_W *
            DASH_THUMB_CANVAS_H * sizeof(uint16_t)) {
        ui_dashboard_v32_thumb_delete_canvas();
        ui_dashboard_v32_thumb_set_placeholder(
            "PRINT\nTHUMBNAIL\n\nNo preview loaded");
        return;
    }

    if (!ui_dashboard_v32_thumb_ensure_canvas_buffer(
            DASH_THUMB_CANVAS_W * DASH_THUMB_CANVAS_H)) {
        return;
    }

    memcpy(
        dash_thumb_canvas_buf,
        image->data,
        DASH_THUMB_CANVAS_W *
            DASH_THUMB_CANVAS_H * sizeof(uint16_t));

    ui_dashboard_v32_thumb_show_canvas_from_buffer(
        DASH_THUMB_CANVAS_W,
        DASH_THUMB_CANVAS_H,
        file);
}


static void dash_thumb_render_task(void *arg)
{
    (void)arg;

    dash_thumb_render_ready = false;
    dash_thumb_render_failed = false;

    if (!thumbnail_manager_v32_has_png()) {
        dash_thumb_render_failed = true;
        dash_thumb_render_running = false;
        vTaskDelete(NULL);
        return;
    }

    if (!dash_thumb_canvas_buf) {
        dash_thumb_canvas_buf = heap_caps_malloc(
            DASH_THUMB_CANVAS_W * DASH_THUMB_CANVAS_H * sizeof(uint16_t),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );

        if (!dash_thumb_canvas_buf) {
            dash_thumb_canvas_buf = heap_caps_malloc(
                DASH_THUMB_CANVAS_W * DASH_THUMB_CANVAS_H * sizeof(uint16_t),
                MALLOC_CAP_8BIT
            );
        }
    }

    if (!dash_thumb_canvas_buf) {
        ESP_LOGW(TAG, "DASH_WORKER canvas malloc failed");
        dash_thumb_render_failed = true;
        dash_thumb_render_running = false;
        vTaskDelete(NULL);
        return;
    }

    bool ok = false;

    if (bsp_display_lock(1000)) {
        ok = thumbnail_render_v32_to_rgb565(
            thumbnail_manager_v32_image_dsc(),
            dash_thumb_canvas_buf,
            DASH_THUMB_CANVAS_W,
            DASH_THUMB_CANVAS_H);

        bsp_display_unlock();

        if (ok) {
            bool same_profile =
                dash_thumb_render_generation ==
                    moonraker_config_generation() &&
                dash_thumb_render_profile_index ==
                    moonraker_config_active_profile_index();

            if (same_profile && dash_thumb_render_file[0]) {
                bool cache_published =
                    printer_preview_cache_v32_publish_active(
                        dash_thumb_render_file,
                        dash_thumb_canvas_buf,
                        DASH_THUMB_CANVAS_W,
                        DASH_THUMB_CANVAS_H);

                if (cache_published &&
                    thumbnail_manager_v32_has_png()) {
                    printer_preview_store_v32_store_active(
                        dash_thumb_render_file,
                        thumbnail_manager_v32_png_data(),
                        thumbnail_manager_v32_png_size());
                }
            } else if (!same_profile) {
                ESP_LOGW(TAG,
                         "Discarded stale Dashboard preview render");
            }

        } else {
            ESP_LOGW(TAG,
                     "DASH_WORKER shared render failed");
        }
    } else {
        ESP_LOGW(TAG, "DASH_WORKER display lock timeout");
    }

    dash_thumb_render_ready = ok;
    dash_thumb_render_failed = !ok;
    dash_thumb_render_running = false;
    vTaskDelete(NULL);
}

static void dash_thumb_render_ui_poll_cb(lv_timer_t *t)
{
    if (dash_thumb_render_running) return;

    lv_timer_delete(t);
    dash_thumb_render_timer = NULL;

    if (dash_thumb_render_ready &&
        !dash_thumb_render_failed &&
        dash_thumb_render_generation == moonraker_config_generation() &&
        dash_thumb_render_profile_index ==
            moonraker_config_active_profile_index()) {
        dashboard_apply_rendered_thumbnail();
    }
}

static void dash_thumb_start_render_task(void)
{
    if (dash_thumb_render_running) return;
    if (!thumbnail_manager_v32_has_png()) return;

    safe_copy(dash_thumb_render_file, sizeof(dash_thumb_render_file), thumbnail_session_v32_selected_file());
    dash_thumb_render_profile_index =
        moonraker_config_active_profile_index();
    dash_thumb_render_generation =
        moonraker_config_generation();

    dash_thumb_render_running = true;
    dash_thumb_render_ready = false;
    dash_thumb_render_failed = false;

    BaseType_t rc = xTaskCreatePinnedToCore(
        dash_thumb_render_task,
        "dash_thumb_render",
        8192,
        NULL,
        4,
        NULL,
        0
    );

    if (rc != pdPASS) {
        ESP_LOGW(TAG, "DASH_WORKER task create failed");
        dash_thumb_render_running = false;
        dash_thumb_render_failed = true;
        return;
    }

    if (!dash_thumb_render_timer) {
        dash_thumb_render_timer = lv_timer_create(dash_thumb_render_ui_poll_cb, 100, NULL);
    }
}


static void printer_thumb_ui_poll_cb(lv_timer_t *t)
{
    bool is_live_thumb =
        (printer_thumb_target == THUMB_TARGET_LIVE);

    thumbnail_manager_v32_result_t result =
        thumbnail_manager_v32_result();

    if (result == THUMBNAIL_MANAGER_V32_RESULT_LOADING) {
        return;
    }

    lv_timer_delete(t);
    thumb_poll_timer = NULL;

    switch (result) {
    case THUMBNAIL_MANAGER_V32_RESULT_FAILED:
        if (!is_live_thumb) {
            printer_thumb_set_label("THUMBNAIL\nTIMEOUT");
        }
        return;

    case THUMBNAIL_MANAGER_V32_RESULT_IDLE:
        if (!is_live_thumb) {
            printer_thumb_set_label("NO THUMBNAIL");
        }
        return;

    case THUMBNAIL_MANAGER_V32_RESULT_READY:
        break;

    case THUMBNAIL_MANAGER_V32_RESULT_LOADING:
    default:
        return;
    }

    if (is_live_thumb) {
        dash_thumb_start_render_task();
        return;
    }

    /* Popup/selected-file preview path only. */
    if (!ui_files_v32_detail_is_open() || !printer_thumb_box) {
        return;
    }

    ui_thumbnail_v32_show_image(
        printer_thumb_view,
        thumbnail_manager_v32_image_dsc(),
        160);

    if (!printer_controller_is_live_state(printer_state)) {
        ui_dashboard_v32_thumb_canvas_file()[0] = 0;
        dashboard_show_loaded_thumbnail();
    }
}


static void printer_thumb_start_delayed(void)
{
    thumbnail_manager_v32_mark_pending();

    if (!thumbnail_session_v32_selected_thumbnail_path()[0]) {
        printer_thumb_set_label("NO THUMBNAIL");
        return;
    }

    if (!s_got_ip) {
        printer_thumb_set_label("WAITING\nFOR WIFI");
        return;
    }

    if (!s_moonraker_ok) {
        printer_thumb_set_label("WAITING\nMOONRAKER");
        return;
    }

    /* Thumbnail fetch is allowed to run after live polling. */

    if (thumbnail_manager_v32_task_running()) {
        printer_thumb_set_label("LOADING...");
        if (!thumb_poll_timer)
        thumb_poll_timer = lv_timer_create(printer_thumb_ui_poll_cb, 200, NULL);
        return;
    }

    printer_thumb_set_label("LOADING...");
    bool started =
        thumbnail_manager_v32_start_download_task(
            moonraker_config_host(),
            moonraker_config_port(),
            thumbnail_session_v32_selected_file(),
            thumbnail_session_v32_selected_thumbnail_path(),
            thumbnail_manager_v32_force_refresh(),
            sd_card_ok);

    if (!started) {
        thumbnail_manager_v32_mark_failed();
        printer_thumb_set_label("THUMBNAIL\nTASK FAIL");
        return;
    }

    if (!thumb_poll_timer)
        thumb_poll_timer = lv_timer_create(printer_thumb_ui_poll_cb, 200, NULL);
}

/* BEGIN FILES APP BRIDGE BLOCK */
static void file_detail_ready_cb(
    const char *file,
    bool metadata_ok,
    const char *metadata_text,
    const char *thumbnail_path)
{
    if (!ui_files_v32_detail_is_open() ||
        !file ||
        strcmp(file, thumbnail_session_v32_selected_file()) != 0) {
        return;
    }

    safe_copy(
        thumbnail_session_v32_metadata_info(),
        thumbnail_session_v32_metadata_info_size(),
        metadata_text);
    safe_copy(
        thumbnail_session_v32_selected_thumbnail_path(),
        thumbnail_session_v32_selected_thumbnail_path_size(),
        thumbnail_path);

    ui_files_v32_update_detail_metadata(metadata_text, true);

    if (thumbnail_path && thumbnail_path[0]) {
        printer_thumb_set_label("THUMBNAIL\nFOUND");
        printer_thumb_target = THUMB_TARGET_POPUP;
        printer_thumb_start_delayed();
    } else {
        printer_thumb_set_label("NO THUMBNAIL");
    }

    if (!metadata_ok) {
        ui_toast_v32_show(
            UI_STATUS_WARNING,
            "METADATA UNAVAILABLE",
            "The file can still be started.");
    }
}

static void show_printer_file_detail_popup(void)
{
    close_printer_file_detail_popup();

    thumbnail_session_v32_selected_thumbnail_path()[0] = 0;
    safe_copy(thumbnail_session_v32_metadata_info(),
              thumbnail_session_v32_metadata_info_size(),
              "Loading metadata...");

    printer_thumb_box = NULL;
    printer_thumb_view = NULL;

    ui_files_v32_show_detail_popup(
        thumbnail_session_v32_selected_file(),
        thumbnail_session_v32_metadata_info(),
        &printer_thumb_box,
        &printer_thumb_view,
        close_printer_file_detail_popup,
        printer_file_detail_start_bridge);
    printer_thumb_set_label("LOADING...");

    if (!file_detail_loader_v32_start(
            moonraker_config_host(),
            moonraker_config_port(),
            MOONRAKER_API_KEY,
            thumbnail_session_v32_selected_file(),
            file_detail_ready_cb)) {
        ui_files_v32_update_detail_metadata(
            "Unable to start the metadata worker.",
            true);
        printer_thumb_set_label("NO THUMBNAIL");
    }
}

static void files_refresh_bridge(void)
{
    app_files_reload();
}

static void files_select_bridge(const char *path)
{
    printer_file_controller_select_file(
        path,
        show_printer_file_detail_popup);
}

static void files_preview_bridge(const char *path)
{
    files_page_controller_request_preview(path);
}


void app_files_reload(void)
{
    files_page_controller_reload(
        s_got_ip,
        s_moonraker_ok,
        sd_card_ok,
        moonraker_config_host(),
        moonraker_config_port(),
        MOONRAKER_API_KEY);
}


/* END FILES APP BRIDGE BLOCK */


void ui_printer_v32_create(void)
{
    bool printer_is_live = printer_controller_is_live_state(printer_state);

    if (printer_is_live && printer_file[0] && strcmp(thumbnail_session_v32_selected_file(), printer_file) != 0) {
        safe_copy(thumbnail_session_v32_selected_file(), thumbnail_session_v32_selected_file_size(), printer_file);
        thumbnail_session_v32_selected_thumbnail_path()[0] = 0;
        ui_dashboard_v32_thumb_canvas_file()[0] = 0;
        ui_printer_v32_preview_reset();
        thumbnail_preview_coordinator_v32_reset();

        thumbnail_session_v32_clear_png_buffer();
    }

    if (printer_panel) {
        lv_obj_move_foreground(printer_panel);
        return;
    }

    printer_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(printer_panel,
                    UI_PAGE_ROOT_WIDTH,
                    UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(printer_panel,
                   UI_PAGE_ROOT_X,
                   UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(printer_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(printer_panel, UI_BG, 0);
    lv_obj_set_style_bg_opa(printer_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(printer_panel, UI_BORDER_SOFT, 0);
    lv_obj_set_style_border_width(printer_panel, 0, 0);
    lv_obj_set_style_radius(printer_panel, 0, 0);
    lv_obj_set_style_pad_all(printer_panel, 0, 0);

    ui_page_title_create(
        printer_panel,
        LV_SYMBOL_LIST " PRINTER",
        "Machine Status and Print Control");

    if (!ui_printer_layout_v32_create(
            printer_panel,
            &printer_layout)) {
        ESP_LOGE(
            TAG,
            "Failed to create Printer layout");
        return;
    }

    ui_printer_banner_create(printer_panel,
                             &printer_banner_label,
                             printer_banner_text());
    ui_printer_live_status_create(
        printer_layout.active_panel,
        &printer_active_file_label,
        &printer_fan_label,
        &printer_speed_label,
        &printer_flow_label,
        printer_speed_factor,
        printer_flow_factor,
        moonraker_send_gcode);
    ui_printer_info_cards_create(printer_layout.status_panel,
                                 &printer_info_cards,
                                 nozzle_card_event_cb,
                                 bed_card_event_cb,
                                 part_fan_card_event_cb);

    printer_progress_label = printer_info_cards.progress;
    printer_nozzle_label = printer_info_cards.nozzle;
    printer_bed_label = printer_info_cards.bed;
    printer_part_fan_label = printer_info_cards.part_fan;
    printer_elapsed_label = printer_info_cards.elapsed;
    printer_remaining_label = printer_info_cards.remaining;
    printer_eta_label = printer_info_cards.eta;

    ui_printer_v32_preview_create(printer_layout.active_panel);

    lv_obj_t *divider = lv_obj_create(printer_panel);
    lv_obj_set_size(divider, 0, 0);

    printer_ui_controller_init(
        moonraker_send_gcode,
        printer_ui_controller_show_cancel_bridge,
        printer_ui_controller_show_object_bridge,
        show_motion_popup);

    ui_printer_actions_create(
        printer_layout.action_panel,
        &printer_actions,
        printer_ui_controller_command_event_cb,
        printer_ui_controller_motion_event_cb);

    printer_home_btn = printer_actions.home;
    printer_pause_btn = printer_actions.pause;
    printer_resume_btn = printer_actions.resume;
    printer_object_btn = printer_actions.object;
    printer_cancel_btn = printer_actions.cancel;

    ui_printer_v32_preview_show(
        printer_state,
        printer_file,
        thumbnail_session_v32_selected_file());
    lv_obj_set_pos(divider, 20, 345);
    lv_obj_set_style_bg_color(divider, UI_BORDER_SOFT, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_radius(divider, 0, 0);

    
    
    
    

    ui_drybox_v32_refresh();

    ui_network_v32_refresh_bridge();

    printer_ui_controller_update_action_buttons(
        printer_home_btn,
        printer_pause_btn,
        printer_resume_btn,
        printer_object_btn,
        printer_cancel_btn,
        moonraker_exclude_objects_available(),
        printer_state);
}
/* END LEGACY PRINTER PAGE BLOCK */

static void build_drybox_dashboard(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, UI_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, UI_TEXT, 0);

    /* Top bar now belongs to ui_shell. */
    ui_shell_create();
    ui_shell_set_printer_switch_callback(
        printer_chooser_open_from_topbar);

    /* Navigation rail now belongs to ui_shell. */
    ui_shell_create_nav();

    /* Legacy dashboard body removed.
     * The shell/topbar/nav stay here for now.
     * The only dashboard is ui_dashboard_v32.
     */
    ui_dashboard_v32_create();

    app_create_wifi_status_label();

    /*
     * OTA progress and cancellation need prompt UI acknowledgement without
     * forcing the full application refresh path to run more frequently.
     */
    lv_timer_create(ota_ui_timer_cb, 50, NULL);
    lv_timer_create(ui_refresh_timer_cb, 500, NULL);
}


#define JC1060_SD_CLK GPIO_NUM_43
#define JC1060_SD_CMD GPIO_NUM_44
#define JC1060_SD_D0  GPIO_NUM_39
#define JC1060_SD_D1  GPIO_NUM_40
#define JC1060_SD_D2  GPIO_NUM_41
#define JC1060_SD_D3  GPIO_NUM_42

static esp_err_t sdmmc_host_init_dummy(void)
{
    return ESP_OK;
}

static esp_err_t sdmmc_host_deinit_dummy(void)
{
    return ESP_OK;
}

static void init_sd_card_storage(void)
{
    sd_card_ok = false;
    snprintf(sd_status, sizeof(sd_status), "SD: mounting...");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card = NULL;
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    /* ESP-Hosted already initializes the SDMMC peripheral. */
    host.init = sdmmc_host_init_dummy;
    host.deinit = sdmmc_host_deinit_dummy;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = JC1060_SD_CLK;
    slot_config.cmd = JC1060_SD_CMD;
    slot_config.d0  = JC1060_SD_D0;
    slot_config.d1  = JC1060_SD_D1;
    slot_config.d2  = JC1060_SD_D2;
    slot_config.d3  = JC1060_SD_D3;
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    ESP_LOGI(TAG, "SD mount start /sdcard GPIO clk=%d cmd=%d d0=%d",
             JC1060_SD_CLK, JC1060_SD_CMD, JC1060_SD_D0);

    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        snprintf(sd_status, sizeof(sd_status), "SD: mount failed %s", esp_err_to_name(ret));
        ESP_LOGW(TAG, "%s", sd_status);
        return;
    }

    sd_card_ok = true;
    snprintf(sd_status, sizeof(sd_status), "SD: mounted");

    ESP_LOGI(TAG, "SD mounted OK");

    FILE *f = fopen("/sdcard/test.txt", "wb");
    if (f) {
        const char *msg = "ESP32-P4 Drybox HMI SD write test OK\n";
        size_t wr = fwrite(msg, 1, strlen(msg), f);
        int cr = fclose(f);
        if (wr != strlen(msg) || cr != 0) {
            ESP_LOGW(TAG,
                     "SD write test incomplete: bytes=%u fclose=%d",
                     (unsigned)wr,
                     cr);
        }
    } else {
        ESP_LOGW(TAG, "SD write test failed errno=%d", errno);
    }

}


static void hmi_runtime_task(void *arg)
{
    (void)arg;

    while (1) {
        if (s_sta_netif) {
            esp_netif_ip_info_t ip;
            if (esp_netif_get_ip_info(s_sta_netif, &ip) == ESP_OK && ip.ip.addr != 0) {
                s_ip = ip.ip;
                s_got_ip = true;
                wifi_status[0] = '\0';

                if (!sd_mount_attempted) {
                    sd_mount_attempted = true;
                    ESP_LOGI(TAG, "SD delayed mount after GOT_IP");
                    init_sd_card_storage();
                }
            }
        }

        /* MOONRAKER_WEBSOCKET_PHASE2
         * Observe the active profile over a persistent subscription while
         * the proven HTTP poller remains authoritative.
         */
        moonraker_live_websocket_tasklet(
            s_got_ip,
            moonraker_config_host(),
            moonraker_config_port(),
            MOONRAKER_API_KEY,
            moonraker_config_generation());

        moonraker_live_poll_tasklet();
        printer_profile_preview_worker_v32_poll_one(
            MOONRAKER_API_KEY);
        /* BOOT_PREVIEW_PROFILE_STORE_ONLY
         * Reboot restoration is profile-indexed and endpoint-validated.
         * The legacy global last-file cache must not publish into whichever
         * printer profile happens to be active at boot.
         */
        printer_preview_store_v32_restore_one(sd_card_ok);
        if (bsp_display_lock(50)) {
            if (wifi_label) {
                lv_label_set_text(wifi_label, wifi_status);
            }

            ui_shell_update_status_icons();

            update_live_cards();

            bsp_display_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}


static void app_splash_locked(void (*fn)(void))
{
    if (!fn) return;

    /*
     * Splash transitions are lifecycle operations, not optional refreshes.
     * A short lock timeout can expire while LVGL is decoding or flushing the
     * embedded logo.  Skipping that callback leaves the progress text stale
     * and, if the skipped callback is destroy(), leaves the overlay onscreen.
     * A zero timeout is the BSP's blocking/wait-forever mode.
     */
    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Splash display lock failed");
        return;
    }

    fn();
    bsp_display_unlock();
}


static void app_splash_wifi_waiting_locked(bool connected)
{
    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Splash WiFi display lock failed");
        return;
    }

    ui_splash_v32_wifi_waiting(connected);
    bsp_display_unlock();
}


static void app_startup_show_initial_ui(void)
{
    bsp_display_lock(0);
    build_drybox_dashboard();
    hide_settings_tab();
    ui_dashboard_v32_create();

    /* STARTUP_OPEN_PRINTER_CHOOSER
     * Keep the active Dashboard built behind the startup splash, then place
     * the multi-printer chooser in front. Selecting a printer continues into
     * that profile's Dashboard through printer_chooser_select_bridge().
     */
    ui_printer_chooser_v32_show(
        printer_chooser_select_bridge,
        printer_chooser_manage_bridge);
    ui_splash_v32_create();
    ui_splash_v32_display_ready();
    bsp_display_unlock();
}


void app_main(void)
{
    ESP_LOGI(TAG, "BOOT_RESET_REASON=%d", esp_reset_reason());

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    operator_event_log_init();
    operator_event_log_add(
        OPERATOR_EVENT_INFO,
        "Controller started; reset reason %d",
        (int)esp_reset_reason());

    theme_manager_init();

    /* Apply the saved local timezone before any clock or SNTP path starts. */
    timezone_config_init();

    /* Initialize synchronized Moonraker state after PSRAM is available and
     * before any UI or transport path can take a state snapshot.
     */
    moonraker_module_init();

    /*
     * ESP-Hosted resets and initializes the C6 transport. Keep the panel
     * backlight dark until that disruptive one-time operation is complete.
     */
    gpio_reset_pin(BSP_LCD_BACKLIGHT);
    gpio_set_direction(BSP_LCD_BACKLIGHT, GPIO_MODE_OUTPUT);
    gpio_set_level(BSP_LCD_BACKLIGHT, 0);
    wifi_prepare_transport();

    s_moonraker_objects = heap_caps_calloc(
        1,
        MOONRAKER_OBJECTS_CAPACITY,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_moonraker_objects) {
        ESP_LOGI(TAG,
                 "Moonraker HTTP buffer allocated in PSRAM: %u bytes",
                 (unsigned)MOONRAKER_OBJECTS_CAPACITY);
    } else {
        s_moonraker_objects = heap_caps_calloc(
            1,
            MOONRAKER_OBJECTS_CAPACITY,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        if (s_moonraker_objects) {
            ESP_LOGW(TAG,
                     "Moonraker HTTP buffer using internal RAM fallback");
        } else {
            ESP_LOGE(TAG,
                     "Moonraker HTTP buffer allocation failed; HTTP fallback disabled");
        }
    }

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .sw_rotate = false,
        }
    };

    /*
     * Keep display servicing isolated from ESP32-C6 hosted WiFi startup,
     * which performs its initialization work on CPU 0.
     */
    cfg.lvgl_port_cfg.task_affinity = 1;

    ESP_LOGI(TAG, "Starting known-good BSP display/touch path");
    bsp_display_start_with_config(&cfg);

    /*
     * Keep the BSP-initialized backlight at 0% until the splash has been
     * created and its first frame has reached the display.
     */
    app_startup_show_initial_ui();
    vTaskDelay(pdMS_TO_TICKS(50));

    /*
     * Load persistent display settings and restore the saved brightness.
     * The lock also protects LVGL timer creation.
     */
    bsp_display_lock(0);
    ui_settings_module_init();
    bsp_display_unlock();

    vTaskDelay(pdMS_TO_TICKS(250));

    app_splash_locked(ui_splash_v32_wifi_starting);

    /* Start WiFi after dashboard is visible. Touch scaling fix remains in BSP. */
    ESP_LOGI(TAG, "Starting WiFi after display/touch/dashboard");
    wifi_init_sta();

    app_splash_wifi_waiting_locked(s_got_ip);

    vTaskDelay(pdMS_TO_TICKS(350));

    app_splash_locked(ui_splash_v32_moonraker_ready);

    vTaskDelay(pdMS_TO_TICKS(350));

    app_splash_locked(ui_splash_v32_dashboard_ready);

    vTaskDelay(pdMS_TO_TICKS(500));

    app_splash_locked(ui_splash_v32_destroy);

    ESP_LOGI(TAG, "Drybox HMI v4.0.0 WiFi/display baseline ready");
    ota_confirm_running_app_valid();

    BaseType_t rc = xTaskCreatePinnedToCore(
        hmi_runtime_task,
        "hmi_runtime",
        12288,
        NULL,
        4,
        NULL,
        0
    );

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Failed to start hmi_runtime task");
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
