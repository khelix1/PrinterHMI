#include "ui_printer_popups.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_dashboard_v32.h"
#include "moonraker.h"
#include "esp_heap_caps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_cancel_confirm_popup = NULL;
static lv_obj_t *s_object_list_popup = NULL;
static lv_obj_t *s_object_confirm_popup = NULL;
static ui_printer_popups_send_gcode_cb_t s_send_gcode_cb = NULL;
static moonraker_exclude_state_t *s_exclude_snapshot = NULL;
static char s_selected_object[MOONRAKER_EXCLUDE_NAME_MAX];
static lv_obj_t *s_object_map = NULL;
static lv_obj_t *s_object_rows[MOONRAKER_EXCLUDE_MAX_OBJECTS];
static lv_obj_t *s_exclude_action_button = NULL;
static int s_selected_object_index = -1;
static lv_obj_t *s_control_popup = NULL;
static lv_obj_t *s_custom_temp_popup = NULL;
static lv_obj_t *s_custom_temp_textarea = NULL;
static lv_obj_t *s_custom_temp_status = NULL;
static const char *s_custom_temp_title = NULL;
static const char *s_custom_temp_command_prefix = NULL;
static int s_custom_temp_max = 0;
static int s_custom_temp_initial = 0;

static void close_cancel_confirm_cb(lv_event_t *e)
{
    (void)e;

    if (s_cancel_confirm_popup) {
        lv_obj_delete(s_cancel_confirm_popup);
        s_cancel_confirm_popup = NULL;
    }
}

static void confirm_cancel_cb(lv_event_t *e)
{
    (void)e;

    if (s_send_gcode_cb) {
        s_send_gcode_cb("CANCEL_PRINT");
    }

    close_cancel_confirm_cb(e);
}

static void show_cancel_print_confirm(
    ui_printer_popups_send_gcode_cb_t send_cb)
{
    s_send_gcode_cb = send_cb;

    if (s_cancel_confirm_popup) {
        lv_obj_move_foreground(s_cancel_confirm_popup);
        return;
    }

    s_cancel_confirm_popup =
        ui_popup_create(lv_screen_active(),
                        420,
                        230,
                        UI_POPUP_DANGER);

    if (!s_cancel_confirm_popup) return;

    ui_popup_add_title(
        s_cancel_confirm_popup,
        "CANCEL PRINT?",
        true,
        4);

    ui_popup_add_header_divider(
        s_cancel_confirm_popup,
        44);

    ui_popup_add_body(
        s_cancel_confirm_popup,
        "This will stop the active print job.",
        20,
        72,
        340);

    ui_popup_add_standard_footer_divider(s_cancel_confirm_popup);

    lv_obj_t *back_label = NULL;

    ui_popup_add_footer_action(
        s_cancel_confirm_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        150,
        UI_POPUP_FOOTER_LEFT,
        close_cancel_confirm_cb,
        NULL,
        &back_label);

    lv_obj_t *cancel_label = NULL;

    ui_popup_add_footer_action(
        s_cancel_confirm_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_STOP " CANCEL",
        150,
        UI_POPUP_FOOTER_RIGHT,
        confirm_cancel_cb,
        NULL,
        &cancel_label);

}


static void close_object_confirm_cb(lv_event_t *event)
{
    (void)event;

    if (s_object_confirm_popup) {
        lv_obj_delete(s_object_confirm_popup);
        s_object_confirm_popup = NULL;
    }
}


static void close_object_list_cb(lv_event_t *event)
{
    (void)event;

    close_object_confirm_cb(NULL);

    if (s_object_list_popup) {
        lv_obj_delete(s_object_list_popup);
        s_object_list_popup = NULL;
    }

    if (s_exclude_snapshot) {
        heap_caps_free(s_exclude_snapshot);
        s_exclude_snapshot = NULL;
    }

    memset(s_object_rows, 0, sizeof(s_object_rows));
    s_object_map = NULL;
    s_exclude_action_button = NULL;
    s_selected_object_index = -1;
}


static bool object_name_is_safe(const char *name)
{
    if (!name || !name[0]) return false;

    for (const unsigned char *cursor =
             (const unsigned char *)name;
         *cursor;
         ++cursor) {
        if (*cursor <= ' ' || *cursor == 0x7f) {
            return false;
        }
    }

    return true;
}


static void confirm_cancel_object_cb(lv_event_t *event)
{
    (void)event;

    if (s_send_gcode_cb && object_name_is_safe(s_selected_object)) {
        char command[sizeof(s_selected_object) + 32];
        int written = snprintf(command,
                               sizeof(command),
                               "EXCLUDE_OBJECT NAME=%s",
                               s_selected_object);

        if (written > 0 && (size_t)written < sizeof(command)) {
            s_send_gcode_cb(command);
        }
    }

    close_object_list_cb(NULL);
}


