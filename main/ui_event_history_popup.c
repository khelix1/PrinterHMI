#include "ui_event_history_popup.h"
#include "ui_text.h"

#include <stdio.h>
#include <time.h>

#include "lvgl.h"
#include "operator_event_log.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_button.h"
#include "ui_responsive_layout.h"
#include "ui_text_fit.h"

static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_list = NULL;
static uint32_t s_history_epoch;
static bool s_rebuilding;
static lv_timer_t *s_refresh_timer = NULL;
static size_t s_rendered_count = 0;
static uint32_t s_rendered_sequence = 0;
static lv_obj_t *s_follow_button;
static bool s_follow_live = true;


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


static void rebuild_event_list_impl(void)
{
    if (!s_list) {
        return;
    }

    uint32_t epoch=s_history_epoch;lv_obj_t *list=s_list;
    lv_obj_clean(s_list);
    if(epoch!=s_history_epoch || s_list!=list || !s_popup)return;

    size_t count = operator_event_log_count();
    if(epoch!=s_history_epoch || s_list!=list || !s_popup)return;
    if(count>OPERATOR_EVENT_LOG_CAPACITY)count=OPERATOR_EVENT_LOG_CAPACITY;
    s_rendered_count = count;
    s_rendered_sequence = 0;

    operator_event_t newest;

    if (count > 0 &&
        operator_event_log_get(0, &newest)) {
        if(epoch!=s_history_epoch || s_list!=list || !s_popup)return;
        s_rendered_sequence = newest.sequence;
    }

    if(epoch!=s_history_epoch || s_list!=list || !s_popup)return;
    if (count == 0) {
        lv_obj_t *empty = lv_label_create(s_list);

        lv_label_set_text(
            empty,
            ui_text("No operator events have been recorded."));

        lv_obj_set_width(empty, LV_PCT(100));
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);

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

        bool got=operator_event_log_get(newest_index,&event);
        if(epoch!=s_history_epoch || s_list!=list || !s_popup)return;
        if(!got)continue;

        char time_text[24] = "";
        char metadata[48];

        format_event_time(
            &event,
            time_text,
            sizeof(time_text));

        snprintf(metadata,sizeof(metadata),"%s  %s",time_text,event_level_text(event.level));
        lv_obj_t *row=lv_obj_create(s_list);
        lv_obj_remove_style_all(row);
        lv_obj_clear_flag(row,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row,LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(row,12,0);
        lv_obj_set_style_pad_row(row,6,0);
        lv_obj_set_style_border_width(row,1,0);
        lv_obj_set_style_border_color(row,UI_BORDER_SOFT,0);
        lv_obj_set_style_radius(row,UI_RADIUS_ACTION,0);
        lv_obj_t *meta=lv_label_create(row);
        lv_label_set_text(meta,metadata);lv_obj_set_width(meta,LV_PCT(100));
        lv_label_set_long_mode(meta,LV_LABEL_LONG_WRAP);
        ui_apply_custom_label_style(meta,UI_FONT_CAPTION,event_level_color(event.level));
        lv_obj_t *label=lv_label_create(row);
        lv_label_set_text(label,event.message);lv_obj_set_width(label,LV_PCT(100));
        lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
        ui_apply_custom_label_style(label,UI_FONT_BODY,UI_TEXT);
        last_label = row;
    }

    /*
     * Keep the newest appended event visible.
     */
    lv_obj_update_layout(s_list);

    if (last_label && s_follow_live) {
        lv_obj_scroll_to_view(
            last_label,
            LV_ANIM_OFF);
    }
}


static void rebuild_event_list(void)
{
    if(s_rebuilding)return;
    s_rebuilding=true;rebuild_event_list_impl();s_rebuilding=false;
}
static void history_field_deleted(lv_event_t *event)
{
    lv_obj_t *target=lv_event_get_target_obj(event);
    if(target==s_list){s_list=NULL;if(s_refresh_timer)lv_timer_delete(s_refresh_timer);s_refresh_timer=NULL;}
    else if(target==s_follow_button)s_follow_button=NULL;
    else return;
    ++s_history_epoch;
    s_rendered_count=SIZE_MAX;s_rendered_sequence=0;
}
static void refresh_timer_cb(lv_timer_t *timer)
{
    if(timer && timer!=s_refresh_timer)return;
    uint32_t epoch=s_history_epoch;
    if (!s_popup || !s_list || !s_follow_live) {
        return;
    }

    size_t count = operator_event_log_count();
    if(epoch!=s_history_epoch || !s_popup || !s_list)return;
    uint32_t newest_sequence = 0;
    operator_event_t newest;

    if (count > 0 &&
        operator_event_log_get(0, &newest)) {
        if(epoch!=s_history_epoch || !s_popup || !s_list)return;
        newest_sequence = newest.sequence;
    }

    /*
     * Sequence comparison also detects replacement of the oldest entry
     * after the bounded ring reaches capacity and its count stays at 40.
     */
    if(epoch!=s_history_epoch || !s_popup || !s_list)return;
    if (count != s_rendered_count ||
        newest_sequence != s_rendered_sequence) {
        rebuild_event_list();
    }
}


