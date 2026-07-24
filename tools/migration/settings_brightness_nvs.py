from pathlib import Path

settings_path = Path("main/ui_settings.c")
main_path = Path("main/main.c")

settings = settings_path.read_text()
main = main_path.read_text()

include_anchor = """#include "esp_err.h"
"""

include_replacement = """#include "esp_err.h"
#include "nvs.h"
"""

state_anchor = """static int s_display_brightness_percent = 100;
"""

state_replacement = """static int s_display_brightness_percent = 100;
static int s_saved_display_brightness_percent = 100;

#define SETTINGS_NVS_NAMESPACE "display"
#define SETTINGS_NVS_BRIGHTNESS_KEY "brightness"
"""

old_init = """void ui_settings_module_init(void)
{
    /* Settings page state is initialized statically. */
}
"""

new_init = """void ui_settings_module_init(void)
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

function_anchor = """void ui_settings_module_init(void)
"""

save_function = """static void settings_brightness_save_cb(lv_event_t *e)
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


"""

old_slider = """    ui_settings_section_add_percent_slider_row(
        display,
        "Brightness",
        "Display backlight level",
        s_display_brightness_percent,
        10,
        100,
        48,
        settings_brightness_changed_cb);
"""

new_slider = """    lv_obj_t *brightness_slider =
        ui_settings_section_add_percent_slider_row(
            display,
            "Brightness",
            "Display backlight level",
            s_display_brightness_percent,
            10,
            100,
            48,
            settings_brightness_changed_cb);

    if (brightness_slider) {
        lv_obj_add_event_cb(
            brightness_slider,
            settings_brightness_save_cb,
            LV_EVENT_RELEASED,
            NULL);
    }
"""

main_include_anchor = """#include "ui_ota_popup.h"
"""

main_include_replacement = """#include "ui_ota_popup.h"
#include "ui_settings.h"
"""

old_startup = """    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
"""

new_startup = """    bsp_display_start_with_config(&cfg);

    /*
     * The Settings module loads and applies the saved display brightness
     * after the BSP has initialized the backlight PWM.
     */
    ui_settings_module_init();
"""

checks = [
    (settings, include_anchor, "NVS include anchor"),
    (settings, state_anchor, "brightness state"),
    (settings, old_init, "Settings initialization function"),
    (settings, old_slider, "brightness slider construction"),
    (main, old_startup, "display startup sequence"),
]

for text, anchor, description in checks:
    count = text.count(anchor)
    if count != 1:
        raise RuntimeError(
            f"expected one {description}, found {count}")

settings = settings.replace(
    include_anchor,
    include_replacement,
    1)

settings = settings.replace(
    state_anchor,
    state_replacement,
    1)

settings = settings.replace(
    old_init,
    new_init,
    1)

settings = settings.replace(
    function_anchor,
    save_function + function_anchor,
    1)

settings = settings.replace(
    old_slider,
    new_slider,
    1)

if '#include "ui_settings.h"' not in main:
    count = main.count(main_include_anchor)

    if count != 1:
        raise RuntimeError(
            f"expected one main include anchor, found {count}")

    main = main.replace(
        main_include_anchor,
        main_include_replacement,
        1)

main = main.replace(
    old_startup,
    new_startup,
    1)

settings_path.write_text(settings)
main_path.write_text(main)

print("Installed persistent display brightness.")
print("NVS namespace: display")
print("NVS key: brightness")
print("Writes occur only when the slider is released.")
