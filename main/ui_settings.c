#include "ui_settings.h"
#include "ui_page_layout_profile.h"
#include "ui_settings_popups.h"
#include "ui_settings_components.h"
#include "ui_settings_backup_popup.h"
#include "ui_event_history_popup.h"
#include "settings_system_info.h"
#include "timezone_config.h"
#include "theme_manager.h"
#include "ui_appearance_popups.h"
#include "ui_theme_lab.h"

#include "bsp/display.h"
#include "esp_err.h"
#include "nvs.h"


#include "ui_theme.h"
#include "ui_page_geometry_v32.h"
#include "ui_page_title.h"
#include "ui_widgets.h"
#include "ui_shell.h"

static const char *TAG = "ui_settings";

#include "esp_log.h"
#include "esp_system.h"
#include "esp_app_desc.h"

#include <stdio.h>

static int s_display_brightness_percent = 100;
static int s_saved_display_brightness_percent = 100;

#define SETTINGS_NVS_NAMESPACE "display"
#define SETTINGS_NVS_BRIGHTNESS_KEY "brightness"
#define SETTINGS_NVS_SLEEP_KEY "sleep_min"

static uint8_t s_sleep_timeout_minutes = 0;
static lv_timer_t *s_sleep_timer = NULL;
static lv_obj_t *s_sleep_wake_overlay = NULL;
static bool s_display_sleeping = false;
static lv_obj_t *s_timezone_label = NULL;
static lv_obj_t *s_theme_label = NULL;
static ui_settings_theme_rebuild_cb_t s_theme_rebuild_cb = NULL;

static void settings_timezone_changed(void)
{
    if (s_timezone_label) {
        lv_label_set_text(
            s_timezone_label,
            timezone_config_selected_label());
    }

    ui_shell_refresh_clock();
}

static void settings_timezone_card_cb(lv_event_t *event)
{
    (void)event;
    ui_settings_popups_show_timezone(settings_timezone_changed);
}

static void settings_theme_changed(void)
{
    if (s_theme_label) {
        lv_label_set_text(
            s_theme_label,
            theme_manager_active_label());
    }

    if (s_theme_rebuild_cb) {
        s_theme_rebuild_cb();
    }
}

static void settings_theme_card_cb(lv_event_t *event)
{
    (void)event;
    ui_settings_popups_show_theme(settings_theme_changed);
}

static void settings_appearance_changed(void)
{
    if (s_theme_rebuild_cb) s_theme_rebuild_cb();
}

static void settings_accent_card_cb(lv_event_t *event)
{
    (void)event;
    ui_appearance_popups_show_accent(settings_appearance_changed);
}

static void settings_density_card_cb(lv_event_t *event)
{
    (void)event;
    ui_appearance_popups_show_density(settings_appearance_changed);
}

static void settings_accessibility_card_cb(lv_event_t *event)
{
    (void)event;
    ui_appearance_popups_show_accessibility(settings_appearance_changed);
}

static void settings_theme_lab_cb(lv_event_t *event)
{
    (void)event;
    ui_theme_lab_show();
}


static void settings_event_history_cb(lv_event_t *event)
{
    (void)event;
    ui_event_history_popup_show();
}


static void settings_backup_cb(lv_event_t *event)
{
    (void)event;
    ui_settings_backup_popup_show();
}

static void settings_brightness_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);

    if (!slider) {
        return;
    }

    int brightness =
        lv_slider_get_value(slider);

    lv_obj_t *value_label =
        lv_obj_get_user_data(slider);

    if (value_label) {
        char text[16];

        lv_snprintf(
            text,
            sizeof(text),
            "%d%%",
            brightness);

        lv_label_set_text(
            value_label,
            text);
    }

    esp_err_t err =
        bsp_display_brightness_set(brightness);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Brightness update failed: %s",
            esp_err_to_name(err));
        return;
    }

    s_display_brightness_percent = brightness;
}


