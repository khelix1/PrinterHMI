#include "ui_telemetry.h"
#include "ui_telemetry_components.h"
#include "ui_telemetry_charts.h"
#include "ui_theme.h"
#include "ui_text.h"
#include "ui_button.h"
#include "ui_shell.h"
#include "ui_value_update.h"
#include "telemetry_history.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static lv_obj_t *s_panel,*s_status,*s_history_status,*s_graph_host;
static lv_obj_t *s_tabs[3],*s_range,*s_hotend_selector,*s_hold_button,*s_scale_button;
static ui_telemetry_view_t s_view=UI_TELEMETRY_HEAT;
static unsigned s_window=300;
static bool s_held,s_include_targets=true,s_history_available;
static char s_hotend[MOONRAKER_HOTEND_NAME_MAX],s_hotend_options[128];
static moonraker_state_t s_latest;
static int64_t s_now_us,s_plot_tick=-1;
static uint32_t s_generation;

static void telemetry_render(bool force);

static lv_obj_t *layout_row(lv_obj_t *parent,bool wrap)
{
    lv_obj_t *row=lv_obj_create(parent);lv_obj_remove_style_all(row);
    lv_obj_clear_flag(row,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row,wrap?LV_FLEX_FLOW_ROW_WRAP:LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row,8,0);lv_obj_set_style_pad_row(row,8,0);
    return row;
}

static lv_obj_t *action(lv_obj_t *parent,const char *text,lv_event_cb_t cb,void *data)
{
    lv_obj_t *button=ui_button_create(parent,UI_BUTTON_OUTLINED,text);
    lv_obj_t *label=lv_obj_get_child(button,0);
    ui_apply_custom_label_style(label,UI_FONT_BODY,UI_TEXT_BRIGHT);
    lv_obj_set_width(label,LV_SIZE_CONTENT);lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
    int32_t width=lv_obj_get_self_width(label)+28;if(width<96)width=96;
    lv_obj_set_size(button,width,44);lv_obj_center(label);
    if(cb)lv_obj_add_event_cb(button,cb,LV_EVENT_CLICKED,data);
    return button;
}

static void back_cb(lv_event_t *e)
{
    (void)e;ui_shell_page_action(UI_SHELL_PAGE_DEVICES);ui_shell_set_active_nav(UI_SHELL_PAGE_TOOLS);
}

static void view_cb(lv_event_t *e)
{
    if(s_held)return;
    s_view=(ui_telemetry_view_t)(uintptr_t)lv_event_get_user_data(e);
    telemetry_render(true);
}

static void range_cb(lv_event_t *e)
{
    (void)e;if(s_held)return;unsigned selected=lv_dropdown_get_selected(s_range);
    s_window=(unsigned[]){60,150,300}[selected<3?selected:2];telemetry_render(true);
}

static void hotend_cb(lv_event_t *e)
{
    (void)e;if(s_held)return;lv_dropdown_get_selected_str(s_hotend_selector,s_hotend,sizeof(s_hotend));
    if(!strcmp(s_hotend,"Nozzle"))snprintf(s_hotend,sizeof(s_hotend),"%s",s_latest.active_hotend);
    telemetry_render(true);
}

static void hold_cb(lv_event_t *e)
{
    (void)e;s_held=!s_held;telemetry_render(!s_held);
}

static void scale_cb(lv_event_t *e)
{
    (void)e;if(s_held)return;s_include_targets=!s_include_targets;telemetry_render(true);
}

