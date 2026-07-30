#include "ui_devices_v32.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "device_catalog_controller.h"
#include "moonraker.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include "ui_button.h"
#include "ui_page_geometry_v32.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#define DEVICE_FILTER_COUNT 8
#define DEVICE_UI_MAX_VISIBLE 48

typedef enum {
    DEVICE_FILTER_ALL = 0,
    DEVICE_FILTER_THERMAL,
    DEVICE_FILTER_AIR,
    DEVICE_FILTER_POWER,
    DEVICE_FILTER_SENSOR,
    DEVICE_FILTER_OUTPUT,
    DEVICE_FILTER_MOTION,
    DEVICE_FILTER_OTHER
} device_filter_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *banner_status;
    lv_obj_t *list;
    lv_obj_t *pagination_label;
    lv_obj_t *previous_button;
    lv_obj_t *next_button;
    lv_obj_t *filter_strip;
    lv_obj_t *filter_buttons[DEVICE_FILTER_COUNT];
    lv_obj_t *value_labels[DEVICE_UI_MAX_VISIBLE];
    size_t value_catalog_indices[DEVICE_UI_MAX_VISIBLE];
    size_t visible_count;
    lv_timer_t *refresh_timer;
    ui_devices_open_telemetry_cb_t open_telemetry;
    device_filter_t filter;
    size_t page_index;
    size_t page_count;
    uint32_t rendered_generation;
} ui_devices_state_t;

static const char TAG[] = "ui_devices";
static ui_devices_state_t *s_devices;


static bool devices_state_init(void)
{
    if (s_devices) {
        return true;
    }

    s_devices = heap_caps_calloc(
        1,
        sizeof(*s_devices),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_devices) {
        ESP_LOGI(
            TAG,
            "Devices page state allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_devices));
        return true;
    }

    s_devices = heap_caps_calloc(
        1,
        sizeof(*s_devices),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_devices) {
        ESP_LOGE(TAG, "Unable to allocate Devices page state");
        return false;
    }

    ESP_LOGW(TAG, "Devices page state using internal RAM fallback");
    return true;
}


static lv_obj_t *devices_label(
    lv_obj_t *parent,
    const char *text,
    const lv_font_t *font,
    lv_color_t color,
    int x,
    int y,
    int width)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text ? text : "--");
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(label, width);
    lv_obj_set_pos(label, x, y);
    ui_apply_custom_label_style(label, font, color);

    return label;
}


static bool filter_matches(
    device_filter_t filter,
    device_kind_t kind)
{
    if (filter == DEVICE_FILTER_ALL) {
        return true;
    }

    return kind == (device_kind_t)(filter - 1);
}


static size_t filter_count(
    const device_catalog_status_t *status,
    device_filter_t filter)
{
    if (!status) {
        return 0;
    }

    if (filter == DEVICE_FILTER_ALL) {
        return status->stored_count;
    }

    return status->kind_count[filter - 1];
}


static const char *filter_name(
    device_filter_t filter)
{
    static const char *names[DEVICE_FILTER_COUNT] = {
        "ALL",
        "HEAT",
        "AIR",
        "POWER",
        "SENSOR",
        "OUTPUT",
        "MOTION",
        "OTHER",
    };

    return filter < DEVICE_FILTER_COUNT
        ? names[filter]
        : "ALL";
}


static double percent_value(
    double value)
{
    if (value >= 0.0 && value <= 1.01) {
        return value * 100.0;
    }

    return value;
}


