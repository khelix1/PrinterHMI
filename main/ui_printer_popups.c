#include "ui_printer_popups.h"
#include "ui_widgets.h"
#include "ui_button.h"
#include "ui_printer_toolhead.h"
#include "ui_value_update.h"
#include "moonraker_config_controller.h"
#include "ui_filament_recovery.h"
#include "ui_text.h"
#include "ui_popup.h"
#include "ui_form_dialog.h"
#include "ui_theme.h"
#include "ui_dashboard.h"
#include "moonraker.h"
#include "printer_controller.h"
#include "printer_connection_policy.h"
#include "esp_heap_caps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_cancel_confirm_popup = NULL;
static lv_obj_t *s_cancel_status;
static ui_printer_popups_send_gcode_cb_t s_cancel_send;
static uint32_t s_cancel_owner, s_cancel_epoch;
static bool s_cancel_dispatching, s_cancel_pending;
static uint32_t s_cancel_sent_owner, s_cancel_sent_at;
static lv_obj_t *s_object_list_popup = NULL;
static lv_obj_t *s_object_confirm_popup = NULL;
static ui_printer_popups_send_gcode_cb_t s_send_gcode_cb = NULL;
static moonraker_exclude_state_t *s_exclude_snapshot = NULL;
static char s_selected_object[MOONRAKER_EXCLUDE_NAME_MAX];
static lv_obj_t *s_object_map = NULL;
static lv_obj_t *s_object_rows[MOONRAKER_EXCLUDE_MAX_OBJECTS];
static lv_obj_t *s_exclude_action_button = NULL;
static int s_selected_object_index = -1;
static uint32_t s_object_owner, s_object_epoch;
static ui_printer_popups_send_gcode_cb_t s_object_send;
static lv_obj_t *s_object_status;
static bool s_object_dispatching, s_object_pending;
static uint32_t s_object_sent_owner, s_object_sent_at;
static char s_object_sent_name[MOONRAKER_EXCLUDE_NAME_MAX];
static char s_confirm_object[MOONRAKER_EXCLUDE_NAME_MAX];
static lv_obj_t *control_layout(lv_obj_t *popup, const char *title);
static lv_obj_t *control_footer(lv_obj_t *popup);
static int32_t control_width(int32_t preferred);
static int32_t control_height(int32_t preferred);
static lv_obj_t *s_control_popup = NULL;
static uint32_t s_control_owner;
static uint32_t s_control_epoch;
static bool s_control_dispatching;
static ui_printer_popups_send_gcode_cb_t s_control_legacy_send;
static bool (*s_control_send)(const char *command);
static lv_obj_t *s_hotend_list_popup = NULL;
static lv_obj_t *s_hotend_activate_popup = NULL;
static lv_timer_t *s_hotend_refresh_timer = NULL;
static moonraker_state_t s_hotend_list_state = {0};
static lv_obj_t *s_hotend_name_labels[MOONRAKER_MAX_HOTENDS] = {0};
static lv_obj_t *s_hotend_temp_labels[MOONRAKER_MAX_HOTENDS] = {0};
static lv_obj_t *s_hotend_activate_buttons[MOONRAKER_MAX_HOTENDS] = {0};
static lv_obj_t *s_hotend_activate_labels[MOONRAKER_MAX_HOTENDS] = {0};
static size_t s_hotend_activate_index = 0;
static char s_hotend_title[48] = "";
static char s_hotend_custom_prefix[96] = "";
static char s_hotend_commands[6][96] = {{0}};
static const char *s_hotend_command_ptrs[6] = {0};
static lv_obj_t *s_custom_temp_popup = NULL;
static lv_obj_t *s_custom_temp_textarea = NULL;
static lv_obj_t *s_custom_temp_status = NULL;
static lv_obj_t *s_filament_list_popup = NULL;
static lv_timer_t *s_filament_refresh_timer = NULL;
static moonraker_filament_state_t s_filament_list_state = {0};
static lv_obj_t *s_filament_status_labels[
    MOONRAKER_MAX_FILAMENT_SENSORS] = {0};
static lv_obj_t *s_filament_toggle_buttons[
    MOONRAKER_MAX_FILAMENT_SENSORS] = {0};
static lv_obj_t *s_filament_toggle_labels[
    MOONRAKER_MAX_FILAMENT_SENSORS] = {0};
static size_t s_filament_row_count;
static uint32_t s_filament_owner, s_filament_epoch;
static bool s_filament_dispatching;
static ui_printer_popups_send_gcode_cb_t s_filament_send;
static char s_filament_row_names[MOONRAKER_MAX_FILAMENT_SENSORS][MOONRAKER_FILAMENT_SENSOR_NAME_MAX];
static char s_filament_feedback[MOONRAKER_MAX_FILAMENT_SENSORS][96];
/* Accepted requests outlive the popup; reopening must not resend them. */
static struct {
    bool pending, expected;
    uint32_t owner, sent_at;
    char name[MOONRAKER_FILAMENT_SENSOR_NAME_MAX];
} s_filament_requests[MOONRAKER_MAX_FILAMENT_SENSORS];
static const char *s_custom_temp_title = NULL;
static const char *s_custom_temp_command_prefix = NULL;
static int s_custom_temp_max = 0;
static int s_custom_temp_initial = 0;