static void show_object_confirm(const char *name)
{
    if (!name || !name[0]) return;

    size_t name_length = strnlen(
        name, sizeof(s_selected_object) - 1);
    memmove(s_selected_object, name, name_length);
    s_selected_object[name_length] = '\0';

    if (s_object_confirm_popup) {
        lv_obj_move_foreground(s_object_confirm_popup);
        return;
    }

    s_object_confirm_popup =
        ui_popup_create(lv_screen_active(),
                        480,
                        260,
                        UI_POPUP_DANGER);

    if (!s_object_confirm_popup) return;

    ui_popup_add_title(s_object_confirm_popup,
                       "CANCEL OBJECT?",
                       true,
                       4);
    ui_popup_add_header_divider(s_object_confirm_popup, 44);

    char body[160];
    snprintf(body,
             sizeof(body),
             "Stop printing only this object?\n\n%.95s",
             s_selected_object);

    ui_popup_add_body(s_object_confirm_popup,
                      body,
                      20,
                      64,
                      440);
    ui_popup_add_standard_footer_divider(s_object_confirm_popup);

    ui_popup_add_footer_action(
        s_object_confirm_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        170,
        UI_POPUP_FOOTER_LEFT,
        close_object_confirm_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_object_confirm_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_STOP " EXCLUDE",
        170,
        UI_POPUP_FOOTER_RIGHT,
        confirm_cancel_object_cb,
        NULL,
        NULL);
}


static void object_map_bounds(
    double *min_x,
    double *min_y,
    double *max_x,
    double *max_y)
{
    if (!min_x || !min_y || !max_x || !max_y ||
        !s_exclude_snapshot) {
        return;
    }

    if (s_exclude_snapshot->bed_bounds_valid) {
        *min_x = s_exclude_snapshot->bed_min_x;
        *min_y = s_exclude_snapshot->bed_min_y;
        *max_x = s_exclude_snapshot->bed_max_x;
        *max_y = s_exclude_snapshot->bed_max_y;
        return;
    }

    bool found = false;
    *min_x = *min_y = 0.0;
    *max_x = *max_y = 100.0;

    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        const moonraker_exclude_object_t *object =
            &s_exclude_snapshot->objects[i];

        for (uint16_t point_index = 0;
             point_index < object->polygon_count;
             ++point_index) {
            const moonraker_exclude_point_t *point =
                &object->polygon[point_index];
            if (!found) {
                *min_x = *max_x = point->x;
                *min_y = *max_y = point->y;
                found = true;
            } else {
                if (point->x < *min_x) *min_x = point->x;
                if (point->x > *max_x) *max_x = point->x;
                if (point->y < *min_y) *min_y = point->y;
                if (point->y > *max_y) *max_y = point->y;
            }
        }

        if (object->polygon_count == 0 && object->has_center) {
            const moonraker_exclude_point_t *point = &object->center;
            if (!found) {
                *min_x = *max_x = point->x;
                *min_y = *max_y = point->y;
                found = true;
            } else {
                if (point->x < *min_x) *min_x = point->x;
                if (point->x > *max_x) *max_x = point->x;
                if (point->y < *min_y) *min_y = point->y;
                if (point->y > *max_y) *max_y = point->y;
            }
        }
    }

    if (found) {
        double width = *max_x - *min_x;
        double height = *max_y - *min_y;
        double pad = (width > height ? width : height) * 0.06;
        if (pad < 5.0) pad = 5.0;
        *min_x -= pad;
        *min_y -= pad;
        *max_x += pad;
        *max_y += pad;
    }
}


static lv_point_precise_t object_map_point(
    const lv_area_t *area,
    const moonraker_exclude_point_t *point,
    double min_x,
    double min_y,
    double max_x,
    double max_y)
{
    lv_point_precise_t result = {0};
    double width = max_x - min_x;
    double height = max_y - min_y;
    if (!area || !point || width <= 0.0 || height <= 0.0) {
        return result;
    }

    result.x = area->x1 +
        (lv_value_precise_t)((point->x - min_x) *
            (double)lv_area_get_width(area) / width);
    result.y = area->y2 -
        (lv_value_precise_t)((point->y - min_y) *
            (double)lv_area_get_height(area) / height);
    return result;
}


