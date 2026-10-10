#include "ui_devices_catalog_view.h"
#include "ui_text.h"
#include "ui_text_fit.h"

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
    struct {
        lv_obj_t *card, *name, *kind, *object, *value;
    } rows[DEVICE_UI_MAX_VISIBLE];
    lv_obj_t *empty;
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

    lv_label_set_text(label, text ? text : ui_text("--"));
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
    int x = column == 0 ? 0 : (ui_theme_is_studio()?408:410);
    int y = row * (ui_theme_is_studio()?152:122);

    lv_obj_t *card = s_devices->rows[visible_index].card;
    if (!card) {
        card = ui_create_operator_card(s_devices->list, x, y, ui_theme_is_studio()?384:390, ui_theme_is_studio()?142:110);
        if (!card) return;
        s_devices->rows[visible_index].card = card;
        s_devices->rows[visible_index].name = devices_label(card, "", UI_FONT_BODY_LARGE, UI_TEXT_BRIGHT, 14, 8, ui_theme_is_studio()?356:362);
        s_devices->rows[visible_index].kind = devices_label(card, "", UI_FONT_CAPTION, UI_ACCENT_BRIGHT, 14, ui_theme_is_studio()?98:76, 104);
        lv_obj_set_style_text_align(s_devices->rows[visible_index].kind, LV_TEXT_ALIGN_RIGHT, 0);
        ui_create_operator_card_divider(card, 14, ui_theme_is_studio()?84:67, ui_theme_is_studio()?356:362);
        s_devices->rows[visible_index].object = devices_label(card, "", UI_FONT_CAPTION, UI_TEXT_DIM, 14, ui_theme_is_studio()?52:38, ui_theme_is_studio()?356:362);
        s_devices->rows[visible_index].value = devices_label(card, "--", UI_FONT_CAPTION, UI_TEXT_BRIGHT, 130, ui_theme_is_studio()?98:76, ui_theme_is_studio()?240:246);
        lv_obj_set_style_text_align(s_devices->rows[visible_index].value, LV_TEXT_ALIGN_RIGHT, 0);
    }
    lv_obj_remove_flag(card, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_devices->rows[visible_index].name, device->display_name);
    lv_label_set_text(s_devices->rows[visible_index].kind, device_catalog_kind_label(device->kind));
    lv_label_set_text(s_devices->rows[visible_index].object, device->object_name);
    lv_label_set_long_mode(s_devices->rows[visible_index].name, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_long_mode(s_devices->rows[visible_index].object, LV_LABEL_LONG_SCROLL_CIRCULAR);
    ui_text_fit_single_line(s_devices->rows[visible_index].kind, UI_FONT_CAPTION);
    lv_obj_t *value = s_devices->rows[visible_index].value;
    lv_label_set_text(value, "--");

    ui_devices_live_values_register(
        visible_index,
        value,
        catalog_index);
}


static size_t devices_page_size(void){return ui_theme_is_studio()?4:DEVICE_UI_MAX_VISIBLE;}

static void update_pagination_controls(
    size_t matching_count)
{
    if (!s_devices) {
        return;
    }

    size_t page_count = matching_count == 0
        ? 1
        : (matching_count + devices_page_size() - 1) /
            devices_page_size();

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
            ui_text("NO MATCHING DEVICES"));
        return;
    }

    size_t first =
        s_devices->page_index * devices_page_size() + 1;
    size_t last =
        first + devices_page_size() - 1;

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
    for (size_t i = 0; i < DEVICE_UI_MAX_VISIBLE; ++i)
        if (s_devices->rows[i].card) lv_obj_add_flag(s_devices->rows[i].card, LV_OBJ_FLAG_HIDDEN);
    if (s_devices->empty) lv_obj_add_flag(s_devices->empty, LV_OBJ_FLAG_HIDDEN);

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
    ui_text_fit_single_line(s_devices->banner_status, UI_FONT_CAPTION);

    if (!status.discovered) {
        if (!s_devices->empty) s_devices->empty = devices_label(
            s_devices->list,
            "Waiting for the active printer's WebSocket capability discovery.",
            UI_FONT_BODY_LARGE,
            UI_TEXT_DIM,
            20,
            80,
            ui_theme_is_studio()?752:760);

        lv_label_set_long_mode(s_devices->empty, LV_LABEL_LONG_WRAP);
        lv_obj_t *waiting = s_devices->empty;
        lv_obj_remove_flag(waiting, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(waiting, "Waiting for the active printer's WebSocket capability discovery.");
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
        if (!s_devices->empty) s_devices->empty = devices_label(
            s_devices->list,
            "No devices in this category were reported by the active printer.",
            UI_FONT_BODY_LARGE,
            UI_TEXT_DIM,
            20,
            80,
            ui_theme_is_studio()?752:760);

        lv_label_set_long_mode(s_devices->empty, LV_LABEL_LONG_WRAP);
        lv_obj_t *empty = s_devices->empty;
        lv_obj_remove_flag(empty, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(empty, "No devices in this category were reported by the active printer.");
        lv_obj_set_style_text_align(
            empty,
            LV_TEXT_ALIGN_CENTER,
            0);
    } else {
        size_t first_match =
            s_devices->page_index * devices_page_size();
        size_t matching_index = 0;
        size_t visible = 0;

        for (size_t index = 0;
             index < status.stored_count &&
             visible < devices_page_size();
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
        48);
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

    lv_obj_set_flex_flow(s_devices->filter_strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(s_devices->filter_strip, 8, 0);
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

        /* Fit both the category and its changing count on one line.
         * Native row layout keeps wider items spaced and swipeable. */
        lv_obj_set_size(button, LV_SIZE_CONTENT, 44);
        lv_obj_set_style_min_width(button, 108, 0);
        lv_obj_set_style_pad_hor(button, 10, 0);

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
            160,
            44);
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
            160,
            44);
        lv_obj_set_pos(
            s_devices->next_button,
            660,
            466);
        lv_obj_add_event_cb(
            s_devices->next_button,
            devices_page_event_cb,
            LV_EVENT_CLICKED,
            (void *)(intptr_t)1);
    }

    if(ui_theme_is_studio()) {
        lv_obj_set_pos(s_devices->filter_strip,0,64);lv_obj_set_size(s_devices->filter_strip,164,352);
        lv_obj_set_flex_flow(s_devices->filter_strip,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(s_devices->filter_strip,0,0);
        lv_obj_clear_flag(s_devices->filter_strip,LV_OBJ_FLAG_SCROLLABLE);
        for(size_t i=0;i<DEVICE_FILTER_COUNT;i++){lv_obj_t *b=s_devices->filter_buttons[i];if(b){lv_obj_set_width(b,164);lv_obj_set_style_min_width(b,0,0);}}
        lv_obj_set_pos(s_devices->list,184,64);lv_obj_set_size(s_devices->list,792,304);lv_obj_clear_flag(s_devices->list,LV_OBJ_FLAG_SCROLLABLE);
        if(s_devices->previous_button)lv_obj_set_pos(s_devices->previous_button,184,380);
        if(s_devices->next_button)lv_obj_set_pos(s_devices->next_button,816,380);
        lv_obj_set_pos(s_devices->pagination_label,360,390);lv_obj_set_width(s_devices->pagination_label,440);
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