static void cancel_confirm_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != s_cancel_confirm_popup) return;
    s_cancel_confirm_popup = s_cancel_status = NULL;
    s_cancel_send = NULL;
    ++s_cancel_epoch;
}
static void close_cancel_confirm_cb(lv_event_t *event)
{
    (void)event;
    if (s_cancel_confirm_popup) lv_obj_delete(s_cancel_confirm_popup);
}
static void cancel_status_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_cancel_status) s_cancel_status = NULL;
}
static void cancel_feedback(const char *text)
{
    if (s_cancel_status) lv_label_set_text(s_cancel_status, text);
}
static void confirm_cancel_cb(lv_event_t *event)
{
    if (!s_cancel_confirm_popup || s_cancel_dispatching) return;
    if (event && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (s_cancel_owner != moonraker_config_generation()) {
        cancel_feedback("Printer changed. Back out and reopen Cancel.");return;
    }
    moonraker_state_t state;moonraker_state_snapshot(&state);
    const char *reason = printer_connection_reason(&state);
    if (reason[0]) { cancel_feedback(reason);return; }
    if (strcmp(state.printer_state,"printing") && strcmp(state.printer_state,"paused")) {
        cancel_feedback("No active print to cancel. Back out and check the printer.");return;
    }
    if (s_cancel_pending && s_cancel_sent_owner == s_cancel_owner &&
        lv_tick_elaps(s_cancel_sent_at) < 5000) {
        cancel_feedback("Cancel already sent. Check the printer before retrying.");return;
    }
    if (!s_control_send && !s_cancel_send) {
        cancel_feedback("Cancel unavailable. Back out and reopen this control.");return;
    }
    lv_obj_t *popup = s_cancel_confirm_popup;
    uint32_t epoch = s_cancel_epoch, owner = s_cancel_owner;
    s_cancel_dispatching = true;
    bool accepted = false;
    if (s_control_send) accepted = s_control_send("CANCEL_PRINT");
    else { s_cancel_send("CANCEL_PRINT");accepted = true; }
    s_cancel_dispatching = false;
    if (accepted) {
        s_cancel_pending = true;s_cancel_sent_owner = owner;s_cancel_sent_at = lv_tick_get();
    }
    if (epoch != s_cancel_epoch || popup != s_cancel_confirm_popup ||
        owner != moonraker_config_generation()) return;
    if (!accepted) { cancel_feedback("Cancel not sent. Check connection and retry.");return; }
    close_cancel_confirm_cb(NULL);
}
static void show_cancel_print_confirm(ui_printer_popups_send_gcode_cb_t send_cb)
{
    if (s_cancel_confirm_popup && s_cancel_owner != moonraker_config_generation()) close_cancel_confirm_cb(NULL);
    if (s_cancel_confirm_popup) { lv_obj_move_foreground(s_cancel_confirm_popup); return; }
    s_cancel_send = send_cb;
    s_cancel_owner = moonraker_config_generation();
    s_cancel_confirm_popup = ui_popup_create(lv_layer_top(), control_width(620), control_height(320), UI_POPUP_DANGER);
    if (!s_cancel_confirm_popup) return;
    ++s_cancel_epoch;
    lv_obj_add_event_cb(s_cancel_confirm_popup, cancel_confirm_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = control_layout(s_cancel_confirm_popup, ui_text("Stop print?"));
    lv_obj_t *warning = lv_label_create(body);
    lv_label_set_text(warning, "This will stop the active print job.");
    ui_apply_custom_label_style(warning, UI_FONT_BODY, UI_TEXT);
    lv_obj_set_width(warning, LV_PCT(100));
    s_cancel_status = lv_label_create(body);
    lv_label_set_text(s_cancel_status, "");
    ui_apply_custom_label_style(s_cancel_status, UI_FONT_BODY, UI_WARN);
    lv_obj_set_width(s_cancel_status, LV_PCT(100));
    lv_obj_add_event_cb(s_cancel_status, cancel_status_deleted, LV_EVENT_DELETE, NULL);
    if (s_cancel_pending && s_cancel_sent_owner == s_cancel_owner && lv_tick_elaps(s_cancel_sent_at) < 5000)
        cancel_feedback("Cancel already sent. Check the printer before retrying.");
    lv_obj_t *footer = control_footer(s_cancel_confirm_popup);
    lv_obj_t *back = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_LEFT " Back",
        0, 0, 150, 48, close_cancel_confirm_cb, NULL, NULL);
    lv_obj_t *cancel = ui_popup_add_action_at(footer, UI_POPUP_ACTION_DANGER, LV_SYMBOL_STOP " Stop print",
        0, 0, 150, 48, confirm_cancel_cb, NULL, NULL);
    lv_obj_set_flex_grow(back, 1); lv_obj_set_flex_grow(cancel, 1);
}

static void object_confirm_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) == s_object_confirm_popup) {
        s_object_confirm_popup = s_object_status = NULL;
        ++s_object_epoch;
        s_confirm_object[0] = 0;
    }
}
static void close_object_confirm_cb(lv_event_t *event)
{
    (void)event;
    if (s_object_confirm_popup) lv_obj_delete(s_object_confirm_popup);
}
static void object_list_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != s_object_list_popup) return;
    s_object_list_popup = NULL;
    s_object_send = NULL;
    ++s_object_epoch;
    close_object_confirm_cb(NULL);
    if (s_exclude_snapshot) heap_caps_free(s_exclude_snapshot);
    s_exclude_snapshot = NULL;
    memset(s_object_rows, 0, sizeof(s_object_rows));
    s_object_map = s_exclude_action_button = NULL;
    s_selected_object_index = -1;
    s_selected_object[0] = 0;
}
static void close_object_list_cb(lv_event_t *event)
{
    (void)event;
    if (s_object_list_popup) lv_obj_delete(s_object_list_popup);
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


static void object_status_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_object_status) s_object_status = NULL;
}
static void object_feedback(const char *message)
{
    if (s_object_status) lv_label_set_text(s_object_status, message);
}
static void confirm_cancel_object_cb(lv_event_t *event)
{
    if (!s_object_confirm_popup || !s_object_list_popup || s_object_dispatching) return;
    if (event && lv_event_get_code(event) != LV_EVENT_CLICKED) return;
    if (s_object_owner != moonraker_config_generation()) {
        object_feedback("Printer changed. Close and reopen the object list.");return;
    }
    moonraker_state_t state;moonraker_state_snapshot(&state);
    const char *reason = printer_connection_reason(&state);
    if (reason[0]) { object_feedback(reason);return; }
    if (strcmp(state.printer_state,"printing") && strcmp(state.printer_state,"paused")) {
        object_feedback("No active print. Close and check the printer.");return;
    }
    if (!object_name_is_safe(s_confirm_object)) return;
    if (s_object_pending && s_object_sent_owner == s_object_owner &&
        !strcmp(s_object_sent_name,s_confirm_object) && lv_tick_elaps(s_object_sent_at) < 5000) {
        object_feedback("Exclusion already sent. Check the printer before retrying.");return;
    }
    /* Fresh validation must not overwrite the displayed list or its row data. */
    moonraker_exclude_state_t *live = heap_caps_calloc(1, sizeof(*live), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!live) live = calloc(1, sizeof(*live));
    if (!live) { object_feedback("Not enough memory to verify. Back out and retry.");return; }
    moonraker_exclude_state_snapshot(live);
    size_t count = live->object_count;
    if (count > MOONRAKER_EXCLUDE_MAX_OBJECTS) count = MOONRAKER_EXCLUDE_MAX_OBJECTS;
    bool valid = false;
    for (size_t i = 0; i < count; ++i)
        if (live->available && !live->objects[i].excluded &&
            !strcmp(live->objects[i].name,s_confirm_object)) valid = true;
    heap_caps_free(live);
    if (!valid) { object_feedback("Object unavailable or already excluded. Back out and refresh the list.");return; }
    if (!s_control_send && !s_object_send) { object_feedback("Exclusion unavailable. Close and reopen the list.");return; }
    char name[sizeof(s_confirm_object)], command[sizeof(s_confirm_object)+32];
    snprintf(name,sizeof(name),"%s",s_confirm_object);
    int written = snprintf(command,sizeof(command),"EXCLUDE_OBJECT NAME=%s",name);
    if (written <= 0 || (size_t)written >= sizeof(command)) return;
    lv_obj_t *list = s_object_list_popup, *confirm = s_object_confirm_popup;
    uint32_t owner = s_object_owner, epoch = s_object_epoch;
    s_object_dispatching = true;
    bool accepted = false;
    if (s_control_send) accepted = s_control_send(command);
    else { s_object_send(command);accepted = true; }
    s_object_dispatching = false;
    if (accepted) {
        s_object_pending = true;s_object_sent_owner = owner;s_object_sent_at = lv_tick_get();
        snprintf(s_object_sent_name,sizeof(s_object_sent_name),"%s",name);
    }
    if (epoch != s_object_epoch || list != s_object_list_popup || confirm != s_object_confirm_popup ||
        owner != moonraker_config_generation()) return;
    if (!accepted) { object_feedback("Exclusion not sent. Selection kept; check connection and retry.");return; }
    close_object_list_cb(NULL);
}
static void show_object_confirm(const char *name)
{
    if (!object_name_is_safe(name) || s_object_owner != moonraker_config_generation()) return;
    if (s_object_confirm_popup) { lv_obj_move_foreground(s_object_confirm_popup); return; }
    size_t length = strnlen(name, sizeof(s_confirm_object) - 1);
    memcpy(s_confirm_object, name, length); s_confirm_object[length] = 0;
    s_object_confirm_popup = ui_popup_create(lv_layer_top(), control_width(560), control_height(320), UI_POPUP_DANGER);
    if (!s_object_confirm_popup) return;
    ++s_object_epoch;
    lv_obj_add_event_cb(s_object_confirm_popup, object_confirm_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = control_layout(s_object_confirm_popup, ui_text("Cancel object?"));
    lv_obj_t *message = lv_label_create(body);
    lv_label_set_text(message, "Stop printing only this object? The rest of the print continues.");
    ui_apply_custom_label_style(message, UI_FONT_BODY, UI_TEXT);
    lv_obj_set_width(message, LV_PCT(100));
    lv_obj_t *object = lv_label_create(body);
    lv_label_set_text(object, s_confirm_object);
    ui_apply_custom_label_style(object, UI_FONT_BODY_LARGE, UI_TEXT_BRIGHT);
    lv_obj_set_width(object, LV_PCT(100));
    s_object_status = lv_label_create(body);
    lv_label_set_text(s_object_status, "");
    ui_apply_custom_label_style(s_object_status, UI_FONT_BODY, UI_WARN);
    lv_obj_set_width(s_object_status, LV_PCT(100));
    lv_obj_add_event_cb(s_object_status, object_status_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *footer = control_footer(s_object_confirm_popup);
    lv_obj_t *back = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_LEFT " Back",
        0, 0, 170, 48, close_object_confirm_cb, NULL, NULL);
    lv_obj_t *exclude = ui_popup_add_action_at(footer, UI_POPUP_ACTION_DANGER, LV_SYMBOL_STOP " Exclude",
        0, 0, 170, 48, confirm_cancel_object_cb, NULL, NULL);
    lv_obj_set_flex_grow(back, 1); lv_obj_set_flex_grow(exclude, 1);
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
        s_exclude_snapshot->objects[index].excluded ||
        s_object_owner != moonraker_config_generation()) {
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
        if(ui_theme_is_studio()){
            ui_popup_set_selectable_row_selected(row,selected);
            continue;
        }
        lv_obj_set_style_bg_color(
            row, selected ? UI_ACCENT_CYAN : UI_BG, 0);
        lv_obj_set_style_bg_opa(
            row,
            selected
                ? ui_theme_accessible_opacity(LV_OPA_30)
                : LV_OPA_COVER,
            0);
        lv_obj_set_style_border_width(
            row,
            selected
                ? ui_theme_accessible_border_width(3)
                : (s_exclude_snapshot->objects[i].current
                    ? UI_BORDER_STRONG
                    : UI_BORDER_THIN),
            0);
        lv_obj_set_style_border_color(
            row,
            selected ? UI_ACCENT_CYAN :
                (s_exclude_snapshot->objects[i].current
                    ? UI_ACCENT_CYAN : UI_BORDER_SOFT),
            0);
    }

    if (s_exclude_action_button) {
        if (object_name_is_safe(s_selected_object)) lv_obj_clear_state(s_exclude_action_button, LV_STATE_DISABLED);
        else lv_obj_add_state(s_exclude_action_button, LV_STATE_DISABLED);
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


static const int32_t object_columns_wide[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t object_columns_narrow[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t object_layout_rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
static void object_content_resize(lv_event_t *event)
{
    lv_obj_t *content = lv_event_get_target_obj(event);
    bool wide = lv_obj_get_content_width(content) >= ui_theme_density_metric(560, 600, 640);
    const int32_t *columns = wide ? object_columns_wide : object_columns_narrow;
    if (lv_obj_get_style_grid_column_dsc_array(content, 0) == columns) return;
    lv_obj_set_grid_dsc_array(content, columns, object_layout_rows);
    lv_obj_t *map = lv_obj_get_child(content, 0), *list = lv_obj_get_child(content, 1);
    lv_obj_set_height(map, wide ? 280 : 210);
    lv_obj_set_height(list, wide ? 280 : 260);
    lv_obj_set_grid_cell(map, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_grid_cell(list, LV_GRID_ALIGN_STRETCH, wide ? 1 : 0, 1, LV_GRID_ALIGN_START, wide ? 0 : 1, 1);
}
static void show_cancel_object_list(void)
{
    s_object_list_popup = ui_popup_create(lv_layer_top(), control_width(800), control_height(520), UI_POPUP_STANDARD);
    if (!s_object_list_popup) {
        heap_caps_free(s_exclude_snapshot); s_exclude_snapshot = NULL; return;
    }
    lv_obj_add_event_cb(s_object_list_popup, object_list_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = control_layout(s_object_list_popup, ui_text("Cancel object"));
    char status[128];
    if (s_exclude_snapshot->truncated) snprintf(status, sizeof(status),
        "Select an object to stop. Showing the first %u objects.", (unsigned)MOONRAKER_EXCLUDE_MAX_OBJECTS);
    else snprintf(status, sizeof(status), "Select an object to stop; the rest of the print continues.");
    lv_obj_t *hint = lv_label_create(body);
    lv_label_set_text(hint, status);
    ui_apply_custom_label_style(hint, UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_t *content = lv_obj_create(body);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(content, UI_GAP_CARD, 0);
    lv_obj_set_style_pad_row(content, UI_GAP_ROW, 0);
    lv_obj_add_event_cb(content, object_content_resize, LV_EVENT_SIZE_CHANGED, NULL);
    s_object_map = lv_obj_create(content);
    ui_apply_surface_role(s_object_map,ui_theme_is_studio()?UI_SURFACE_PREVIEW_WELL:UI_SURFACE_POPUP_LIST);
    lv_obj_set_style_pad_all(s_object_map, UI_PAD_PANEL, 0);
    lv_obj_clear_flag(s_object_map, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_object_map, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_object_map, object_map_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(s_object_map, object_map_input_cb, LV_EVENT_ALL, NULL);
    lv_obj_t *list = lv_obj_create(content);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    ui_apply_surface_role(list,ui_theme_is_studio()?UI_SURFACE_TRANSPARENT:UI_SURFACE_POPUP_LIST);
    lv_obj_set_scrollbar_mode(list,LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_row(list, UI_GAP_ROW, 0);
    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        moonraker_exclude_object_t *object = &s_exclude_snapshot->objects[i];
        lv_obj_t *row = ui_button_create_empty(list, UI_BUTTON_OUTLINED);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(row, 48, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_hor(row, UI_PAD_CARD, 0);
        lv_obj_set_style_pad_ver(row, 10, 0);
        lv_obj_set_style_border_width(row, object->current ? UI_BORDER_STRONG : UI_BORDER_THIN, 0);
        lv_obj_set_style_border_color(row, object->current ? UI_ACCENT_CYAN : UI_BORDER_SOFT, 0);
        if(ui_theme_is_studio()){
            ui_apply_surface_role(row,UI_SURFACE_TRANSPARENT);
            ui_apply_divider_border(row,LV_BORDER_SIDE_BOTTOM);
            lv_obj_set_style_pad_hor(row,UI_PAD_CARD,0);
            lv_obj_set_style_pad_ver(row,10,0);
        }
        char text[MOONRAKER_EXCLUDE_NAME_MAX + 24];
        snprintf(text, sizeof(text), object->excluded ? "[Excluded] %s" : object->current ? "[Current] %s" : "%s", object->name);
        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, text);
        ui_apply_custom_label_style(label, UI_FONT_BODY, object->excluded ? UI_TEXT_DIM : UI_TEXT);
        lv_obj_set_width(label, LV_PCT(100));
        if (object->excluded) { lv_obj_add_state(row, LV_STATE_DISABLED); lv_obj_set_style_opa(row, LV_OPA_50, 0); }
        else { s_object_rows[i] = row; lv_obj_add_event_cb(row, object_row_event_cb, LV_EVENT_ALL, object); }
    }
    lv_obj_t *footer = control_footer(s_object_list_popup);
    lv_obj_t *close = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " Close",
        0, 0, 160, 48, close_object_list_cb, NULL, NULL);
    s_exclude_action_button = ui_popup_add_action_at(footer, UI_POPUP_ACTION_DANGER, LV_SYMBOL_STOP " Exclude",
        0, 0, 230, 48, exclude_selected_cb, NULL, NULL);
    lv_obj_set_flex_grow(close, 1); lv_obj_set_flex_grow(s_exclude_action_button, 1);
    lv_obj_add_state(s_exclude_action_button, LV_STATE_DISABLED);
    int initial = -1;
    for (size_t i = 0; i < s_exclude_snapshot->object_count; ++i) {
        if (!s_exclude_snapshot->objects[i].excluded && s_exclude_snapshot->objects[i].current) { initial = (int)i; break; }
        if (initial < 0 && !s_exclude_snapshot->objects[i].excluded) initial = (int)i;
    }
    if (initial >= 0) select_object_index(initial);
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
    if (s_object_list_popup && s_object_owner != moonraker_config_generation()) close_object_list_cb(NULL);
    if (s_object_list_popup) { lv_obj_move_foreground(s_object_list_popup); return; }
    s_object_send = send_cb;
    s_send_gcode_cb = send_cb;
    s_object_owner = moonraker_config_generation();

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
        ui_dashboard_status_popup_show(
            "CANCEL OBJECT",
            "Not enough memory to load the object list.");
        return;
    }

    moonraker_exclude_state_snapshot(s_exclude_snapshot);
    if (s_exclude_snapshot->object_count > MOONRAKER_EXCLUDE_MAX_OBJECTS)
        s_exclude_snapshot->object_count = MOONRAKER_EXCLUDE_MAX_OBJECTS;

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
        ui_dashboard_status_popup_show(
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
            popup = ui_popup_find_owner(obj);
            if (!popup) popup = lv_obj_get_parent(obj);
        }

        if (popup) {
            lv_obj_delete(popup);
        }
    }
}

void ui_printer_popups_set_control_sender(bool (*send)(const char *command))
{
    s_control_send=send;
}
static bool control_send(const char *command,const char **error)
{
    *error="Send failed. Input kept; check the connection and retry.";
    if (!s_control_popup || s_control_dispatching) return false;
    if(s_control_owner!=moonraker_config_generation()) {
        *error="Printer changed. Cancel and reopen this control.";return false;
    }
    moonraker_state_t state;moonraker_state_snapshot(&state);
    if (printer_connection_reason(&state)[0]) {
        *error="Printer is offline or Klipper is unavailable. Input kept.";return false;
    }
    s_control_dispatching = true;
    bool accepted = false;
    if (s_control_send) accepted = s_control_send(command);
    else if (s_control_legacy_send) { s_control_legacy_send(command); accepted = true; }
    s_control_dispatching = false;
    return accepted;
}

static void gcode_button_event_cb(lv_event_t *e)
{
    if (!e || lv_event_get_code(e) != LV_EVENT_CLICKED ||
        !s_control_popup || s_control_dispatching) return;
    lv_obj_t *popup = s_control_popup;
    uint32_t epoch = s_control_epoch, owner = moonraker_config_generation();
    const char *cmd = (const char *)lv_event_get_user_data(e);
    char command[128];
    if (!cmd || !cmd[0]) return;
    int written = snprintf(command, sizeof(command), "%s", cmd);
    if (written <= 0 || (size_t)written >= sizeof(command)) return;
    const char *error;
    bool accepted = control_send(command, &error);
    /* Sending may delete the modal or switch the active printer. Never use
     * the event target, borrowed command, or another modal after dispatch. */
    if (s_control_epoch != epoch || s_control_popup != popup ||
        owner != moonraker_config_generation()) return;
    if (!accepted) {
        ui_popup_feedback(lv_obj_get_child(popup,1),UI_STATUS_WARNING,"NOT SENT",error);
        return;
    }
    lv_obj_delete(popup);
}


static void control_popup_deleted_cb(lv_event_t *event)
{
    if (event && lv_event_get_target(event) == s_control_popup) {
        s_control_popup = NULL;
        ++s_control_epoch;
        s_control_legacy_send = NULL;
        if(s_custom_temp_popup)lv_obj_delete(s_custom_temp_popup);
    }
}


static void custom_temp_popup_deleted_cb(lv_event_t *event)
{
    if (event && lv_event_get_target(event) == s_custom_temp_popup) {
        ++s_control_epoch;
        s_custom_temp_popup = NULL;
        s_custom_temp_textarea = NULL;
        s_custom_temp_status = NULL;
    }
}


static void close_custom_temp_cb(lv_event_t *event)
{
    (void)event;
    lv_obj_t *popup = s_custom_temp_popup;
    s_custom_temp_popup = NULL;
    s_custom_temp_textarea = NULL;
    s_custom_temp_status = NULL;
    if (popup) { ++s_control_epoch; lv_obj_delete(popup); }
}


static void set_custom_temp_cb(lv_event_t *event)
{
    (void)event;
    if (s_control_dispatching || !s_control_popup || !s_custom_temp_popup ||
        !s_custom_temp_textarea ||
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
                "Enter 0-%d C (0 = off)",
                s_custom_temp_max);
            lv_obj_set_style_text_color(
                s_custom_temp_status, UI_DANGER_BRIGHT, 0);
        }
        return;
    }

    char command[128];
    int written = snprintf(command,
                           sizeof(command),
                           "%s%ld",
                           s_custom_temp_command_prefix,
                           value);
    if (written <= 0 || (size_t)written >= sizeof(command)) {
        return;
    }

    lv_obj_t *popup = s_control_popup, *editor = s_custom_temp_popup;
    lv_obj_t *textarea = s_custom_temp_textarea, *status = s_custom_temp_status;
    uint32_t epoch = s_control_epoch, owner = moonraker_config_generation();
    const char *error;
    bool accepted = control_send(command, &error);
    if (s_control_epoch != epoch || s_control_popup != popup ||
        s_custom_temp_popup != editor || s_custom_temp_textarea != textarea ||
        s_custom_temp_status != status || owner != moonraker_config_generation()) return;
    if (!accepted) {
        if (status) {
            lv_label_set_text(status,error);
            lv_obj_set_style_text_color(status,UI_WARN,0);
        }
        ui_popup_focus_input(textarea);return;
    }
    close_custom_temp_cb(NULL);
    if (s_control_popup == popup) lv_obj_delete(popup);
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


/* Temperature/fan modals use native layout. Header/footer remain visible;
 * only the body scrolls when density, font size or viewport needs more space. */
static lv_obj_t *control_layout(lv_obj_t *popup, const char *title)
{
    lv_obj_set_flex_flow(popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup, UI_PAD_POPUP, 0);
    lv_obj_set_style_pad_row(popup, UI_GAP_CARD, 0);
    lv_obj_t *label = lv_label_create(popup);
    lv_label_set_text(label, title);
    ui_apply_custom_label_style(label, UI_FONT_TITLE, UI_TEXT_BRIGHT);
    lv_obj_set_width(label, LV_PCT(100));
    lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    ui_form_divide_title(popup);
    lv_obj_t *body = lv_obj_create(popup);
    lv_obj_remove_style_all(body);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_height(body, 0);
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, UI_GAP_CARD, 0);
    lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body, LV_SCROLLBAR_MODE_OFF);
    return body;
}

static lv_obj_t *control_footer(lv_obj_t *popup)
{
    lv_obj_t *footer = lv_obj_create(popup);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, LV_PCT(100), LV_SIZE_CONTENT);
    ui_apply_divider_border(footer, LV_BORDER_SIDE_TOP);
    lv_obj_set_style_pad_top(footer, 8, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(footer, UI_GAP_CARD, 0);
    return footer;
}

static int32_t control_width(int32_t preferred)
{
    int32_t available = lv_display_get_horizontal_resolution(NULL) - 32;
    return available < preferred ? available : preferred;
}

static int32_t control_height(int32_t preferred)
{
    int32_t available = lv_display_get_vertical_resolution(NULL) - 32;
    return available < preferred ? available : preferred;
}

static const int32_t control_columns_one[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t control_columns_two[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t control_columns_three[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t control_rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
    LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

static void control_grid_resize(lv_event_t *event)
{
    lv_obj_t *grid = lv_event_get_target_obj(event);
    unsigned maximum = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    int32_t width = lv_obj_get_content_width(grid);
    int32_t minimum = maximum == 2 ? 220 : ui_theme_density_metric(140, 160, 180);
    int32_t gap = UI_GAP_CARD;
    unsigned columns = width >= minimum * 3 + gap * 2 && maximum == 3 ? 3 :
        width >= minimum * 2 + gap && maximum >= 2 ? 2 : 1;
    const int32_t *descriptor = columns == 3 ? control_columns_three :
        columns == 2 ? control_columns_two : control_columns_one;
    if (lv_obj_get_style_grid_column_dsc_array(grid, 0) == descriptor) return;
    lv_obj_set_grid_dsc_array(grid, descriptor, control_rows);
    for (uint32_t i = 0; i < lv_obj_get_child_count(grid); ++i) {
        lv_obj_set_grid_cell(lv_obj_get_child(grid, i), LV_GRID_ALIGN_STRETCH,
            i % columns, 1, LV_GRID_ALIGN_START, i / columns, 1);
    }
}

static lv_obj_t *control_grid(lv_obj_t *parent, unsigned maximum)
{
    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(grid, UI_GAP_CARD, 0);
    lv_obj_set_style_pad_row(grid, UI_GAP_CARD, 0);
    lv_obj_add_event_cb(grid, control_grid_resize, LV_EVENT_SIZE_CHANGED,
        (void *)(uintptr_t)maximum);
    return grid;
}

static void custom_temp_field_deleted(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);
    if (target == s_custom_temp_textarea) s_custom_temp_textarea = NULL;
    if (target == s_custom_temp_status) s_custom_temp_status = NULL;
}

static void open_custom_temp_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED ||
        s_control_dispatching || !s_control_popup ||
        !s_custom_temp_title || !s_custom_temp_command_prefix || s_custom_temp_max <= 0) return;
    if (s_custom_temp_popup) { lv_obj_move_foreground(s_custom_temp_popup); return; }
    s_custom_temp_popup = ui_popup_create(lv_layer_top(), control_width(600),
        control_height(500), UI_POPUP_STANDARD);
    if (!s_custom_temp_popup) return;
    lv_obj_add_event_cb(s_custom_temp_popup, custom_temp_popup_deleted_cb,
        LV_EVENT_DELETE, NULL);
    lv_obj_t *body = control_layout(s_custom_temp_popup, s_custom_temp_title);
    char instruction[64];
    snprintf(instruction, sizeof(instruction), "Enter 0-%d C (0 = off)", s_custom_temp_max);
    s_custom_temp_status = lv_label_create(body);
    lv_obj_add_event_cb(s_custom_temp_status, custom_temp_field_deleted, LV_EVENT_DELETE, NULL);
    lv_label_set_text(s_custom_temp_status, instruction);
    ui_apply_custom_label_style(s_custom_temp_status, UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_set_width(s_custom_temp_status, LV_PCT(100));
    char initial_text[16] = "";
    if (s_custom_temp_initial > 0) snprintf(initial_text, sizeof(initial_text), "%d", s_custom_temp_initial);
    s_custom_temp_textarea = ui_popup_add_textarea(body, 220, 56,
        LV_ALIGN_TOP_MID, 0, 0, true, false, 3, ui_text("Temperature"),
        initial_text, ui_text("0123456789"));
    lv_obj_add_event_cb(s_custom_temp_textarea, custom_temp_field_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_set_width(s_custom_temp_textarea, LV_PCT(100));
    lv_obj_t *keyboard = ui_popup_add_keyboard(body, s_custom_temp_textarea,
        500, 200, LV_ALIGN_TOP_MID, 0, 0, LV_KEYBOARD_MODE_NUMBER);
    if (keyboard) {
        lv_obj_set_width(keyboard, LV_PCT(100));
        lv_obj_add_event_cb(keyboard, custom_temp_keyboard_cb, LV_EVENT_READY, NULL);
        lv_obj_add_event_cb(keyboard, custom_temp_keyboard_cb, LV_EVENT_CANCEL, NULL);
    }
    lv_obj_t *footer = control_footer(s_custom_temp_popup);
    lv_obj_t *back = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " Back", 0, 0, 160, 48, close_custom_temp_cb, NULL, NULL);
    lv_obj_t *set = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_OK " Set", 0, 0, 160, 48, set_custom_temp_cb, NULL, NULL);
    lv_obj_set_flex_grow(back, 1);
    lv_obj_set_flex_grow(set, 1);
    ui_popup_pin_keyboard(s_custom_temp_popup,keyboard);
}

static lv_obj_t *control_popup_button(lv_obj_t *parent, const char *text,
    const char *cmd, bool off, bool selected)
{
    lv_obj_t *button = ui_popup_add_action_at(parent,
        off ? UI_POPUP_ACTION_DANGER : UI_POPUP_ACTION_CHOICE,
        text, 0, 0, 1, 58, gcode_button_event_cb, (void *)cmd, NULL);
    ui_button_set_filter_active(button, selected);
    return button;
}

static lv_obj_t *control_value_card(lv_obj_t *parent, const char *caption,
    const char *value, lv_color_t value_color)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_surface_role(card, UI_SURFACE_SECTION);
    lv_obj_set_style_pad_all(card, 14, 0);
    lv_obj_set_style_pad_row(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_border_side(card, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(card, 4, 0);
    lv_obj_set_style_border_color(card, value_color, 0);
    if (ui_theme_is_studio()) {
        ui_apply_surface_role(card, UI_SURFACE_TRANSPARENT);
        ui_apply_divider_border(card, LV_BORDER_SIDE_BOTTOM);
        lv_obj_set_style_pad_all(card, 14, 0);
    }
    lv_obj_t *caption_label = lv_label_create(card);
    lv_label_set_text(caption_label, caption);
    ui_apply_custom_label_style(caption_label, UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_set_width(caption_label, LV_PCT(100));
    lv_obj_t *value_label = lv_label_create(card);
    lv_label_set_text(value_label, value);
    ui_apply_custom_label_style(value_label, UI_FONT_BODY_LARGE, value_color);
    lv_obj_set_width(value_label, LV_PCT(100));
    return card;
}

static void show_control_popup(const char *title, double current_value,
    double target_value, const char *unit, bool show_target, const char *cmds[],
    const char *labels[], const double preset_values[], int count,
    const char *custom_title, const char *custom_command_prefix, int custom_max)
{
    if (s_control_dispatching) return;
    /* Avoid retaining another preset modal with commands from an old hotend. */
    close_custom_temp_cb(NULL);
    if (s_control_popup) lv_obj_delete(s_control_popup);
    lv_obj_t *popup = ui_popup_create(lv_layer_top(), control_width(660),
        control_height(460), UI_POPUP_STANDARD);
    if (!popup) return;
    s_control_popup = popup;
    ++s_control_epoch;
    s_control_legacy_send = s_send_gcode_cb;
    s_control_owner=moonraker_config_generation();
    s_custom_temp_title = custom_title;
    s_custom_temp_command_prefix = custom_command_prefix;
    s_custom_temp_max = custom_max;
    s_custom_temp_initial = target_value > 0.0 ? (int)(target_value + 0.5) : 0;
    lv_obj_add_event_cb(popup, control_popup_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = control_layout(popup, title);
    char current_text[32], target_text[32];
    if (current_value > -100.0) snprintf(current_text, sizeof(current_text), "%.0f %s", current_value, unit);
    else snprintf(current_text, sizeof(current_text), "-- %s", unit);
    if (target_value > 0.0) snprintf(target_text, sizeof(target_text), "%.0f %s", target_value, unit);
    else if (target_value >= 0.0) snprintf(target_text, sizeof(target_text), "Off");
    else snprintf(target_text, sizeof(target_text), "-- %s", unit);
    lv_obj_t *cards = control_grid(body, show_target ? 2 : 1);
    control_value_card(cards, show_target ? "Current" : "Current speed", current_text, UI_TEXT);
    if (show_target) {
        lv_obj_t *target_card = control_value_card(cards,
            custom_command_prefix ? "Target - tap to edit" : "Target", target_text, UI_ACCENT_CYAN);
        if (custom_command_prefix) {
            lv_obj_add_flag(target_card, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_border_color(target_card, UI_ACCENT_CYAN, LV_STATE_PRESSED);
            lv_obj_add_event_cb(target_card, open_custom_temp_cb, LV_EVENT_CLICKED, NULL);
        }
    }
    lv_obj_t *hint = lv_label_create(body);
    lv_label_set_text(hint, ui_text("Choose a preset"));
    ui_apply_custom_label_style(hint, UI_FONT_CAPTION, UI_TEXT_DIM);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_t *presets = control_grid(body, 3);
    for (int i = 0; i < count; ++i) {
        double difference = target_value - preset_values[i];
        if (difference < 0.0) difference = -difference;
        control_popup_button(presets, labels[i], cmds[i], preset_values[i] == 0,
            target_value >= 0 && difference < 0.6);
    }
    lv_obj_t *footer = control_footer(popup);
    ui_popup_add_action_at(footer, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_LEFT " Back",
        0, 0, 160, 48, close_popup_event_cb, popup, NULL);
}

static size_t s_hotend_row_count;
static uint32_t s_hotend_owner;
static char s_hotend_row_names[MOONRAKER_MAX_HOTENDS][MOONRAKER_HOTEND_NAME_MAX];
static lv_obj_t *s_hotend_temp_buttons[MOONRAKER_MAX_HOTENDS];
static lv_obj_t *s_hotend_confirm_button;
static char s_hotend_confirm_name[MOONRAKER_HOTEND_NAME_MAX];
static lv_obj_t *s_hotend_confirm_status;
static uint32_t s_hotend_epoch;
static bool s_hotend_dispatching, s_hotend_pending;
static uint32_t s_hotend_sent_at;
static uint32_t s_hotend_pending_owner;
static char s_hotend_pending_name[MOONRAKER_HOTEND_NAME_MAX];
static bool (*s_hotend_send)(const char *command);

static void close_hotend_list_cb(lv_event_t *event);

static const moonraker_hotend_t *hotend_row(size_t row)
{
    size_t count = s_hotend_list_state.hotend_count;
    if (count > MOONRAKER_MAX_HOTENDS) count = MOONRAKER_MAX_HOTENDS;
    if (row >= s_hotend_row_count) return NULL;
    for (size_t i = 0; i < count; ++i)
        if (!strcmp(s_hotend_list_state.hotends[i].object_name, s_hotend_row_names[row]))
            return &s_hotend_list_state.hotends[i];
    return NULL;
}
static bool hotend_name_safe(const char *name)
{
    return name && name[0] && strspn(name,
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") == strlen(name);
}
static bool hotend_live_here(void)
{
    return s_hotend_owner == moonraker_config_generation() &&
        s_hotend_list_state.moonraker_ok && s_hotend_list_state.live_data_ok;
}
static void hotend_activate_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) == s_hotend_activate_popup) {
        s_hotend_activate_popup = s_hotend_confirm_button = NULL;
        s_hotend_confirm_name[0] = 0;
        s_hotend_confirm_status = NULL;
        ++s_hotend_epoch;
    }
}
static void close_hotend_activate_cb(lv_event_t *event)
{
    (void)event;
    if (s_hotend_activate_popup) lv_obj_delete(s_hotend_activate_popup);
}
static void confirm_hotend_activate_cb(lv_event_t *event)
{
    (void)event;
    if (!s_hotend_activate_popup || s_hotend_dispatching || s_hotend_pending) return;
    if (s_hotend_owner != moonraker_config_generation()) {
        close_hotend_list_cb(NULL);
        return;
    }
    moonraker_state_snapshot(&s_hotend_list_state);
    const moonraker_hotend_t *hotend = hotend_row(s_hotend_activate_index);
    if (!hotend_live_here() || printer_connection_reason(&s_hotend_list_state)[0] ||
        !hotend || !hotend_name_safe(hotend->object_name) ||
        strcmp(hotend->object_name, s_hotend_confirm_name) ||
        printer_controller_is_live_state(s_hotend_list_state.printer_state)) {
        lv_label_set_text(s_hotend_confirm_status, "Unavailable. Check printer state and retry.");
        return;
    }
    if (hotend->active) { close_hotend_activate_cb(NULL); return; }
    char command[MOONRAKER_HOTEND_NAME_MAX + 40];
    int written = snprintf(command, sizeof(command),
        "ACTIVATE_EXTRUDER EXTRUDER=%s", s_hotend_confirm_name);
    if (written <= 0 || (size_t)written >= sizeof(command)) return;
    lv_obj_t *popup = s_hotend_activate_popup;
    uint32_t epoch = s_hotend_epoch, owner = s_hotend_owner;
    char selected_name[MOONRAKER_HOTEND_NAME_MAX];
    snprintf(selected_name, sizeof(selected_name), "%s", s_hotend_confirm_name);
    s_hotend_dispatching = true;
    lv_obj_add_state(s_hotend_confirm_button, LV_STATE_DISABLED);
    bool accepted = s_hotend_send && s_hotend_send(command);
    s_hotend_dispatching = false;
    /* Keep a submitted selection pending even if BACK/cleanup closed the UI.
     * Reopening the list must not submit the same outstanding action again. */
    if (accepted && owner == moonraker_config_generation()) {
        s_hotend_pending = true;
        s_hotend_pending_owner = owner;
        snprintf(s_hotend_pending_name, sizeof(s_hotend_pending_name), "%s", selected_name);
        s_hotend_sent_at = lv_tick_get();
    }
    if (s_hotend_epoch != epoch || s_hotend_activate_popup != popup ||
        owner != moonraker_config_generation()) return;
    if (!accepted) {
        lv_label_set_text(s_hotend_confirm_status, "Not sent. Check connection and retry.");
        lv_obj_remove_state(s_hotend_confirm_button, LV_STATE_DISABLED);
        return;
    }
    lv_label_set_text(s_hotend_confirm_status, "Sent. Waiting for active-hotend confirmation...");
}
static void hotend_activate_event_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !s_hotend_list_popup || s_hotend_dispatching || s_hotend_pending) return;
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    moonraker_state_snapshot(&s_hotend_list_state);
    const moonraker_hotend_t *hotend = hotend_row(index);
    if (!hotend_live_here() || printer_connection_reason(&s_hotend_list_state)[0] || !hotend || hotend->active || !hotend_name_safe(hotend->object_name) ||
        printer_controller_is_live_state(s_hotend_list_state.printer_state)) return;
    if (s_hotend_activate_popup) { lv_obj_move_foreground(s_hotend_activate_popup); return; }
    s_hotend_activate_index = index;
    snprintf(s_hotend_confirm_name, sizeof(s_hotend_confirm_name), "%s", hotend->object_name);
    s_hotend_activate_popup = ui_popup_create(lv_layer_top(), control_width(560),
        control_height(300), UI_POPUP_STANDARD);
    if (!s_hotend_activate_popup) return;
    ++s_hotend_epoch;
    lv_obj_add_event_cb(s_hotend_activate_popup, hotend_activate_deleted, LV_EVENT_DELETE, NULL);
    char title[48], text[180];
    snprintf(title, sizeof(title), "Activate T%u?", (unsigned)index);
    lv_obj_t *body = control_layout(s_hotend_activate_popup, title);
    snprintf(text, sizeof(text), "Make %s the active Klipper hotend?\n\n"
        "This does not move or park a physical toolchanger.", s_hotend_confirm_name);
    lv_obj_t *label = lv_label_create(body);
    lv_label_set_text(label, text);
    ui_apply_custom_label_style(label, UI_FONT_BODY, UI_TEXT);
    lv_obj_set_width(label, LV_PCT(100));
    s_hotend_confirm_status = lv_label_create(body);
    lv_label_set_text(s_hotend_confirm_status, "");
    ui_apply_custom_label_style(s_hotend_confirm_status, UI_FONT_CAPTION, UI_TEXT);
    lv_obj_set_width(s_hotend_confirm_status, LV_PCT(100));
    lv_obj_t *footer = control_footer(s_hotend_activate_popup);
    lv_obj_t *back = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_LEFT " Back", 0, 0, 170, 48, close_hotend_activate_cb, NULL, NULL);
    s_hotend_confirm_button = ui_popup_add_action_at(footer, UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_OK " Activate", 0, 0, 170, 48, confirm_hotend_activate_cb, NULL, NULL);
    lv_obj_set_flex_grow(back, 1); lv_obj_set_flex_grow(s_hotend_confirm_button, 1);
}
static void refresh_hotend_list(lv_timer_t *timer)
{
    (void)timer;
    if (!s_hotend_list_popup) return;
    if (s_hotend_owner != moonraker_config_generation()) {
        close_hotend_list_cb(NULL);
        return;
    }
    moonraker_state_snapshot(&s_hotend_list_state);
    bool live = hotend_live_here() && !printer_connection_reason(&s_hotend_list_state)[0];
    bool activation_locked = printer_controller_is_live_state(s_hotend_list_state.printer_state);
    if (s_hotend_pending) {
        bool confirmed = false;
        size_t count = s_hotend_list_state.hotend_count;
        if (count > MOONRAKER_MAX_HOTENDS) count = MOONRAKER_MAX_HOTENDS;
        for (size_t i = 0; live && i < count; ++i)
            if (s_hotend_list_state.hotends[i].active &&
                !strcmp(s_hotend_list_state.hotends[i].object_name, s_hotend_pending_name))
                confirmed = true;
        if (s_hotend_pending_owner != moonraker_config_generation() || confirmed ||
            lv_tick_elaps(s_hotend_sent_at) >= 5000) {
            s_hotend_pending = false;
            s_hotend_pending_name[0] = 0;
            if (confirmed) close_hotend_activate_cb(NULL);
            else if (s_hotend_confirm_status) lv_label_set_text(s_hotend_confirm_status,
                "Selection not confirmed. Check printer before retrying.");
        }
    }
    for (size_t i = 0; i < s_hotend_row_count; ++i) {
        const moonraker_hotend_t *hotend = live ? hotend_row(i) : NULL;
        if (!hotend) {
            ui_value_set_text(s_hotend_temp_labels[i], s_hotend_owner == moonraker_config_generation() ?
                "Unavailable - reopen" : "Printer changed - reopen");
            lv_obj_add_state(s_hotend_temp_buttons[i], LV_STATE_DISABLED);
            lv_obj_add_state(s_hotend_activate_buttons[i], LV_STATE_DISABLED);
            continue;
        }
        char name[64], temperature[64];
        snprintf(name, sizeof(name), "T%u %s(%s)", (unsigned)i, hotend->active ? "Active " : "", hotend->object_name);
        ui_value_set_text(s_hotend_name_labels[i], name);
        ui_value_set_color(s_hotend_name_labels[i], hotend->active ? UI_OK_BRIGHT : UI_TEXT_DIM, 0);
        if (hotend->temperature > -100.0) snprintf(temperature, sizeof(temperature), "%.1f / %.1f C", hotend->temperature, hotend->target);
        else snprintf(temperature, sizeof(temperature), "-- / -- C");
        ui_value_set_text(s_hotend_temp_labels[i], temperature);
        ui_value_set_text(s_hotend_activate_labels[i], hotend->active ? ui_text(LV_SYMBOL_OK " Active") : ui_text("Make active"));
        bool disabled = hotend->active || activation_locked || s_hotend_pending ||
            s_hotend_dispatching || !hotend_name_safe(hotend->object_name);
        if (disabled) lv_obj_add_state(s_hotend_activate_buttons[i], LV_STATE_DISABLED);
        else lv_obj_remove_state(s_hotend_activate_buttons[i], LV_STATE_DISABLED);
        ui_value_set_color(s_hotend_temp_labels[i], UI_TEXT_BRIGHT, 0);
        if (hotend_name_safe(hotend->object_name)) lv_obj_remove_state(s_hotend_temp_buttons[i], LV_STATE_DISABLED);
        else lv_obj_add_state(s_hotend_temp_buttons[i], LV_STATE_DISABLED);
        /* ui_button owns disabled opacity for the active theme. Comparing
         * resolved disabled opacity with a base selector causes Studio to
         * invalidate the button on every unchanged telemetry refresh. */
    }
    if (s_hotend_confirm_button) {
        const moonraker_hotend_t *hotend = hotend_row(s_hotend_activate_index);
        if (live && hotend && !hotend->active && !activation_locked &&
            !s_hotend_pending && !s_hotend_dispatching && hotend_name_safe(hotend->object_name) &&
            !strcmp(hotend->object_name, s_hotend_confirm_name))
            lv_obj_remove_state(s_hotend_confirm_button, LV_STATE_DISABLED);
        else lv_obj_add_state(s_hotend_confirm_button, LV_STATE_DISABLED);
    }
}
static void hotend_list_deleted(lv_event_t *event)
{
    if (lv_event_get_target(event) != s_hotend_list_popup) return;
    s_hotend_list_popup = NULL;
    close_hotend_activate_cb(NULL);
    if (s_hotend_refresh_timer) lv_timer_delete(s_hotend_refresh_timer);
    s_hotend_refresh_timer = NULL;
    s_hotend_row_count = 0;
    s_hotend_send = NULL;
    memset(s_hotend_row_names, 0, sizeof(s_hotend_row_names));
    memset(s_hotend_name_labels, 0, sizeof(s_hotend_name_labels));
    memset(s_hotend_temp_labels, 0, sizeof(s_hotend_temp_labels));
    memset(s_hotend_temp_buttons, 0, sizeof(s_hotend_temp_buttons));
    memset(s_hotend_activate_buttons, 0, sizeof(s_hotend_activate_buttons));
    memset(s_hotend_activate_labels, 0, sizeof(s_hotend_activate_labels));
}
static void close_hotend_list_cb(lv_event_t *event)
{
    (void)event;
    if (s_hotend_list_popup) lv_obj_delete(s_hotend_list_popup);
}

static void hotend_temperature_event_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !s_hotend_list_popup || s_hotend_dispatching || s_hotend_pending) return;

    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    moonraker_state_snapshot(&s_hotend_list_state);
    const moonraker_hotend_t *selected = hotend_row(index);
    if (!hotend_live_here() || !selected || !hotend_name_safe(selected->object_name)) return;
    moonraker_hotend_t hotend = *selected;

    close_hotend_list_cb(NULL);

    static const char *labels[] = {
        "180 C",
        "200 C",
        "215 C",
        "230 C",
        "250 C",
        "Off",
    };

    static const double values[] = {
        180,
        200,
        215,
        230,
        250,
        0,
    };

    const int targets[] = {
        180,
        200,
        215,
        230,
        250,
        0,
    };

    snprintf(
        s_hotend_title,
        sizeof(s_hotend_title),
        "T%u temperature",
        (unsigned)index);

    snprintf(
        s_hotend_custom_prefix,
        sizeof(s_hotend_custom_prefix),
        "SET_HEATER_TEMPERATURE HEATER=%s TARGET=",
        hotend.object_name);

    for (size_t i = 0; i < 6; ++i) {
        snprintf(
            s_hotend_commands[i],
            sizeof(s_hotend_commands[i]),
            "SET_HEATER_TEMPERATURE HEATER=%s TARGET=%d",
            hotend.object_name,
            targets[i]);

        s_hotend_command_ptrs[i] =
            s_hotend_commands[i];
    }

    show_control_popup(
        s_hotend_title,
        hotend.temperature,
        hotend.target,
        "C",
        true,
        s_hotend_command_ptrs,
        labels,
        values,
        6,
        s_hotend_title,
        s_hotend_custom_prefix,
        300);
}