static void settings_brightness_save_cb(lv_event_t *e)
{
    (void)e;

    if (s_display_brightness_percent ==
        s_saved_display_brightness_percent) {
        return;
    }

    nvs_handle_t handle;

    esp_err_t err =
        nvs_open(
            SETTINGS_NVS_NAMESPACE,
            NVS_READWRITE,
            &handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not open brightness storage: %s",
            esp_err_to_name(err));
        return;
    }

    err = nvs_set_u8(
        handle,
        SETTINGS_NVS_BRIGHTNESS_KEY,
        (uint8_t)s_display_brightness_percent);

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not save display brightness: %s",
            esp_err_to_name(err));
        return;
    }

    s_saved_display_brightness_percent =
        s_display_brightness_percent;

    ESP_LOGI(
        TAG,
        "Saved display brightness: %d%%",
        s_saved_display_brightness_percent);
}


static const char *settings_sleep_timeout_text(void)
{
    switch (s_sleep_timeout_minutes) {
        case 5:
            return "5 MIN";

        case 15:
            return "15 MIN";

        case 30:
            return "30 MIN";

        case 0:
        default:
            return "OFF";
    }
}


static void settings_wake_display_cb(lv_event_t *e)
{
    (void)e;

    if (!s_display_sleeping) {
        return;
    }

    s_display_sleeping = false;

    esp_err_t err =
        bsp_display_brightness_set(
            s_display_brightness_percent);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Display wake failed: %s",
            esp_err_to_name(err));
    }

    lv_display_trigger_activity(
        lv_display_get_default());

    if (s_sleep_wake_overlay) {
        lv_obj_delete_async(
            s_sleep_wake_overlay);

        s_sleep_wake_overlay = NULL;
    }

    ESP_LOGI(
        TAG,
        "Display awake at %d%%",
        s_display_brightness_percent);
}


static void settings_enter_display_sleep(void)
{
    if (s_display_sleeping ||
        s_sleep_timeout_minutes == 0) {
        return;
    }

    /*
     * This transparent top-layer object consumes the first touch.
     * That touch wakes the display without activating a hidden control.
     */
    s_sleep_wake_overlay =
        lv_obj_create(lv_layer_top());

    if (!s_sleep_wake_overlay) {
        ESP_LOGE(TAG, "Could not create display wake overlay");
        return;
    }

    lv_obj_set_size(
        s_sleep_wake_overlay,
        LV_PCT(100),
        LV_PCT(100));

    lv_obj_set_pos(
        s_sleep_wake_overlay,
        0,
        0);

    lv_obj_clear_flag(
        s_sleep_wake_overlay,
        LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_flag(
        s_sleep_wake_overlay,
        LV_OBJ_FLAG_CLICKABLE);

    ui_apply_surface_role(s_sleep_wake_overlay, UI_SURFACE_TRANSPARENT);

    lv_obj_add_event_cb(
        s_sleep_wake_overlay,
        settings_wake_display_cb,
        LV_EVENT_PRESSED,
        NULL);

    lv_obj_move_foreground(
        s_sleep_wake_overlay);

    s_display_sleeping = true;

    esp_err_t err =
        bsp_display_backlight_off();

    if (err != ESP_OK) {
        s_display_sleeping = false;

        lv_obj_delete(
            s_sleep_wake_overlay);

        s_sleep_wake_overlay = NULL;

        ESP_LOGE(
            TAG,
            "Display sleep failed: %s",
            esp_err_to_name(err));
        return;
    }

    ESP_LOGI(
        TAG,
        "Display sleeping after %u minutes",
        (unsigned)s_sleep_timeout_minutes);
}


static void settings_sleep_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_display_sleeping ||
        s_sleep_timeout_minutes == 0) {
        return;
    }

    lv_display_t *display =
        lv_display_get_default();

    if (!display) {
        return;
    }

    uint32_t timeout_ms =
        (uint32_t)s_sleep_timeout_minutes *
        60U *
        1000U;

    if (lv_display_get_inactive_time(display) >=
        timeout_ms) {
        settings_enter_display_sleep();
    }
}


