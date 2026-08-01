#include "ui_devices_catalog_view.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "device_catalog_controller.h"
#include "esp_heap_caps.h"
#include "ui_button.h"
#include "ui_devices_live_values.h"
#include "ui_theme.h"
#include "ui_widgets.h"

#define DEVICE_FILTER_COUNT 8
#define DEVICE_UI_MAX_VISIBLE 12

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
    lv_timer_t *refresh_timer;
    device_filter_t filter;
    size_t page_index;
    size_t page_count;
    uint32_t rendered_generation;
} ui_devices_catalog_state_t;

static ui_devices_catalog_state_t *s_devices;


static bool catalog_state_init(void)
{
    if (s_devices) {
        return true;
    }

    s_devices = heap_caps_calloc(
        1,
        sizeof(*s_devices),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!s_devices) {
        s_devices = heap_caps_calloc(
            1,
            sizeof(*s_devices),
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    return s_devices != NULL;
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

    ui_devices_live_values_register(
        visible_index,
        value,
        catalog_index);
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

    ui_devices_live_values_clear();

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
     * change without ever constructing more than 12 LVGL cards. Two full
     * viewports keep scrolling useful while bounding first-render latency.
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

    ui_devices_live_values_update();
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

    ui_devices_live_values_update();
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




void ui_devices_catalog_view_create(
    lv_obj_t *owner,
    lv_obj_t *banner_status)
{
    ui_devices_catalog_view_close();

    if (!owner || !banner_status) {
        return;
    }

    if (!catalog_state_init()) {
        return;
    }

    s_devices->root = owner;
    s_devices->banner_status = banner_status;
    ui_devices_live_values_init(owner);

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


void ui_devices_catalog_view_refresh(void)
{
    render_catalog();
}


void ui_devices_catalog_view_close(void)
{
    if (!s_devices) {
        ui_devices_live_values_close();
        return;
    }

    if (s_devices->refresh_timer) {
        lv_timer_delete(
            s_devices->refresh_timer);
        s_devices->refresh_timer = NULL;
    }

    ui_devices_live_values_close();
    memset(s_devices, 0, sizeof(*s_devices));
}