static void object_map_draw_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_DRAW_MAIN ||
        !s_exclude_snapshot) {
        return;
    }

    lv_obj_t *map = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    if (!map || !layer) return;

    lv_area_t area;
    lv_obj_get_content_coords(map, &area);

    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = UI_BORDER_SOFT;
    line.opa = LV_OPA_30;
    line.width = 1;

    for (int division = 1; division < 5; ++division) {
        line.p1.x = area.x1 +
            (lv_area_get_width(&area) * division) / 5;
        line.p1.y = area.y1;
        line.p2.x = line.p1.x;
        line.p2.y = area.y2;
        lv_draw_line(layer, &line);

        line.p1.x = area.x1;
        line.p1.y = area.y1 +
            (lv_area_get_height(&area) * division) / 5;
        line.p2.x = area.x2;
        line.p2.y = line.p1.y;
        lv_draw_line(layer, &line);
    }

    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 100.0;
    double max_y = 100.0;
    object_map_bounds(&min_x, &min_y, &max_x, &max_y);

    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        const moonraker_exclude_object_t *object =
            &s_exclude_snapshot->objects[i];
        bool selected = (int)i == s_selected_object_index;

        line.color = selected ? UI_ACCENT_CYAN :
            (object->current ? UI_TEXT : UI_BORDER_SOFT);
        line.opa = object->excluded ? LV_OPA_30 : LV_OPA_COVER;
        line.width = selected ? 4 : (object->current ? 3 : 2);
        line.round_start = true;
        line.round_end = true;

        if (object->polygon_count >= 2) {
            for (uint16_t point_index = 0;
                 point_index < object->polygon_count;
                 ++point_index) {
                uint16_t next =
                    (uint16_t)((point_index + 1) % object->polygon_count);
                line.p1 = object_map_point(
                    &area, &object->polygon[point_index],
                    min_x, min_y, max_x, max_y);
                line.p2 = object_map_point(
                    &area, &object->polygon[next],
                    min_x, min_y, max_x, max_y);
                lv_draw_line(layer, &line);
            }
        }

        moonraker_exclude_point_t marker;
        bool has_marker = object->has_center;
        if (has_marker) {
            marker = object->center;
        } else if (object->polygon_count > 0) {
            marker.x = 0.0;
            marker.y = 0.0;
            for (uint16_t point_index = 0;
                 point_index < object->polygon_count;
                 ++point_index) {
                marker.x += object->polygon[point_index].x;
                marker.y += object->polygon[point_index].y;
            }
            marker.x /= object->polygon_count;
            marker.y /= object->polygon_count;
            has_marker = true;
        }

        if (has_marker) {
            lv_point_precise_t center = object_map_point(
                &area, &marker, min_x, min_y, max_x, max_y);
            lv_draw_rect_dsc_t marker_dsc;
            lv_draw_rect_dsc_init(&marker_dsc);
            marker_dsc.bg_color = line.color;
            marker_dsc.bg_opa = line.opa;
            marker_dsc.radius = LV_RADIUS_CIRCLE;
            lv_area_t marker_area = {
                .x1 = (int32_t)center.x - (selected ? 5 : 3),
                .y1 = (int32_t)center.y - (selected ? 5 : 3),
                .x2 = (int32_t)center.x + (selected ? 5 : 3),
                .y2 = (int32_t)center.y + (selected ? 5 : 3),
            };
            lv_draw_rect(layer, &marker_dsc, &marker_area);
        }
    }
}


static void select_object_index(int index)
{
    if (!s_exclude_snapshot || index < 0 ||
        (size_t)index >= s_exclude_snapshot->object_count ||
        s_exclude_snapshot->objects[index].excluded) {
        return;
    }

    s_selected_object_index = index;
    snprintf(s_selected_object,
             sizeof(s_selected_object),
             "%s",
             s_exclude_snapshot->objects[index].name);

    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        lv_obj_t *row = s_object_rows[i];
        if (!row) continue;
        bool selected = (int)i == s_selected_object_index;
        lv_obj_set_style_bg_color(
            row, selected ? UI_ACCENT_CYAN : UI_BG, 0);
        lv_obj_set_style_bg_opa(
            row, selected ? LV_OPA_30 : LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(
            row,
            selected ? 3 :
                (s_exclude_snapshot->objects[i].current ? 2 : 1),
            0);
        lv_obj_set_style_border_color(
            row,
            selected ? UI_ACCENT_CYAN :
                (s_exclude_snapshot->objects[i].current
                    ? UI_ACCENT_CYAN : UI_BORDER_SOFT),
            0);
    }

    if (s_exclude_action_button) {
        lv_obj_clear_state(s_exclude_action_button, LV_STATE_DISABLED);
    }
    if (s_object_map) lv_obj_invalidate(s_object_map);
}


static bool point_in_object(
    const moonraker_exclude_object_t *object,
    double x,
    double y)
{
    if (!object || object->polygon_count < 3) return false;

    bool inside = false;
    uint16_t previous = object->polygon_count - 1;
    for (uint16_t current = 0;
         current < object->polygon_count;
         previous = current++) {
        const moonraker_exclude_point_t *a = &object->polygon[current];
        const moonraker_exclude_point_t *b = &object->polygon[previous];
        bool crosses = ((a->y > y) != (b->y > y)) &&
            (x < (b->x - a->x) * (y - a->y) /
                ((b->y - a->y) == 0.0 ? 1.0 : (b->y - a->y)) + a->x);
        if (crosses) inside = !inside;
    }
    return inside;
}