static bool format_known_live_value(
    const device_descriptor_t *device,
    const moonraker_state_t *state,
    const moonraker_filament_state_t *filament,
    char *output,
    size_t output_size)
{
    if (!device || !state || !output || output_size == 0) {
        return false;
    }

    for (size_t index = 0;
         index < state->hotend_count;
         ++index) {
        const moonraker_hotend_t *hotend =
            &state->hotends[index];

        if (strcmp(
                device->object_name,
                hotend->object_name) == 0) {
            lv_snprintf(
                output,
                output_size,
                "%.1f / %.1f C%s",
                hotend->temperature,
                hotend->target,
                hotend->active ? "  ACTIVE" : "");
            return true;
        }
    }

    if (strcmp(device->object_name, "heater_bed") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.1f / %.1f C",
            state->bed_temp,
            state->bed_target);
        return true;
    }

    if (strcmp(device->object_name, "fan") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.0f%%",
            percent_value(state->part_fan_speed));
        return true;
    }

    if (strcmp(
            device->object_name,
            "temperature_sensor drybox_center") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.1f C",
            state->chamber_temp);
        return true;
    }

    if (strcmp(
            device->object_name,
            "sht3x drybox_env") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.1f C  %.0f%% RH",
            state->air_temp,
            state->humidity);
        return true;
    }

    if (strcmp(
            device->object_name,
            "heater_generic drybox_heater") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%s  TARGET %.0f C",
            state->heater_on ? "ON" : "OFF",
            state->heater_target);
        return true;
    }

    if (strcmp(
            device->object_name,
            "fan_generic drybox_fan") == 0) {
        lv_snprintf(
            output,
            output_size,
            "%.0f%%",
            percent_value(state->drybox_fan_speed));
        return true;
    }

    if (strcmp(device->object_name, "toolhead") == 0 &&
        state->toolhead_position_valid) {
        lv_snprintf(
            output,
            output_size,
            "X%.1f Y%.1f Z%.2f",
            state->toolhead_x,
            state->toolhead_y,
            state->toolhead_z);
        return true;
    }

    if (filament) {
        for (size_t index = 0;
             index < filament->sensor_count;
             ++index) {
            const moonraker_filament_sensor_t *sensor =
                &filament->sensors[index];

            if (strcmp(
                    device->object_name,
                    sensor->object_name) != 0) {
                continue;
            }

            if (!sensor->enabled) {
                lv_snprintf(
                    output,
                    output_size,
                    "DISABLED");
            } else if (!sensor->status_known) {
                lv_snprintf(
                    output,
                    output_size,
                    "CHECKING");
            } else {
                lv_snprintf(
                    output,
                    output_size,
                    "%s",
                    sensor->filament_detected
                        ? "FILAMENT PRESENT"
                        : "RUNOUT");
            }

            return true;
        }
    }

    return false;
}


static void update_live_values(void)
{
    if (!s_devices || !s_devices->root) {
        return;
    }

    moonraker_state_t state;
    moonraker_filament_state_t filament;

    moonraker_state_snapshot(&state);
    moonraker_filament_state_snapshot(&filament);

    for (size_t visible = 0;
         visible < s_devices->visible_count;
         ++visible) {
        lv_obj_t *label =
            s_devices->value_labels[visible];

        if (!label) {
            continue;
        }

        device_descriptor_t device;

        if (!device_catalog_controller_get(
                s_devices->value_catalog_indices[visible],
                &device)) {
            lv_label_set_text(label, "--");
            continue;
        }

        char value[80];

        if (format_known_live_value(
                &device,
                &state,
                &filament,
                value,
                sizeof(value))) {
            lv_label_set_text(label, value);
            ui_apply_label_bright(label);
        } else if (device.live_value_valid) {
            lv_label_set_text(
                label,
                device.live_value);
            ui_apply_label_bright(label);
        } else {
            lv_label_set_text(
                label,
                device.kind == DEVICE_KIND_OTHER
                    ? "DISCOVERED"
                    : "WAITING FOR DATA");
            ui_apply_label_dim(label);
        }
    }
}


