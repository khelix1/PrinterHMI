from pathlib import Path

settings_path = Path("main/ui_settings.c")
main_path = Path("main/main.c")

settings = settings_path.read_text()
main = main_path.read_text()

old_defines = """#define SETTINGS_NVS_NAMESPACE "display"
#define SETTINGS_NVS_BRIGHTNESS_KEY "brightness"
"""

new_defines = """#define SETTINGS_NVS_NAMESPACE "display"
#define SETTINGS_NVS_BRIGHTNESS_KEY "brightness"
#define SETTINGS_NVS_SLEEP_KEY "sleep_min"

static uint8_t s_sleep_timeout_minutes = 0;
static lv_timer_t *s_sleep_timer = NULL;
static lv_obj_t *s_sleep_wake_overlay = NULL;
static bool s_display_sleeping = false;
"""

init_anchor = """void ui_settings_module_init(void)
"""

sleep_runtime = r'''static const char *settings_sleep_timeout_text(void)
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

    lv_obj_set_style_radius(
        s_sleep_wake_overlay,
        0,
        0);

    lv_obj_set_style_bg_opa(
        s_sleep_wake_overlay,
        LV_OPA_TRANSP,
        0);

    lv_obj_set_style_border_width(
        s_sleep_wake_overlay,
        0,
        0);

    lv_obj_set_style_pad_all(
        s_sleep_wake_overlay,
        0,
        0);

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


'''

old_init = """void ui_settings_module_init(void)
{
    uint8_t saved_brightness = 100;
    nvs_handle_t handle;

    esp_err_t err =
        nvs_open(
            SETTINGS_NVS_NAMESPACE,
            NVS_READONLY,
            &handle);

    if (err == ESP_OK) {
        err = nvs_get_u8(
            handle,
            SETTINGS_NVS_BRIGHTNESS_KEY,
            &saved_brightness);

        nvs_close(handle);
    }

    if (err == ESP_OK &&
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

        if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(
                TAG,
                "Using default display brightness: %s",
                esp_err_to_name(err));
        }
    }

    err = bsp_display_brightness_set(
        s_display_brightness_percent);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Initial brightness update failed: %s",
            esp_err_to_name(err));
    }
}
"""

new_init = """void ui_settings_module_init(void)
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
"""

old_row_value = """"Sleep Timeout",
        "Tap to change display sleep behavior",
        "OFF",
"""

new_row_value = """"Sleep Timeout",
        "Tap to change display sleep behavior",
        settings_sleep_timeout_text(),
"""

old_callback = """void settings_sleep_card_cb(lv_event_t *e)
{
    (void)e;

    static int sleep_mode = 0;

    sleep_mode = (sleep_mode + 1) % 4;

    const char *txt = "OFF";

    if (sleep_mode == 1) txt = "5 MIN";
    else if (sleep_mode == 2) txt = "15 MIN";
    else if (sleep_mode == 3) txt = "30 MIN";

    ESP_LOGI(TAG, "SLEEP: %s", txt);

    if (settings_sleep_label) {
        lv_label_set_text(settings_sleep_label, txt);
    }
}
"""

new_callback = """void settings_sleep_card_cb(lv_event_t *e)
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
"""

old_main_init = """    /*
     * The Settings module loads and applies the saved display brightness
     * after the BSP has initialized the backlight PWM.
     */
    ui_settings_module_init();
"""

new_main_init = """    /*
     * Load persistent display settings after the BSP initializes
     * backlight PWM. The lock also protects LVGL timer creation.
     */
    bsp_display_lock(0);
    ui_settings_module_init();
    bsp_display_unlock();
"""

checks = [
    (settings, old_defines, "Settings NVS definitions"),
    (settings, init_anchor, "Settings initialization anchor"),
    (settings, old_init, "brightness initialization function"),
    (settings, old_row_value, "Sleep Timeout row"),
    (settings, old_callback, "placeholder sleep callback"),
    (main, old_main_init, "Settings startup call"),
]

for text, anchor, description in checks:
    count = text.count(anchor)
    if count != 1:
        raise RuntimeError(
            f"expected one {description}, found {count}")

settings = settings.replace(
    old_defines,
    new_defines,
    1)

settings = settings.replace(
    init_anchor,
    sleep_runtime + init_anchor,
    1)

settings = settings.replace(
    old_init,
    new_init,
    1)

settings = settings.replace(
    old_row_value,
    new_row_value,
    1)

settings = settings.replace(
    old_callback,
    new_callback,
    1)

main = main.replace(
    old_main_init,
    new_main_init,
    1)

settings_path.write_text(settings)
main_path.write_text(main)

print("Installed persistent display sleep timeout.")
print("Modes: OFF, 5 MIN, 15 MIN, 30 MIN")
print("First touch wakes without activating the underlying control.")
