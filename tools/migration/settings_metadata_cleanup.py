from pathlib import Path
import re

cmake_path = Path("CMakeLists.txt")
header_path = Path("main/ui_settings.h")
settings_path = Path("main/ui_settings.c")
main_path = Path("main/main.c")

cmake = cmake_path.read_text()
header = header_path.read_text()
settings = settings_path.read_text()
main = main_path.read_text()

# ------------------------------------------------------------
# Embed an actual semantic firmware version in esp_app_desc_t.
# ------------------------------------------------------------

old_project = """project(PrinterHMI_v3_2)
"""

new_project = """set(PROJECT_VER "3.2.0")
project(PrinterHMI_v3_2)
"""

if "set(PROJECT_VER " not in cmake:
    if cmake.count(old_project) != 1:
        raise RuntimeError("could not locate project() declaration")

    cmake = cmake.replace(
        old_project,
        new_project,
        1)

# ------------------------------------------------------------
# Simplify the Settings public API.
# Settings now reads its own running-image metadata.
# ------------------------------------------------------------

old_header_signature = """void ui_settings_show_page(
    const char *firmware_text,
    const char *build_text,
    const char *compiled_text,
    const char *sd_card_text,
    const char *storage_text,
    ui_settings_make_info_cb_t make_info_cb,
    lv_event_cb_t ota_cb,
    lv_event_cb_t dashboard_cb);
"""

new_header_signature = """void ui_settings_show_page(
    const char *sd_card_text,
    const char *storage_text,
    lv_event_cb_t ota_cb);
"""

if header.count(old_header_signature) != 1:
    raise RuntimeError(
        "could not locate Settings header signature")

header = header.replace(
    old_header_signature,
    new_header_signature,
    1)

old_source_signature = """void ui_settings_show_page(
    const char *firmware_text,
    const char *build_text,
    const char *compiled_text,
    const char *sd_card_text,
    const char *storage_text,
    ui_settings_make_info_cb_t make_info_cb,
    lv_event_cb_t ota_cb,
    lv_event_cb_t dashboard_cb)
{
    /*
     * make_info_cb and dashboard_cb belonged to the old dashboard-style
     * Settings layout. Keep the API stable while the new Theme B owner
     * takes over construction.
     */
    (void)make_info_cb;
    (void)dashboard_cb;

"""

new_source_signature = """void ui_settings_show_page(
    const char *sd_card_text,
    const char *storage_text,
    lv_event_cb_t ota_cb)
{
"""

if settings.count(old_source_signature) != 1:
    raise RuntimeError(
        "could not locate Settings source signature")

settings = settings.replace(
    old_source_signature,
    new_source_signature,
    1)

# ------------------------------------------------------------
# Read metadata from the image that is actually running.
# ------------------------------------------------------------

include_anchor = """#include "esp_system.h"
"""

include_replacement = """#include "esp_system.h"
#include "esp_app_desc.h"

#include <stdio.h>
"""

if settings.count(include_anchor) != 1:
    raise RuntimeError(
        "could not locate Settings include anchor")

settings = settings.replace(
    include_anchor,
    include_replacement,
    1)

panel_anchor = """    if (settings_panel) {
        lv_obj_move_foreground(settings_panel);
        return;
    }

"""

metadata_block = """    if (settings_panel) {
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

"""

if settings.count(panel_anchor) != 1:
    raise RuntimeError(
        "could not locate Settings panel guard")

settings = settings.replace(
    panel_anchor,
    metadata_block,
    1)

# ------------------------------------------------------------
# Replace the duplicated section stack.
# ------------------------------------------------------------

section_pattern = re.compile(
    r"""    /\*
(?:     \* Firmware and updates
)+     \*/
.*?    ui_settings_refresh\(\);""",
    re.DOTALL)