static void settings_save_sleep_timeout(void)
{
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open(
            SETTINGS_NVS_NAMESPACE,
            NVS_READWRITE,
            &handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not open sleep-timeout storage: %s",
            esp_err_to_name(err));
        return;
    }

    err = nvs_set_u8(
        handle,
        SETTINGS_NVS_SLEEP_KEY,
        s_sleep_timeout_minutes);

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not save sleep timeout: %s",
            esp_err_to_name(err));
        return;
    }

    ESP_LOGI(
        TAG,
        "Saved sleep timeout: %s",
        settings_sleep_timeout_text());
}


void ui_settings_module_init(void)
{
    uint8_t saved_brightness = 100;
    uint8_t saved_sleep_timeout = 0;
    nvs_handle_t handle;

    esp_err_t open_err =
        nvs_open(
            SETTINGS_NVS_NAMESPACE,
            NVS_READONLY,
            &handle);

    esp_err_t brightness_err = open_err;
    esp_err_t sleep_err = open_err;

    if (open_err == ESP_OK) {
        brightness_err =
            nvs_get_u8(
                handle,
                SETTINGS_NVS_BRIGHTNESS_KEY,
                &saved_brightness);

        sleep_err =
            nvs_get_u8(
                handle,
                SETTINGS_NVS_SLEEP_KEY,
                &saved_sleep_timeout);

        nvs_close(handle);
    }

    if (brightness_err == ESP_OK &&
        saved_brightness >= 10 &&
        saved_brightness <= 100) {
        s_display_brightness_percent =
            saved_brightness;

        s_saved_display_brightness_percent =
            saved_brightness;

        ESP_LOGI(
            TAG,
            "Loaded display brightness: %d%%",
            s_display_brightness_percent);
    } else {
        s_display_brightness_percent = 100;
        s_saved_display_brightness_percent = 100;

        if (brightness_err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(
                TAG,
                "Using default display brightness: %s",
                esp_err_to_name(brightness_err));
        }
    }

    if (sleep_err == ESP_OK &&
        (saved_sleep_timeout == 0 ||
         saved_sleep_timeout == 5 ||
         saved_sleep_timeout == 15 ||
         saved_sleep_timeout == 30)) {
        s_sleep_timeout_minutes =
            saved_sleep_timeout;
    } else {
        s_sleep_timeout_minutes = 0;

        if (sleep_err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(
                TAG,
                "Using default sleep timeout: %s",
                esp_err_to_name(sleep_err));
        }
    }

    esp_err_t err =
        bsp_display_brightness_set(
            s_display_brightness_percent);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Initial brightness update failed: %s",
            esp_err_to_name(err));
    }

    if (!s_sleep_timer) {
        s_sleep_timer =
            lv_timer_create(
                settings_sleep_timer_cb,
                1000,
                NULL);
    }

    ESP_LOGI(
        TAG,
        "Loaded sleep timeout: %s",
        settings_sleep_timeout_text());
}

static lv_obj_t *settings_make_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text ? text : "--");
    ui_apply_custom_label_style(label, font, color);

    return label;
}

