#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_event_history_popup.c"
static uint16_t raster[1024*600];
static size_t event_count;
static uint32_t sequence;
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
size_t operator_event_log_count(void){return event_count;}
bool operator_event_log_get(size_t index,operator_event_t *out)
{
    if(index>=event_count)return false;
    *out=(operator_event_t){.sequence=sequence-(uint32_t)index,.uptime_seconds=100+(uint32_t)index,.level=index%3};
    snprintf(out->message,sizeof(out->message),"Sermoon D1: connection or command needs review. A complete long event message stays readable with large text.");
    return true;
}
void operator_event_log_clear(void){event_count=0;}
static void inside(lv_obj_t *child,lv_obj_t *parent)
{
    lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);
    assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);
}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels)
{
    uint16_t *values=(uint16_t *)pixels;int width=lv_display_get_horizontal_resolution(display);
    for(int y=area->y1;y<=area->y2;y++)for(int x=area->x1;x<=area->x2;x++)raster[y*width+x]=*values++;
    lv_display_flush_ready(display);
}
static void capture(int theme)
{
    const char *folder=getenv("EVENT_HISTORY_SCREENSHOTS");if(!folder)return;
    lv_obj_scroll_to_y(s_list,0,LV_ANIM_OFF);lv_refr_now(NULL);
    char path[512];snprintf(path,sizeof(path),"%s/history-theme%d.ppm",folder,theme);
    FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P6\n1024 600\n255\n");
    for(unsigned i=0;i<1024*600;i++) {
        uint16_t v=raster[i];unsigned char color[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};
        fwrite(color,1,3,file);
    }
    fclose(file);
}
static void check_layout(void)
{
    lv_obj_update_layout(s_popup);inside(s_popup,lv_screen_active());
    lv_obj_t *footer=lv_obj_get_child(s_popup,2);
    inside(footer,s_popup);inside(s_list,s_popup);
    for(unsigned i=0;i<lv_obj_get_child_count(footer);i++) {
        lv_obj_t *button=lv_obj_get_child(footer,i),*label=lv_obj_get_child(button,0);
        inside(button,footer);inside(label,button);assert(lv_obj_get_height(button)==48);
        lv_area_t a,b;lv_obj_get_coords(button,&a);lv_obj_get_coords(label,&b);
        assert(abs(a.x1+a.x2-b.x1-b.x2)<=2&&abs(a.y1+a.y2-b.y1-b.y2)<=2);
        assert(lv_obj_get_style_text_align(label,0)==LV_TEXT_ALIGN_CENTER);
        assert(lv_obj_get_style_text_font(label,0)->line_height>=UI_FONT_BODY->line_height);
    }
    lv_area_t before,after;lv_obj_get_coords(footer,&before);
    lv_obj_scroll_to_y(s_list,0,LV_ANIM_OFF);lv_obj_update_layout(s_popup);
    lv_obj_t *row=lv_obj_get_child(s_list,0);
    for(unsigned i=0;i<lv_obj_get_child_count(row);i++)inside(lv_obj_get_child(row,i),row);
    lv_obj_scroll_to_y(s_list,lv_obj_get_scroll_bottom(s_list),LV_ANIM_OFF);
    lv_obj_update_layout(s_popup);assert(lv_obj_get_scroll_bottom(s_list)<=1);
    lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));
}
int main(void)
{
    lv_init();lv_display_t *display=lv_display_create(1024,600);
    static uint8_t buffer[1024*40*2];lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++)
    for(int viewport=0;viewport<3;viewport++) {
        lv_display_set_resolution(display,viewport==0?1024:viewport==1?640:480,viewport==2?400:600);
        ui_theme_set_active(theme);ui_theme_set_density(density);
        ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
        event_count=0;sequence=100;ui_event_history_popup_show();assert(s_popup&&s_list&&s_refresh_timer);
        check_layout();assert(lv_obj_get_child_count(s_list)==1);
        event_count=40;refresh_timer_cb(s_refresh_timer);check_layout();assert(lv_obj_get_child_count(s_list)==40);
        if(viewport==0&&density==1&&large)capture(theme);
        lv_obj_t *first=lv_obj_get_child(s_list,0);refresh_timer_cb(s_refresh_timer);assert(first==lv_obj_get_child(s_list,0));
        follow_cb(NULL);assert(!s_follow_live);lv_obj_scroll_to_y(s_list,0,LV_ANIM_OFF);
        sequence++;refresh_timer_cb(s_refresh_timer);assert(s_rendered_sequence==100&&first==lv_obj_get_child(s_list,0));
        ui_event_history_popup_show();assert(!s_follow_live&&first==lv_obj_get_child(s_list,0));
        follow_cb(NULL);assert(s_follow_live&&s_rendered_sequence==101);check_layout();
        clear_cb(NULL);assert(!event_count&&lv_obj_get_child_count(s_list)==1);check_layout();
        lv_obj_delete(s_popup);assert(!s_popup&&!s_list&&!s_refresh_timer&&!s_follow_button);
        ui_event_history_popup_show();ui_event_history_popup_close();assert(!s_popup&&!s_refresh_timer);
    }
    lv_display_delete(display);lv_deinit();
    puts("PASS: Event History five themes, densities/text sizes/viewports, centered pinned actions, complete rows, Hold/Follow, ring replacement, clear and deletion cleanup");
    return 0;
}