static const int32_t hotend_columns_wide[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
static const int32_t hotend_columns_narrow[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
static const int32_t hotend_rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
static void hotend_row_resize(lv_event_t *event)
{
    lv_obj_t *row = lv_event_get_target_obj(event);
    bool wide = lv_obj_get_content_width(row) >= ui_theme_density_metric(520, 560, 600);
    const int32_t *columns = wide ? hotend_columns_wide : hotend_columns_narrow;
    if (lv_obj_get_style_grid_column_dsc_array(row, 0) == columns) return;
    lv_obj_set_grid_dsc_array(row, columns, hotend_rows);
    lv_obj_t *info = lv_obj_get_child(row, 0), *actions = lv_obj_get_child(row, 1);
    lv_obj_set_grid_cell(info, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
    lv_obj_set_width(actions, wide ? ui_theme_density_metric(320, 350, 390) : LV_PCT(100));
    lv_obj_set_grid_cell(actions, wide ? LV_GRID_ALIGN_END : LV_GRID_ALIGN_STRETCH,
        wide ? 1 : 0, 1, LV_GRID_ALIGN_CENTER, wide ? 0 : 1, 1);
}
void ui_printer_popups_show_hotends(ui_printer_popups_send_gcode_cb_t send_cb,
    const moonraker_state_t *state)
{
    if (!state || state->hotend_count == 0 || s_hotend_dispatching) return;
    if (s_hotend_list_popup && s_hotend_owner != moonraker_config_generation())
        close_hotend_list_cb(NULL);
    if (s_hotend_list_popup) { lv_obj_move_foreground(s_hotend_list_popup); return; }
    s_send_gcode_cb = send_cb;
    s_hotend_list_state = *state;
    s_hotend_send = s_control_send;
    s_hotend_owner = moonraker_config_generation();
    s_hotend_row_count = state->hotend_count < MOONRAKER_MAX_HOTENDS ? state->hotend_count : MOONRAKER_MAX_HOTENDS;
    s_hotend_list_popup = ui_popup_create(lv_layer_top(), control_width(760), control_height(500), UI_POPUP_STANDARD);
    if (!s_hotend_list_popup) { s_hotend_row_count = 0; return; }
    lv_obj_add_event_cb(s_hotend_list_popup, hotend_list_deleted, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = control_layout(s_hotend_list_popup, ui_text("Hotend control"));
    static const int32_t action_columns[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t action_rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    for (size_t i = 0; i < s_hotend_row_count; ++i) {
        memcpy(s_hotend_row_names[i], state->hotends[i].object_name, sizeof(s_hotend_row_names[i]));
        s_hotend_row_names[i][sizeof(s_hotend_row_names[i]) - 1] = '\0';
        lv_obj_t *row = lv_obj_create(body);
        ui_apply_surface_role(row, UI_SURFACE_SECTION);
        if (ui_theme_is_studio()) {
            ui_apply_surface_role(row, UI_SURFACE_TRANSPARENT);
            ui_apply_divider_border(row, LV_BORDER_SIDE_BOTTOM);
        }
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(row, 12, 0);
        lv_obj_set_style_pad_column(row, UI_GAP_CARD, 0);
        lv_obj_set_style_pad_row(row, UI_GAP_ROW, 0);
        lv_obj_add_event_cb(row, hotend_row_resize, LV_EVENT_SIZE_CHANGED, NULL);
        lv_obj_t *info = lv_obj_create(row);
        lv_obj_remove_style_all(info);
        lv_obj_set_height(info, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(info, UI_GAP_ROW, 0);
        s_hotend_name_labels[i] = lv_label_create(info);
        lv_label_set_text(s_hotend_name_labels[i], "");
        ui_apply_custom_label_style(s_hotend_name_labels[i], UI_FONT_CAPTION, UI_TEXT_DIM);
        lv_obj_set_width(s_hotend_name_labels[i], LV_PCT(100));
        s_hotend_temp_labels[i] = lv_label_create(info);
        lv_label_set_text(s_hotend_temp_labels[i], "");
        ui_apply_custom_label_style(s_hotend_temp_labels[i], UI_FONT_BODY_LARGE, UI_TEXT_BRIGHT);
        lv_obj_set_width(s_hotend_temp_labels[i], LV_PCT(100));
        lv_obj_t *actions = lv_obj_create(row);
        lv_obj_remove_style_all(actions);
        lv_obj_set_height(actions, LV_SIZE_CONTENT);
        lv_obj_set_grid_dsc_array(actions, action_columns, action_rows);
        lv_obj_set_style_pad_column(actions, UI_GAP_CARD, 0);
        s_hotend_temp_buttons[i] = ui_popup_add_action_at(actions, UI_POPUP_ACTION_CHOICE,
            ui_text("Set temp"), 0, 0, 1, ui_theme_is_studio() ? 72 : 52,
            hotend_temperature_event_cb, (void *)(uintptr_t)i, NULL);
        s_hotend_activate_buttons[i] = ui_popup_add_action_at(actions, UI_POPUP_ACTION_CONFIRM,
            ui_text("Make active"), 0, 0, 1, ui_theme_is_studio() ? 72 : 52,
            hotend_activate_event_cb, (void *)(uintptr_t)i,
            &s_hotend_activate_labels[i]);
        lv_obj_set_grid_cell(s_hotend_temp_buttons[i], LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
        lv_obj_set_grid_cell(s_hotend_activate_buttons[i], LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 0, 1);
    }
    lv_obj_t *footer = control_footer(s_hotend_list_popup);
    ui_popup_add_action_at(footer, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " Close",
        0, 0, 170, 48, close_hotend_list_cb, NULL, NULL);
    refresh_hotend_list(NULL);
    s_hotend_refresh_timer = lv_timer_create(refresh_hotend_list, 500, NULL);
}

void ui_printer_popups_show_part_fan(
    ui_printer_popups_send_gcode_cb_t send_cb,
    double current_fan_percent)
{
    if (s_control_dispatching) return;
    s_send_gcode_cb = send_cb;

    static const char *cmds[] = {
        "M106 S0",
        "M106 S64",
        "M106 S128",
        "M106 S191",
        "M106 S255"
    };

    static const char *labels[] = {
        "Off",
        "25%",
        "50%",
        "75%",
        "100%"
    };

    static const double values[] = {0, 25, 50, 75, 100};

    show_control_popup("Part cooling fan",
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
    if (s_control_dispatching) return;
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
        "Off"
    };

    static const double values[] = {180, 200, 215, 230, 250, 0};

    show_control_popup("Nozzle temperature",
                       current_temp,
                       target_temp,
                       "C",
                       true,
                       cmds,
                       labels,
                       values,
                       6,
                       "Custom nozzle temperature",
                       "M104 S",
                       300);
}

void ui_printer_popups_show_bed(
    ui_printer_popups_send_gcode_cb_t send_cb,
    double current_temp,
    double target_temp)
{
    if (s_control_dispatching) return;
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
        "Off"
    };

    static const double values[] = {50, 60, 70, 80, 100, 0};

    show_control_popup("Bed temperature",
                       current_temp,
                       target_temp,
                       "C",
                       true,
                       cmds,
                       labels,
                       values,
                       6,
                       "Custom bed temperature",
                       "M140 S",
                       120);
}

static const char *filament_sensor_display_name(
    const char *object_name)
{
    if (!object_name) return "Sensor";

    const char *separator =
        strchr(object_name, ' ');

    return separator && separator[1]
        ? separator + 1
        : object_name;
}


static bool filament_sensor_command_name_is_safe(
    const char *name)
{
    if (!name || !name[0]) return false;

    for (const unsigned char *cursor =
             (const unsigned char *)name;
         *cursor;
         ++cursor) {
        bool safe =
            (*cursor >= 'a' && *cursor <= 'z') ||
            (*cursor >= 'A' && *cursor <= 'Z') ||
            (*cursor >= '0' && *cursor <= '9') ||
            *cursor == '_' ||
            *cursor == '-' ||
            *cursor == '.';

        if (!safe) return false;
    }

    return true;
}


static void filament_list_popup_deleted_cb(lv_event_t *event)
{
    if (!event ||
        lv_event_get_target(event) != s_filament_list_popup) {
        return;
    }

    s_filament_list_popup = NULL;
    s_filament_send = NULL;
    ++s_filament_epoch;
    s_filament_row_count = 0;
    memset(s_filament_row_names, 0, sizeof(s_filament_row_names));

    if (s_filament_refresh_timer) {
        lv_timer_delete(s_filament_refresh_timer);
        s_filament_refresh_timer = NULL;
    }

    memset(
        &s_filament_list_state,
        0,
        sizeof(s_filament_list_state));
    memset(
        s_filament_status_labels,
        0,
        sizeof(s_filament_status_labels));
    memset(
        s_filament_toggle_buttons,
        0,
        sizeof(s_filament_toggle_buttons));
    memset(
        s_filament_toggle_labels,
        0,
        sizeof(s_filament_toggle_labels));
    memset(s_filament_feedback, 0, sizeof(s_filament_feedback));
}


static void close_filament_list_cb(lv_event_t *event)
{
    (void)event;

    if (s_filament_list_popup) {
        lv_obj_delete(s_filament_list_popup);
    }
}


static const moonraker_filament_sensor_t *filament_row_sensor(size_t row)
{
    size_t count = s_filament_list_state.sensor_count;
    if (count > MOONRAKER_MAX_FILAMENT_SENSORS) count = MOONRAKER_MAX_FILAMENT_SENSORS;
    for (size_t i = 0; i < count; ++i)
        if (!strcmp(s_filament_list_state.sensors[i].object_name, s_filament_row_names[row]))
            return &s_filament_list_state.sensors[i];
    return NULL;
}

static int filament_request(const char *name, uint32_t owner)
{
    for (size_t i = 0; i < MOONRAKER_MAX_FILAMENT_SENSORS; ++i)
        if (s_filament_requests[i].pending && s_filament_requests[i].owner == owner &&
            !strcmp(s_filament_requests[i].name, name)) return (int)i;
    return -1;
}

static void refresh_filament_list(lv_timer_t *timer)
{
    (void)timer;
    if (!s_filament_list_popup) return;
    moonraker_filament_state_snapshot(&s_filament_list_state);
    moonraker_state_t state;
    moonraker_state_snapshot(&state);
    const char *reason = printer_connection_reason(&state);
    bool owns = s_filament_owner == moonraker_config_generation();
    for (size_t i = 0; i < s_filament_row_count; ++i) {
        const moonraker_filament_sensor_t *sensor = owns && s_filament_list_state.discovered ? filament_row_sensor(i) : NULL;
        lv_obj_t *button = s_filament_toggle_buttons[i], *label = s_filament_toggle_labels[i];
        if (!sensor) {
            ui_value_set_text(s_filament_status_labels[i], owns ? "Unavailable - reopen" : "Printer changed - reopen");
            ui_value_set_color(s_filament_status_labels[i], UI_TEXT_DIM, 0);
            lv_obj_add_state(button, LV_STATE_DISABLED);
            continue;
        }
        int request = filament_request(sensor->object_name, s_filament_owner);
        if (request >= 0) {
            if (!reason[0] && sensor->enabled == s_filament_requests[request].expected) {
                s_filament_requests[request].pending = false;
                s_filament_feedback[i][0] = 0;
                request = -1;
            } else if (lv_tick_elaps(s_filament_requests[request].sent_at) >= 5000) {
                s_filament_requests[request].pending = false;
                snprintf(s_filament_feedback[i], sizeof(s_filament_feedback[i]),
                    "Update unconfirmed. Check printer before retrying.");
                request = -1;
            }
        }
        const char *status = reason[0] ? "Printer unavailable" : s_filament_feedback[i][0] ? s_filament_feedback[i] :
            !sensor->enabled ? "Disabled" : !sensor->status_known ? "Checking" :
            sensor->filament_detected ? "Filament present" : "Runout";
        ui_value_set_text(s_filament_status_labels[i], status);
        ui_value_set_color(s_filament_status_labels[i], reason[0] || s_filament_feedback[i][0] ? UI_DANGER_BRIGHT :
            !sensor->enabled || !sensor->status_known ? UI_TEXT :
            sensor->filament_detected ? UI_OK_BRIGHT : UI_DANGER_BRIGHT, 0);
        bool changed = ui_value_set_text(label, request >= 0 ? ui_text("Updating") :
            sensor->enabled ? ui_text("Disable") : ui_text("Enable"));
        if (changed) ui_button_apply_kind(button, sensor->enabled ? UI_BUTTON_WARNING : UI_BUTTON_SUCCESS);
        if (request >= 0 || reason[0] || s_filament_dispatching ||
            (!s_control_send && !s_filament_send) ||
            !filament_sensor_command_name_is_safe(filament_sensor_display_name(sensor->object_name)))
            lv_obj_add_state(button, LV_STATE_DISABLED);
        else lv_obj_remove_state(button, LV_STATE_DISABLED);
    }
}

static void filament_toggle_event_cb(lv_event_t *event)
{
    if (!event || lv_event_get_code(event) != LV_EVENT_CLICKED ||
        !s_filament_list_popup || s_filament_dispatching) return;
    size_t index = (size_t)(uintptr_t)lv_event_get_user_data(event);
    if (index >= s_filament_row_count ||
        lv_event_get_target(event) != s_filament_toggle_buttons[index]) return;
    refresh_filament_list(NULL);
    if (s_filament_owner != moonraker_config_generation() ||
        lv_obj_has_state(s_filament_toggle_buttons[index], LV_STATE_DISABLED)) return;
    const moonraker_filament_sensor_t *sensor = filament_row_sensor(index);
    if (!sensor) return;
    char name[MOONRAKER_FILAMENT_SENSOR_NAME_MAX];
    snprintf(name, sizeof(name), "%s", sensor->object_name);
    const char *sensor_name = filament_sensor_display_name(name);
    if (!filament_sensor_command_name_is_safe(sensor_name)) return;
    bool enable = !sensor->enabled;
    char command[MOONRAKER_FILAMENT_SENSOR_NAME_MAX + 48];
    int written = snprintf(command, sizeof(command),
        "SET_FILAMENT_SENSOR SENSOR=%s ENABLE=%u", sensor_name, enable ? 1U : 0U);
    if (written <= 0 || (size_t)written >= sizeof(command)) return;
    int slot = -1;
    for (size_t i = 0; i < MOONRAKER_MAX_FILAMENT_SENSORS; ++i) {
        if (!s_filament_requests[i].pending || s_filament_requests[i].owner != s_filament_owner ||
            lv_tick_elaps(s_filament_requests[i].sent_at) >= 5000) { slot = (int)i; break; }
    }
    if (slot < 0) return;
    uint32_t owner = s_filament_owner, epoch = s_filament_epoch;
    bool (*checked_send)(const char *) = s_control_send;
    ui_printer_popups_send_gcode_cb_t legacy_send = s_filament_send;
    s_filament_dispatching = true;
    bool accepted = false;
    if (checked_send) accepted = checked_send(command);
    else if (legacy_send) { legacy_send(command); accepted = true; }
    s_filament_dispatching = false;
    if (accepted) {
        s_filament_requests[slot].pending = true;
        s_filament_requests[slot].expected = enable;
        s_filament_requests[slot].owner = owner;
        s_filament_requests[slot].sent_at = lv_tick_get();
        snprintf(s_filament_requests[slot].name, sizeof(s_filament_requests[slot].name), "%s", name);
    }
    /* Sender callbacks may close, reopen, or switch printers. */
    if (!s_filament_list_popup || epoch != s_filament_epoch ||
        owner != moonraker_config_generation()) return;
    snprintf(s_filament_feedback[index], sizeof(s_filament_feedback[index]), "%s",
        accepted ? "" : "Send failed. Check connection and retry.");
    refresh_filament_list(NULL);
}


void ui_printer_popups_show_filament_sensors(ui_printer_popups_send_gcode_cb_t send_cb,
    const moonraker_filament_state_t *state)
{
    if (!state || !state->discovered || state->total_count == 0) return;
    if (s_filament_list_popup && s_filament_owner != moonraker_config_generation())
        close_filament_list_cb(NULL);
    if (s_filament_list_popup) { lv_obj_move_foreground(s_filament_list_popup); refresh_filament_list(NULL); return; }
    ++s_filament_epoch;
    s_filament_send = send_cb;
    s_filament_list_state = *state;
    s_filament_owner = moonraker_config_generation();
    s_filament_row_count = state->sensor_count < MOONRAKER_MAX_FILAMENT_SENSORS ?
        state->sensor_count : MOONRAKER_MAX_FILAMENT_SENSORS;
    s_filament_list_popup = ui_popup_create(lv_layer_top(), control_width(720),
        control_height(480), UI_POPUP_STANDARD);
    if (!s_filament_list_popup) { s_filament_send = NULL; s_filament_row_count = 0; return; }
    lv_obj_add_event_cb(s_filament_list_popup, filament_list_popup_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_t *body = control_layout(s_filament_list_popup, ui_text("Filament sensors"));
    static const int32_t columns[] = {LV_GRID_FR(1), LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    for (size_t i = 0; i < s_filament_row_count; ++i) {
        const moonraker_filament_sensor_t *sensor = &state->sensors[i];
        memcpy(s_filament_row_names[i], sensor->object_name, sizeof(s_filament_row_names[i]));
        s_filament_row_names[i][sizeof(s_filament_row_names[i]) - 1] = '\0';
        lv_obj_t *row = lv_obj_create(body);
        ui_apply_surface_role(row, UI_SURFACE_SECTION);
        if (ui_theme_is_studio()) {
            ui_apply_surface_role(row, UI_SURFACE_TRANSPARENT);
            ui_apply_divider_border(row, LV_BORDER_SIDE_BOTTOM);
        }
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(row, 12, 0);
        lv_obj_set_style_pad_column(row, UI_GAP_CARD, 0);
        lv_obj_set_grid_dsc_array(row, columns, rows);
        lv_obj_t *info = lv_obj_create(row);
        lv_obj_remove_style_all(info);
        lv_obj_set_height(info, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(info, UI_GAP_ROW, 0);
        lv_obj_set_grid_cell(info, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_START, 0, 1);
        char name[96];
        bool motion = strncmp(sensor->object_name, "filament_motion_sensor ", strlen("filament_motion_sensor ")) == 0;
        snprintf(name, sizeof(name), "%s (%s)", filament_sensor_display_name(sensor->object_name), motion ? "motion" : "switch");
        lv_obj_t *label = lv_label_create(info);
        lv_label_set_text(label, name);
        ui_apply_custom_label_style(label, UI_FONT_CAPTION, UI_TEXT_DIM);
        lv_obj_set_width(label, LV_PCT(100));
        s_filament_status_labels[i] = lv_label_create(info);
        lv_label_set_text(s_filament_status_labels[i], ui_text("Checking"));
        ui_apply_custom_label_style(s_filament_status_labels[i], UI_FONT_BODY_LARGE, UI_TEXT);
        lv_obj_set_width(s_filament_status_labels[i], LV_PCT(100));
        s_filament_toggle_buttons[i] = ui_popup_add_action_at(row,
            sensor->enabled ? UI_POPUP_ACTION_CHOICE : UI_POPUP_ACTION_CONFIRM,
            sensor->enabled ? ui_text("Disable") : ui_text("Enable"), 0, 0,
            ui_theme_density_metric(150, 170, 190), 52, filament_toggle_event_cb,
            (void *)(uintptr_t)i, &s_filament_toggle_labels[i]);
        ui_button_apply_kind(s_filament_toggle_buttons[i], sensor->enabled ? UI_BUTTON_WARNING : UI_BUTTON_SUCCESS);
        lv_obj_set_grid_cell(s_filament_toggle_buttons[i], LV_GRID_ALIGN_END, 1, 1, LV_GRID_ALIGN_CENTER, 0, 1);
    }
    if (state->truncated) {
        lv_obj_t *note = lv_label_create(body);
        lv_label_set_text(note, ui_text("Additional sensors are not shown."));
        ui_apply_custom_label_style(note, UI_FONT_CAPTION, UI_TEXT_MUTED);
        lv_obj_set_width(note, LV_PCT(100));
    }
    lv_obj_t *footer = control_footer(s_filament_list_popup);
    ui_popup_add_action_at(footer, UI_POPUP_ACTION_CLOSE, LV_SYMBOL_CLOSE " Close",
        0, 0, 170, 48, close_filament_list_cb, NULL, NULL);
    refresh_filament_list(NULL);
    s_filament_refresh_timer = lv_timer_create(refresh_filament_list, 500, NULL);
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
    char body[1024];

    snprintf(body,
             sizeof(body),
             "State: %s\n"
             "File: %.255s\n"
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

    ui_dashboard_status_popup_show(
        "PRINTER STATUS",
        body);
}

void ui_printer_popups_close_all(void)
{
    ui_printer_toolhead_close();
    ui_filament_recovery_close();
    ui_dashboard_status_popup_close();
    close_custom_temp_cb(NULL);
    if (s_control_popup) lv_obj_delete(s_control_popup);

    close_hotend_list_cb(NULL);
    close_filament_list_cb(NULL);

    close_object_confirm_cb(NULL);
    close_object_list_cb(NULL);

    if (s_cancel_confirm_popup) {
        lv_obj_delete(s_cancel_confirm_popup);
        s_cancel_confirm_popup = NULL;
    }
}
