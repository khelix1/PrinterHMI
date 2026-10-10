#include "ui_console.h"
#include "ui_text.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "console_controller.h"
#include "console_filter.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "moonraker.h"
#include "ui_button.h"
#include "ui_page_geometry.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_text_fit.h"
#include "ui_value_update.h"
#include "moonraker_config_controller.h"

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_output = NULL;
static lv_obj_t *s_connection = NULL;
static lv_obj_t *s_follow_button = NULL;
static lv_obj_t *s_follow_label = NULL;
static lv_obj_t *s_command_popup = NULL;
static lv_obj_t *s_command_input = NULL;
static lv_timer_t *s_refresh_timer = NULL;
static ui_console_command_cb_t s_command_callback = NULL;
static uint32_t s_rendered_sequence = 0;
static size_t s_rendered_count = 0;
static size_t s_history_cursor = SIZE_MAX;
typedef struct {
    lv_obj_t *rows[CONSOLE_LOG_CAPACITY];
    lv_obj_t *empty;
} console_row_store_t;
static console_row_store_t *s_rows;
static bool console_rows_init(void)
{
    if (s_rows) return true;
    s_rows = heap_caps_calloc(1, sizeof(*s_rows), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_rows) s_rows = heap_caps_calloc(1, sizeof(*s_rows), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    return s_rows != NULL;
}
static bool s_follow = true;
static lv_obj_t *s_header, *s_filters, *s_footer, *s_command_button;
static lv_obj_t *s_command_feedback;
static lv_obj_t *s_command_send, *s_command_keyboard, *s_command_actions;
static uint32_t s_command_owner;
static bool s_programmatic_scroll;
static uint32_t s_row_sequence[CONSOLE_LOG_CAPACITY];
static console_filter_kind_t s_filter = CONSOLE_FILTER_ALL;
static bool s_hide_temperatures;
static char s_query[64];
static lv_obj_t *s_filter_dropdown, *s_temperature_button, *s_temperature_label;
static lv_obj_t *s_search_label, *s_filter_count;
static lv_obj_t *s_search_popup, *s_search_input;



static lv_color_t entry_color(console_entry_type_t type)
{
    switch (type) {
    case CONSOLE_ENTRY_COMMAND:
        return UI_ACCENT_CYAN;

    case CONSOLE_ENTRY_WARNING:
        return UI_WARN;

    case CONSOLE_ENTRY_ERROR:
        return UI_DANGER_BRIGHT;

    case CONSOLE_ENTRY_SYSTEM:
        return UI_TEXT_DIM;

    case CONSOLE_ENTRY_RESPONSE:
    default:
        return UI_TEXT_BRIGHT;
    }
}


static const char *entry_prefix(console_entry_type_t type)
{
    switch (type) {
    case CONSOLE_ENTRY_COMMAND:
        return ">";

    case CONSOLE_ENTRY_WARNING:
        return "!";

    case CONSOLE_ENTRY_ERROR:
        return "!!";

    case CONSOLE_ENTRY_SYSTEM:
        return "*";

    case CONSOLE_ENTRY_RESPONSE:
    default:
        return "<";
    }
}


static void format_entry_time(
    const console_entry_t *entry,
    char *output,
    size_t output_size)
{
    if (!entry || !output || output_size == 0) {
        return;
    }

    if (entry->timestamp >= 1700000000) {
        struct tm local_time = {0};
        localtime_r(
            &entry->timestamp,
            &local_time);
        strftime(
            output,
            output_size,
            "%H:%M:%S",
            &local_time);
        return;
    }

    uint32_t seconds = entry->uptime_seconds;
    snprintf(
        output,
        output_size,
        "+%02u:%02u:%02u",
        (unsigned)(seconds / 3600U),
        (unsigned)((seconds / 60U) % 60U),
        (unsigned)(seconds % 60U));
}


static void rebuild_output(void)
{
    if (!s_output) return;
    int32_t scroll_y = lv_obj_get_scroll_y(s_output), anchor_y = 0;
    uint32_t anchor = 0;
    lv_area_t viewport; lv_obj_get_coords(s_output, &viewport);
    if (!s_follow) for (size_t i=0;i<s_rendered_count && i<CONSOLE_LOG_CAPACITY;i++) {
        lv_obj_t *row=s_rows->rows[i]; if(!row || lv_obj_has_flag(row,LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t area;lv_obj_get_coords(row,&area);
        if(area.y2>=viewport.y1){anchor=s_row_sequence[i];anchor_y=area.y1;break;}
    }
    size_t count=console_controller_count(), visible=0;
    s_rendered_count=count; s_rendered_sequence=console_controller_latest_sequence();
    for(size_t i=0;i<count;i++) {
        console_entry_t entry;
        if(!console_controller_get(count-1-i,&entry) || !console_filter_matches(&entry,s_filter,s_query,s_hide_temperatures))continue;
        char stamp[24],line[256];format_entry_time(&entry,stamp,sizeof(stamp));
        snprintf(line,sizeof(line),"%s  %s  %s",stamp,entry_prefix(entry.type),entry.message);
        lv_obj_t *label=s_rows->rows[visible];
        if(!label){label=lv_label_create(s_output);
    s_rows->rows[visible]=label;lv_obj_set_width(label,LV_PCT(100));
    lv_obj_set_height(label,LV_SIZE_CONTENT);
    lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);}
        ui_value_set_text(label,line);
    lv_obj_remove_flag(label,LV_OBJ_FLAG_HIDDEN);
        ui_apply_custom_label_style(label,UI_FONT_CAPTION,entry_color(entry.type));
        s_row_sequence[visible++]=entry.sequence;
    }
    char count_text[40];snprintf(count_text,sizeof(count_text),"%u / %u",(unsigned)visible,(unsigned)count);
    ui_value_set_text(s_filter_count,count_text);
    for(size_t i=visible;i<CONSOLE_LOG_CAPACITY;i++)if(s_rows->rows[i])lv_obj_add_flag(s_rows->rows[i],LV_OBJ_FLAG_HIDDEN);
    if(!visible){
        if(!s_rows->empty){s_rows->empty=lv_label_create(s_output);
    lv_obj_set_width(s_rows->empty,LV_PCT(100));
    lv_label_set_long_mode(s_rows->empty,LV_LABEL_LONG_WRAP);
    ui_apply_custom_label_style(s_rows->empty,UI_FONT_BODY,UI_TEXT_DIM);}
        lv_obj_remove_flag(s_rows->empty,LV_OBJ_FLAG_HIDDEN);
        ui_value_set_text(s_rows->empty,count?"No entries match these filters.":"Console history is empty.");
    }else if(s_rows->empty)lv_obj_add_flag(s_rows->empty,LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(s_output);
    s_programmatic_scroll=true;
    if(s_follow)lv_obj_scroll_to_y(s_output,lv_obj_get_scroll_y(s_output)+lv_obj_get_scroll_bottom(s_output),LV_ANIM_OFF);
    else {
        bool found=false;
        if(anchor)for(size_t i=0;i<visible;i++)if(s_row_sequence[i]==anchor){lv_area_t area;lv_obj_get_coords(s_rows->rows[i],&area);scroll_y=lv_obj_get_scroll_y(s_output)+area.y1-anchor_y;found=true;break;}
        if(anchor&&!found)scroll_y=0; /* Oldest retained row if the ring evicted the anchor. */
        lv_obj_scroll_to_y(s_output,scroll_y,LV_ANIM_OFF);
    }
    s_programmatic_scroll=false;
}

static void update_connection(void)
{
    if(!s_connection)return;
    moonraker_state_t state;moonraker_state_snapshot(&state);
    bool live=state.moonraker_ok;
    const char *name=moonraker_config_active_profile_name();
    char caption[192];snprintf(caption,sizeof(caption),"%s | %s",name&&name[0]?name:"Active printer",live?"LINKED":"OFFLINE");
    if(ui_value_set_text(s_connection,caption)){ui_text_fit_single_line(s_connection,UI_FONT_CAPTION);
    lv_label_set_long_mode(s_connection,LV_LABEL_LONG_CLIP);}
    ui_value_set_color(s_connection,live?UI_OK_BRIGHT:UI_DANGER_BRIGHT,0);
    bool owner_ok=!s_command_popup || s_command_owner==moonraker_config_generation();
    bool ready=live && owner_ok;
    if(s_command_feedback) {
        if(!owner_ok)ui_value_set_text(s_command_feedback,"Printer changed. Close and reopen this editor.");
        else if(!live)ui_value_set_text(s_command_feedback,"Printer offline. Reconnect before sending.");
        else if(!strcmp(lv_label_get_text(s_command_feedback),"Printer offline. Reconnect before sending."))ui_value_set_text(s_command_feedback,"Commands run immediately on this printer.");
        ui_value_set_color(s_command_feedback,ready&&!strcmp(lv_label_get_text(s_command_feedback),"Commands run immediately on this printer.")?UI_TEXT_DIM:UI_WARN,0);
    }
    if(s_command_button && lv_obj_has_state(s_command_button,LV_STATE_DISABLED)==live){if(live)lv_obj_remove_state(s_command_button,LV_STATE_DISABLED);else lv_obj_add_state(s_command_button,LV_STATE_DISABLED);}
    if(s_command_send && lv_obj_has_state(s_command_send,LV_STATE_DISABLED)==ready){if(ready)lv_obj_remove_state(s_command_send,LV_STATE_DISABLED);else lv_obj_add_state(s_command_send,LV_STATE_DISABLED);}
}

static void refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_root) {
        return;
    }

    size_t count = console_controller_count();
    uint32_t sequence =
        console_controller_latest_sequence();

    if (count != s_rendered_count ||
        sequence != s_rendered_sequence) {
        rebuild_output();
    }

    update_connection();
}


static void update_follow_button(void)
{
    if (s_follow_label) {
        lv_label_set_text(
            s_follow_label,
            s_follow
                ? ui_text("FOLLOW ON")
                : ui_text("FOLLOW OFF"));
    }

    if (s_follow_label) { ui_text_fit_single_line(s_follow_label,UI_FONT_BODY_LARGE);
    lv_label_set_long_mode(s_follow_label,LV_LABEL_LONG_CLIP); }
    if (s_follow_button) {
        ui_button_apply_kind(
            s_follow_button,
            s_follow
                ? UI_BUTTON_SUCCESS
                : UI_BUTTON_SECONDARY);
    }
}


static void follow_cb(lv_event_t *event)
{
    (void)event;
    s_follow = !s_follow;
    update_follow_button();
    rebuild_output();
}


static void clear_cb(lv_event_t *event)
{
    (void)event;
    console_controller_clear();
    rebuild_output();
}


static void close_search_popup(void)
{
    if (s_search_popup) lv_obj_delete(s_search_popup);
    s_search_popup = s_search_input = NULL;
}

static void search_cancel_cb(lv_event_t *event) { (void)event; close_search_popup(); }
static void search_done_cb(lv_event_t *event)
{
    (void)event;
    if (!s_search_input) return;
    snprintf(s_query, sizeof(s_query), "%s", lv_textarea_get_text(s_search_input));
    close_search_popup();
    lv_label_set_text(s_search_label, s_query[0] ? "SEARCH*" : "SEARCH");
    ui_text_fit_single_line(s_search_label,UI_FONT_BODY_LARGE);
    lv_label_set_long_mode(s_search_label,LV_LABEL_LONG_CLIP);
    rebuild_output();
}

static void search_keyboard_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_READY) search_done_cb(event);
    else if (lv_event_get_code(event) == LV_EVENT_CANCEL) search_cancel_cb(event);
}