static lv_obj_t *selector(lv_obj_t *parent,const char *options,int width,lv_event_cb_t callback)
{
    lv_obj_t *dropdown=lv_dropdown_create(parent);
    /* A flat field, independent of the theme's card/pill widget defaults. */
    lv_obj_remove_style_all(dropdown);
    lv_obj_set_style_pad_hor(dropdown,12,0);
    lv_obj_set_style_pad_ver(dropdown,8,0);
    lv_obj_set_style_outline_width(dropdown,2,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_color(dropdown,UI_ACCENT_BRIGHT,LV_STATE_FOCUS_KEY);
    lv_obj_set_style_text_opa(dropdown,LV_OPA_50,LV_STATE_DISABLED);
    lv_obj_set_size(dropdown,width,44);lv_dropdown_set_options(dropdown,options);
    ui_apply_custom_label_style(dropdown,UI_FONT_BODY,UI_TEXT_BRIGHT);
    lv_obj_set_style_bg_color(dropdown,UI_CONTROL,0);lv_obj_set_style_bg_opa(dropdown,LV_OPA_COVER,0);
    lv_obj_set_style_border_color(dropdown,UI_BORDER_BRIGHT,0);lv_obj_set_style_border_width(dropdown,1,0);
    lv_obj_set_style_radius(dropdown,6,0);
    lv_obj_add_event_cb(dropdown,callback,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_t *list=lv_dropdown_get_list(dropdown);
    ui_apply_popup_style(list);
    lv_obj_set_style_radius(list,6,0);
    lv_obj_set_style_shadow_width(list,0,0);
    lv_obj_set_style_border_width(list,1,0);
    lv_obj_set_style_pad_all(list,6,0);
    ui_apply_custom_label_style(list,UI_FONT_BODY,UI_TEXT_BRIGHT);
    return dropdown;
}

static void panel_deleted(lv_event_t *event)
{
    if(lv_event_get_target(event)!=s_panel)return;
    s_panel=s_status=s_history_status=s_graph_host=NULL;
    memset(s_tabs,0,sizeof(s_tabs));s_range=s_hotend_selector=s_hold_button=s_scale_button=NULL;
    s_held=false;s_plot_tick=-1;ui_telemetry_charts_reset();
}

void ui_telemetry_show(void)
{
    if(s_panel){lv_obj_move_foreground(s_panel);return;}
    s_history_available=telemetry_history_init();s_generation=telemetry_history_generation();
    s_now_us=esp_timer_get_time();moonraker_state_snapshot(&s_latest);
    s_held=false;s_plot_tick=-1;s_view=UI_TELEMETRY_HEAT;
    s_hotend_options[0]='\0';snprintf(s_hotend,sizeof(s_hotend),"%s",s_latest.active_hotend);
    s_panel=lv_obj_create(lv_screen_active());
    int32_t screen_width=lv_display_get_horizontal_resolution(NULL),screen_height=lv_display_get_vertical_resolution(NULL);
    int32_t width=ui_theme_is_studio()?976:854,height=ui_theme_is_studio()?424:528;
    int32_t x=ui_theme_is_studio()?24:170,y=ui_theme_is_studio()?80:72;
    if(screen_width<1024){width=screen_width-32;x=16;}
    if(screen_height<600){height=screen_height-32;y=16;}
    lv_obj_set_size(s_panel,width,height);lv_obj_set_pos(s_panel,x,y);
    ui_apply_surface_role(s_panel,UI_SURFACE_TELEMETRY_ROOT);
    lv_obj_clear_flag(s_panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_panel,ui_theme_is_studio()?0:12,0);lv_obj_set_style_pad_row(s_panel,8,0);
    lv_obj_set_flex_flow(s_panel,LV_FLEX_FLOW_COLUMN);lv_obj_add_event_cb(s_panel,panel_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *header=layout_row(s_panel,false);
    lv_obj_t *title=telemetry_make_label(header,"Telemetry",UI_FONT_TITLE,UI_TEXT_BRIGHT);
    lv_obj_set_width(title,LV_SIZE_CONTENT);
    action(header,"Devices",back_cb,NULL);
    s_status=telemetry_make_label(header,"WAITING",UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_obj_set_width(s_status,0);lv_obj_set_flex_grow(s_status,1);lv_obj_set_style_text_align(s_status,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_t *tabs=layout_row(s_panel,true);
    const char *names[]={"Heat","Motion","Environment"};
    for(unsigned i=0;i<3;i++)s_tabs[i]=action(tabs,names[i],view_cb,(void *)(uintptr_t)i);
    lv_obj_t *controls=tabs;
    s_range=selector(controls,"2 min\n5 min\n10 min",120,range_cb);
    lv_dropdown_set_selected(s_range,s_window==60?0:s_window==150?1:2);
    s_hotend_selector=selector(controls,"Nozzle",170,hotend_cb);
    s_scale_button=action(controls,"Targets on",scale_cb,NULL);
    s_hold_button=action(controls,"Resume live",hold_cb,NULL);
    ui_value_set_text(lv_obj_get_child(s_hold_button,0),"Hold graph");
    s_graph_host=lv_obj_create(s_panel);lv_obj_remove_style_all(s_graph_host);
    lv_obj_set_size(s_graph_host,LV_PCT(100),0);lv_obj_set_flex_grow(s_graph_host,1);
    lv_obj_set_scroll_dir(s_graph_host,LV_DIR_VER);lv_obj_set_scrollbar_mode(s_graph_host,LV_SCROLLBAR_MODE_AUTO);
    ui_telemetry_charts_create(s_graph_host);
    s_history_status=telemetry_make_label(s_panel,"",UI_FONT_CAPTION,UI_TEXT_DIM);
    lv_obj_set_width(s_history_status,LV_PCT(100));
    telemetry_render(true);
}

void ui_telemetry_hide(void)
{
    if(s_panel)lv_obj_delete(s_panel);
}

static void synchronize_hotends(void)
{
    char options[sizeof(s_hotend_options)]="",first[MOONRAKER_HOTEND_NAME_MAX]="";
    unsigned selected=0,count=0,active_index=0;bool found=false,active_found=false;
    for(size_t i=0;i<s_latest.hotend_count && i<MOONRAKER_MAX_HOTENDS;i++) {
        const char *name=s_latest.hotends[i].object_name;if(!name[0])continue;
        if(!count)snprintf(first,sizeof(first),"%s",name);
        if(!strcmp(name,s_hotend)){selected=count;found=true;}
        if(!strcmp(name,s_latest.active_hotend)){active_index=count;active_found=true;}
        size_t used=strlen(options);snprintf(options+used,sizeof(options)-used,"%s%s",count?"\n":"",name);count++;
    }
    if(!count){snprintf(options,sizeof(options),"Nozzle");snprintf(s_hotend,sizeof(s_hotend),"%s",s_latest.active_hotend);}
    else if(!found) {
        selected=active_found?active_index:0;
        snprintf(s_hotend,sizeof(s_hotend),"%s",active_found?s_latest.active_hotend:first);
    }
    if(strcmp(options,s_hotend_options)) {
        snprintf(s_hotend_options,sizeof(s_hotend_options),"%s",options);
        lv_dropdown_set_options(s_hotend_selector,options);lv_dropdown_set_selected(s_hotend_selector,selected);
        s_held=false;s_plot_tick=-1;
    }
}

static void telemetry_render(bool force)
{
    if(!s_panel)return;
    if(s_generation!=telemetry_history_generation()) {
        s_generation=telemetry_history_generation();s_held=false;s_plot_tick=-1;
        s_hotend[0]='\0';s_hotend_options[0]='\0';force=true;
    }
    synchronize_hotends();
    for(unsigned i=0;i<3;i++) {
        if(i==(unsigned)s_view)lv_obj_add_state(s_tabs[i],LV_STATE_CHECKED);
        else lv_obj_remove_state(s_tabs[i],LV_STATE_CHECKED);
        ui_value_set_bg_color(s_tabs[i],UI_CONTROL,LV_PART_MAIN|LV_STATE_CHECKED);
        ui_value_set_border_color(s_tabs[i],UI_ACCENT_BRIGHT,LV_PART_MAIN|LV_STATE_CHECKED);
        ui_value_set_border_color(s_tabs[i],i==(unsigned)s_view?UI_ACCENT_BRIGHT:UI_BORDER_BRIGHT,LV_PART_MAIN);
        ui_value_set_bg_color(s_tabs[i],i==(unsigned)s_view?UI_CONTROL:UI_BG_DEEP,LV_PART_MAIN);
        ui_value_set_bg_opa(s_tabs[i],LV_OPA_COVER,LV_PART_MAIN);
        if(s_held)lv_obj_add_state(s_tabs[i],LV_STATE_DISABLED);else lv_obj_remove_state(s_tabs[i],LV_STATE_DISABLED);
    }
    lv_obj_t *selectors[]={s_range,s_hotend_selector,s_scale_button};
    for(unsigned i=0;i<3;i++){if(s_held)lv_obj_add_state(selectors[i],LV_STATE_DISABLED);else lv_obj_remove_state(selectors[i],LV_STATE_DISABLED);}
    if(s_view==UI_TELEMETRY_HEAT){lv_obj_remove_flag(s_hotend_selector,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(s_scale_button,LV_OBJ_FLAG_HIDDEN);}
    else {lv_obj_add_flag(s_hotend_selector,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(s_scale_button,LV_OBJ_FLAG_HIDDEN);}
    /* Both action labels have room for either state, keeping touch geometry stable. */
    lv_obj_t *hold_label=lv_obj_get_child(s_hold_button,0);ui_value_set_text(hold_label,s_held?"Resume live":"Hold graph");
    ui_value_set_text(lv_obj_get_child(s_scale_button,0),s_include_targets?"Targets on":"Detail only");
    telemetry_sample_t sample;telemetry_history_from_state(&s_latest,s_now_us,&sample);
    bool live=s_latest.live_data_ok;
    char status[96];
    if(!live)snprintf(status,sizeof(status),"%s%s",s_latest.moonraker_ok?"WAITING FOR KLIPPER":"OFFLINE",s_held?" | GRAPH HELD":"");
    else if(s_held)snprintf(status,sizeof(status),"GRAPH HELD | VALUES LIVE");
    else snprintf(status,sizeof(status),"LIVE | %s",s_latest.printer_state[0]?s_latest.printer_state:"connected");
    ui_value_set_text(s_status,status);ui_value_set_color(s_status,live?UI_OK_BRIGHT:UI_WARN,LV_PART_MAIN);
    ui_telemetry_charts_configure(s_view,s_window,s_hotend,s_include_targets);
    ui_telemetry_charts_update_live(&sample,live,s_held);
    int64_t tick=s_now_us/TELEMETRY_HISTORY_SAMPLE_INTERVAL_US;
    if(!s_held && (force || tick!=s_plot_tick)) {ui_telemetry_charts_refresh(s_now_us,false);s_plot_tick=tick;}
    size_t count=telemetry_history_count();telemetry_sample_t last;
    if(count)s_history_available=true;
    if(!s_history_available)snprintf(status,sizeof(status),"Live values only | History storage unavailable");
    else if(count && telemetry_history_get(count-1,&last)) {
        long long age=s_now_us>=last.time_us?(s_now_us-last.time_us)/1000000:0;
        snprintf(status,sizeof(status),"2 s sampling | Last sample %lld s ago | %u / 300 samples | Scroll for more%s",age,(unsigned)count,s_held?" | held":"");
    } else snprintf(status,sizeof(status),"2 s sampling | Waiting for this printer's first live sample");
    ui_value_set_text(s_history_status,status);
}

void ui_telemetry_refresh(const moonraker_state_t *state,int64_t now_us)
{
    telemetry_history_sample(state,now_us);
    if(state)s_latest=*state;else memset(&s_latest,0,sizeof(s_latest));
    s_now_us=now_us;telemetry_render(false);
}
