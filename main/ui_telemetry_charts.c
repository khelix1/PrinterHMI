#include "ui_telemetry_charts.h"
#include "ui_telemetry_components.h"
#include "ui_theme.h"
#include "ui_value_update.h"
#include "ui_text_fit.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef enum {CHANNEL_HOTEND,CHANNEL_BED,CHANNEL_CENTER,CHANNEL_AIR,CHANNEL_HUMIDITY,
    CHANNEL_VELOCITY,CHANNEL_FLOW,CHANNEL_SPEED_FACTOR,CHANNEL_FLOW_FACTOR,CHANNEL_PART_FAN,CHANNEL_DRYBOX_FAN} telemetry_channel_t;
typedef struct {
    lv_obj_t *card,*title,*value,*target,*chart,*stats,*scale,*time,*time_left,*marker;
    lv_chart_series_t *actual,*reference;
    telemetry_channel_t channel;
    double low,high;
    int32_t newest;
} telemetry_chart_t;
static telemetry_chart_t s_charts[4];
static lv_obj_t *s_chart_host;
static unsigned s_points=300;
static bool s_chart_include_targets=true;
static char s_chart_hotend[MOONRAKER_HOTEND_NAME_MAX];
static const char *const s_channel_names[]={"Hotend","Bed","Drybox center","Drybox air","Humidity","Velocity","Volumetric flow","Speed override","Flow override","Part fan","Drybox fan"};
static const char *const s_units[]={"C","C","C","C","%RH","mm/s","mm3/s","%","%","%","%"};
static const telemetry_channel_t s_view_channels[3][4]={{CHANNEL_HOTEND,CHANNEL_BED,CHANNEL_CENTER,CHANNEL_AIR},
    {CHANNEL_VELOCITY,CHANNEL_FLOW,CHANNEL_SPEED_FACTOR,CHANNEL_FLOW_FACTOR},
    {CHANNEL_AIR,CHANNEL_HUMIDITY,CHANNEL_PART_FAN,CHANNEL_DRYBOX_FAN}};

static double channel_value(const telemetry_sample_t *s,telemetry_channel_t channel,bool target)
{
    if(!s)return NAN;
    if(channel==CHANNEL_HOTEND) {
        for(size_t i=0;i<s->hotend_count;i++)if(!strcmp(s->hotends[i].object_name,s_chart_hotend))return target?s->hotends[i].target:s->hotends[i].temperature;
        /* Compatibility source is usable only for its named active tool. */
        if(!s->hotend_count && (!s_chart_hotend[0] || !strcmp(s_chart_hotend,s->active_hotend)))return target?s->nozzle_target:s->nozzle_temp;
        return NAN;
    }
    if(target) {
        if(channel==CHANNEL_BED)return s->bed_target;
        if(channel==CHANNEL_CENTER)return s->heater_target;
        return NAN;
    }
    switch(channel) {
        case CHANNEL_BED:return s->bed_temp;
        case CHANNEL_CENTER:return s->center_temp;
        case CHANNEL_AIR:return s->air_temp;
        case CHANNEL_HUMIDITY:return s->humidity;
        case CHANNEL_VELOCITY:return s->live_velocity;
        case CHANNEL_FLOW:return s->live_flow;
        case CHANNEL_SPEED_FACTOR:return s->speed_factor;
        case CHANNEL_FLOW_FACTOR:return s->flow_factor;
        case CHANNEL_PART_FAN:return s->part_fan_speed;
        case CHANNEL_DRYBOX_FAN:return s->drybox_fan_speed;
        default:return NAN;
    }
}

static bool channel_unavailable(const telemetry_sample_t *s,telemetry_channel_t channel)
{
    if(!s)return false;
    uint32_t flag=channel==CHANNEL_BED?TELEMETRY_NO_BED:channel==CHANNEL_CENTER?TELEMETRY_NO_CENTER:
        channel==CHANNEL_AIR || channel==CHANNEL_HUMIDITY?TELEMETRY_NO_ENV:
        channel==CHANNEL_PART_FAN?TELEMETRY_NO_PART_FAN:channel==CHANNEL_DRYBOX_FAN?TELEMETRY_NO_DRYBOX_FAN:0;
    return (s->unavailable & flag)!=0;
}

