#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <math.h>
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
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include "ui_dashboard.h"
#include "ui_dashboard_status.h"
#include "ui_command_bar.h"
#include "ui_ota_popup.h"
#include "ui_ota_release_browser.h"
#include "ota_manager.h"
#include "ota_ui_controller.h"
#include "ota_boot_validation.h"
#include "operator_event_log.h"

/*
 * PrinterHMI capability-driven firmware
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
static lv_obj_t *topbar_eta_label = NULL;


void app_files_reload(void);
void ui_files_refresh(void);
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
#include "ui_page_geometry.h"
#include "ui_page_layout_profile.h"
#include "ui_widgets.h"
#include "ui_settings.h"
#include "timezone_config.h"
#include "ui_splash.h"
#include "ui_shell.h"
#include "ui_devices.h"
#include "ui_calibration.h"
#include "ui_telemetry.h"
#include "ui_console.h"
#include "console_controller.h"
#include "ui_macros.h"
#include "macro_controller.h"
#include "printer_action_resolver.h"
#include "device_catalog_controller.h"
#include "calibration_session_controller.h"
#include "telemetry_history.h"
#include "ui_printer.h"
#include "ui_printer_motion.h"
#include "ui_printer_popups.h"
#include "ui_printer_live_status.h"
#include "ui_printer_layout.h"
#include "ui_printer_info_cards.h"
#include "ui_bed_mesh.h"
#include "ui_printer_actions.h"
#include "ui_printer_banner.h"
#include "printer_controller.h"
#include "printer_layer_resolver.h"
#include "printer_files.h"
#include "thumbnail_manager.h"
#include "thumbnail_render.h"
#include "ui_network.h"
#include "ui_printer_profiles.h"
#include "ui_printer_chooser.h"
#include "printer_profile_health.h"
#include "printer_preview_cache.h"
#include "printer_profile_preview_worker.h"
#include "printer_preview_store.h"
#include "network_status_controller.h"
#include "network_activity_controller.h"
#include "ui_network_tools.h"
#include "moonraker_discovery.h"
#include "moonraker_probe.h"
#include "network_wifi_scan.h"
#include "wifi_credentials_store.h"
#include "ui_drybox.h"
#include "ui_files.h"
#include "ui_thumbnail.h"
#include "ui_toast.h"
#include "file_detail_loader.h"
#include "moonraker.h"
#include "moonraker_config_controller.h"
#include "moonraker_live_transport.h"
#include "moonraker_poll.h"
#include "moonraker_live_websocket.h"


#include "thumbnail_preview_coordinator.h"

#include "thumbnail_session.h"

#include "printer_ui_controller.h"
#include "printer_file_controller.h"

#include "files_page_controller.h"
#include "dashboard_live_controller.h"
#include "dashboard_runtime_controller.h"
void ui_shell_raise(void)
{
    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}


/*
 * These application-lifetime text buffers used to occupy internal .bss
 * before the FreeRTOS scheduler started. Keep only this owner pointer in
 * startup internal RAM; allocate the bounded context permanently in PSRAM
 * from app_main().
 */
typedef struct {
    char sd_status[128];
    char wifi_status[128];
    char moonraker_status[160];
    char selected_wifi_password[65];
    char saved_wifi_ssid[33];
    char saved_wifi_password[65];
    char network_scan_status[512];
    char selected_wifi_ssid[33];
    char printer_state[32];
    char printer_file[160];
    char dash_thumb_render_file[160];
    char last_dashboard_print_state[32];
    char connected_wifi_ssid[33];
} app_runtime_buffers_t;

static app_runtime_buffers_t *s_app_buffers = NULL;

#define sd_status \
    (s_app_buffers->sd_status)
#define wifi_status \
    (s_app_buffers->wifi_status)
#define moonraker_status \
    (s_app_buffers->moonraker_status)
#define ui_network_tools_selected_wifi_password \
    (s_app_buffers->selected_wifi_password)
#define saved_wifi_ssid \
    (s_app_buffers->saved_wifi_ssid)
#define saved_wifi_password \
    (s_app_buffers->saved_wifi_password)
#define ui_network_tools_network_scan_status \
    (s_app_buffers->network_scan_status)
#define ui_network_tools_selected_wifi_ssid \
    (s_app_buffers->selected_wifi_ssid)
/*
 * Use pointer aliases for these two buffers instead of macros. Their names
 * also exist as fields in Moonraker/UI structs, and object-like macros would
 * incorrectly expand member access such as state.printer_state.
 */
static char *printer_state = NULL;
static char *printer_file = NULL;
#define dash_thumb_render_file \
    (s_app_buffers->dash_thumb_render_file)
#define last_dashboard_print_state \
    (s_app_buffers->last_dashboard_print_state)
#define connected_wifi_ssid \
    (s_app_buffers->connected_wifi_ssid)