static int object_at_map_point(lv_obj_t *map, const lv_point_t *screen)
{
    if (!map || !screen || !s_exclude_snapshot) return -1;

    lv_area_t area;
    lv_obj_get_content_coords(map, &area);
    if (screen->x < area.x1 || screen->x > area.x2 ||
        screen->y < area.y1 || screen->y > area.y2) {
        return -1;
    }

    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 100.0;
    double max_y = 100.0;
    object_map_bounds(&min_x, &min_y, &max_x, &max_y);
    double x = min_x +
        ((double)(screen->x - area.x1) /
         (double)lv_area_get_width(&area)) * (max_x - min_x);
    double y = max_y -
        ((double)(screen->y - area.y1) /
         (double)lv_area_get_height(&area)) * (max_y - min_y);

    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        const moonraker_exclude_object_t *object =
            &s_exclude_snapshot->objects[i];
        if (!object->excluded && point_in_object(object, x, y)) {
            return (int)i;
        }
    }

    int nearest = -1;
    int64_t nearest_distance = 30 * 30;
    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        const moonraker_exclude_object_t *object =
            &s_exclude_snapshot->objects[i];
        if (object->excluded || !object->has_center) continue;
        lv_point_precise_t center = object_map_point(
            &area, &object->center, min_x, min_y, max_x, max_y);
        int64_t dx = (int64_t)screen->x - (int64_t)center.x;
        int64_t dy = (int64_t)screen->y - (int64_t)center.y;
        int64_t distance = dx * dx + dy * dy;
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest = (int)i;
        }
    }
    return nearest;
}


static void object_map_input_cb(lv_event_t *event)
{
    if (!event) return;
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING &&
        code != LV_EVENT_CLICKED && code != LV_EVENT_HOVER_OVER) {
        return;
    }

    lv_indev_t *indev = lv_event_get_indev(event);
    if (!indev) return;
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    int index = object_at_map_point(lv_event_get_target(event), &point);
    if (index >= 0) select_object_index(index);
}


static void object_row_event_cb(lv_event_t *event)
{
    if (!event) return;
    lv_event_code_t code = lv_event_get_code(event);
    if (code != LV_EVENT_CLICKED && code != LV_EVENT_PRESSED &&
        code != LV_EVENT_FOCUSED && code != LV_EVENT_HOVER_OVER) {
        return;
    }

    const moonraker_exclude_object_t *object =
        (const moonraker_exclude_object_t *)lv_event_get_user_data(event);
    if (!object || object->excluded || !s_exclude_snapshot) return;
    ptrdiff_t index = object - s_exclude_snapshot->objects;
    if (index >= 0 &&
        (size_t)index < s_exclude_snapshot->object_count) {
        select_object_index((int)index);
    }
}


static void exclude_selected_cb(lv_event_t *event)
{
    (void)event;
    if (s_selected_object_index >= 0 &&
        object_name_is_safe(s_selected_object)) {
        show_object_confirm(s_selected_object);
    }
}