static void update_filter_buttons(
    const device_catalog_status_t *status)
{
    if (!s_devices) {
        return;
    }

    for (size_t index = 0;
         index < DEVICE_FILTER_COUNT;
         ++index) {
        lv_obj_t *button =
            s_devices->filter_buttons[index];

        if (!button) {
            continue;
        }

        ui_button_apply_kind(
            button,
            index == (size_t)s_devices->filter
                ? UI_BUTTON_PRIMARY
                : UI_BUTTON_OUTLINED);

        lv_obj_t *label =
            lv_obj_get_child(button, 0);

        if (label) {
            char text[24];

            lv_snprintf(
                text,
                sizeof(text),
                "%s %u",
                filter_name((device_filter_t)index),
                (unsigned)filter_count(
                    status,
                    (device_filter_t)index));

            lv_label_set_text(label, text);
        }
    }
}


static void add_device_card(
    const device_descriptor_t *device,
    size_t catalog_index,
    size_t visible_index)
{
    if (!s_devices || !s_devices->list || !device) {
        return;
    }

    int column = (int)(visible_index % 2);
    int row = (int)(visible_index / 2);
    int x = column == 0 ? 0 : 410;
    int y = row * 102;

    lv_obj_t *card = ui_create_operator_card(
        s_devices->list,
        x,
        y,
        390,
        90);

    if (!card) {
        return;
    }

    devices_label(
        card,
        device->display_name,
        UI_FONT_BODY_LARGE,
        UI_TEXT_BRIGHT,
        14,
        10,
        250);

    lv_obj_t *kind = devices_label(
        card,
        device_catalog_kind_label(device->kind),
        UI_FONT_CAPTION,
        UI_ACCENT_BRIGHT,
        270,
        13,
        104);

    lv_obj_set_style_text_align(
        kind,
        LV_TEXT_ALIGN_RIGHT,
        0);

    ui_create_operator_card_divider(
        card,
        14,
        40,
        362);

    devices_label(
        card,
        device->object_name,
        UI_FONT_CAPTION,
        UI_TEXT_DIM,
        14,
        55,
        205);

    lv_obj_t *value = devices_label(
        card,
        "--",
        UI_FONT_CAPTION,
        UI_TEXT_BRIGHT,
        220,
        55,
        156);

    lv_obj_set_style_text_align(
        value,
        LV_TEXT_ALIGN_RIGHT,
        0);

    if (visible_index < DEVICE_UI_MAX_VISIBLE) {
        s_devices->value_labels[visible_index] =
            value;
        s_devices->value_catalog_indices[visible_index] =
            catalog_index;

        if (visible_index + 1 >
            s_devices->visible_count) {
            s_devices->visible_count =
                visible_index + 1;
        }
    }
}


