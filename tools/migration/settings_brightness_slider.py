from pathlib import Path

header = Path("main/ui_settings_components.h")
components = Path("main/ui_settings_components.c")
settings = Path("main/ui_settings.c")

header_text = header.read_text()
components_text = components.read_text()
settings_text = settings.read_text()

header_anchor = """lv_obj_t *ui_settings_section_add_action_row(
"""

header_insert = """lv_obj_t *ui_settings_section_add_percent_slider_row(
    lv_obj_t *section,
    const char *title,
    const char *description,
    int value,
    int minimum,
    int maximum,
    int y,
    lv_event_cb_t event_cb);

"""

components_anchor = """lv_obj_t *ui_settings_section_add_action_row(
"""

components_insert = r'''lv_obj_t *ui_settings_section_add_percent_slider_row(
    lv_obj_t *section,
    const char *title,
    const char *description,
    int value,
    int minimum,
    int maximum,
    int y,
    lv_event_cb_t event_cb)
{
    lv_obj_t *row = lv_obj_create(section);

    lv_obj_set_size(row, 746, 64);
    lv_obj_set_pos(row, 18, y);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    lv_obj_t *title_label = settings_component_make_label(
        row,
        title,
        &lv_font_montserrat_16,
        UI_TEXT_BRIGHT);

    lv_obj_set_pos(title_label, 0, description ? 8 : 20);

    if (description && description[0]) {
        lv_obj_t *description_label = settings_component_make_label(
            row,
            description,
            &lv_font_montserrat_14,
            UI_TEXT_DIM);

        lv_obj_set_pos(description_label, 0, 34);
    }

    lv_obj_t *value_label = settings_component_make_label(
        row,
        "",
        &lv_font_montserrat_14,
        UI_TEXT_BRIGHT);

    char value_text[16];
    lv_snprintf(value_text, sizeof(value_text), "%d%%", value);
    lv_label_set_text(value_label, value_text);

    lv_obj_set_width(value_label, 64);
    lv_obj_set_style_text_align(
        value_label,
        LV_TEXT_ALIGN_RIGHT,
        0);
    lv_obj_align(
        value_label,
        LV_ALIGN_RIGHT_MID,
        -4,
        0);

    lv_obj_t *slider = lv_slider_create(row);

    lv_obj_set_size(slider, 270, 12);
    lv_obj_align(
        slider,
        LV_ALIGN_RIGHT_MID,
        -86,
        0);

    lv_slider_set_range(
        slider,
        minimum,
        maximum);

    lv_slider_set_value(
        slider,
        value,
        LV_ANIM_OFF);

    lv_obj_set_style_radius(
        slider,
        LV_RADIUS_CIRCLE,
        LV_PART_MAIN);

    lv_obj_set_style_bg_color(
        slider,
        UI_BG_DEEP,
        LV_PART_MAIN);

    lv_obj_set_style_bg_opa(
        slider,
        LV_OPA_COVER,
        LV_PART_MAIN);

    lv_obj_set_style_border_color(
        slider,
        UI_BORDER,
        LV_PART_MAIN);

    lv_obj_set_style_border_width(
        slider,
        1,
        LV_PART_MAIN);

    lv_obj_set_style_radius(
        slider,
        LV_RADIUS_CIRCLE,
        LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(
        slider,
        UI_ACCENT_CYAN,
        LV_PART_INDICATOR);

    lv_obj_set_style_bg_opa(
        slider,
        LV_OPA_COVER,
        LV_PART_INDICATOR);

    lv_obj_set_style_radius(
        slider,
        LV_RADIUS_CIRCLE,
        LV_PART_KNOB);

    lv_obj_set_style_bg_color(
        slider,
        UI_TEXT_BRIGHT,
        LV_PART_KNOB);

    lv_obj_set_style_bg_opa(
        slider,
        LV_OPA_COVER,
        LV_PART_KNOB);

    lv_obj_set_style_border_color(
        slider,
        UI_ACCENT_CYAN,
        LV_PART_KNOB);

    lv_obj_set_style_border_width(
        slider,
        2,
        LV_PART_KNOB);

    lv_obj_set_style_pad_all(
        slider,
        6,
        LV_PART_KNOB);

    lv_obj_set_user_data(slider, value_label);

    if (event_cb) {
        lv_obj_add_event_cb(
            slider,
            event_cb,
            LV_EVENT_VALUE_CHANGED,
            NULL);
    }

    return slider;
}


'''

include_anchor = """#include "settings_system_info.h"
"""

include_replacement = """#include "settings_system_info.h"

#include "bsp/display.h"
#include "esp_err.h"
"""

function_anchor = """void ui_settings_module_init(void)
"""

function_insert = r'''static int s_display_brightness_percent = 100;

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


'''

old_brightness = """    ui_settings_section_add_row(
        display,
        "Brightness",
        "Display backlight level",
        "100%",
        48,
        NULL);
"""

new_brightness = """    ui_settings_section_add_percent_slider_row(
        display,
        "Brightness",
        "Display backlight level",
        s_display_brightness_percent,
        10,
        100,
        48,
        settings_brightness_changed_cb);
"""

checks = [
    (header_text, header_anchor, "settings component header anchor"),
    (components_text, components_anchor, "settings component source anchor"),
    (settings_text, include_anchor, "Settings include anchor"),
    (settings_text, function_anchor, "Settings function anchor"),
    (settings_text, old_brightness, "Brightness placeholder"),
]

for text, anchor, description in checks:
    count = text.count(anchor)
    if count != 1:
        raise RuntimeError(
            f"expected one {description}, found {count}")

header_text = header_text.replace(
    header_anchor,
    header_insert + header_anchor,
    1)

components_text = components_text.replace(
    components_anchor,
    components_insert + components_anchor,
    1)

settings_text = settings_text.replace(
    include_anchor,
    include_replacement,
    1)

settings_text = settings_text.replace(
    function_anchor,
    function_insert + function_anchor,
    1)

settings_text = settings_text.replace(
    old_brightness,
    new_brightness,
    1)

header.write_text(header_text)
components.write_text(components_text)
settings.write_text(settings_text)

print("Installed live Settings brightness slider.")
print("Range: 10–100%")
print("Hardware: bsp_display_brightness_set()")