static void show_cancel_object_list(void)
{
    if (s_object_list_popup) {
        lv_obj_move_foreground(s_object_list_popup);
        return;
    }

    s_object_list_popup =
        ui_popup_create(lv_screen_active(),
                        760,
                        450,
                        UI_POPUP_STANDARD);

    if (!s_object_list_popup) {
        heap_caps_free(s_exclude_snapshot);
        s_exclude_snapshot = NULL;
        return;
    }

    ui_popup_add_title(s_object_list_popup,
                       "CANCEL OBJECT",
                       false,
                       8);
    ui_popup_add_header_divider(s_object_list_popup, 44);

    char status[128];
    if (s_exclude_snapshot->truncated) {
        snprintf(status,
                 sizeof(status),
                 "Select an object to stop. Showing the first %u objects.",
                 (unsigned)MOONRAKER_EXCLUDE_MAX_OBJECTS);
    } else {
        snprintf(status,
                 sizeof(status),
                 "%s",
                 "Select an object to stop; the rest of the print continues.");
    }

    ui_popup_add_status_label(s_object_list_popup,
                              status,
                              22,
                              50,
                              716);

    s_object_map = lv_obj_create(s_object_list_popup);
    lv_obj_set_pos(s_object_map, 20, 82);
    lv_obj_set_size(s_object_map, 420, 292);
    ui_apply_surface_role(s_object_map, UI_SURFACE_POPUP_LIST);
    lv_obj_set_style_pad_all(s_object_map, 12, 0);
    lv_obj_clear_flag(s_object_map, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_object_map, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        s_object_map, object_map_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(
        s_object_map, object_map_input_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *list = lv_obj_create(s_object_list_popup);
    lv_obj_set_pos(list, 452, 82);
    lv_obj_set_size(list, 288, 292);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    ui_apply_surface_role(list, UI_SURFACE_POPUP_LIST);
    lv_obj_set_style_pad_row(list, 8, 0);

    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        moonraker_exclude_object_t *object =
            &s_exclude_snapshot->objects[i];

        lv_obj_t *row = ui_button_create_empty(list, UI_BUTTON_OUTLINED);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, 46);
        lv_obj_set_style_border_width(row, object->current ? 2 : 1, 0);
        lv_obj_set_style_border_color(
            row,
            object->current ? UI_ACCENT_CYAN : UI_BORDER_SOFT,
            0);
        lv_obj_set_style_pad_hor(row, 10, 0);

        char label_text[MOONRAKER_EXCLUDE_NAME_MAX + 24];
        snprintf(label_text,
                 sizeof(label_text),
                 object->excluded
                     ? "[EXCLUDED] %s"
                     : object->current
                         ? "[CURRENT] %s"
                         : "%s",
                 object->name);

        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, label_text);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(label, 242);
        lv_obj_center(label);
        lv_obj_set_style_text_color(
            label,
            object->excluded ? UI_BORDER_SOFT : UI_TEXT,
            0);

        if (object->excluded) {
            lv_obj_add_state(row, LV_STATE_DISABLED);
            lv_obj_set_style_opa(row, LV_OPA_50, 0);
        } else {
            s_object_rows[i] = row;
            lv_obj_add_event_cb(row,
                                object_row_event_cb,
                                LV_EVENT_ALL,
                                object);
        }
    }

    ui_popup_add_standard_footer_divider(s_object_list_popup);

    ui_popup_add_footer_action(
        s_object_list_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        160,
        UI_POPUP_FOOTER_LEFT,
        close_object_list_cb,
        NULL,
        NULL);

    lv_obj_t *exclude_action_label = NULL;
    ui_popup_add_footer_action(
        s_object_list_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_STOP " EXCLUDE SELECTED",
        230,
        UI_POPUP_FOOTER_RIGHT,
        exclude_selected_cb,
        NULL,
        &exclude_action_label);

    s_exclude_action_button = exclude_action_label
        ? lv_obj_get_parent(exclude_action_label)
        : NULL;

    if (s_exclude_action_button) {
        lv_obj_add_state(s_exclude_action_button, LV_STATE_DISABLED);
    }

    int initial_index = -1;
    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        if (!s_exclude_snapshot->objects[i].excluded &&
            s_exclude_snapshot->objects[i].current) {
            initial_index = (int)i;
            break;
        }
        if (initial_index < 0 &&
            !s_exclude_snapshot->objects[i].excluded) {
            initial_index = (int)i;
        }
    }
    if (initial_index >= 0) select_object_index(initial_index);
}


void ui_printer_popups_show_cancel(
    ui_printer_popups_send_gcode_cb_t send_cb)
{
    s_send_gcode_cb = send_cb;
    show_cancel_print_confirm(send_cb);
}


void ui_printer_popups_show_cancel_object(
    ui_printer_popups_send_gcode_cb_t send_cb)
{
    s_send_gcode_cb = send_cb;

    if (s_exclude_snapshot) {
        heap_caps_free(s_exclude_snapshot);
        s_exclude_snapshot = NULL;
    }

    s_exclude_snapshot = heap_caps_calloc(
        1,
        sizeof(*s_exclude_snapshot),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!s_exclude_snapshot) {
        s_exclude_snapshot = calloc(1, sizeof(*s_exclude_snapshot));
    }

    if (!s_exclude_snapshot) {
        ui_dashboard_v32_status_popup_show(
            "CANCEL OBJECT",
            "Not enough memory to load the object list.");
        return;
    }

    moonraker_exclude_state_snapshot(s_exclude_snapshot);

    bool has_available_object = false;
    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        if (!s_exclude_snapshot->objects[i].excluded) {
            has_available_object = true;
            break;
        }
    }

    if (!s_exclude_snapshot->available || !has_available_object) {
        heap_caps_free(s_exclude_snapshot);
        s_exclude_snapshot = NULL;
        ui_dashboard_v32_status_popup_show(
            "CANCEL OBJECT",
            "No cancellable objects are available for the active print.");
        return;
    }

    show_cancel_object_list();
}

static void close_popup_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t *obj = lv_event_get_target(e);
        lv_obj_t *popup = (lv_obj_t *)lv_event_get_user_data(e);

        if (!popup) {
            popup = lv_obj_get_parent(obj);
        }

        if (popup) {
            lv_obj_delete(popup);
        }
    }
}

static void gcode_button_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    const char *cmd =
        (const char *)lv_event_get_user_data(e);

    if (cmd && cmd[0] && s_send_gcode_cb) {
        s_send_gcode_cb(cmd);
    }

    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *popup = btn;

    while (popup &&
           lv_obj_get_parent(popup) != lv_layer_top()) {
        popup = lv_obj_get_parent(popup);
    }

    if (popup) {
        lv_obj_delete(popup);
    }
}