void ui_settings_show_page(
    const char *sd_card_text,
    const char *storage_text,
    lv_event_cb_t ota_cb,
    lv_event_cb_t network_cb,
    ui_settings_theme_rebuild_cb_t theme_rebuild_cb)
{
    s_theme_rebuild_cb = theme_rebuild_cb;

    const ui_settings_layout_profile_t *layout =
        &ui_page_layout_profile_current()->settings;

    if (settings_panel) {
        lv_obj_move_foreground(settings_panel);
        return;
    }

    /*
     * esp_app_desc_t describes the OTA image that is currently running.
     * This avoids stale manually maintained firmware/build strings.
     */
    const esp_app_desc_t *app =
        esp_app_get_description();

    const char *firmware_version =
        app && app->version[0]
            ? app->version
            : "--";

    char image_build[48];

    if (app && app->date[0] && app->time[0]) {
        snprintf(
            image_build,
            sizeof(image_build),
            "%s  %s",
            app->date,
            app->time);
    } else {
        snprintf(
            image_build,
            sizeof(image_build),
            "--");
    }

    settings_panel = lv_obj_create(lv_screen_active());

    lv_obj_set_size(settings_panel,
                    UI_PAGE_ROOT_WIDTH,
                    UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(settings_panel,
                   UI_PAGE_ROOT_X,
                   UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(settings_panel, UI_SURFACE_PAGE_DEEP);

    /*
     * Operator banner
     */
    lv_obj_t *banner = lv_obj_create(settings_panel);

    lv_obj_set_size(
        banner,
        layout->banner.width,
        layout->banner.height);
    lv_obj_set_pos(
        banner,
        layout->banner.x,
        layout->banner.y);
    lv_obj_clear_flag(banner, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(banner, UI_SURFACE_SECTION);

    lv_obj_t *banner_title = settings_make_label(
        banner,
        "SETTINGS",
        &lv_font_montserrat_16,
        UI_TEXT_BRIGHT);

    lv_obj_set_pos(banner_title, 20, 17);

    lv_obj_t *banner_subtitle = settings_make_label(
        banner,
        layout->subtitle,
        &lv_font_montserrat_14,
        UI_TEXT_DIM);

    lv_obj_set_pos(banner_subtitle, 20, 48);

    lv_obj_t *status = lv_obj_create(banner);

    lv_obj_set_size(status, 142, 34);
    lv_obj_align(status, LV_ALIGN_RIGHT_MID, -18, 0);
    lv_obj_clear_flag(status, LV_OBJ_FLAG_SCROLLABLE);

    ui_apply_surface_role(status, UI_SURFACE_STATUS_PILL);

    lv_obj_t *status_label = settings_make_label(
        status,
        "SYSTEM READY",
        &lv_font_montserrat_14,
        UI_TEXT_BRIGHT);

    lv_obj_center(status_label);

    /*
     * Scrollable Settings body
     */
    lv_obj_t *content = lv_obj_create(settings_panel);

    lv_obj_set_size(
        content,
        layout->content.width,
        layout->content.height);
    lv_obj_set_pos(
        content,
        layout->content.x,
        layout->content.y);

    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_AUTO);

    ui_apply_surface_role(content, UI_SURFACE_TRANSPARENT);
    /*
     * Sections align exactly with the shared 800px content rail.
     */
    lv_obj_set_style_pad_left(content, 0, 0);
    lv_obj_set_style_pad_right(content, 0, 0);
    lv_obj_set_style_pad_top(content, 0, 0);
    lv_obj_set_style_pad_bottom(content, 20, 0);

    const int row_height = ui_theme_density_metric(54, 64, 72);
    const int action_height = ui_theme_density_metric(60, 70, 80);
    const int section_gap = ui_theme_density_metric(10, 14, 18);
    const int first_row_y = 48;
    int section_y = 0;

    /*
     * Connections
     */
    const int connections_height =
        first_row_y + action_height + 6;

    lv_obj_t *connections = ui_settings_section_create(
        content,
        "CONNECTIONS",
        section_y,
        connections_height);

    ui_settings_section_add_action_row(
        connections,
        "Network & Moonraker",
        "Wi-Fi, active printer connection and discovery settings",
        "OPEN NETWORK",
        first_row_y,
        network_cb,
        false);

    section_y += connections_height + section_gap;

    /*
     * Firmware and updates
     */
    const int firmware_row_1 = first_row_y;
    const int firmware_row_2 = firmware_row_1 + row_height + 1;
    const int firmware_row_3 = firmware_row_2 + row_height + 1;
    const int firmware_height = firmware_row_3 + action_height + 6;
    lv_obj_t *firmware = ui_settings_section_create(
        content,
        "FIRMWARE & UPDATES",
        section_y,
        firmware_height);

    ui_settings_section_add_row(
        firmware,
        "Firmware Version",
        "Version embedded in the running OTA image",
        firmware_version,
        firmware_row_1,
        NULL);

    ui_settings_section_add_divider(
        firmware, firmware_row_1 + row_height);

    ui_settings_section_add_row(
        firmware,
        "Image Build",
        "Compilation date and time of the running image",
        image_build,
        firmware_row_2,
        NULL);

    ui_settings_section_add_divider(
        firmware, firmware_row_2 + row_height);

    ui_settings_section_add_action_row(
        firmware,
        "Firmware Update",
        "Check for and install an OTA update",
        "OTA UPDATE",
        firmware_row_3,
        ota_cb,
        false);

    section_y += firmware_height + section_gap;

    /*
     * Device
     */
    const int device_row_1 = first_row_y;
    const int device_row_2 = device_row_1 + action_height + 1;
    const int device_row_3 = device_row_2 + row_height + 1;
    const int device_height = device_row_3 + action_height + 6;
    lv_obj_t *device = ui_settings_section_create(
        content,
        "DEVICE",
        section_y,
        device_height);

    ui_settings_section_add_action_row(
        device,
        "Reboot Controller",
        "Restart the ESP32-P4 controller",
        "REBOOT",
        device_row_1,
        settings_reboot_cb,
        false);

    ui_settings_section_add_divider(
        device, device_row_1 + action_height);

    settings_sleep_label = ui_settings_section_add_row(
        device,
        "Sleep Timeout",
        "Tap to change display sleep behavior",
        settings_sleep_timeout_text(),
        device_row_2,
        settings_sleep_card_cb);

    ui_settings_section_add_divider(
        device, device_row_2 + row_height);

    ui_settings_section_add_action_row(
        device,
        "Factory Reset",
        "Erase saved connections and preferences",
        "FACTORY RESET",
        device_row_3,
        reset_settings_cb,
        true);

    section_y += device_height + section_gap;

    /*
     * Time and region
     */
    const int time_height = first_row_y + row_height + 6;
    lv_obj_t *time_region = ui_settings_section_create(
        content,
        "TIME & REGION",
        section_y,
        time_height);

    s_timezone_label = ui_settings_section_add_row(
        time_region,
        "Time Zone",
        "Tap to change local time and daylight-saving rules",
        timezone_config_selected_label(),
        first_row_y,
        settings_timezone_card_cb);

    section_y += time_height + section_gap;

    /*
     * System information
     */
    const int system_row_1 = first_row_y;
    const int system_row_2 = system_row_1 + row_height + 1;
    const int system_row_3 = system_row_2 + row_height + 1;
    const int system_row_4 = system_row_3 + row_height + 1;
    const int system_height = system_row_4 + row_height + 6;
    lv_obj_t *system = ui_settings_section_create(
        content,
        "SYSTEM INFORMATION",
        section_y,
        system_height);

    settings_system_info_bind_idf_label(
        ui_settings_section_add_row(
            system,
            "ESP-IDF Version",
            "Framework used to build the running image",
            app && app->idf_ver[0]
                ? app->idf_ver
                : settings_system_info_idf_version(),
            system_row_1,
            NULL));

    ui_settings_section_add_divider(
        system, system_row_1 + row_height);

    settings_system_info_bind_heap_label(
        ui_settings_section_add_row(
            system,
            "Free Heap",
            "Available internal memory",
            "--",
            system_row_2,
            NULL));

    ui_settings_section_add_divider(
        system, system_row_2 + row_height);

    settings_system_info_bind_psram_label(
        ui_settings_section_add_row(
            system,
            "Free PSRAM",
            "Available external memory",
            "--",
            system_row_3,
            NULL));

    ui_settings_section_add_divider(
        system, system_row_3 + row_height);

    settings_system_info_bind_uptime_label(
        ui_settings_section_add_row(
            system,
            "Uptime",
            "Time since controller startup",
            "--",
            system_row_4,
            NULL));

    section_y += system_height + section_gap;

    /*
     * Storage
     */
    const int storage_row_1 = first_row_y;
    const int storage_row_2 = storage_row_1 + row_height + 1;
    const int storage_row_3 = storage_row_2 + row_height + 1;
    const int storage_height =
        storage_row_3 + action_height + 6;
    lv_obj_t *storage = ui_settings_section_create(
        content,
        "STORAGE",
        section_y,
        storage_height);

    ui_settings_section_add_row(
        storage,
        "SD Card",
        "Removable storage status",
        sd_card_text ? sd_card_text : "--",
        storage_row_1,
        NULL);

    ui_settings_section_add_divider(
        storage, storage_row_1 + row_height);

    ui_settings_section_add_row(
        storage,
        "Storage Capacity",
        NULL,
        storage_text ? storage_text : "--",
        storage_row_2,
        NULL);

    ui_settings_section_add_divider(
        storage,
        storage_row_2 + row_height);

    ui_settings_section_add_action_row(
        storage,
        "Configuration Backup",
        "Back up or restore profiles and interface settings",
        "BACKUP / RESTORE",
        storage_row_3,
        settings_backup_cb,
        false);

    section_y += storage_height + section_gap;

    /*
     * Operator history
     */
    const int operator_height =
        first_row_y + action_height + 6;

    lv_obj_t *operator_section =
        ui_settings_section_create(
            content,
            "OPERATOR",
            section_y,
            operator_height);

    ui_settings_section_add_action_row(
        operator_section,
        "Event History",
        "Recent connection, firmware, and machine events",
        "VIEW HISTORY",
        first_row_y,
        settings_event_history_cb,
        false);

    section_y += operator_height + section_gap;

    /*
     * Display
     */
    const int display_row_1 = first_row_y;
    const int display_row_2 = display_row_1 + row_height + 1;
    const int display_row_3 = display_row_2 + row_height + 1;
    const int display_row_4 = display_row_3 + row_height + 1;
    const int display_row_5 = display_row_4 + row_height + 1;
    const int display_row_6 = display_row_5 + row_height + 1;
    const int display_height = display_row_6 + action_height + 6;
    lv_obj_t *display = ui_settings_section_create(
        content,
        "DISPLAY",
        section_y,
        display_height);

    lv_obj_t *brightness_slider =
        ui_settings_section_add_percent_slider_row(
            display,
            "Brightness",
            "Display backlight level",
            s_display_brightness_percent,
            10,
            100,
            display_row_1,
            settings_brightness_changed_cb);

    if (brightness_slider) {
        lv_obj_add_event_cb(
            brightness_slider,
            settings_brightness_save_cb,
            LV_EVENT_RELEASED,
            NULL);
    }

    ui_settings_section_add_divider(
        display, display_row_1 + row_height);

    s_theme_label = ui_settings_section_add_row(
        display,
        "Theme",
        "Tap to change the operator interface appearance",
        theme_manager_active_label(),
        display_row_2,
        settings_theme_card_cb);

    ui_settings_section_add_divider(
        display, display_row_2 + row_height);

    ui_settings_section_add_row(
        display,
        "Accent Color",
        "Override the theme accent while preserving semantic colors",
        theme_manager_accent_label(),
        display_row_3,
        settings_accent_card_cb);

    ui_settings_section_add_divider(
        display, display_row_3 + row_height);

    ui_settings_section_add_row(
        display,
        "Display Density",
        "Choose compact, comfortable, or spacious type and spacing",
        theme_manager_density_label(),
        display_row_4,
        settings_density_card_cb);

    ui_settings_section_add_divider(
        display, display_row_4 + row_height);

    ui_settings_section_add_row(
        display,
        "Accessibility",
        "Text, contrast, transparency, and motion preferences",
        theme_manager_accessibility_label(),
        display_row_5,
        settings_accessibility_card_cb);

    ui_settings_section_add_divider(
        display, display_row_5 + row_height);

    ui_settings_section_add_action_row(
        display,
        "Theme Laboratory",
        "Inspect shared components and semantic states",
        "OPEN LAB",
        display_row_6,
        settings_theme_lab_cb,
        false);

    ui_settings_refresh();
}

lv_obj_t *settings_panel = NULL;
lv_obj_t *settings_sleep_label = NULL;



void ui_settings_refresh(void)
{
    if (!settings_panel) {
        return;
    }

    settings_system_info_refresh();
}

int ui_settings_brightness_percent(void)
{
    return s_display_brightness_percent;
}


uint8_t ui_settings_sleep_timeout_minutes(void)
{
    return s_sleep_timeout_minutes;
}


bool ui_settings_restore_display_preferences(
    int brightness_percent,
    uint8_t sleep_timeout_minutes)
{
    if (brightness_percent < 10 ||
        brightness_percent > 100 ||
        (sleep_timeout_minutes != 0 &&
         sleep_timeout_minutes != 5 &&
         sleep_timeout_minutes != 15 &&
         sleep_timeout_minutes != 30)) {
        return false;
    }

    nvs_handle_t handle;

    esp_err_t error = nvs_open(
        SETTINGS_NVS_NAMESPACE,
        NVS_READWRITE,
        &handle);

    if (error != ESP_OK) {
        return false;
    }

    error = nvs_set_u8(
        handle,
        SETTINGS_NVS_BRIGHTNESS_KEY,
        (uint8_t)brightness_percent);

    if (error == ESP_OK) {
        error = nvs_set_u8(
            handle,
            SETTINGS_NVS_SLEEP_KEY,
            sleep_timeout_minutes);
    }

    if (error == ESP_OK) {
        error = nvs_commit(handle);
    }

    nvs_close(handle);

    if (error != ESP_OK) {
        return false;
    }

    s_display_brightness_percent =
        brightness_percent;

    s_saved_display_brightness_percent =
        brightness_percent;

    s_sleep_timeout_minutes =
        sleep_timeout_minutes;

    (void)bsp_display_brightness_set(
        brightness_percent);

    if (settings_sleep_label) {
        lv_label_set_text(
            settings_sleep_label,
            settings_sleep_timeout_text());
    }

    return true;
}


void hide_settings_tab(void)
{
    if (settings_panel) {
        lv_obj_delete(settings_panel);
        settings_panel = NULL;

        settings_sleep_label = NULL;
        s_timezone_label = NULL;
        s_theme_label = NULL;
        s_theme_rebuild_cb = NULL;
        settings_system_info_unbind();
        ui_appearance_popups_close_all();
        ui_theme_lab_close();
        ui_settings_popups_close_all();
        ui_settings_backup_popup_close();
    }


    ui_shell_raise();
}


void settings_reboot_cb(lv_event_t *e)
{
    (void)e;
    esp_restart();
}


void settings_sleep_card_cb(lv_event_t *e)
{
    (void)e;

    switch (s_sleep_timeout_minutes) {
        case 0:
            s_sleep_timeout_minutes = 5;
            break;

        case 5:
            s_sleep_timeout_minutes = 15;
            break;

        case 15:
            s_sleep_timeout_minutes = 30;
            break;

        case 30:
        default:
            s_sleep_timeout_minutes = 0;
            break;
    }

    lv_display_trigger_activity(
        lv_display_get_default());

    if (settings_sleep_label) {
        lv_label_set_text(
            settings_sleep_label,
            settings_sleep_timeout_text());
    }

    settings_save_sleep_timeout();
}