static void update_pagination_controls(
    size_t matching_count)
{
    if (!s_devices) {
        return;
    }

    size_t page_count = matching_count == 0
        ? 1
        : (matching_count + DEVICE_UI_MAX_VISIBLE - 1) /
            DEVICE_UI_MAX_VISIBLE;

    if (s_devices->page_index >= page_count) {
        s_devices->page_index = page_count - 1;
    }

    s_devices->page_count = page_count;

    bool has_previous =
        matching_count > 0 && s_devices->page_index > 0;
    bool has_next =
        matching_count > 0 &&
        s_devices->page_index + 1 < page_count;

    if (s_devices->previous_button) {
        if (has_previous) {
            lv_obj_clear_state(
                s_devices->previous_button,
                LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(
                s_devices->previous_button,
                LV_STATE_DISABLED);
        }
    }

    if (s_devices->next_button) {
        if (has_next) {
            lv_obj_clear_state(
                s_devices->next_button,
                LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(
                s_devices->next_button,
                LV_STATE_DISABLED);
        }
    }

    if (!s_devices->pagination_label) {
        return;
    }

    if (matching_count == 0) {
        lv_label_set_text(
            s_devices->pagination_label,
            "NO MATCHING DEVICES");
        return;
    }

    size_t first =
        s_devices->page_index * DEVICE_UI_MAX_VISIBLE + 1;
    size_t last =
        first + DEVICE_UI_MAX_VISIBLE - 1;

    if (last > matching_count) {
        last = matching_count;
    }

    char text[64];

    lv_snprintf(
        text,
        sizeof(text),
        "PAGE %u / %u     %u-%u OF %u",
        (unsigned)(s_devices->page_index + 1),
        (unsigned)page_count,
        (unsigned)first,
        (unsigned)last,
        (unsigned)matching_count);

    lv_label_set_text(
        s_devices->pagination_label,
        text);
}


static void render_catalog(void)
{
    if (!s_devices || !s_devices->root || !s_devices->list) {
        return;
    }

    device_catalog_status_t status;
    device_catalog_controller_status(&status);

    update_filter_buttons(&status);
    lv_obj_clean(s_devices->list);

    s_devices->visible_count = 0;

    for (size_t index = 0;
         index < DEVICE_UI_MAX_VISIBLE;
         ++index) {
        s_devices->value_labels[index] = NULL;
        s_devices->value_catalog_indices[index] = 0;
    }

    char banner[64];

    if (!status.discovered) {
        lv_snprintf(
            banner,
            sizeof(banner),
            "WAITING FOR PRINTER");
    } else if (status.truncated) {
        lv_snprintf(
            banner,
            sizeof(banner),
            "%u OF %u OBJECTS",
            (unsigned)status.stored_count,
            (unsigned)status.total_object_count);
    } else {
        lv_snprintf(
            banner,
            sizeof(banner),
            "%u OBJECTS",
            (unsigned)status.stored_count);
    }

    lv_label_set_text(
        s_devices->banner_status,
        banner);

    if (!status.discovered) {
        lv_obj_t *waiting = devices_label(
            s_devices->list,
            "Waiting for the active printer's WebSocket capability discovery.",
            UI_FONT_BODY_LARGE,
            UI_TEXT_DIM,
            20,
            80,
            760);

        lv_obj_set_style_text_align(
            waiting,
            LV_TEXT_ALIGN_CENTER,
            0);
        s_devices->page_index = 0;
        update_pagination_controls(0);
        return;
    }

    size_t matching = 0;

    /*
     * Count first so page bounds can be clamped after a filter or printer
     * change without ever constructing more than 48 LVGL cards.
     */
    for (size_t index = 0;
         index < status.stored_count;
         ++index) {
        device_descriptor_t device;

        if (device_catalog_controller_get(
                index,
                &device) &&
            filter_matches(
                s_devices->filter,
                device.kind)) {
            ++matching;
        }
    }

    update_pagination_controls(matching);

    if (matching == 0) {
        lv_obj_t *empty = devices_label(
            s_devices->list,
            "No devices in this category were reported by the active printer.",
            UI_FONT_BODY_LARGE,
            UI_TEXT_DIM,
            20,
            80,
            760);

        lv_obj_set_style_text_align(
            empty,
            LV_TEXT_ALIGN_CENTER,
            0);
    } else {
        size_t first_match =
            s_devices->page_index * DEVICE_UI_MAX_VISIBLE;
        size_t matching_index = 0;
        size_t visible = 0;

        for (size_t index = 0;
             index < status.stored_count &&
             visible < DEVICE_UI_MAX_VISIBLE;
             ++index) {
            device_descriptor_t device;

            if (!device_catalog_controller_get(
                    index,
                    &device) ||
                !filter_matches(
                    s_devices->filter,
                    device.kind)) {
                continue;
            }

            if (matching_index++ < first_match) {
                continue;
            }

            add_device_card(
                &device,
                index,
                visible);
            ++visible;
        }
    }

    s_devices->rendered_generation =
        status.generation;

    update_live_values();
}


static void devices_refresh_timer_cb(
    lv_timer_t *timer)
{
    (void)timer;

    if (!s_devices || !s_devices->root) {
        return;
    }

    device_catalog_status_t status;
    device_catalog_controller_status(&status);

    if (status.generation !=
        s_devices->rendered_generation) {
        render_catalog();
        return;
    }

    update_live_values();
}


static void devices_page_event_cb(
    lv_event_t *event)
{
    if (!s_devices ||
        lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    intptr_t direction =
        (intptr_t)lv_event_get_user_data(event);

    if (direction < 0) {
        if (s_devices->page_index == 0) {
            return;
        }

        --s_devices->page_index;
    } else {
        if (s_devices->page_index + 1 >=
            s_devices->page_count) {
            return;
        }

        ++s_devices->page_index;
    }

    render_catalog();
}


static void devices_filter_event_cb(
    lv_event_t *event)
{
    if (!s_devices ||
        lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    intptr_t selected =
        (intptr_t)lv_event_get_user_data(event);

    if (selected < DEVICE_FILTER_ALL ||
        selected >= DEVICE_FILTER_COUNT) {
        return;
    }

    s_devices->filter =
        (device_filter_t)selected;
    s_devices->page_index = 0;

    lv_obj_t *selected_button =
        s_devices->filter_buttons[selected];

    if (selected_button) {
        lv_obj_scroll_to_view(
            selected_button,
            LV_ANIM_ON);
    }

    render_catalog();
}


static void devices_open_telemetry_event_cb(
    lv_event_t *event)
{
    (void)event;

    if (s_devices && s_devices->open_telemetry) {
        s_devices->open_telemetry();
    }
}


void ui_devices_v32_show(
    ui_devices_open_telemetry_cb_t open_telemetry_cb)
{
    if (!devices_state_init()) {
        return;
    }

    s_devices->open_telemetry =
        open_telemetry_cb;

    if (s_devices->root) {
        lv_obj_move_foreground(s_devices->root);
        render_catalog();
        return;
    }

    s_devices->root = lv_obj_create(
        lv_screen_active());

    lv_obj_set_size(
        s_devices->root,
        UI_PAGE_ROOT_WIDTH,
        UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(
        s_devices->root,
        UI_PAGE_ROOT_X,
        UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(
        s_devices->root,
        LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(
        s_devices->root,
        UI_SURFACE_PAGE_DEEP);

    lv_obj_t *banner = ui_create_operator_banner(
        s_devices->root,
        20,
        20,
        800,
        86,
        UI_STATUS_INFO);

    devices_label(
        banner,
        "DEVICES",
        UI_FONT_TITLE,
        UI_TEXT_BRIGHT,
        20,
        15,
        300);

    devices_label(
        banner,
        "Active-printer objects, grouped by capability",
        UI_FONT_CAPTION,
        UI_TEXT_DIM,
        20,
        50,
        480);

    s_devices->banner_status = devices_label(
        banner,
        "WAITING FOR PRINTER",
        UI_FONT_CAPTION,
        UI_ACCENT_BRIGHT,
        560,
        22,
        220);

    lv_obj_set_style_text_align(
        s_devices->banner_status,
        LV_TEXT_ALIGN_RIGHT,
        0);

    lv_obj_t *telemetry = ui_button_create(
        banner,
        UI_BUTTON_OUTLINED,
        "TELEMETRY");

    if (telemetry) {
        lv_obj_set_size(telemetry, 132, 34);
        lv_obj_align(
            telemetry,
            LV_ALIGN_BOTTOM_RIGHT,
            -18,
            -8);
        lv_obj_add_event_cb(
            telemetry,
            devices_open_telemetry_event_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    s_devices->filter_strip = lv_obj_create(
        s_devices->root);

    lv_obj_set_size(
        s_devices->filter_strip,
        800,
        42);
    lv_obj_set_pos(
        s_devices->filter_strip,
        20,
        122);
    lv_obj_set_scroll_dir(
        s_devices->filter_strip,
        LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(
        s_devices->filter_strip,
        LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(
        s_devices->filter_strip,
        0,
        0);
    lv_obj_set_style_border_width(
        s_devices->filter_strip,
        0,
        0);
    ui_apply_surface_role(
        s_devices->filter_strip,
        UI_SURFACE_TRANSPARENT);

    for (size_t index = 0;
         index < DEVICE_FILTER_COUNT;
         ++index) {
        lv_obj_t *button = ui_button_create(
            s_devices->filter_strip,
            index == 0
                ? UI_BUTTON_PRIMARY
                : UI_BUTTON_OUTLINED,
            filter_name((device_filter_t)index));

        if (!button) {
            continue;
        }

        s_devices->filter_buttons[index] =
            button;

        /*
         * Use the original compact target size. Eight complete catalog
         * categories now extend beyond the viewport and remain reachable by
         * horizontal swipe.
         */
        lv_obj_set_size(button, 108, 38);
        lv_obj_set_pos(
            button,
            (int)index * 115,
            0);

        lv_obj_add_event_cb(
            button,
            devices_filter_event_cb,
            LV_EVENT_CLICKED,
            (void *)(intptr_t)index);
    }

    s_devices->list = lv_obj_create(
        s_devices->root);

    lv_obj_set_size(
        s_devices->list,
        800,
        282);
    lv_obj_set_pos(
        s_devices->list,
        20,
        174);
    lv_obj_set_scroll_dir(
        s_devices->list,
        LV_DIR_VER);
    lv_obj_set_scrollbar_mode(
        s_devices->list,
        LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(
        s_devices->list,
        0,
        0);
    ui_apply_surface_role(
        s_devices->list,
        UI_SURFACE_TRANSPARENT);

    s_devices->previous_button = ui_button_create(
        s_devices->root,
        UI_BUTTON_OUTLINED,
        LV_SYMBOL_LEFT " PREVIOUS");

    if (s_devices->previous_button) {
        lv_obj_set_size(
            s_devices->previous_button,
            132,
            36);
        lv_obj_set_pos(
            s_devices->previous_button,
            20,
            466);
        lv_obj_add_event_cb(
            s_devices->previous_button,
            devices_page_event_cb,
            LV_EVENT_CLICKED,
            (void *)(intptr_t)-1);
    }

    s_devices->pagination_label = devices_label(
        s_devices->root,
        "PAGE 1 / 1",
        UI_FONT_CAPTION,
        UI_TEXT_BRIGHT,
        176,
        476,
        488);

    lv_obj_set_style_text_align(
        s_devices->pagination_label,
        LV_TEXT_ALIGN_CENTER,
        0);

    s_devices->next_button = ui_button_create(
        s_devices->root,
        UI_BUTTON_OUTLINED,
        "NEXT " LV_SYMBOL_RIGHT);

    if (s_devices->next_button) {
        lv_obj_set_size(
            s_devices->next_button,
            132,
            36);
        lv_obj_set_pos(
            s_devices->next_button,
            688,
            466);
        lv_obj_add_event_cb(
            s_devices->next_button,
            devices_page_event_cb,
            LV_EVENT_CLICKED,
            (void *)(intptr_t)1);
    }

    s_devices->filter = DEVICE_FILTER_ALL;
    s_devices->rendered_generation = UINT32_MAX;
    render_catalog();

    s_devices->refresh_timer = lv_timer_create(
        devices_refresh_timer_cb,
        500,
        NULL);
}


void ui_devices_v32_hide(void)
{
    if (!s_devices) {
        return;
    }

    if (s_devices->refresh_timer) {
        lv_timer_delete(
            s_devices->refresh_timer);
        s_devices->refresh_timer = NULL;
    }

    if (s_devices->root) {
        lv_obj_delete(s_devices->root);
    }

    s_devices->root = NULL;
    s_devices->banner_status = NULL;
    s_devices->list = NULL;
    s_devices->pagination_label = NULL;
    s_devices->previous_button = NULL;
    s_devices->next_button = NULL;
    s_devices->filter_strip = NULL;
    s_devices->visible_count = 0;
    s_devices->page_index = 0;
    s_devices->page_count = 0;
    s_devices->rendered_generation = 0;

    for (size_t index = 0;
         index < DEVICE_FILTER_COUNT;
         ++index) {
        s_devices->filter_buttons[index] = NULL;
    }
}