static void control_popup_deleted_cb(lv_event_t *event)
{
    if (event && lv_event_get_target(event) == s_control_popup) {
        s_control_popup = NULL;
    }
}


static void custom_temp_popup_deleted_cb(lv_event_t *event)
{
    if (event && lv_event_get_target(event) == s_custom_temp_popup) {
        s_custom_temp_popup = NULL;
        s_custom_temp_textarea = NULL;
        s_custom_temp_status = NULL;
    }
}


static void close_custom_temp_cb(lv_event_t *event)
{
    (void)event;
    if (s_custom_temp_popup) {
        lv_obj_delete(s_custom_temp_popup);
    }
}


static void set_custom_temp_cb(lv_event_t *event)
{
    (void)event;
    if (!s_custom_temp_textarea ||
        !s_custom_temp_command_prefix ||
        s_custom_temp_max <= 0) {
        return;
    }

    const char *text = lv_textarea_get_text(s_custom_temp_textarea);
    char *end = NULL;
    long value = text && text[0] ? strtol(text, &end, 10) : -1;

    if (!text || !text[0] || !end || *end != '\0' ||
        value < 0 || value > s_custom_temp_max) {
        if (s_custom_temp_status) {
            lv_label_set_text_fmt(
                s_custom_temp_status,
                "ENTER 0-%d C  (0 = OFF)",
                s_custom_temp_max);
            lv_obj_set_style_text_color(
                s_custom_temp_status, UI_DANGER_BRIGHT, 0);
        }
        return;
    }

    char command[32];
    int written = snprintf(command,
                           sizeof(command),
                           "%s%ld",
                           s_custom_temp_command_prefix,
                           value);
    if (written <= 0 || (size_t)written >= sizeof(command)) {
        return;
    }

    if (s_send_gcode_cb) s_send_gcode_cb(command);

    close_custom_temp_cb(NULL);
    if (s_control_popup) lv_obj_delete(s_control_popup);
}


static void custom_temp_keyboard_cb(lv_event_t *event)
{
    if (!event) return;
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY) {
        set_custom_temp_cb(event);
    } else if (code == LV_EVENT_CANCEL) {
        close_custom_temp_cb(event);
    }
}


static void open_custom_temp_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !s_custom_temp_title || !s_custom_temp_command_prefix ||
        s_custom_temp_max <= 0) {
        return;
    }

    if (s_custom_temp_popup) {
        lv_obj_move_foreground(s_custom_temp_popup);
        return;
    }

    s_custom_temp_popup = ui_popup_create(
        lv_layer_top(), 560, 460, UI_POPUP_STANDARD);
    if (!s_custom_temp_popup) return;

    lv_obj_add_event_cb(
        s_custom_temp_popup,
        custom_temp_popup_deleted_cb,
        LV_EVENT_DELETE,
        NULL);

    ui_popup_add_title(
        s_custom_temp_popup, s_custom_temp_title, false, 8);
    ui_popup_add_header_divider(s_custom_temp_popup, 44);

    char instruction[64];
    snprintf(instruction,
             sizeof(instruction),
             "ENTER 0-%d C  (0 = OFF)",
             s_custom_temp_max);
    s_custom_temp_status = ui_popup_add_status_label(
        s_custom_temp_popup, instruction, 30, 52, 500);

    char initial_text[8] = "";
    if (s_custom_temp_initial > 0) {
        snprintf(initial_text,
                 sizeof(initial_text),
                 "%d",
                 s_custom_temp_initial);
    }

    s_custom_temp_textarea = ui_popup_add_textarea(
        s_custom_temp_popup,
        220,
        56,
        LV_ALIGN_TOP_MID,
        0,
        82,
        true,
        false,
        3,
        "Temperature",
        initial_text,
        "0123456789");

    lv_obj_t *keyboard = ui_popup_add_keyboard(
        s_custom_temp_popup,
        s_custom_temp_textarea,
        500,
        190,
        LV_ALIGN_TOP_MID,
        0,
        150,
        LV_KEYBOARD_MODE_NUMBER);

    if (keyboard) {
        lv_obj_add_event_cb(
            keyboard,
            custom_temp_keyboard_cb,
            LV_EVENT_READY,
            NULL);
        lv_obj_add_event_cb(
            keyboard,
            custom_temp_keyboard_cb,
            LV_EVENT_CANCEL,
            NULL);
    }

    ui_popup_add_standard_footer_divider(s_custom_temp_popup);
    ui_popup_add_footer_action(
        s_custom_temp_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        160,
        UI_POPUP_FOOTER_LEFT,
        close_custom_temp_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_custom_temp_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_OK " SET",
        160,
        UI_POPUP_FOOTER_RIGHT,
        set_custom_temp_cb,
        NULL,
        NULL);

    if (s_custom_temp_textarea) {
        lv_obj_add_state(s_custom_temp_textarea, LV_STATE_FOCUSED);
    }
}