static lv_obj_t *plain(lv_obj_t *parent, lv_flex_flow_t flow)
{
    lv_obj_t *obj=lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_width(obj,LV_PCT(100));
    lv_obj_set_height(obj,LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(obj,flow);
    lv_obj_set_style_pad_row(obj,8,0);
    lv_obj_set_style_pad_column(obj,8,0);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static lv_obj_t *console_action(lv_obj_t *parent,const char *text,ui_button_kind_t kind,int width,lv_event_cb_t cb)
{
    lv_obj_t *button=ui_button_create(parent,kind,text);
    lv_obj_set_size(button,width,48);
    lv_obj_add_event_cb(button,cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *label=lv_obj_get_child(button,0);
    ui_text_fit_single_line(label,UI_FONT_BODY_LARGE);
    lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    return button;
}
static lv_obj_t *editor_popup(const char *title)
{
    int w=lv_display_get_horizontal_resolution(NULL)-32,h=lv_display_get_vertical_resolution(NULL)-32;
    if(w>820)w=820;
    if(h>520)h=520;
    lv_obj_t *popup=ui_popup_create(lv_layer_top(),w,h,UI_POPUP_STANDARD);if(!popup)return NULL;
    lv_obj_set_flex_flow(popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup,12,0);
    lv_obj_set_style_pad_row(popup,8,0);
    lv_obj_remove_flag(popup,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *heading=lv_label_create(popup);
    lv_obj_set_width(heading,LV_PCT(100));
    lv_label_set_text(heading,title);
    ui_apply_text_title(heading);
    ui_value_set_color(heading,UI_TEXT,0);
    ui_text_fit_single_line(heading,UI_FONT_TITLE);
    lv_label_set_long_mode(heading,LV_LABEL_LONG_CLIP);
    return popup;
}
static lv_obj_t *editor_input(lv_obj_t *popup,int max,const char *placeholder,const char *text)
{
    lv_obj_t *input=lv_textarea_create(popup);
    lv_obj_set_width(input,LV_PCT(100));
    lv_textarea_set_one_line(input,true);
    lv_obj_set_height(input,56);
    lv_textarea_set_max_length(input,max);
    lv_textarea_set_placeholder_text(input,placeholder);
    lv_textarea_set_text(input,text);
    ui_apply_surface_role(input,UI_SURFACE_TEXT_INPUT);
    lv_obj_set_style_text_font(input,UI_FONT_BODY,0);
    return input;
}
static lv_obj_t *editor_keyboard(lv_obj_t *popup,lv_obj_t *input,lv_event_cb_t cb)
{
    lv_obj_t *keyboard=lv_keyboard_create(popup);
    ui_apply_surface_role(keyboard,UI_SURFACE_KEYBOARD);
    lv_obj_set_width(keyboard,LV_PCT(100));
    lv_obj_set_height(keyboard,0);
    lv_obj_set_flex_grow(keyboard,1);
    lv_keyboard_set_mode(keyboard,LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(keyboard,input);
    lv_obj_add_event_cb(keyboard,cb,LV_EVENT_ALL,NULL);
    return keyboard;
}
static void search_deleted(lv_event_t *e){(void)e;s_search_popup=s_search_input=NULL;}
static void search_cb(lv_event_t *event)
{
    (void)event;if(s_search_popup){lv_obj_move_foreground(s_search_popup);
    return;}
    s_search_popup=editor_popup("FILTER CONSOLE TEXT");if(!s_search_popup)return;
    lv_obj_add_event_cb(s_search_popup,search_deleted,LV_EVENT_DELETE,NULL);
    s_search_input=editor_input(s_search_popup,sizeof(s_query)-1,"Match message text",s_query);
    lv_obj_t *actions=plain(s_search_popup,LV_FLEX_FLOW_ROW_WRAP);
    console_action(actions,"CANCEL",UI_BUTTON_CANCEL,128,search_cancel_cb);console_action(actions,"APPLY",UI_BUTTON_PRIMARY,128,search_done_cb);
    editor_keyboard(s_search_popup,s_search_input,search_keyboard_cb);
}

static void filter_cb(lv_event_t *event)
{
    (void)event;
    s_filter = (console_filter_kind_t)lv_dropdown_get_selected(s_filter_dropdown);
    rebuild_output();
}

static void update_temperature_button(void)
{
    lv_label_set_text(s_temperature_label, s_hide_temperatures ? "TEMPS OFF" : "TEMPS ON");
    ui_button_apply_kind(s_temperature_button, s_hide_temperatures ? UI_BUTTON_SECONDARY : UI_BUTTON_OUTLINED);
    ui_text_fit_single_line(s_temperature_label,UI_FONT_BODY_LARGE);
    lv_label_set_long_mode(s_temperature_label,LV_LABEL_LONG_CLIP);
}
static void temperature_cb(lv_event_t *event)
{
    (void)event;
    s_hide_temperatures = !s_hide_temperatures;
    update_temperature_button();
    rebuild_output();
}
static void reset_filters_cb(lv_event_t *event)
{
    (void)event;
    s_filter = CONSOLE_FILTER_ALL;
    s_hide_temperatures = false;
    s_query[0] = 0;
    lv_dropdown_set_selected(s_filter_dropdown, 0);
    update_temperature_button();
    lv_label_set_text(s_search_label, "SEARCH");
    rebuild_output();
}

static void create_filters(void)
{
    s_filters=plain(s_root,LV_FLEX_FLOW_ROW_WRAP);
    s_filter_dropdown=lv_dropdown_create(s_filters);
    lv_obj_set_size(s_filter_dropdown,184,48);
    ui_apply_surface_role(s_filter_dropdown,UI_SURFACE_TEXT_INPUT);
    lv_obj_set_style_text_font(s_filter_dropdown,UI_FONT_CAPTION,0);
    lv_dropdown_set_options(s_filter_dropdown,"All entries\nErrors + warnings\nErrors\nWarnings\nCommands\nResponses\nSystem");
    lv_dropdown_set_selected(s_filter_dropdown,(uint32_t)s_filter);
    lv_obj_add_event_cb(s_filter_dropdown,filter_cb,LV_EVENT_VALUE_CHANGED,NULL);
    s_temperature_button=console_action(s_filters,"TEMPS ON",UI_BUTTON_OUTLINED,116,temperature_cb);
    s_temperature_label=lv_obj_get_child(s_temperature_button,0);update_temperature_button();
    lv_obj_t *search=console_action(s_filters,s_query[0]?"SEARCH*":"SEARCH",UI_BUTTON_OUTLINED,104,search_cb);
    s_search_label=lv_obj_get_child(search,0);
    console_action(s_filters,"RESET",UI_BUTTON_SECONDARY,96,reset_filters_cb);
}

static void close_command_popup(void)
{
    if (s_command_popup) {
        lv_obj_t *popup = s_command_popup;
        s_command_popup = NULL;
        s_command_input = NULL;
        lv_obj_delete(popup);
    }

    s_history_cursor = SIZE_MAX;
}


static void close_command_cb(lv_event_t *event)
{
    (void)event;
    close_command_popup();
}


static void history_prev_cb(lv_event_t *event)
{
    (void)event;

    size_t count =
        console_controller_history_count();

    if (!s_command_input || count == 0) {
        return;
    }

    if (s_history_cursor == SIZE_MAX) {
        s_history_cursor = 0;
    } else if (s_history_cursor + 1 < count) {
        ++s_history_cursor;
    }

    char command[CONSOLE_COMMAND_MAX + 1];
    if (console_controller_history_get(
            s_history_cursor,
            command,
            sizeof(command))) {
        lv_textarea_set_text(
            s_command_input,
            command);
        lv_textarea_set_cursor_pos(
            s_command_input,
            LV_TEXTAREA_CURSOR_LAST);
    }
}


static void history_next_cb(lv_event_t *event)
{
    (void)event;

    if (!s_command_input ||
        s_history_cursor == SIZE_MAX) {
        return;
    }

    if (s_history_cursor > 0) {
        --s_history_cursor;

        char command[CONSOLE_COMMAND_MAX + 1];
        if (console_controller_history_get(
                s_history_cursor,
                command,
                sizeof(command))) {
            lv_textarea_set_text(
                s_command_input,
                command);
        }
    } else {
        s_history_cursor = SIZE_MAX;
        lv_textarea_set_text(s_command_input, "");
    }

    lv_textarea_set_cursor_pos(
        s_command_input,
        LV_TEXTAREA_CURSOR_LAST);
}


static void send_command_cb(lv_event_t *event)
{
    (void)event;

    if (!s_command_input) {
        return;
    }

    moonraker_state_t state;moonraker_state_snapshot(&state);
    if(s_command_owner!=moonraker_config_generation() || !state.moonraker_ok){
        update_connection();
    return;
    }
    const char *input =
        lv_textarea_get_text(s_command_input);

    while (input && (*input == ' ' || *input == '\t')) {
        ++input;
    }

    if (!input || !input[0]) {
        ui_value_set_text(s_command_feedback,"Enter a command before sending.");
        ui_value_set_color(s_command_feedback,UI_WARN,0);
        return;
    }

    char command[CONSOLE_COMMAND_MAX + 1];
    snprintf(
        command,
        sizeof(command),
        "%.*s",
        CONSOLE_COMMAND_MAX,
        input);

    size_t length = strlen(command);
    while (length > 0 &&
           (command[length - 1] == ' ' ||
            command[length - 1] == '\t' ||
            command[length - 1] == '\r' ||
            command[length - 1] == '\n')) {
        command[--length] = '\0';
    }

    if (length == 0) {
        return;
    }

    console_controller_add_command(command);

    bool sent =
        s_command_callback &&
        s_command_callback(command);

    if (!sent) {
        console_controller_add(
            CONSOLE_ENTRY_ERROR,
            "Command was not accepted by Moonraker.");
    }

    if(sent)close_command_popup();
    else {
        ui_value_set_text(s_command_feedback,"Send failed. Input preserved; check the connection.");
        ui_value_set_color(s_command_feedback,UI_WARN,0);
    }
    rebuild_output();
}


static void keyboard_event_cb(lv_event_t *event)
{
    lv_event_code_t code =
        lv_event_get_code(event);

    if (code == LV_EVENT_READY) {
        send_command_cb(event);
    } else if (code == LV_EVENT_CANCEL) {
        close_command_popup();
    }
}


static void command_deleted(lv_event_t *e)
{
    (void)e;s_command_popup=s_command_input=s_command_send=s_command_keyboard=s_command_actions=s_command_feedback=NULL;s_history_cursor=SIZE_MAX;
}
static void open_command_cb(lv_event_t *event)
{
    (void)event;if(s_command_popup){lv_obj_move_foreground(s_command_popup);
    return;}
    moonraker_state_t state;moonraker_state_snapshot(&state);if(!state.moonraker_ok){update_connection();
    return;}
    s_command_owner=moonraker_config_generation();
    s_history_cursor=SIZE_MAX;
    s_command_popup=editor_popup("SEND KLIPPER COMMAND");if(!s_command_popup)return;
    lv_obj_add_event_cb(s_command_popup,command_deleted,LV_EVENT_DELETE,NULL);
    s_command_input=editor_input(s_command_popup,CONSOLE_COMMAND_MAX,"G-code or macro, for example: STATUS","");
    s_command_actions=plain(s_command_popup,LV_FLEX_FLOW_ROW_WRAP);
    console_action(s_command_actions,"CLOSE",UI_BUTTON_CLOSE,96,close_command_cb);
    console_action(s_command_actions,"PREV",UI_BUTTON_SECONDARY,96,history_prev_cb);
    console_action(s_command_actions,"NEXT",UI_BUTTON_SECONDARY,96,history_next_cb);
    s_command_send=console_action(s_command_actions,"SEND",UI_BUTTON_PRIMARY,96,send_command_cb);
    s_command_feedback=lv_label_create(s_command_popup);
    lv_obj_set_width(s_command_feedback,LV_PCT(100));
    lv_obj_set_height(s_command_feedback,2*UI_FONT_CAPTION->line_height);
    lv_label_set_long_mode(s_command_feedback,LV_LABEL_LONG_WRAP);
    ui_apply_custom_label_style(s_command_feedback,UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_label_set_text(s_command_feedback,"Commands run immediately on this printer.");
    s_command_keyboard=editor_keyboard(s_command_popup,s_command_input,keyboard_event_cb);
    update_connection();
}
static void history_scroll(lv_event_t *e)
{
    if(!s_programmatic_scroll && s_follow && lv_event_get_code(e)==LV_EVENT_SCROLL_BEGIN){s_follow=false;update_follow_button();}
}
void ui_console_show(ui_console_command_cb_t command_callback)
{
    if(!console_rows_init())return;
    s_command_callback=command_callback;
    if(s_root){rebuild_output();
    update_connection();
    lv_obj_move_foreground(s_root);
    return;}
    s_root=lv_obj_create(lv_screen_active());
    ui_apply_root_style(s_root);
    int sw=lv_display_get_horizontal_resolution(NULL),sh=lv_display_get_vertical_resolution(NULL);
    int x=ui_theme_is_studio()?24:UI_PAGE_ROOT_X,y=ui_theme_is_studio()?80:UI_PAGE_ROOT_Y;
    if(sw<1024)x=16;
    int w=ui_theme_is_studio()?976:UI_PAGE_ROOT_WIDTH,h=ui_theme_is_studio()?424:UI_PAGE_ROOT_HEIGHT;
    if(w>sw-x-16)w=sw-x-16;
    if(h>sh-y-8)h=sh-y-8;
    lv_obj_set_pos(s_root,x,y);
    lv_obj_set_size(s_root,w,h);
    lv_obj_set_flex_flow(s_root,LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(s_root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_root,ui_theme_is_studio()?0:12,0);
    lv_obj_set_style_pad_row(s_root,8,0);
    s_header=plain(s_root,LV_FLEX_FLOW_ROW);
    lv_obj_t *title=lv_label_create(s_header);
    lv_label_set_text(title,"CONSOLE");
    ui_apply_text_title(title);
    ui_value_set_color(title,UI_TEXT,0);
    lv_obj_set_flex_grow(title,1);
    s_filter_count=lv_label_create(s_header);
    lv_obj_set_width(s_filter_count,100);
    ui_apply_custom_label_style(s_filter_count,UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_obj_set_style_text_align(s_filter_count,LV_TEXT_ALIGN_RIGHT,0);
    s_connection=lv_label_create(s_root);
    lv_obj_set_width(s_connection,LV_PCT(100));
    lv_label_set_long_mode(s_connection,LV_LABEL_LONG_CLIP);
    ui_apply_custom_label_style(s_connection,UI_FONT_CAPTION,UI_TEXT_DIM);
    create_filters();
    s_output=lv_obj_create(s_root);
    ui_apply_card_style(s_output);
    lv_obj_set_width(s_output,LV_PCT(100));
    lv_obj_set_height(s_output,0);
    lv_obj_set_flex_grow(s_output,1);
    lv_obj_set_flex_flow(s_output,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s_output,10,0);
    lv_obj_set_style_pad_row(s_output,8,0);
    lv_obj_set_scroll_dir(s_output,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_output,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_add_event_cb(s_output,history_scroll,LV_EVENT_SCROLL_BEGIN,NULL);
    s_footer=plain(s_root,LV_FLEX_FLOW_ROW_WRAP);
    s_command_button=console_action(s_footer,"COMMAND",UI_BUTTON_PRIMARY,136,open_command_cb);
    s_follow_button=console_action(s_footer,s_follow?"FOLLOW ON":"FOLLOW OFF",UI_BUTTON_SUCCESS,136,follow_cb);
    s_follow_label=lv_obj_get_child(s_follow_button,0);
    console_action(s_footer,"CLEAR",UI_BUTTON_DANGER,96,clear_cb);
    update_follow_button();
    rebuild_output();
    update_connection();
    s_refresh_timer=lv_timer_create(refresh_timer_cb,250,NULL);
}


void ui_console_hide(void)
{
    close_command_popup();
    close_search_popup();

    if (s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }

    if (s_root) {
        lv_obj_delete(s_root);
    }

    if (s_rows) memset(s_rows, 0, sizeof(*s_rows));
    s_root = NULL;
    s_output = NULL;
    s_connection = NULL;
    s_follow_button = NULL;
    s_follow_label = NULL;
    s_filter_dropdown = s_temperature_button = s_temperature_label = NULL;
    s_search_label = s_filter_count = NULL;
    s_header=s_filters=s_footer=s_command_button=NULL;
    memset(s_row_sequence,0,sizeof(s_row_sequence));
    s_command_callback = NULL;
    s_rendered_sequence = 0;
    s_rendered_count = 0;
}