new_sections = """    /*
     * Firmware and updates
     */
    lv_obj_t *firmware = ui_settings_section_create(
        content,
        "FIRMWARE & UPDATES",
        0,
        254);

    ui_settings_section_add_row(
        firmware,
        "Firmware Version",
        "Version embedded in the running OTA image",
        firmware_version,
        48,
        NULL);

    ui_settings_section_add_divider(firmware, 112);

    ui_settings_section_add_row(
        firmware,
        "Image Build",
        "Compilation date and time of the running image",
        image_build,
        113,
        NULL);

    ui_settings_section_add_divider(firmware, 177);

    ui_settings_section_add_action_row(
        firmware,
        "Firmware Update",
        "Check for and install an OTA update",
        "OTA UPDATE",
        178,
        ota_cb,
        false);

    /*
     * Device
     */
    lv_obj_t *device = ui_settings_section_create(
        content,
        "DEVICE",
        268,
        260);

    ui_settings_section_add_action_row(
        device,
        "Reboot Controller",
        "Restart the ESP32-P4 controller",
        "REBOOT",
        48,
        settings_reboot_cb,
        false);

    ui_settings_section_add_divider(device, 118);

    settings_sleep_label = ui_settings_section_add_row(
        device,
        "Sleep Timeout",
        "Tap to change display sleep behavior",
        settings_sleep_timeout_text(),
        119,
        settings_sleep_card_cb);

    ui_settings_section_add_divider(device, 183);

    ui_settings_section_add_action_row(
        device,
        "Factory Reset",
        "Erase saved connections and preferences",
        "FACTORY RESET",
        184,
        reset_settings_cb,
        true);

    /*
     * System information
     */
    lv_obj_t *system = ui_settings_section_create(
        content,
        "SYSTEM INFORMATION",
        542,
        326);

    settings_system_info_bind_idf_label(
        ui_settings_section_add_row(
            system,
            "ESP-IDF Version",
            "Framework used to build the running image",
            app && app->idf_ver[0]
                ? app->idf_ver
                : settings_system_info_idf_version(),
            48,
            NULL));

    ui_settings_section_add_divider(system, 112);

    settings_system_info_bind_heap_label(
        ui_settings_section_add_row(
            system,
            "Free Heap",
            "Available internal memory",
            "--",
            113,
            NULL));

    ui_settings_section_add_divider(system, 177);

    settings_system_info_bind_psram_label(
        ui_settings_section_add_row(
            system,
            "Free PSRAM",
            "Available external memory",
            "--",
            178,
            NULL));

    ui_settings_section_add_divider(system, 242);

    settings_system_info_bind_uptime_label(
        ui_settings_section_add_row(
            system,
            "Uptime",
            "Time since controller startup",
            "--",
            243,
            NULL));

    /*
     * Storage
     */
    lv_obj_t *storage = ui_settings_section_create(
        content,
        "STORAGE",
        882,
        180);

    ui_settings_section_add_row(
        storage,
        "SD Card",
        "Removable storage status",
        sd_card_text ? sd_card_text : "--",
        48,
        NULL);

    ui_settings_section_add_divider(storage, 112);

    ui_settings_section_add_row(
        storage,
        "Storage Capacity",
        NULL,
        storage_text ? storage_text : "--",
        113,
        NULL);

    /*
     * Display
     */
    lv_obj_t *display = ui_settings_section_create(
        content,
        "DISPLAY",
        1076,
        179);

    lv_obj_t *brightness_slider =
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

    ui_settings_section_add_divider(display, 112);

    ui_settings_section_add_row(
        display,
        "Theme",
        "Operator interface appearance",
        "OPERATOR THEME B",
        113,
        NULL);

    ui_settings_refresh();"""

settings, replacements = section_pattern.subn(
    new_sections,
    settings,
    count=1)

if replacements != 1:
    raise RuntimeError(
        f"expected one Settings section stack, found {replacements}")

# ------------------------------------------------------------
# Stop passing manually maintained firmware/build strings.
# ------------------------------------------------------------

old_show_function = """static void show_settings_tab(void)
{
    char firmware_text[96];

    snprintf(
        firmware_text,
        sizeof(firmware_text),
        "%s %s",
        FW_NAME,
        FW_VERSION);

    ui_settings_show_page(
        firmware_text,
        FW_BUILD_NAME,
        FW_BUILD_STAMP,
        sd_card_ok ? "Mounted" : "Not mounted",
        sd_card_ok ? "30.4 GB SDHC" : "Not mounted",
        make_printer_info,
        ota_open_popup_cb,
        open_v32_dashboard_cb);
}
"""

new_show_function = """static void show_settings_tab(void)
{
    ui_settings_show_page(
        sd_card_ok ? "Mounted" : "Not mounted",
        sd_card_ok ? "30.4 GB SDHC" : "Not mounted",
        ota_open_popup_cb);
}
"""

if main.count(old_show_function) != 1:
    raise RuntimeError(
        "could not locate show_settings_tab()")

main = main.replace(
    old_show_function,
    new_show_function,
    1)

cmake_path.write_text(cmake)
header_path.write_text(header)
settings_path.write_text(settings)
main_path.write_text(main)

print("Settings metadata cleanup installed.")
print("Firmware version source: running esp_app_desc_t")
print("Embedded project version: 3.2.0")
print("Removed duplicate Firmware, About, Build Date, and Theme Selection rows.")