static bool app_runtime_buffers_init(void)
{
    if (s_app_buffers) {
        return true;
    }

    s_app_buffers = heap_caps_calloc(
        1,
        sizeof(*s_app_buffers),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_app_buffers) {
        ESP_LOGI(
            TAG,
            "Application buffers allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_app_buffers));
    } else {
        s_app_buffers = heap_caps_calloc(
            1,
            sizeof(*s_app_buffers),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        if (!s_app_buffers) {
            ESP_LOGE(
                TAG,
                "Unable to allocate application runtime buffers");
            return false;
        }

        ESP_LOGW(
            TAG,
            "Application buffers using internal RAM fallback");
    }

    printer_state =
        s_app_buffers->printer_state;
    printer_file =
        s_app_buffers->printer_file;

    snprintf(
        sd_status,
        sizeof(sd_status),
        "SD: not mounted");

    snprintf(
        moonraker_status,
        sizeof(moonraker_status),
        "Moonraker: waiting for WiFi...");

    snprintf(
        ui_network_tools_network_scan_status,
        sizeof(ui_network_tools_network_scan_status),
        "WiFi scan: not run");

    snprintf(
        printer_state,
        sizeof(s_app_buffers->printer_state),
        "--");

    snprintf(
        printer_file,
        sizeof(s_app_buffers->printer_file),
        "--");

    return true;
}


static bool sd_card_ok = false;
static bool sd_mount_attempted = false;
static lv_obj_t *printer_panel = NULL;

static ui_printer_layout_t printer_layout = {0};
static lv_obj_t *printer_thumb_box = NULL;
static ui_thumbnail_t *printer_thumb_view = NULL;

#define THUMB_TARGET_LIVE  0
#define THUMB_TARGET_POPUP 1
static volatile int printer_thumb_target = THUMB_TARGET_LIVE;

static lv_timer_t *thumb_poll_timer = NULL;

static lv_obj_t *network_selected_ssid_label = NULL;
static lv_obj_t *network_password_ta = NULL;
static lv_obj_t *network_keyboard = NULL;
/*
 * Drybox program selection is application state, not inferred from
 * heater activity or banner text.
 *
 * selected_program remembers the material profile so RESUME can
 * return from HOLD to PLA or PETG.
 */
static ui_drybox_program_t s_drybox_selected_program =
    UI_DRYBOX_PROGRAM_NONE;

static ui_drybox_program_t s_drybox_active_program =
    UI_DRYBOX_PROGRAM_NONE;
static lv_obj_t *printer_state_label = NULL;
static lv_obj_t *printer_file_label = NULL;
static lv_obj_t *printer_active_file_box = NULL;
static lv_obj_t *printer_active_file_label = NULL;
static lv_obj_t *printer_progress_label = NULL;
static lv_obj_t *printer_tuning_label = NULL;
static lv_obj_t *printer_fan_label = NULL;
static lv_obj_t *printer_speed_label = NULL;
static lv_obj_t *printer_flow_label = NULL;
static lv_obj_t *printer_filament_label = NULL;
static double printer_progress = -1.0;
static double printer_print_duration = 0.0;
static double printer_jog_step = 10.0;
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
#define dash_thumb_canvas (*ui_dashboard_thumb_canvas_ref())
#define dash_thumb_canvas_buf (*ui_dashboard_thumb_canvas_buf_ref())
static volatile bool dash_thumb_render_running = false;
static volatile bool dash_thumb_render_ready = false;
static volatile bool dash_thumb_render_failed = false;
static lv_timer_t *dash_thumb_render_timer = NULL;
static int dash_thumb_render_profile_index = -1;
static uint32_t dash_thumb_render_generation = 0;

#define DASH_THUMB_CANVAS_W 286
#define DASH_THUMB_CANVAS_H 215


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
static double printer_current_z = -1.0;
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

#define WIFI_RSSI_SAMPLE_INTERVAL_US 5000000LL

static int64_t s_wifi_rssi_next_sample_us = 0;
static int s_wifi_rssi_filtered = -127;
static bool s_wifi_rssi_valid = false;
static bool s_wifi_signal_dirty = true;


static const char *network_banner_text(void)
{
    if (!s_got_ip) return "NETWORK OFFLINE";
    if (s_moonraker_ok) return "NETWORK LINKED";
    return "WIFI CONNECTED";
}

static const char *printer_banner_text(void);


static void test_moonraker_now(void);
static void safe_copy(char *dst, size_t dst_len, const char *src);
static void dashboard_restore_active_profile_preview(void);


static void reset_preview_state_for_host_change(void)
{
    printer_thumb_target = THUMB_TARGET_LIVE;
    thumbnail_manager_set_force_refresh(true);
    thumbnail_manager_mark_pending();

    thumbnail_session_selected_thumbnail_path()[0] = 0;
    ui_dashboard_thumb_canvas_file()[0] = 0;
    ui_printer_preview_reset();
    thumbnail_preview_coordinator_reset();

    thumbnail_session_free_thumbnail();
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
    printer_current_z = -1.0;
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
        sizeof(s_app_buffers->printer_state),
        "--");

    safe_copy(
        printer_file,
        sizeof(s_app_buffers->printer_file),
        "No file");

    last_dashboard_print_state[0] = '\0';

    thumbnail_session_clear_selected_file();
    thumbnail_session_clear_thumbnail_path();
}


static void moonraker_configuration_changed(
    const char *status,
    bool retest_now)
{
    reset_preview_state_for_host_change();

    ui_network_set_port(
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
        case WIFI_REASON_DISASSOC_DUE_TO_INACTIVITY: return "ASSOC_EXPIRE";
        case WIFI_REASON_ASSOC_TOOMANY: return "ASSOC_TOOMANY";
        case WIFI_REASON_CLASS2_FRAME_FROM_NONAUTH_STA: return "NOT_AUTHED";
        case WIFI_REASON_CLASS3_FRAME_FROM_NONASSOC_STA: return "NOT_ASSOCED";
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


static void wifi_prepare_transport(void)
{
    if (s_wifi_transport_ready) {
        return;
    }

    bool have_saved_wifi = wifi_credentials_store_load(saved_wifi_ssid, sizeof(saved_wifi_ssid), saved_wifi_password, sizeof(saved_wifi_password));
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


    /* PrinterHMI ESP-Hosted co-processor version audit. */
    esp_hosted_coprocessor_fwver_t hosted_fwver = {0};
    esp_err_t hosted_fwver_err =
        esp_hosted_get_coprocessor_fwversion(&hosted_fwver);
    if (hosted_fwver_err == ESP_OK) {
        ESP_LOGI(
            TAG,
            "ESP-Hosted co-processor firmware: %u.%u.%u",
            (unsigned)hosted_fwver.major1,
            (unsigned)hosted_fwver.minor1,
            (unsigned)hosted_fwver.patch1);
    } else {
        ESP_LOGW(
            TAG,
            "ESP-Hosted co-processor firmware query failed: %s",
            esp_err_to_name(hosted_fwver_err));
    }

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
        moonraker_config_api_key(),
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
                     "state", printer_state, sizeof(s_app_buffers->printer_state));

    json_find_string(strstr(s_moonraker_objects, "\"print_stats\"") ? strstr(s_moonraker_objects, "\"print_stats\"") : s_moonraker_objects,
                     "filename", printer_file, sizeof(s_app_buffers->printer_file));

    if (strlen(printer_state) == 0) {
        safe_copy(printer_state, sizeof(s_app_buffers->printer_state), "--");
    }

    if (strlen(printer_file) == 0) {
        safe_copy(printer_file, sizeof(s_app_buffers->printer_file), "No file");
    }
    /*
     * Use virtual_sdcard.progress as the authoritative completion value.
     * This is the file-position progress normally presented by Moonraker
     * frontends. display_status.progress may instead reflect slicer M73
     * commands and can disagree substantially.
     */
    printer_progress = 0.0;

    if (!json_find_number_after(
            s_moonraker_objects,
            "\"virtual_sdcard\"",
            "progress",
            &printer_progress)) {
        /*
         * Fallback for printers without virtual_sdcard or while no file
         * is active.
         */
        json_find_number_after(
            s_moonraker_objects,
            "\"display_status\"",
            "progress",
            &printer_progress);
    }

    if (printer_progress < 0.0) {
        printer_progress = 0.0;
    } else if (printer_progress > 1.0) {
        printer_progress = 1.0;
    }

    json_find_number_after(
        s_moonraker_objects,
        "\"print_stats\"",
        "print_duration",
        &printer_print_duration);

    double lv = 0.0;
    if (json_find_number_after(s_moonraker_objects, "\"motion_report\"", "live_velocity", &lv)) {
        printer_live_velocity = lv;
    }

    double lf = 0.0;
    if (json_find_number_after(
            s_moonraker_objects,
            "\"motion_report\"",
            "live_extruder_velocity",
            &lf)) {
        /*
         * Klipper motion_report.live_extruder_velocity is linear
         * filament speed in mm/s. Moonraker frontends normally show
         * volumetric flow in mm^3/s.
         *
         * For standard 1.75 mm filament:
         *     area = pi * (1.75 / 2)^2 = 2.40528 mm^2
         */
        static const double filament_area_mm2 = 2.405281875;
        printer_live_flow = fabs(lf) * filament_area_mm2;

        /* Suppress tiny interpolation noise while the extruder is idle. */
        if (printer_live_flow < 0.01) {
            printer_live_flow = 0.0;
        }
    }

    /*
     * current_layer and total_layer belong to print_stats.info.
     * Anchor the lookup inside print_stats so another generic "info"
     * object in the Moonraker response cannot capture the search.
     */
    /*
     * Do not clear the cached layer values here. The thumbnail/file
     * metadata fallback may have already calculated them. Replace each
     * value only when print_stats.info publishes a valid value.
     */
    const char *print_stats_json =
        strstr(s_moonraker_objects, "\"print_stats\"");

    const char *print_info_json =
        print_stats_json
            ? strstr(print_stats_json, "\"info\"")
            : NULL;

    double layer_val = 0.0;

    if (print_info_json &&
        json_find_number_after(
            print_info_json,
            "\"info\"",
            "current_layer",
            &layer_val)) {
        printer_current_layer = (int)(layer_val + 0.5);
    }

    if (print_info_json &&
        json_find_number_after(
            print_info_json,
            "\"info\"",
            "total_layer",
            &layer_val)) {
        printer_total_layer = (int)(layer_val + 0.5);
    }

    /*
     * Klipper commonly leaves print_stats.info layer values null.
     * Match Moonraker's displayed layer using current G-code Z and
     * the active file's layer metadata.
     */
    const char *gcode_move_json =
        strstr(s_moonraker_objects, "\"gcode_move\"");

    const char *gcode_position_json =
        gcode_move_json
            ? strstr(gcode_move_json, "\"position\"")
            : NULL;

    const char *gcode_position_array =
        gcode_position_json
            ? strchr(gcode_position_json, '[')
            : NULL;

    if (gcode_position_array) {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;

        if (sscanf(gcode_position_array,
                   "[ %lf , %lf , %lf",
                   &x,
                   &y,
                   &z) == 3) {
            printer_current_z = z;
        }
    }

    if (printer_current_layer <= 0 ||
        printer_total_layer <= 0) {
        double object_height = 0.0;
        double layer_height = 0.0;

        if (thumbnail_session_get_layer_metadata(
                &object_height,
                &layer_height) &&
            object_height > 0.0 &&
            layer_height > 0.0) {

            printer_meta_object_height = object_height;
            printer_meta_layer_height = layer_height;

            printer_total_layer =
                (int)floor(
                    (object_height / layer_height) +
                    0.001);

            if (printer_current_z >= 0.0) {
                printer_current_layer =
                    (int)floor(
                        (printer_current_z / layer_height) +
                        0.001);

                if (printer_current_layer < 1 &&
                    printer_current_z > 0.0) {
                    printer_current_layer = 1;
                }

                if (printer_current_layer >
                    printer_total_layer) {
                    printer_current_layer =
                        printer_total_layer;
                }
            }
        }
    }

    json_find_number_after(s_moonraker_objects, "\"extruder\"", "temperature", &printer_nozzle_temp);
    json_find_number_after(s_moonraker_objects, "\"extruder\"", "target", &printer_nozzle_target);
    json_find_number_after(s_moonraker_objects, "\"heater_bed\"", "temperature", &printer_bed_temp);
    json_find_number_after(s_moonraker_objects, "\"heater_bed\"", "target", &printer_bed_target);

    s_live_data_ok = true;
    
    moonraker_state_publish_http_fallback(
        &(moonraker_http_fallback_update_t) {
            .chamber_temp = live_chamber_temp,
            .air_temp = live_air_temp,
            .humidity = live_humidity,
            .heater_target = live_heater_target,
            .heater_on = live_heater_power,
            .drybox_fan_speed = live_fan_speed,
            .part_fan_speed = printer_part_fan_speed,
            .speed_factor = printer_speed_factor,
            .flow_factor = printer_flow_factor,
            .live_velocity = printer_live_velocity,
            .live_flow = printer_live_flow,
            .nozzle_temp = printer_nozzle_temp,
            .nozzle_target = printer_nozzle_target,
            .bed_temp = printer_bed_temp,
            .bed_target = printer_bed_target,
            .progress = printer_progress,
            .print_duration = printer_print_duration,
            .current_layer = printer_current_layer,
            .total_layer = printer_total_layer,
            .live_data_ok = s_live_data_ok,
            .moonraker_ok = true,
            .printer_state = printer_state,
            .printer_file = printer_file,
        });

    moonraker_state_set_drybox_programs(
        (int)s_drybox_selected_program,
        (int)s_drybox_active_program);

return true;
}

static void printer_thumb_start_delayed(void);
static void printer_build_metadata_text(const char *file, char *out, size_t out_sz);
static void thumbnail_preview_coordinator_set_live_target(void)
{
    printer_thumb_target = THUMB_TARGET_LIVE;
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
        ESP_LOGI(TAG, "LIVE_TRANSPORT HTTP bootstrap");
    }
    websocket_was_authoritative = false;

    /*
     * esp_websocket_client owns reconnects once created.  Do not run the
     * legacy synchronous HTTP fallback on the shell runtime task while
     * that reconnect is pending: an unreachable Moonraker can otherwise
     * hold this task for the HTTP timeout every poll interval.
     */
    if (moonraker_live_websocket_running()) {
        s_moonraker_ok = false;
        s_live_data_ok = false;
        moonraker_state_set_connection(false, false);
        safe_copy(
            moonraker_status,
            sizeof(moonraker_status),
            "Moonraker: reconnecting...");
        return;
    }

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

static bool moonraker_send_gcode_http(const char *cmd)
{
    if (!s_got_ip || !cmd || !cmd[0]) {
        safe_copy(moonraker_status,
                  sizeof(moonraker_status),
                  "Moonraker: no WiFi for command");
        ui_toast_show(
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
        moonraker_config_api_key(),
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
    ui_toast_show(
        UI_STATUS_DANGER,
        "COMMAND FAILED",
        toast_detail);

    return false;
}


static bool moonraker_send_gcode_raw(
    const char *command)
{
    if (!command || !command[0]) {
        return false;
    }

    if (moonraker_live_websocket_send_gcode(
            command)) {
        return true;
    }

    return moonraker_send_gcode_http(command);
}


static bool moonraker_send_gcode(
    const char *requested)
{
    printer_action_resolution_t resolution;

    if (!printer_action_resolver_resolve(
            requested,
            &resolution)) {
        return false;
    }

    if (resolution.macro_used &&
        strcmp(requested, resolution.command) != 0) {
        console_controller_add(
            CONSOLE_ENTRY_SYSTEM,
            "Using printer macro %s for %s",
            resolution.command,
            requested);
        ESP_LOGI(
            TAG,
            "ACTION_MACRO requested=%.96s resolved=%s",
            requested,
            resolution.command);
    }

    return moonraker_send_gcode_raw(
        resolution.command);
}

void ui_printer_create(void);
/* BEGIN LEGACY PRINTER PAGE BLOCK */
void ui_printer_destroy(void);
static void ui_network_tools_wifi_scan_now(void);
static void scan_moonraker_now(void);
static void moonraker_discovery_selected_bridge(
    const char *host,
    int port,
    const char *identity);

static void ui_network_tools_open_wifi_scan_cb(lv_event_t *e)
{
    (void)e;
    ui_network_tools_wifi_scan_now();
}

static void printer_profiles_active_changed_bridge(void)
{
    /* FAILURE_STATE_POLISH_V1 */
    ui_printer_popups_close_all();

    moonraker_live_websocket_prepare_profile_change(
        moonraker_config_generation());

    reset_preview_state_for_host_change();
    reset_active_printer_runtime_state();
    dashboard_restore_active_profile_preview();

    ui_shell_set_active_printer_name(
        moonraker_config_active_profile_name());

    /* Selecting a printer moves active transport ownership.  It does not
     * invalidate health already confirmed for other unchanged endpoints. */
    printer_profile_health_reconcile();
    printer_profile_preview_worker_reset();
    printer_preview_store_reset_restore();
    ui_printer_chooser_refresh();

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

void ui_network_create(void)
{
    ui_network_create_objects(
        network_banner_text(),
        moonraker_config_port(),
        make_printer_info,
        ui_network_tools_open_wifi_scan_cb,
        ui_network_tools_open_printer_profiles_cb,
        ui_network_tools_open_port_edit_cb);
}

static void ui_network_refresh_bridge(void)
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

void ui_files_destroy(void);
static void dashboard_dry_status_event_cb(lv_event_t *e);

static void printer_popup_send_gcode_bridge(const char *cmd)
{
    (void)moonraker_send_gcode(cmd);
}


void ui_command_bar_action(const char *action)
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
    ui_files_set_callbacks(
        files_refresh_bridge,
        files_select_bridge,
        files_preview_bridge);
    ui_files_show();
    app_files_reload();
return;
    }

    if (strcmp(action, "DRYBOX") == 0) {
        ui_files_hide();
        ui_printer_hide();
        ui_network_hide();
        hide_settings_tab();
        ui_drybox_show();
        ui_shell_set_active_nav(
            UI_SHELL_PAGE_DRYBOX);
        return;
    }

    if (strcmp(action, "STATUS") == 0) {
        dashboard_dry_status_event_cb(NULL);
        return;
    }

    moonraker_send_gcode(action);
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

    ui_printer_chooser_hide();
    ui_dashboard_create();
    dashboard_restore_active_profile_preview();
    ui_shell_set_active_nav(UI_SHELL_PAGE_DASHBOARD);
}


static void printer_chooser_manage_bridge(int profile_index)
{
    ui_printer_profiles_show_for_slot(
        profile_index,
        printer_profiles_active_changed_bridge,
        scan_moonraker_now);
}



static void app_hide_operator_pages(void)
{
    ui_bed_mesh_close();
    ui_calibration_hide();
    ui_devices_hide();
    ui_macros_hide();
    ui_console_hide();
    ui_telemetry_hide();
    ui_files_hide();
    ui_printer_hide();
    ui_drybox_hide();
    ui_network_hide();
    hide_settings_tab();
}


static void devices_open_telemetry_bridge(void)
{
    app_hide_operator_pages();
    ui_telemetry_show();
    ui_shell_set_active_nav(UI_SHELL_PAGE_DEVICES);
}


static void calibration_open_bed_mesh_bridge(void)
{
    ui_shell_set_active_nav(UI_SHELL_PAGE_BED_MESH);
    ui_shell_page_action(UI_SHELL_PAGE_BED_MESH);
}


static void settings_open_network_bridge(lv_event_t *event)
{
    (void)event;

    app_hide_operator_pages();
    ui_network_show();
    ui_shell_set_active_nav(UI_SHELL_PAGE_SETTINGS);
}


static void printer_chooser_open_from_topbar(void)
{
    app_hide_operator_pages();

    ui_printer_chooser_show(
        printer_chooser_select_bridge,
        printer_chooser_manage_bridge);

    /* The chooser is a destination, not the Dashboard nav selection. */
    ui_shell_set_active_nav(-1);
}


void ui_shell_page_action(ui_shell_page_t page)
{
    /* Every sidebar destination closes the explicit printer chooser. */
    ui_printer_chooser_hide();
    app_hide_operator_pages();

    switch (page) {
    case UI_SHELL_PAGE_DASHBOARD:
        ui_dashboard_create();
        dashboard_restore_active_profile_preview();
        return;

    case UI_SHELL_PAGE_PRINTER:
        ui_printer_show();
        return;

    case UI_SHELL_PAGE_FILES:
        ui_files_set_callbacks(
            files_refresh_bridge,
            files_select_bridge,
            files_preview_bridge);
        ui_files_show();
        app_files_reload();
        return;

    case UI_SHELL_PAGE_BED_MESH:
        ui_bed_mesh_show(printer_popup_send_gcode_bridge);
        return;

    case UI_SHELL_PAGE_CALIBRATION:
        ui_calibration_show(
            calibration_open_bed_mesh_bridge,
            moonraker_send_gcode);
        return;

    case UI_SHELL_PAGE_DEVICES:
        ui_devices_show(
            devices_open_telemetry_bridge);
        return;

    case UI_SHELL_PAGE_MACROS:
        /*
         * The operator explicitly selected this detected macro. Preserve its
         * exact catalog name instead of resolving it as another action.
         */
        ui_macros_show(
            moonraker_send_gcode_raw);
        return;

    case UI_SHELL_PAGE_CONSOLE:
        /*
         * Console is intentionally literal. It still uses asynchronous
         * WebSocket dispatch, but never rewrites operator-entered G-code.
         */
        ui_console_show(
            moonraker_send_gcode_raw);
        return;

    case UI_SHELL_PAGE_DRYBOX:
        ui_drybox_show();
        return;

    case UI_SHELL_PAGE_SETTINGS:
        show_settings_tab();
        return;

    default:
        return;
    }
}


static void printer_thumb_cleanup_for_popup_close(void)
{
    /* The download worker exclusively owns its running flag. Closing the
     * popup must not make a still-running worker appear idle and permit a
     * second overlapping thumbnail transfer.
     */
    thumbnail_manager_mark_pending();

    thumb_poll_timer = NULL;

    dash_thumb_img = NULL;
    printer_thumb_box = NULL;
    printer_thumb_view = NULL;
}

static void close_printer_file_detail_popup(void);
void ui_files_destroy(void)
{
    close_printer_file_detail_popup();

}


static void ota_ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ota_manager_pump_ui();
}


static void ui_refresh_timer_cb(lv_timer_t *timer)
{
    int64_t ui_refresh_t0 = esp_timer_get_time();

    files_page_controller_process_live_notification();

    (void)timer;

    moonraker_state_t telemetry_state;
    moonraker_state_snapshot(&telemetry_state);

    moonraker_filament_state_t filament_state;
    moonraker_filament_state_snapshot(
        &filament_state);

    printer_layer_result_t resolved_layers =
        printer_layer_resolver_resolve(
            telemetry_state.current_layer,
            telemetry_state.total_layer,
            printer_current_z,
            printer_meta_object_height,
            printer_meta_layer_height,
            telemetry_state.progress);

    ui_telemetry_refresh(
        &telemetry_state,
        esp_timer_get_time());
    network_status_controller_update_topbar(
        wifi_label,
        wifi_status);
    printer_ui_controller_refresh_ctx_t printer_refresh = {
        .printer_panel = printer_panel,
        .banner_label = printer_banner_label,
        .state_label = printer_state_label,
        .active_file_box = printer_active_file_box,
        .active_file_label = printer_active_file_label,
        .speed_label = printer_fan_label,
        .flow_label = printer_speed_label,
        .layer_label = printer_flow_label,
        .filament_label = printer_filament_label,
        .file_label = printer_file_label,
        .topbar_eta_label = topbar_eta_label,

        .home_button = printer_home_btn,
        .pause_button = printer_pause_btn,
        .resume_button = printer_resume_btn,
        .object_button = printer_object_btn,
        .cancel_button = printer_cancel_btn,

        .info_cards = &printer_info_cards,

        .banner_text = printer_controller_machine_banner_text(
            telemetry_state.printer_state,
            telemetry_state.moonraker_ok),
        .printer_state = telemetry_state.printer_state,
        .printer_file = telemetry_state.printer_file,
        .selected_preview_file = thumbnail_session_selected_file(),

        .live_velocity = telemetry_state.live_velocity,
        .live_flow = telemetry_state.live_flow,
        .speed_factor = telemetry_state.speed_factor,
        .flow_factor = telemetry_state.flow_factor,
        .current_layer = resolved_layers.current,
        .total_layer = resolved_layers.total,
        .metadata_object_height = printer_meta_object_height,
        .metadata_layer_height = printer_meta_layer_height,
        .progress = telemetry_state.progress,
        .nozzle_temp = telemetry_state.nozzle_temp,
        .nozzle_target = telemetry_state.nozzle_target,
        .bed_temp = telemetry_state.bed_temp,
        .bed_target = telemetry_state.bed_target,
        .part_fan_speed = telemetry_state.part_fan_speed,
        .print_duration = telemetry_state.print_duration,

        .moonraker_ok = telemetry_state.moonraker_ok,
        .live_data_ok = telemetry_state.live_data_ok,
        .exclude_objects_available =
            moonraker_exclude_objects_available(),

        .capabilities = &telemetry_state.capabilities,
        .filament_state = &filament_state,
    };

    printer_ui_controller_refresh(&printer_refresh);

ui_drybox_refresh();

    ui_network_refresh_bridge();
    ui_settings_refresh();


    dashboard_live_controller_push_banner(
        telemetry_state.moonraker_ok);
    dashboard_live_controller_push_machine();
    ui_command_bar_update(
        telemetry_state.printer_state,
        moonraker_exclude_objects_available());

    int64_t ui_refresh_dt = esp_timer_get_time() - ui_refresh_t0;
    if (ui_refresh_dt > 50000) {
        ESP_LOGW(TAG, "UI_REFRESH_SLOW %lld us", (long long)ui_refresh_dt);
    }
}


static void dashboard_dry_status_event_cb(lv_event_t *e)
{
    (void)e;

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

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
             state.air_temp,
             state.chamber_temp,
             state.humidity,
             state.heater_target,
             state.drybox_fan_speed,
             state.heater_on ? "ON" : "OFF",
             state.live_data_ok ? "linked" : "not linked",
             IP2STR(&s_ip));

    ui_dashboard_status_popup_show("DRYBOX LIVE STATUS", body);
}


static const char *printer_banner_text(void)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    return printer_controller_machine_banner_text(
        state.printer_state,
        state.moonraker_ok);
}


