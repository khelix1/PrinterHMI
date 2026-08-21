#include "ui_event_history_popup.h"
#include "ui_text.h"

#include <stdio.h>
#include <time.h>

#include "lvgl.h"
#include "operator_event_log.h"
#include "ui_popup.h"
#include "ui_theme.h"

static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_list = NULL;
static lv_timer_t *s_refresh_timer = NULL;
static size_t s_rendered_count = 0;
static uint32_t s_rendered_sequence = 0;


static const char *event_level_text(
    operator_event_level_t level)
{
    switch (level) {
    case OPERATOR_EVENT_ERROR:
        return "ERROR";

    case OPERATOR_EVENT_WARNING:
        return "WARN";

    case OPERATOR_EVENT_INFO:
    default:
        return "INFO";
    }
}


static lv_color_t event_level_color(
    operator_event_level_t level)
{
    switch (level) {
    case OPERATOR_EVENT_ERROR:
        return UI_DANGER_BRIGHT;

    case OPERATOR_EVENT_WARNING:
        return UI_WARN;

    case OPERATOR_EVENT_INFO:
    default:
        return UI_TEXT_BRIGHT;
    }
}


static void format_event_time(
    const operator_event_t *event,
    char *output,
    size_t output_size)
{
    if (!event || !output || output_size == 0) {
        return;
    }

    if (event->timestamp >= 1700000000) {
        struct tm local_time = {0};

        localtime_r(
            &event->timestamp,
            &local_time);

        strftime(
            output,
            output_size,
            "%I:%M:%S %p",
            &local_time);
        return;
    }

    uint32_t seconds = event->uptime_seconds;
    uint32_t hours = seconds / 3600U;
    uint32_t minutes = (seconds / 60U) % 60U;
    seconds %= 60U;

    snprintf(
        output,
        output_size,
        "+%02u:%02u:%02u",
        (unsigned)hours,
        (unsigned)minutes,
        (unsigned)seconds);
}


static void rebuild_event_list(void)
{
    if (!s_list) {
        return;
    }

    lv_obj_clean(s_list);

    size_t count = operator_event_log_count();
    s_rendered_count = count;
    s_rendered_sequence = 0;

    operator_event_t newest;

    if (count > 0 &&
        operator_event_log_get(0, &newest)) {
        s_rendered_sequence = newest.sequence;
    }

    if (count == 0) {
        lv_obj_t *empty = lv_label_create(s_list);

        lv_label_set_text(
            empty,
            ui_text("No operator events have been recorded."));

        lv_obj_set_width(empty, 680);
        lv_obj_set_pos(empty, 16, 20);

        ui_apply_custom_label_style(
            empty,
            UI_FONT_BODY,
            UI_TEXT_DIM);
        return;
    }

    lv_obj_t *last_label = NULL;

    /*
     * The event owner returns newest-first. Reverse that index here so the
     * operator history reads naturally from oldest at the top to newest at
     * the bottom.
     */
    for (size_t row = 0; row < count; ++row) {
        size_t newest_index =
            count - 1 - row;

        operator_event_t event;

        if (!operator_event_log_get(
                newest_index,
                &event)) {
            continue;
        }

        char time_text[24] = "";
        char line[176];

        format_event_time(
            &event,
            time_text,
            sizeof(time_text));

        snprintf(
            line,
            sizeof(line),
            "%s  %-5s  %s",
            time_text,
            event_level_text(event.level),
            event.message);

        lv_obj_t *label = lv_label_create(s_list);

        lv_label_set_text(label, line);
        lv_label_set_long_mode(
            label,
            LV_LABEL_LONG_WRAP);

        lv_obj_set_size(label, 680, 48);
        lv_obj_set_pos(
            label,
            16,
            (int32_t)row * 52 + 10);

        ui_apply_custom_label_style(
            label,
            UI_FONT_CAPTION,
            event_level_color(event.level));

        last_label = label;
    }

    /*
     * Keep the newest appended event visible.
     */
    lv_obj_update_layout(s_list);

    if (last_label) {
        lv_obj_scroll_to_view(
            last_label,
            LV_ANIM_OFF);
    }
}


static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_popup || !s_list) {
        return;
    }

    size_t count = operator_event_log_count();
    uint32_t newest_sequence = 0;
    operator_event_t newest;

    if (count > 0 &&
        operator_event_log_get(0, &newest)) {
        newest_sequence = newest.sequence;
    }

    /*
     * Sequence comparison also detects replacement of the oldest entry
     * after the bounded ring reaches capacity and its count stays at 40.
     */
    if (count != s_rendered_count ||
        newest_sequence != s_rendered_sequence) {
        rebuild_event_list();
    }
}


void ui_event_history_popup_close(void)
{
    if (s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }

    if (s_popup) {
        lv_obj_delete(s_popup);
    }

    s_popup = NULL;
    s_list = NULL;
    s_rendered_count = 0;
    s_rendered_sequence = 0;
}


static void close_cb(lv_event_t *event)
{
    (void)event;
    ui_event_history_popup_close();
}


static void clear_cb(lv_event_t *event)
{
    (void)event;
    operator_event_log_clear();
    rebuild_event_list();
}


void ui_event_history_popup_show(void)
{
    if (s_popup) {
        rebuild_event_list();
        lv_obj_move_foreground(s_popup);
        return;
    }

    s_popup = ui_popup_create(
        lv_layer_top(),
        800,
        500,
        UI_POPUP_STANDARD);

    if (!s_popup) {
        return;
    }

    ui_popup_add_title(
        s_popup,
        ui_text("OPERATOR EVENT HISTORY"),
        false,
        0);

    ui_popup_add_header_divider(
        s_popup,
        44);

    s_list = ui_popup_add_list(
        s_popup,
        24,
        58,
        752,
        344);

    ui_popup_add_standard_footer_divider(
        s_popup);

    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        220,
        UI_POPUP_FOOTER_LEFT,
        close_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_TRASH " CLEAR",
        220,
        UI_POPUP_FOOTER_RIGHT,
        clear_cb,
        NULL,
        NULL);

    rebuild_event_list();

    if (!s_refresh_timer) {
        s_refresh_timer =
            lv_timer_create(
                refresh_timer_cb,
                500,
                NULL);
    }

    lv_obj_move_foreground(s_popup);
}