static lv_obj_t *control_popup_button(
    lv_obj_t *parent,
    const char *text,
    int x,
    int y,
    int w,
    int h,
    const char *cmd,
    bool off,
    bool selected)
{
    lv_obj_t *button = ui_popup_add_action_at(
        parent,
        off ? UI_POPUP_ACTION_DANGER : UI_POPUP_ACTION_CHOICE,
        text,
        x,
        y,
        w,
        h,
        gcode_button_event_cb,
        (void *)cmd,
        NULL);

    if (button && selected) {
        lv_obj_set_style_border_color(button, UI_ACCENT_CYAN, 0);
        lv_obj_set_style_border_width(button, 3, 0);
    }

    return button;
}


static lv_obj_t *control_value_card(
    lv_obj_t *parent,
    const char *caption,
    const char *value,
    int x,
    int y,
    int width,
    lv_color_t value_color)
{
    if (!parent || !caption || !value) return NULL;

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, width, 78);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(card, UI_SURFACE_SECTION);

    lv_obj_t *accent = lv_obj_create(card);
    lv_obj_set_pos(accent, 0, 0);
    lv_obj_set_size(accent, 5, 78);
    lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(accent, UI_SURFACE_INDICATOR);
    lv_obj_set_style_bg_color(accent, value_color, 0);

    lv_obj_t *caption_label = lv_label_create(card);
    lv_label_set_text(caption_label, caption);
    ui_apply_custom_label_style(caption_label,
                                UI_FONT_BODY,
                                UI_BORDER_SOFT);
    lv_obj_align(caption_label, LV_ALIGN_LEFT_MID, 22, -15);

    lv_obj_t *value_label = lv_label_create(card);
    lv_label_set_text(value_label, value);
    ui_apply_custom_label_style(value_label,
                                UI_FONT_BODY_LARGE,
                                value_color);
    lv_obj_align(value_label, LV_ALIGN_LEFT_MID, 22, 14);

    return card;
}


