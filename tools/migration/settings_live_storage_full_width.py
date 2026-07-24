from pathlib import Path

settings_path = Path("main/ui_settings.c")
components_path = Path("main/ui_settings_components.c")
main_path = Path("main/main.c")

settings = settings_path.read_text()
components = components_path.read_text()
main = main_path.read_text()

# ------------------------------------------------------------
# Make the scrollable content use the banner's full width.
# ------------------------------------------------------------

old_content_padding = """    lv_obj_set_style_pad_left(content, 12, 0);
    lv_obj_set_style_pad_right(content, 12, 0);
"""

new_content_padding = """    /*
     * Sections align exactly with the 806px Settings banner.
     */
    lv_obj_set_style_pad_left(content, 0, 0);
    lv_obj_set_style_pad_right(content, 0, 0);
"""

if settings.count(old_content_padding) != 1:
    raise RuntimeError(
        "could not locate Settings content padding")

settings = settings.replace(
    old_content_padding,
    new_content_padding,
    1)

# ------------------------------------------------------------
# Widen sections and their internal rows/dividers by 24px.
# ------------------------------------------------------------

section_width_count = components.count(
    "lv_obj_set_size(section, 782, h);")

if section_width_count != 1:
    raise RuntimeError(
        f"expected one section width, found {section_width_count}")

components = components.replace(
    "lv_obj_set_size(section, 782, h);",
    "lv_obj_set_size(section, 806, h);",
    1)

inner_width_count = components.count("746")

if inner_width_count != 5:
    raise RuntimeError(
        f"expected five 746px internal widths, found {inner_width_count}")

components = components.replace("746", "770")

# ------------------------------------------------------------
# Replace the hard-coded SD capacity with live FAT statistics.
# ------------------------------------------------------------

show_function_anchor = """static void show_settings_tab(void)
{
"""

storage_helper = r'''static void settings_format_storage_text(
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


'''

if main.count(show_function_anchor) != 1:
    raise RuntimeError(
        "could not locate show_settings_tab()")

main = main.replace(
    show_function_anchor,
    storage_helper + show_function_anchor,
    1)

old_show_function = """static void show_settings_tab(void)
{
    ui_settings_show_page(
        sd_card_ok ? "Mounted" : "Not mounted",
        sd_card_ok ? "30.4 GB SDHC" : "Not mounted",
        ota_open_popup_cb);
}
"""

new_show_function = """static void show_settings_tab(void)
{
    char storage_text[96];

    settings_format_storage_text(
        storage_text,
        sizeof(storage_text));

    ui_settings_show_page(
        sd_card_ok ? "Mounted" : "Not mounted",
        storage_text,
        ota_open_popup_cb);
}
"""

if main.count(old_show_function) != 1:
    raise RuntimeError(
        "could not locate current Settings call")

main = main.replace(
    old_show_function,
    new_show_function,
    1)

settings_path.write_text(settings)
components_path.write_text(components)
main_path.write_text(main)

print("Settings sections now match the 806px banner width.")
print("Storage capacity now comes from /sdcard FAT statistics.")