static void filament_sensor_banner_event_cb(lv_event_t *e)
{
    /* FILAMENT_STATUS_LAYOUT_V2 */
    /* FILAMENT_COUNT_SIMPLIFIED_V1 */
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    moonraker_filament_state_t state;
    moonraker_filament_state_snapshot(&state);

    /* Filament status opens live controls for one or more sensors. */
    ui_printer_popups_show_filament_sensors(
        printer_popup_send_gcode_bridge,
        &state);
}


static void part_fan_card_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (state.capabilities.discovered &&
        !state.capabilities.has_part_fan) {
        return;
    }

    ui_printer_popups_show_part_fan(
        printer_popup_send_gcode_bridge,
        state.part_fan_speed);
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
            state.nozzle_temp,
            state.nozzle_target);
    }
}

static void bed_card_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code != LV_EVENT_CLICKED) return;

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (state.capabilities.discovered &&
        !state.capabilities.has_heated_bed) {
        return;
    }

    ui_printer_popups_show_bed(
        printer_popup_send_gcode_bridge,
        state.bed_temp,
        state.bed_target);
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
void ui_printer_destroy(void)
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
    ui_printer_preview_destroy_refs();
    printer_nozzle_label = NULL;
    printer_bed_label = NULL;
    printer_eta_label = NULL;
    printer_banner_label = NULL;
    printer_elapsed_label = NULL;
    printer_remaining_label = NULL;
    printer_fan_label = NULL;
    printer_speed_label = NULL;
    printer_flow_label = NULL;
    printer_filament_label = NULL;

    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}