static void show_control_popup(
    const char *title,
    double current_value,
    double target_value,
    const char *unit,
    bool show_target,
    const char *cmds[],
    const char *labels[],
    const double preset_values[],
    int count,
    const char *custom_title,
    const char *custom_command_prefix,
    int custom_max)
{
    lv_obj_t *popup =
        ui_popup_create(lv_layer_top(),
                        660,
                        430,
                        UI_POPUP_STANDARD);

    if (!popup) return;

    s_control_popup = popup;
    s_custom_temp_title = custom_title;
    s_custom_temp_command_prefix = custom_command_prefix;
    s_custom_temp_max = custom_max;
    s_custom_temp_initial = target_value > 0.0
        ? (int)(target_value + 0.5)
        : 0;
    lv_obj_add_event_cb(
        popup, control_popup_deleted_cb, LV_EVENT_DELETE, NULL);

    ui_popup_add_title(
        popup,
        title,
        false,
        8);

    ui_popup_add_header_divider(
        popup,
        44);

    char current_text[32];
    char target_text[32];
    if (current_value > -100.0) {
        snprintf(current_text,
                 sizeof(current_text),
                 "%.0f %s",
                 current_value,
                 unit);
    } else {
        snprintf(current_text,
                 sizeof(current_text),
                 "-- %s",
                 unit);
    }

    if (target_value > 0.0) {
        snprintf(target_text,
                 sizeof(target_text),
                 "%.0f %s",
                 target_value,
                 unit);
    } else if (target_value >= 0.0) {
        snprintf(target_text, sizeof(target_text), "OFF");
    } else {
        snprintf(target_text,
                 sizeof(target_text),
                 "-- %s",
                 unit);
    }

    if (show_target) {
        control_value_card(
            popup, "CURRENT", current_text, 24, 60, 294, UI_TEXT);
        lv_obj_t *target_card = control_value_card(
            popup,
            custom_command_prefix
                ? "TARGET  /  TAP TO SET"
                : "TARGET",
            target_text,
            342,
            60,
            294,
            UI_ACCENT_CYAN);
        if (target_card && custom_command_prefix) {
            lv_obj_add_flag(target_card, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_border_color(
                target_card, UI_ACCENT_CYAN, LV_STATE_PRESSED);
            lv_obj_set_style_border_width(
                target_card, 3, LV_STATE_PRESSED);
            lv_obj_add_event_cb(
                target_card,
                open_custom_temp_cb,
                LV_EVENT_CLICKED,
                NULL);
        }
    } else {
        control_value_card(
            popup, "CURRENT SPEED", current_text, 24, 60, 612, UI_TEXT);
    }

    ui_popup_add_status_label(
        popup,
        "CHOOSE A PRESET",
        24,
        146,
        612);

    const int columns = 3;
    const int bw = 188;
    const int bh = 58;
    const int gap_x = 16;
    const int gap_y = 12;
    const int y0 = 174;

    for (int i = 0; i < count; i++) {
        int row = i / columns;
        int col = i % columns;
        int row_start = row * columns;
        int items_in_row = count - row_start;
        if (items_in_row > columns) items_in_row = columns;
        int row_width = items_in_row * bw +
            (items_in_row - 1) * gap_x;
        int x0 = (660 - row_width) / 2;

        double difference = target_value - preset_values[i];
        if (difference < 0.0) difference = -difference;
        bool selected = target_value >= 0.0 && difference < 0.6;
        bool off = preset_values[i] == 0.0;

        control_popup_button(
            popup,
            labels[i],
            x0 + col * (bw + gap_x),
            y0 + row * (bh + gap_y),
            bw,
            bh,
            cmds[i],
            off,
            selected);
    }

    ui_popup_add_standard_footer_divider(popup);

    ui_popup_add_footer_action(
        popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " BACK",
        160,
        UI_POPUP_FOOTER_CENTER,
        close_popup_event_cb,
        popup,
        NULL);
}

void ui_printer_popups_show_part_fan(
    ui_printer_popups_send_gcode_cb_t send_cb,
    double current_fan_percent)
{
    s_send_gcode_cb = send_cb;

    static const char *cmds[] = {
        "M106 S0",
        "M106 S64",
        "M106 S128",
        "M106 S191",
        "M106 S255"
    };

    static const char *labels[] = {
        "OFF",
        "25%",
        "50%",
        "75%",
        "100%"
    };

    static const double values[] = {0, 25, 50, 75, 100};

    show_control_popup("PART COOLING FAN",
                       current_fan_percent,
                       current_fan_percent,
                       "%",
                       false,
                       cmds,
                       labels,
                       values,
                       5,
                       NULL,
                       NULL,
                       0);
}

void ui_printer_popups_show_nozzle(
    ui_printer_popups_send_gcode_cb_t send_cb,
    double current_temp,
    double target_temp)
{
    s_send_gcode_cb = send_cb;

    static const char *cmds[] = {
        "M104 S180",
        "M104 S200",
        "M104 S215",
        "M104 S230",
        "M104 S250",
        "M104 S0"
    };

    static const char *labels[] = {
        "180 C",
        "200 C",
        "215 C",
        "230 C",
        "250 C",
        "OFF"
    };

    static const double values[] = {180, 200, 215, 230, 250, 0};

    show_control_popup("NOZZLE TEMPERATURE",
                       current_temp,
                       target_temp,
                       "C",
                       true,
                       cmds,
                       labels,
                       values,
                       6,
                       "CUSTOM NOZZLE TEMP",
                       "M104 S",
                       300);
}

void ui_printer_popups_show_bed(
    ui_printer_popups_send_gcode_cb_t send_cb,
    double current_temp,
    double target_temp)
{
    s_send_gcode_cb = send_cb;

    static const char *cmds[] = {
        "M140 S50",
        "M140 S60",
        "M140 S70",
        "M140 S80",
        "M140 S100",
        "M140 S0"
    };

    static const char *labels[] = {
        "50 C",
        "60 C",
        "70 C",
        "80 C",
        "100 C",
        "OFF"
    };

    static const double values[] = {50, 60, 70, 80, 100, 0};

    show_control_popup("BED TEMPERATURE",
                       current_temp,
                       target_temp,
                       "C",
                       true,
                       cmds,
                       labels,
                       values,
                       6,
                       "CUSTOM BED TEMP",
                       "M140 S",
                       120);
}

void ui_printer_popups_show_printer_status(
    const char *state,
    const char *file,
    const char *progress,
    const char *elapsed,
    const char *remaining,
    double nozzle_temp,
    double nozzle_target,
    double bed_temp,
    double bed_target,
    bool moonraker_connected)
{
    char body[512];

    snprintf(body,
             sizeof(body),
             "State: %s\n"
             "File: %.120s\n"
             "Progress: %s\n"
             "Elapsed: %s\n"
             "Remaining: %s\n"
             "Nozzle: %.1f / %.1f C\n"
             "Bed: %.1f / %.1f C\n"
             "Moonraker: %s",
             state ? state : "--",
             (file && file[0]) ? file : "No active file",
             progress ? progress : "-- %",
             elapsed ? elapsed : "--:--",
             remaining ? remaining : "--:--",
             nozzle_temp,
             nozzle_target,
             bed_temp,
             bed_target,
             moonraker_connected
                 ? "CONNECTED"
                 : "OFFLINE");

    ui_dashboard_v32_status_popup_show(
        "PRINTER STATUS",
        body);
}

void ui_printer_popups_close_all(void)
{
    close_custom_temp_cb(NULL);
    if (s_control_popup) lv_obj_delete(s_control_popup);

    close_object_confirm_cb(NULL);
    close_object_list_cb(NULL);

    if (s_cancel_confirm_popup) {
        lv_obj_delete(s_cancel_confirm_popup);
        s_cancel_confirm_popup = NULL;
    }
}