static int32_t plot_value(double value)
{
    return isfinite(value)?(int32_t)lround(value*10):LV_CHART_POINT_NONE;
}

static void marker_position(telemetry_chart_t *c)
{
    if(c->newest==LV_CHART_POINT_NONE || c->high<=c->low){lv_obj_add_flag(c->marker,LV_OBJ_FLAG_HIDDEN);return;}
    lv_obj_remove_flag(c->marker,LV_OBJ_FLAG_HIDDEN);
    int32_t w=lv_obj_get_content_width(c->chart),h=lv_obj_get_content_height(c->chart);
    if(w<8 || h<8){lv_obj_add_flag(c->marker,LV_OBJ_FLAG_HIDDEN);return;}
    int32_t y=(int32_t)lround((c->high-c->newest/10.0)/(c->high-c->low)*(h-1));
    if(y<3)y=3;
    if(y>h-4)y=h-4;
    lv_obj_set_pos(c->marker,lv_obj_get_style_pad_left(c->chart,0)+w-7,lv_obj_get_style_pad_top(c->chart,0)+y-3);
}

static void marker_layout(lv_event_t *event){marker_position(lv_event_get_user_data(event));}

static void chart_host_resized(lv_event_t *event)
{
    (void)event;
    if(!s_chart_host)return;
    int32_t width=lv_obj_get_content_width(s_chart_host);
    unsigned columns=width>=600?2:1;
    int32_t card_width=(width-(columns-1)*12)/(int32_t)columns;
    if(card_width<1)card_width=1;
    for(unsigned i=0;i<4;i++)if(s_charts[i].card)lv_obj_set_width(s_charts[i].card,card_width);
}