static void ui_network_tools_close_wifi_password_popup_cb(lv_event_t *e)
{
    (void)e;
    ui_network_tools_wifi_password_close_owned();
}


/*
 * Volatile writes prevent the compiler from optimizing away cleanup of
 * short-lived plaintext credential buffers.
 */
static void clear_sensitive_buffer(
    void *buffer,
    size_t size)
{
    volatile unsigned char *cursor =
        (volatile unsigned char *)buffer;

    while (cursor && size > 0) {
        *cursor++ = 0;
        --size;
    }
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

        clear_sensitive_buffer(
            cfg.sta.password,
            sizeof(cfg.sta.password));

        return;
    }

    /*
     * esp_wifi_set_config() has copied the configuration. The stack copy is
     * no longer needed after this point.
     */
    clear_sensitive_buffer(
        cfg.sta.password,
        sizeof(cfg.sta.password));

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

    if (wifi_credentials_store_save(
            ui_network_tools_selected_wifi_ssid,
            ui_network_tools_selected_wifi_password)) {
        safe_copy(saved_wifi_ssid, sizeof(saved_wifi_ssid),
                  ui_network_tools_selected_wifi_ssid);
        safe_copy(saved_wifi_password, sizeof(saved_wifi_password),
                  ui_network_tools_selected_wifi_password);
    }
    connect_selected_wifi();

    clear_sensitive_buffer(
        ui_network_tools_selected_wifi_password,
        sizeof(ui_network_tools_selected_wifi_password));

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