static void history_deleted(lv_event_t *event)
{
    if(lv_event_get_target_obj(event)!=s_popup)return;
    if(s_refresh_timer)lv_timer_delete(s_refresh_timer);
    ++s_history_epoch;
    s_refresh_timer=NULL;s_popup=NULL;s_list=NULL;s_follow_button=NULL;
    s_rendered_count=0;s_rendered_sequence=0;s_follow_live=true;
}

void ui_event_history_popup_close(void)
{
    if(s_popup)lv_obj_delete(s_popup);
}


static void close_cb(lv_event_t *event)
{
    (void)event;
    ui_event_history_popup_close();
}


static void clear_cb(lv_event_t *event)
{
    (void)event;if(!s_popup || !s_list)return;
    uint32_t epoch=s_history_epoch;
    operator_event_log_clear();
    if(epoch==s_history_epoch && s_popup && s_list)rebuild_event_list();
}


static void follow_cb(lv_event_t *event)
{
    if(event && lv_event_get_code(event)!=LV_EVENT_CLICKED)return;
    if(!s_popup || !s_list || !s_follow_button)return;
    s_follow_live=!s_follow_live;
    lv_label_set_text(lv_obj_get_child(s_follow_button,0),s_follow_live ? "HOLD" : "FOLLOW LIVE");
    if(s_follow_live)rebuild_event_list();
}
static void action_fit(lv_event_t *event)
{
    ui_text_fit_single_line(lv_obj_get_child(lv_event_get_target_obj(event),0),UI_FONT_BODY);
}
static lv_obj_t *history_action(lv_obj_t *footer,ui_button_kind_t kind,const char *text,lv_event_cb_t cb)
{
    lv_obj_t *button=ui_button_create(footer,kind,text);
    lv_obj_set_size(button,148,48);lv_obj_set_flex_grow(button,1);
    lv_obj_add_event_cb(button,action_fit,LV_EVENT_SIZE_CHANGED,NULL);
    lv_obj_add_event_cb(button,cb,LV_EVENT_CLICKED,NULL);
    return button;
}
void ui_event_history_popup_show(void)
{
    if(s_popup){lv_obj_move_foreground(s_popup);return;}
    int width=lv_display_get_horizontal_resolution(NULL)-32;
    int height=lv_display_get_vertical_resolution(NULL)-32;
    if(width>880)width=880;
    if(height>540)height=540;
    s_popup=ui_popup_create(lv_layer_top(),width,height,UI_POPUP_STANDARD);
    if(!s_popup)return;
    ++s_history_epoch;
    s_follow_live=true;
    lv_obj_add_event_cb(s_popup,history_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_flex_flow(s_popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_popup,16,0);lv_obj_set_style_pad_row(s_popup,12,0);
    lv_obj_t *title=lv_label_create(s_popup);
    lv_label_set_text(title,"EVENT HISTORY");lv_obj_set_width(title,LV_PCT(100));
    ui_apply_custom_label_style(title,UI_FONT_TITLE,UI_TEXT);
    lv_label_set_long_mode(title,LV_LABEL_LONG_DOT);
    s_list=lv_obj_create(s_popup);lv_obj_add_event_cb(s_list,history_field_deleted,LV_EVENT_DELETE,NULL);lv_obj_remove_style_all(s_list);
    lv_obj_set_size(s_list,LV_PCT(100),0);lv_obj_set_flex_grow(s_list,1);
    lv_obj_set_flex_flow(s_list,LV_FLEX_FLOW_COLUMN);lv_obj_set_style_pad_row(s_list,8,0);
    lv_obj_set_scroll_dir(s_list,LV_DIR_VER);lv_obj_set_scrollbar_mode(s_list,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_t *footer=lv_obj_create(s_popup);ui_responsive_column(footer);
    lv_obj_set_flex_flow(footer,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(footer,8,0);lv_obj_set_style_pad_row(footer,8,0);
    s_follow_button=history_action(footer,UI_BUTTON_OUTLINED,"HOLD",follow_cb);
    lv_obj_add_event_cb(s_follow_button,history_field_deleted,LV_EVENT_DELETE,NULL);
    history_action(footer,UI_BUTTON_DANGER,"CLEAR",clear_cb);
    history_action(footer,UI_BUTTON_CLOSE,"CLOSE",close_cb);
    uint32_t epoch=s_history_epoch;
    rebuild_event_list();
    if(epoch!=s_history_epoch || !s_popup || !s_list)return;
    lv_obj_update_layout(s_popup);
    for(unsigned i=0;i<lv_obj_get_child_count(footer);i++)
        ui_text_fit_single_line(lv_obj_get_child(lv_obj_get_child(footer,i),0),UI_FONT_BODY);
    s_refresh_timer=lv_timer_create(refresh_timer_cb,500,NULL);
    lv_obj_move_foreground(s_popup);
}