void ui_telemetry_charts_create(lv_obj_t *parent)
{
    s_chart_host=parent;
    lv_obj_set_flex_flow(parent,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_column(parent,12,0);lv_obj_set_style_pad_row(parent,12,0);
    lv_obj_add_event_cb(parent,chart_host_resized,LV_EVENT_SIZE_CHANGED,NULL);
    const lv_color_t colors[]={UI_ACCENT_CYAN,UI_WARN,UI_TELEMETRY_CHAMBER,UI_TELEMETRY_HUMIDITY};
    for(unsigned i=0;i<4;i++) {
        telemetry_chart_t *c=&s_charts[i];
        c->card=telemetry_create_metric_card(parent,"",colors[i],&c->title,&c->value);
        c->target=telemetry_make_label(c->card,"",UI_FONT_CAPTION,UI_TEXT_DIM);
        lv_obj_set_width(c->target,LV_PCT(100));
        c->chart=lv_chart_create(c->card);
        ui_apply_telemetry_plot_style(c->chart);
        lv_obj_clear_flag(c->chart,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(c->chart,LV_PCT(100),96);
        lv_obj_set_style_size(c->chart,0,0,LV_PART_INDICATOR);
        lv_chart_set_type(c->chart,LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(c->chart,s_points);
        lv_chart_set_div_line_count(c->chart,4,6);
        /* LVGL 9.5 draws the series list backwards: insert actual first so
         * it remains visible when the temperature equals its target. */
        c->actual=lv_chart_add_series(c->chart,colors[i],LV_CHART_AXIS_PRIMARY_Y);
        c->reference=lv_chart_add_series(c->chart,UI_TEXT_DIM,LV_CHART_AXIS_PRIMARY_Y);
        c->newest=LV_CHART_POINT_NONE;
        c->marker=lv_obj_create(c->chart);lv_obj_remove_style_all(c->marker);
        lv_obj_set_size(c->marker,6,6);ui_apply_trace_marker_style(c->marker,colors[i]);
        lv_obj_clear_flag(c->marker,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(c->marker,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(c->chart,marker_layout,LV_EVENT_SIZE_CHANGED,c);
        c->scale=telemetry_make_label(c->card,"",UI_FONT_CAPTION,UI_TEXT_DIM);lv_obj_set_width(c->scale,LV_PCT(100));
        c->stats=telemetry_make_label(c->card,"",UI_FONT_CAPTION,UI_TEXT);lv_obj_set_width(c->stats,LV_PCT(100));
        lv_obj_t *footer=lv_obj_create(c->card);lv_obj_remove_style_all(footer);lv_obj_clear_flag(footer,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(footer,LV_PCT(100),LV_SIZE_CONTENT);lv_obj_set_flex_flow(footer,LV_FLEX_FLOW_ROW);
        c->time_left=telemetry_make_label(footer,"",UI_FONT_CAPTION,UI_TEXT_DIM);lv_obj_set_width(c->time_left,LV_PCT(50));
        c->time=telemetry_make_label(footer,"NOW",UI_FONT_CAPTION,UI_TEXT_DIM);lv_obj_set_width(c->time,0);lv_obj_set_flex_grow(c->time,1);lv_obj_set_style_text_align(c->time,LV_TEXT_ALIGN_RIGHT,0);
    }
    chart_host_resized(NULL);
}

void ui_telemetry_charts_configure(ui_telemetry_view_t view,unsigned points,const char *hotend,bool include_targets)
{
    if(view>UI_TELEMETRY_ENVIRONMENT)view=UI_TELEMETRY_HEAT;
    s_points=points==60 || points==150?points:300;
    s_chart_include_targets=include_targets;
    snprintf(s_chart_hotend,sizeof(s_chart_hotend),"%s",hotend?hotend:"");
    for(unsigned i=0;i<4;i++) {
        telemetry_chart_t *c=&s_charts[i];if(!c->chart)continue;
        c->channel=s_view_channels[view][i];
        ui_value_set_text(c->title,c->channel==CHANNEL_HOTEND && s_chart_hotend[0]?s_chart_hotend:s_channel_names[c->channel]);
        if(lv_chart_get_point_count(c->chart)!=s_points)lv_chart_set_point_count(c->chart,s_points);
    }
}

void ui_telemetry_charts_update_live(const telemetry_sample_t *sample,bool live,bool held)
{
    for(unsigned i=0;i<4;i++) {
        telemetry_chart_t *c=&s_charts[i];if(!c->value)continue;
        double value=channel_value(sample,c->channel,false),target=channel_value(sample,c->channel,true);
        char text[112];
        bool unavailable=channel_unavailable(sample,c->channel);
        if(unavailable)snprintf(text,sizeof(text),"N/A");
        else if(live && isfinite(value))snprintf(text,sizeof(text),"%.1f %s",value,s_units[c->channel]);
        else snprintf(text,sizeof(text),"-- %s",s_units[c->channel]);
        if(ui_value_set_text(c->value,text))ui_text_fit_single_line(c->value,UI_FONT_BODY_LARGE);
        if(unavailable)snprintf(text,sizeof(text),"Not configured on this printer");
        else if(!live)snprintf(text,sizeof(text),"Offline / waiting for live data");
        else if(!isfinite(value))snprintf(text,sizeof(text),"No reading reported");
        else if(isfinite(target) && target>0)snprintf(text,sizeof(text),"Target %.1f C | Error %+.1f C%s",target,value-target,held?" | values live":"");
        else if(isfinite(target))snprintf(text,sizeof(text),"Heater off%s",held?" | values live":"");
        else if(c->channel==CHANNEL_FLOW && sample && sample->active_hotend[0])snprintf(text,sizeof(text),"Active tool: %s%s",sample->active_hotend,held?" | graph held":"");
        else snprintf(text,sizeof(text),"Live reading%s",held?" | graph held":"");
        ui_value_set_text(c->target,text);
        ui_value_set_text(c->time,held?"HELD":"NOW");
    }
}

void ui_telemetry_charts_refresh(int64_t end_us,bool held)
{
    if(!s_chart_host)return;
    /* One fixed-time bin per two seconds. Missing intervals stay empty and
     * named tool lookups prevent traces from mixing after active-tool changes. */
    int64_t end_tick=end_us/TELEMETRY_HISTORY_SAMPLE_INTERVAL_US;
    for(unsigned n=0;n<4;n++) {
        telemetry_chart_t *c=&s_charts[n];
        int32_t *actual=lv_chart_get_series_y_array(c->chart,c->actual);
        int32_t *target=lv_chart_get_series_y_array(c->chart,c->reference);
        for(unsigned i=0;i<s_points;i++)actual[i]=target[i]=LV_CHART_POINT_NONE;
        double low=NAN,high=NAN,min=NAN,max=NAN;unsigned count=0;
        size_t history_count=telemetry_history_count();
        for(size_t i=0;i<history_count;i++) {
            telemetry_sample_t sample;if(!telemetry_history_get(i,&sample))continue;
            int64_t age=end_tick-sample.time_us/TELEMETRY_HISTORY_SAMPLE_INTERVAL_US;
            if(age<0 || age>=(int64_t)s_points)continue;
            unsigned index=s_points-1-(unsigned)age;
            double value=channel_value(&sample,c->channel,false),reference=channel_value(&sample,c->channel,true);
            actual[index]=plot_value(value);
            if(isfinite(value)) {
                if(!isfinite(min) || value<min)min=value;
                if(!isfinite(max) || value>max)max=value;
                low=min;high=max;count++;
            }
            if(isfinite(reference) && reference>0)target[index]=plot_value(reference);
        }
        if(!isfinite(low)){low=0;high=100;}
        if(s_chart_include_targets)for(unsigned i=0;i<s_points;i++)if(target[i]!=LV_CHART_POINT_NONE) {
            double v=target[i]/10.0;if(v<low)low=v;if(v>high)high=v;
        }
        double span=high-low;
        double minimum=c->channel==CHANNEL_FLOW?1:4;
        if(span<minimum){double center=(low+high)*.5;low=center-minimum*.5;high=center+minimum*.5;}
        else {low-=span*.08;high+=span*.08;}
        /* Environmental probes can report below zero; percentage/motion
         * channels and heater temperatures keep a nonnegative scale. */
        if(c->channel!=CHANNEL_AIR && c->channel!=CHANNEL_CENTER && low<0)low=0;
        low=floor(low*10)/10;high=ceil(high*10)/10;
        if(high<=low)high=low+minimum;
        c->low=low;c->high=high;c->newest=actual[s_points-1];
        lv_chart_set_range(c->chart,LV_CHART_AXIS_PRIMARY_Y,plot_value(low),plot_value(high));
        lv_chart_set_x_start_point(c->chart,c->actual,0);lv_chart_set_x_start_point(c->chart,c->reference,0);
        lv_chart_hide_series(c->chart,c->reference,!s_chart_include_targets);
        lv_chart_refresh(c->chart);
        marker_position(c);
        char text[128];snprintf(text,sizeof(text),"Scale %.1f to %.1f %s%s",low,high,s_units[c->channel],s_chart_include_targets && (c->channel==CHANNEL_HOTEND || c->channel==CHANNEL_BED || c->channel==CHANNEL_CENTER)?" | dim = target":"");ui_value_set_text(c->scale,text);
        if(count)snprintf(text,sizeof(text),"Min %.1f | Max %.1f | Span %.1f %s",min,max,max-min,s_units[c->channel]);
        else snprintf(text,sizeof(text),"No samples in this time window");
        ui_value_set_text(c->stats,text);
        snprintf(text,sizeof(text),"-%u min",s_points/30);ui_value_set_text(c->time_left,text);ui_value_set_text(c->time,held?"HELD":"NOW");
    }
}

void ui_telemetry_charts_reset(void)
{
    memset(s_charts,0,sizeof(s_charts));s_chart_host=NULL;
}