/* BEGIN LEGACY NETWORK BLOCK */
static void ui_network_tools_wifi_scan_now(void)
{
    if (!network_activity_controller_request_exclusive()) {
        safe_copy(
            ui_network_tools_network_scan_status,
            sizeof(ui_network_tools_network_scan_status),
            "NETWORK BUSY - TRY AGAIN");
        ui_network_set_scan_status(
            ui_network_tools_network_scan_status);
        return;
    }

    safe_copy(
        ui_network_tools_network_scan_status,
        sizeof(ui_network_tools_network_scan_status),
        "SCANNING...");

    ui_network_set_scan_status(
        ui_network_tools_network_scan_status);

    /* Draw the state while the runtime retires WebSocket/background HTTP. */
    lv_refr_now(NULL);

    int elapsed_ms = 0;
    for (; elapsed_ms < 20000; elapsed_ms += 50) {
        if (network_activity_controller_exclusive_ready()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (!network_activity_controller_exclusive_ready()) {
        safe_copy(
            ui_network_tools_network_scan_status,
            sizeof(ui_network_tools_network_scan_status),
            "SCAN WAIT TIMEOUT");
        ui_network_set_scan_status(
            ui_network_tools_network_scan_status);
        network_activity_controller_release_exclusive();
        return;
    }


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

        ui_network_set_scan_status(
            ui_network_tools_network_scan_status);

        goto scan_finished;
    }

    uint16_t ap_count = 0;

    err = esp_wifi_scan_get_ap_num(&ap_count);

    if (err != ESP_OK) {
        snprintf(
            ui_network_tools_network_scan_status,
            sizeof(ui_network_tools_network_scan_status),
            "COUNT FAILED: %s",
            esp_err_to_name(err));

        ui_network_set_scan_status(
            ui_network_tools_network_scan_status);

        goto scan_finished;
    }

    if (ap_count == 0) {
        safe_copy(
            ui_network_tools_network_scan_status,
            sizeof(ui_network_tools_network_scan_status),
            "NO NETWORKS FOUND");

        ui_network_set_scan_status(
            ui_network_tools_network_scan_status);

        goto scan_finished;
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

        ui_network_set_scan_status(
            ui_network_tools_network_scan_status);

        goto scan_finished;
    }

    ui_network_render_scan_results(
        aps,
        visible_count,
        (unsigned)ap_count,
        ui_network_tools_wifi_ssid_selected_cb);

scan_finished:
    network_activity_controller_release_exclusive();
}


static void moonraker_discovery_selected_bridge(
    const char *host,
    int port,
    const char *identity)
{
    if (!host || !host[0] || port <= 0 || port >= 65536) {
        return;
    }

    /*
     * Discovery edits the open profile draft only. The operator remains
     * responsible for reviewing the result and pressing SAVE.
     */
    ui_printer_profiles_set_discovered_endpoint(
        host,
        port,
        identity);

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

void ui_network_destroy(void)
{
    ui_printer_profiles_close_all();
    ui_network_tools_wifi_popup_destroy_all();

    ui_network_destroy_objects(
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
        ota_ui_controller_open_event_cb,
        settings_open_network_bridge,
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
    ui_dashboard_status_popup_close();
    ui_dashboard_destroy();

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

    ui_dashboard_create();
    dashboard_restore_active_profile_preview();
    app_create_wifi_status_label();

    show_settings_tab();
    ui_shell_set_active_nav(UI_SHELL_PAGE_SETTINGS);
    ui_shell_raise();
    ui_shell_update_status_icons();
}


/* END LEGACY NETWORK BLOCK */



static void close_printer_file_detail_popup(void)
{
    file_detail_loader_cancel();
    printer_thumb_cleanup_for_popup_close();
    ui_files_close_detail_popup();

    printer_thumb_box = NULL;
    printer_thumb_view = NULL;
}


static void printer_file_detail_start_bridge(void)
{
    (void)printer_file_controller_start_selected_file(
        s_got_ip,
        moonraker_config_host(),
        moonraker_config_port(),
        moonraker_config_api_key(),
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

    bool metadata_ok = thumbnail_session_build_metadata(
        moonraker_config_host(),
        moonraker_config_port(),
        moonraker_config_api_key(),
        file,
        out,
        out_size);

    if (metadata_ok) {
        (void)thumbnail_session_get_layer_metadata(
            &printer_meta_object_height,
            &printer_meta_layer_height);
    }
}

static void printer_thumb_set_label(const char *txt)
{
    if (!printer_thumb_view) {
        return;
    }

    ui_thumbnail_set_placeholder(
        printer_thumb_view,
        txt ? txt : "NO THUMBNAIL");
}

static void dashboard_show_loaded_thumbnail(void);

static bool printer_publish_selected_preview_cache(void)
{
    const char *file =
        thumbnail_session_selected_file();

    if (!file || !file[0] ||
        !thumbnail_manager_has_png()) {
        return false;
    }

    bool published =
        printer_preview_cache_publish_png(
            moonraker_config_active_profile_index(),
            moonraker_config_host(),
            moonraker_config_port(),
            file,
            thumbnail_manager_png_data(),
            thumbnail_manager_png_size(),
            DASH_THUMB_CANVAS_W,
            DASH_THUMB_CANVAS_H);

    if (published) {
        printer_preview_store_store_active(
            file,
            thumbnail_manager_png_data(),
            thumbnail_manager_png_size());
    }

    return published;
}


static void dashboard_show_loaded_thumbnail(void)
{
    if (!ui_dashboard_thumb_ready() ||
        !thumbnail_manager_has_png()) {
        return;
    }

    if (dash_thumb_canvas &&
        thumbnail_session_selected_file()[0] &&
        strcmp(ui_dashboard_thumb_canvas_file(),
               thumbnail_session_selected_file()) == 0) {
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

    if (!thumbnail_render_to_rgb565(
            thumbnail_manager_image_dsc(),
            dash_thumb_canvas_buf,
            DASH_THUMB_CANVAS_W,
            DASH_THUMB_CANVAS_H)) {
        ESP_LOGW(TAG, "DASH_CANVAS shared render failed");
        return;
    }


    /* CACHE_PUBLISH_SYNC: PREVIEW_PROFILE_OWNERSHIP_COMPLETE */
    const char *cache_file =
        thumbnail_session_selected_file();

    if (cache_file && cache_file[0]) {
        bool cache_published =
            printer_preview_cache_publish_active(
                cache_file,
                dash_thumb_canvas_buf,
                DASH_THUMB_CANVAS_W,
                DASH_THUMB_CANVAS_H);

        if (cache_published &&
            thumbnail_manager_has_png()) {
            printer_preview_store_store_active(
                cache_file,
                thumbnail_manager_png_data(),
                thumbnail_manager_png_size());
        }
    }

    if (dash_thumb_img) {
        lv_obj_delete(dash_thumb_img);
        dash_thumb_img = NULL;
    }

    if (!dash_thumb_canvas) {
        dash_thumb_canvas =
            lv_canvas_create(ui_dashboard_thumb_box());
    }

    lv_canvas_set_buffer(
        dash_thumb_canvas,
        dash_thumb_canvas_buf,
        DASH_THUMB_CANVAS_W,
        DASH_THUMB_CANVAS_H,
        LV_COLOR_FORMAT_RGB565);

    ui_thumbnail_fit_object(
        dash_thumb_canvas,
        ui_dashboard_thumb_box(),
        DASH_THUMB_CANVAS_W,
        DASH_THUMB_CANVAS_H,
        6);
    lv_obj_move_foreground(dash_thumb_canvas);

    ui_dashboard_thumb_clear_placeholder();

    safe_copy(
        ui_dashboard_thumb_canvas_file(),
        ui_dashboard_thumb_canvas_file_size(),
        thumbnail_session_selected_file());

}


static void dashboard_apply_rendered_thumbnail(void)
{
    if (!ui_dashboard_thumb_ready() || !dash_thumb_canvas_buf) return;

    /*
     * Minimal LVGL apply:
     * - Do not delete labels/images here.
     * - Do not recreate canvas if it already exists.
     * - Worker already rendered pixels into dash_thumb_canvas_buf.
     */
    if (!dash_thumb_canvas) {
        dash_thumb_canvas = lv_canvas_create(ui_dashboard_thumb_box());
        lv_canvas_set_buffer(dash_thumb_canvas,
                             dash_thumb_canvas_buf,
                             DASH_THUMB_CANVAS_W,
                             DASH_THUMB_CANVAS_H,
                             LV_COLOR_FORMAT_RGB565);
        lv_obj_move_foreground(dash_thumb_canvas);
    } else {
        lv_obj_invalidate(dash_thumb_canvas);
    }

    ui_thumbnail_fit_object(
        dash_thumb_canvas,
        ui_dashboard_thumb_box(),
        DASH_THUMB_CANVAS_W,
        DASH_THUMB_CANVAS_H,
        6);

    ui_dashboard_thumb_clear_placeholder();

    if (dash_thumb_img) {
        lv_obj_add_flag(dash_thumb_img, LV_OBJ_FLAG_HIDDEN);
    }

    safe_copy(ui_dashboard_thumb_canvas_file(), ui_dashboard_thumb_canvas_file_size(), dash_thumb_render_file);

}



static void dashboard_restore_active_profile_preview(void)
{
    const char *file = NULL;
    uint32_t revision = 0;

    const lv_image_dsc_t *image =
        printer_preview_cache_image(
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
        ui_dashboard_thumb_delete_canvas();
        ui_dashboard_thumb_set_placeholder(
            "PRINT\nTHUMBNAIL\n\nNo preview loaded");
        return;
    }

    if (!ui_dashboard_thumb_ensure_canvas_buffer(
            DASH_THUMB_CANVAS_W * DASH_THUMB_CANVAS_H)) {
        return;
    }

    memcpy(
        dash_thumb_canvas_buf,
        image->data,
        DASH_THUMB_CANVAS_W *
            DASH_THUMB_CANVAS_H * sizeof(uint16_t));

    ui_dashboard_thumb_show_canvas_from_buffer(
        DASH_THUMB_CANVAS_W,
        DASH_THUMB_CANVAS_H,
        file);
}


static void dash_thumb_render_task(void *arg)
{
    (void)arg;

    dash_thumb_render_ready = false;
    dash_thumb_render_failed = false;

    if (!thumbnail_manager_has_png()) {
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
        ok = thumbnail_render_to_rgb565(
            thumbnail_manager_image_dsc(),
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
                    printer_preview_cache_publish_active(
                        dash_thumb_render_file,
                        dash_thumb_canvas_buf,
                        DASH_THUMB_CANVAS_W,
                        DASH_THUMB_CANVAS_H);

                if (cache_published &&
                    thumbnail_manager_has_png()) {
                    printer_preview_store_store_active(
                        dash_thumb_render_file,
                        thumbnail_manager_png_data(),
                        thumbnail_manager_png_size());
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
    if (!thumbnail_manager_has_png()) return;

    safe_copy(dash_thumb_render_file, sizeof(dash_thumb_render_file), thumbnail_session_selected_file());
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

    thumbnail_manager_result_t result =
        thumbnail_manager_result();

    if (result == THUMBNAIL_MANAGER_RESULT_LOADING) {
        return;
    }

    lv_timer_delete(t);
    thumb_poll_timer = NULL;

    switch (result) {
    case THUMBNAIL_MANAGER_RESULT_FAILED:
        if (!is_live_thumb) {
            printer_thumb_set_label("THUMBNAIL\nTIMEOUT");
        }
        return;

    case THUMBNAIL_MANAGER_RESULT_IDLE:
        if (!is_live_thumb) {
            printer_thumb_set_label("NO THUMBNAIL");
        }
        return;

    case THUMBNAIL_MANAGER_RESULT_READY:
        break;

    case THUMBNAIL_MANAGER_RESULT_LOADING:
    default:
        return;
    }

    if (is_live_thumb) {
        dash_thumb_start_render_task();
        return;
    }

    /* Popup/selected-file preview path only. */
    if (!ui_files_detail_is_open() || !printer_thumb_box) {
        return;
    }

    ui_thumbnail_show_image(
        printer_thumb_view,
        thumbnail_manager_image_dsc(),
        0);

    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    if (!printer_controller_is_live_state(state.printer_state)) {
        /*
         * Publish the selected file to the profile-owned preview cache even
         * while Dashboard is not instantiated. Files owns the selection
         * workflow; Dashboard and Printer are independent cache consumers.
         */
        bool published = false;

        if (ui_dashboard_thumb_ready()) {
            ui_dashboard_thumb_canvas_file()[0] = 0;
            dashboard_show_loaded_thumbnail();
            published =
                printer_preview_cache_matches(
                    moonraker_config_active_profile_index(),
                    thumbnail_session_selected_file());
        } else {
            published =
                printer_publish_selected_preview_cache();
        }

        if (published) {
            ui_printer_preview_show(
                state.printer_state,
                state.printer_file,
                thumbnail_session_selected_file());
        }
    }
}


static void printer_thumb_start_delayed(void)
{
    thumbnail_manager_mark_pending();

    if (!thumbnail_session_selected_thumbnail_path()[0]) {
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

    if (thumbnail_manager_task_running()) {
        printer_thumb_set_label("LOADING...");
        if (!thumb_poll_timer)
        thumb_poll_timer = lv_timer_create(printer_thumb_ui_poll_cb, 200, NULL);
        return;
    }

    printer_thumb_set_label("LOADING...");
    bool started =
        thumbnail_manager_start_download_task(
            moonraker_config_host(),
            moonraker_config_port(),
            thumbnail_session_selected_file(),
            thumbnail_session_selected_thumbnail_path(),
            thumbnail_manager_force_refresh(),
            sd_card_ok);

    if (!started) {
        thumbnail_manager_mark_failed();
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
    if (!ui_files_detail_is_open() ||
        !file ||
        strcmp(file, thumbnail_session_selected_file()) != 0) {
        return;
    }

    safe_copy(
        thumbnail_session_metadata_info(),
        thumbnail_session_metadata_info_size(),
        metadata_text);
    safe_copy(
        thumbnail_session_selected_thumbnail_path(),
        thumbnail_session_selected_thumbnail_path_size(),
        thumbnail_path);

    ui_files_update_detail_metadata(metadata_text, true);

    if (thumbnail_path && thumbnail_path[0]) {
        printer_thumb_set_label("THUMBNAIL\nFOUND");
        printer_thumb_target = THUMB_TARGET_POPUP;
        printer_thumb_start_delayed();
    } else {
        printer_thumb_set_label("NO THUMBNAIL");
    }

    if (!metadata_ok) {
        ui_toast_show(
            UI_STATUS_WARNING,
            "METADATA UNAVAILABLE",
            "The file can still be started.");
    }
}

static void show_printer_file_detail_popup(void)
{
    close_printer_file_detail_popup();

    thumbnail_session_selected_thumbnail_path()[0] = 0;
    safe_copy(thumbnail_session_metadata_info(),
              thumbnail_session_metadata_info_size(),
              "Loading metadata...");

    printer_thumb_box = NULL;
    printer_thumb_view = NULL;

    ui_files_show_detail_popup(
        thumbnail_session_selected_file(),
        thumbnail_session_metadata_info(),
        &printer_thumb_box,
        &printer_thumb_view,
        close_printer_file_detail_popup,
        printer_file_detail_start_bridge);
    printer_thumb_set_label("LOADING...");

    if (!file_detail_loader_start(
            moonraker_config_host(),
            moonraker_config_port(),
            moonraker_config_api_key(),
            thumbnail_session_selected_file(),
            file_detail_ready_cb)) {
        ui_files_update_detail_metadata(
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
        moonraker_config_api_key());
}


/* END FILES APP BRIDGE BLOCK */


void ui_printer_create(void)
{
    moonraker_state_t state;
    moonraker_state_snapshot(&state);

    bool printer_is_live =
        printer_controller_is_live_state(state.printer_state);

    if (printer_is_live && state.printer_file[0] &&
        strcmp(
            thumbnail_session_selected_file(),
            state.printer_file) != 0) {
        safe_copy(
            thumbnail_session_selected_file(),
            thumbnail_session_selected_file_size(),
            state.printer_file);
        thumbnail_session_selected_thumbnail_path()[0] = 0;
        ui_dashboard_thumb_canvas_file()[0] = 0;
        ui_printer_preview_reset();
        thumbnail_preview_coordinator_reset();

        thumbnail_session_clear_png_buffer();
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
        ui_page_layout_profile_current()->printer.subtitle);

    if (!ui_printer_layout_create(
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
        &printer_filament_label,
        filament_sensor_banner_event_cb,
        state.speed_factor,
        state.flow_factor,
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

    ui_printer_preview_create(printer_layout.active_panel);

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

    ui_printer_preview_show(
        state.printer_state,
        state.printer_file,
        thumbnail_session_selected_file());
    lv_obj_set_pos(divider, 20, 345);
    lv_obj_set_style_bg_color(divider, UI_BORDER_SOFT, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_radius(divider, 0, 0);

    
    
    
    

    ui_drybox_refresh();

    ui_network_refresh_bridge();

    printer_ui_controller_update_action_buttons(
        printer_home_btn,
        printer_pause_btn,
        printer_resume_btn,
        printer_object_btn,
        printer_cancel_btn,
        moonraker_exclude_objects_available(),
        state.printer_state);
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
    ui_drybox_set_callbacks(
        moonraker_send_gcode,
        dashboard_dry_status_event_cb);
    ui_shell_set_printer_switch_callback(
        printer_chooser_open_from_topbar);

    /* Navigation rail now belongs to ui_shell. */
    ui_shell_create_nav();

    /* Legacy dashboard body removed.
     * The shell/topbar/nav stay here for now.
     * The only dashboard is ui_dashboard.
     */
    ui_dashboard_create();

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

/*
 * ESP-Hosted creates the shared SDMMC controller for slot 1.
 * The removable card adds slot 0 without recreating that controller.
 * IDF6's default per-slot deinit callback remains intact.
 */
static esp_err_t sdmmc_host_init_existing_controller(void)
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
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
    host.init = sdmmc_host_init_existing_controller;

    sd_pwr_ctrl_ldo_config_t ldo_config = {
        .ldo_chan_id = 4,
    };
    sd_pwr_ctrl_handle_t pwr_ctrl_handle = NULL;

    esp_err_t ret =
        sd_pwr_ctrl_new_on_chip_ldo(
            &ldo_config,
            &pwr_ctrl_handle);

    if (ret != ESP_OK) {
        snprintf(sd_status,
                 sizeof(sd_status),
                 "SD: power failed %s",
                 esp_err_to_name(ret));
        ESP_LOGW(TAG, "%s", sd_status);
        return;
    }

    host.pwr_ctrl_handle = pwr_ctrl_handle;
    ESP_LOGI(TAG, "SD LDO channel 4 enabled");

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

    ret = esp_vfs_fat_sdmmc_mount(
        "/sdcard",
        &host,
        &slot_config,
        &mount_config,
        &card);

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

    /*
     * Custom themes live on removable storage, so their saved identity can
     * only be resolved after the delayed SD mount. Rebuild existing LVGL
     * objects once when a saved custom theme becomes available.
     */
    theme_manager_scan_custom_themes();
    if (theme_manager_custom_active() &&
        bsp_display_lock(1000)) {
        app_theme_changed();
        bsp_display_unlock();
    }

}



static void wifi_signal_sample_tasklet(bool allow_remote_query)
{
    /*
     * A valid IP is required before the top bar reports usable Wi-Fi.
     * Clearing this state does not require an ESP-Hosted RPC transaction.
     */
    if (!s_got_ip) {
        if (s_wifi_rssi_valid) {
            s_wifi_rssi_valid = false;
            s_wifi_rssi_filtered = -127;
            s_wifi_signal_dirty = true;
        }

        s_wifi_rssi_next_sample_us = 0;
        return;
    }

    /*
     * OTA owns the transport exclusively. Preserve the last good reading
     * instead of generating an unnecessary Wi-Fi RPC during the transfer.
     */
    if (!allow_remote_query) return;

    int64_t now = esp_timer_get_time();
    if (now < s_wifi_rssi_next_sample_us) return;

    s_wifi_rssi_next_sample_us =
        now + WIFI_RSSI_SAMPLE_INTERVAL_US;

    int rssi = -127;
    esp_err_t err = esp_wifi_sta_get_rssi(&rssi);

    /*
     * Zero is the Wi-Fi Remote unavailable/default value, not a legitimate
     * RSSI measurement. A transient RPC failure preserves the previous
     * reading; loss of IP clears it separately.
     */
    if (err != ESP_OK || rssi >= 0 || rssi < -127) {
        return;
    }

    if (!s_wifi_rssi_valid) {
        s_wifi_rssi_filtered = rssi;
    } else {
        /* Low-cost exponential smoothing: 2/3 previous, 1/3 new. */
        s_wifi_rssi_filtered =
            ((2 * s_wifi_rssi_filtered) + rssi) / 3;
    }

    s_wifi_rssi_valid = true;
    s_wifi_signal_dirty = true;

    ESP_LOGD(
        TAG,
        "WiFi RSSI raw=%d filtered=%d",
        rssi,
        s_wifi_rssi_filtered);
}


static void hmi_runtime_task(void *arg)
{
    (void)arg;
    bool ota_network_quiet = false;

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

        bool exclusive_network =
            network_activity_controller_exclusive_requested();

        wifi_signal_sample_tasklet(!exclusive_network);

        if (exclusive_network) {
            if (!ota_network_quiet) {
                ESP_LOGI(TAG, "NETWORK_EXCLUSIVE_QUIESCE begin");
                moonraker_live_websocket_stop();
                network_activity_controller_set_persistent_quiet(true);
                ota_network_quiet = true;
            }
        } else {
            if (ota_network_quiet) {
                ESP_LOGI(TAG, "NETWORK_EXCLUSIVE_QUIESCE end");
                network_activity_controller_set_persistent_quiet(false);
                ota_network_quiet = false;
            }

            /* MOONRAKER_WEBSOCKET_PHASE2
             * Observe the active profile over a persistent subscription while
             * the proven HTTP poller remains authoritative.
             */
            moonraker_live_websocket_tasklet(
                s_got_ip,
                moonraker_config_host(),
                moonraker_config_port(),
                moonraker_config_api_key(),
                moonraker_config_generation());

            moonraker_live_poll_tasklet();
        }
        /* BOOT_PREVIEW_PROFILE_STORE_ONLY
         * Reboot restoration is profile-indexed and endpoint-validated.
         * The legacy global last-file cache must not publish into whichever
         * printer profile happens to be active at boot.
         */
        printer_preview_store_restore_one(sd_card_ok);
        if (bsp_display_lock(50)) {
            if (wifi_label) {
                lv_label_set_text(wifi_label, wifi_status);
            }

            if (s_wifi_signal_dirty) {
                ui_shell_set_wifi_signal(
                    s_wifi_rssi_valid && s_got_ip,
                    s_wifi_rssi_filtered);
                s_wifi_signal_dirty = false;
            }

            dashboard_runtime_controller_tick(
        &(dashboard_runtime_context_t) {
            .current_z = printer_current_z,
            .meta_object_height = printer_meta_object_height,
            .meta_layer_height = printer_meta_layer_height,

            .nozzle_label = dash_nozzle_label,
            .bed_label = dash_bed_label,
            .chamber_label = card_chamber_temp,
            .humidity_label = card_humidity,
            .target_rh_label = card_target_rh,
            .heater_label = card_heater,
            .fan_label = card_fan,
            .moonraker_label = card_moonraker,

            .dashboard_canvas = &dash_thumb_canvas,
            .dashboard_image = &dash_thumb_img,

            .last_print_state = last_dashboard_print_state,
            .last_print_state_size =
                sizeof(last_dashboard_print_state),

            .preview_network_ready =
                s_got_ip && !exclusive_network,

            .set_live_target =
                thumbnail_preview_coordinator_set_live_target,
            .free_thumbnail =
                thumbnail_session_free_thumbnail,
            .build_metadata =
                printer_build_metadata_text,
            .start_delayed =
                printer_thumb_start_delayed,
        });

            bsp_display_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(250));
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

    ui_splash_wifi_waiting(connected);
    bsp_display_unlock();
}


static void app_startup_show_initial_ui(void)
{
    bsp_display_lock(0);
    build_drybox_dashboard();
    hide_settings_tab();
    ui_dashboard_create();

    /* STARTUP_OPEN_PRINTER_CHOOSER
     * Keep the active Dashboard built behind the startup splash, then place
     * the multi-printer chooser in front. Selecting a printer continues into
     * that profile's Dashboard through printer_chooser_select_bridge().
     */
    ui_printer_chooser_show(
        printer_chooser_select_bridge,
        printer_chooser_manage_bridge);
    ui_splash_create();
    ui_splash_display_ready();
    bsp_display_unlock();
}


void app_main(void)
{
    ESP_LOGI(TAG, "BOOT_RESET_REASON=%d", esp_reset_reason());

    if (!app_runtime_buffers_init()) {
        return;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    operator_event_log_init();
    console_controller_init();
    macro_controller_init();
    device_catalog_controller_init();
    calibration_session_controller_init();
    ui_macros_init();
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

    app_splash_locked(ui_splash_wifi_starting);

    /* Start WiFi after dashboard is visible. Touch scaling fix remains in BSP. */
    if (!sd_mount_attempted) {
        sd_mount_attempted = true;
        ESP_LOGI(TAG, "SD mount before WiFi traffic");
        init_sd_card_storage();
    }

    ESP_LOGI(TAG, "Starting WiFi after display/touch/dashboard");
    wifi_init_sta();

    app_splash_wifi_waiting_locked(s_got_ip);

    vTaskDelay(pdMS_TO_TICKS(350));

    app_splash_locked(ui_splash_moonraker_ready);

    vTaskDelay(pdMS_TO_TICKS(350));

    app_splash_locked(ui_splash_dashboard_ready);

    vTaskDelay(pdMS_TO_TICKS(500));

    app_splash_locked(ui_splash_destroy);

    const esp_app_desc_t *running_app =
        esp_app_get_description();

    ESP_LOGI(
        TAG,
        "PrinterHMI version %s WiFi/display baseline ready",
        running_app && running_app->version[0]
            ? running_app->version
            : "unknown");

    ota_boot_validation_confirm_running_image();

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
    } else {
        /* Inactive-profile HTTP checks may wait for unreachable hosts. */
        printer_profile_preview_worker_start(
            moonraker_config_api_key());
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
