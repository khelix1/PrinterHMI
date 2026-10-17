#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "custom_theme.h"
#include "ui_printer_toolhead.c"
#include "ui_printer_popups.c"
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_state_t fixture;
static uint32_t generation=1;
static unsigned sends;
static bool accept=true;
static int during_send;
static char sent[128];
static lv_obj_t *steps[3];
static double step=10;
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t *out){*out=fixture;}
void ui_filament_recovery_close(void){}
void ui_dashboard_status_popup_close(void){}
static bool send_checked(const char *command)
{
    ++sends;snprintf(sent,sizeof(sent),"%s",command);
    int mode=during_send;during_send=0;
    if(mode==1) toolhead_send("G28");
    if(mode==2) ui_printer_popups_close_all();
    if(mode==3) {++generation;ui_printer_popups_close_all();}
    if(mode==4) confirm_hotend_activate_cb(NULL);
    if(mode==5) {lv_obj_delete(s_hotend_list_popup);ui_printer_popups_show_hotends(NULL,&fixture);}
    return accept;
}
static unsigned timers(void){unsigned n=0;for(lv_timer_t*t=lv_timer_get_next(NULL);t;t=lv_timer_get_next(t))++n;return n;}
static uint16_t raster[1024*600];
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p)
{uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void capture(unsigned theme,unsigned tab)
{
 const char *folder=getenv("TOOLHEAD_SCREENSHOTS");if(!folder)return;
 lv_tick_inc(250);lv_timer_handler();lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/toolhead-theme%u-tab%u.ppm",folder,theme,tab);
 FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");
 for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);
}
static void click(lv_obj_t*o){assert(o);lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static void reset(void)
{
    ui_printer_popups_close_all();++generation;
    memset(&fixture,0,sizeof(fixture));fixture.moonraker_ok=fixture.live_data_ok=true;
    strcpy(fixture.printer_state,"standby");fixture.hotend_count=3;
    for(unsigned i=0;i<3;i++){snprintf(fixture.hotends[i].object_name,24,"extruder%u",i);fixture.hotends[i].active=i==0;}
    accept=true;during_send=0;
    ui_printer_toolhead_set_sender(send_checked);
    ui_printer_popups_set_control_sender(send_checked);
}
static void motion_open(void){ui_printer_toolhead_show(&steps[0],&steps[1],&steps[2],&step,NULL);assert(s_toolhead_popup);}
static void hotend_open(void){ui_printer_popups_show_hotends(NULL,&fixture);click(s_hotend_activate_buttons[1]);assert(s_hotend_activate_popup);}
static lv_obj_t *find_label(lv_obj_t *object, const char *text)
{
 if(lv_obj_check_type(object,&lv_label_class)&&!strcmp(lv_label_get_text(object),text))return object;
 for(unsigned i=0;i<lv_obj_get_child_count(object);i++){lv_obj_t *label=find_label(lv_obj_get_child(object,i),text);if(label)return label;}
 return NULL;
}
static void inside(lv_obj_t *child,lv_obj_t *parent)
{
 lv_area_t a,b;lv_obj_get_coords(child,&a);lv_obj_get_coords(parent,&b);
 if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){
  fprintf(stderr,"outside %d,%d..%d,%d vs %d,%d..%d,%d\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2);assert(false);
 }
}
static unsigned check_buttons(lv_obj_t *object)
{
 if(lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN))return 0;
 unsigned count=0;
 if(lv_obj_check_type(object,&lv_button_class)){
  count++;assert(lv_obj_get_height(object)>=48);inside(object,lv_obj_get_parent(object));
  lv_obj_t *label=lv_obj_get_child(object,0);inside(label,object);
  const lv_font_t *font=lv_obj_get_style_text_font(label,0);assert(lv_obj_get_height(label)==font->line_height);
  lv_point_t size;lv_text_get_size(&size,lv_label_get_text(label),font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);assert(size.x<=lv_obj_get_width(label));
  lv_area_t a,b;lv_obj_get_coords(label,&a);lv_obj_get_coords(object,&b);
  assert(abs((a.x1+a.x2)-(b.x1+b.x2))<=2);assert(abs((a.y1+a.y2)-(b.y1+b.y2))<=2);
 }
 for(unsigned i=0;i<lv_obj_get_child_count(object);i++)count+=check_buttons(lv_obj_get_child(object,i));
 return count;
}
static lv_point_t touch;static bool pressed;
static void read_touch(lv_indev_t *indev,lv_indev_data_t *data)
{(void)indev;data->point=touch;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void native_layout_checks(lv_display_t *display,lv_indev_t *pointer)
{
 for(int size=0;size<3;size++)for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  int width=(int[]){1024,640,480}[size],height=size==2?400:600;lv_display_set_resolution(display,width,height);
  ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});reset();motion_open();lv_obj_update_layout(s_toolhead_popup);
  inside(s_toolhead_popup,lv_layer_top());lv_obj_t *body=ui_form_body(s_toolhead_popup),*footer=lv_obj_get_child(s_toolhead_popup,-1);inside(body,s_toolhead_popup);inside(footer,s_toolhead_popup);
  lv_area_t before,after,body_area;lv_obj_get_coords(footer,&before);lv_obj_get_coords(body,&body_area);assert(body_area.y2<before.y1);
  fixture.toolhead_position_valid=true;fixture.toolhead_x=280;fixture.toolhead_y=260;fixture.toolhead_z=300;fixture.z_offset=-0.125;
  refresh_live_labels();lv_obj_update_layout(s_toolhead_popup);
  assert(!strcmp(lv_label_get_text(s_offset_label),"-0.125 mm"));
  assert(!strcmp(lv_label_get_text(s_position_label),"X  280.00      Y  260.00      Z  300.000 mm"));
  unsigned baseline_sends=sends;lv_timer_t *baseline_timer=s_refresh_timer;
  for(unsigned tab=0;tab<3;tab++) {
   if(width==1024) {
    lv_area_t button_area;lv_obj_get_coords(s_tab_buttons[tab],&button_area);
    touch=(lv_point_t){(button_area.x1+button_area.x2)/2,(button_area.y1+button_area.y2)/2};
    pressed=true;lv_indev_read(pointer);pressed=false;lv_indev_read(pointer);
   } else click(s_tab_buttons[tab]);
   lv_obj_update_layout(s_toolhead_popup);
   assert(s_active_tab==tab&&sends==baseline_sends&&s_refresh_timer==baseline_timer);
   assert(check_buttons(s_toolhead_popup)==((unsigned[]){22,11,15})[tab]);
   for(unsigned i=0;i<3;i++) {
    assert(lv_obj_has_flag(s_tab_panels[i],LV_OBJ_FLAG_HIDDEN)==(i!=tab));
    assert(lv_obj_has_state(s_tab_buttons[i],LV_STATE_CHECKED)==(i==tab));
   }
   assert(lv_obj_get_scrollbar_mode(body)==LV_SCROLLBAR_MODE_OFF);
   if(width==1024) {
    /* Every active panel and button is reachable without scrolling, including large text. */
    if(lv_obj_get_scroll_bottom(body)>0)fprintf(stderr,"overflow tab=%u theme=%d density=%d large=%d bottom=%d\n",tab,theme,density,large,(int)lv_obj_get_scroll_bottom(body));
    assert(lv_obj_get_scroll_bottom(body)<=0);assert(lv_obj_get_scroll_y(body)==0);
    inside(s_tab_panels[tab],body);
    if(density==2&&large) {
     capture(theme,tab);lv_obj_update_layout(s_toolhead_popup);
     inside(s_tab_panels[tab],body);check_buttons(s_toolhead_popup);
    }
   }
   lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));
  }
  click(s_tab_buttons[0]);lv_obj_update_layout(s_toolhead_popup);
  lv_obj_t *panels=s_tab_panels[0],*xy=lv_obj_get_child(panels,0),*z=lv_obj_get_child(panels,1);inside(xy,panels);inside(z,panels);
  lv_area_t a,b;lv_obj_get_coords(xy,&a);lv_obj_get_coords(z,&b);if(width==1024)assert(a.x2<b.x1);else assert(a.y2<b.y1);
  /* Smaller displays retain touch scrolling if the stacked Move panel needs it. */
  if(width!=1024&&lv_obj_get_scroll_bottom(body)>0) {
   touch=(lv_point_t){body_area.x1+3,body_area.y2-20};pressed=true;lv_indev_read(pointer);
   for(int move=0;move<3;move++){touch.y-=35;lv_tick_inc(20);lv_indev_read(pointer);}
   pressed=false;lv_indev_read(pointer);lv_obj_update_layout(s_toolhead_popup);assert(lv_obj_get_scroll_y(body)>0);
   lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));
  }
  lv_obj_t *last=find_label(s_tab_panels[2],"Reset");assert(last);
  /* Preserve exact commands through the new grid buttons. */
  lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);click(steps[0]);assert(step==1);
  lv_tick_inc(350);click(lv_obj_get_parent(find_label(s_toolhead_popup,"X+ " LV_SYMBOL_RIGHT)));assert(!strcmp(sent,"G91\nG1 X1.00 F6000\nG90"));
  click(s_z_step005_btn);lv_tick_inc(350);click(lv_obj_get_parent(find_label(s_toolhead_popup,LV_SYMBOL_DOWN " Z-")));assert(!strcmp(sent,"G91\nG1 Z-0.050 F600\nG90"));
  click(s_tab_buttons[2]);
  lv_tick_inc(350);click(lv_obj_get_parent(find_label(s_toolhead_popup,"+0.01")));assert(!strcmp(sent,"SET_GCODE_OFFSET Z_ADJUST=+0.010 MOVE=1"));
  click(s_tab_buttons[0]);
  const char *z_values[] = {"0.01", "0.05", "0.10", "0.50", "1.00", "5.00", "10.00", "25.00"};
  for (unsigned i=0;i<8;i++) {
   click(lv_obj_get_parent(find_label(s_toolhead_popup,z_values[i])));assert(fabs(s_z_step-atof(z_values[i]))<0.0001);
   for(int sign=0;sign<2;sign++) {
    lv_tick_inc(350);click(lv_obj_get_parent(find_label(s_toolhead_popup,sign?LV_SYMBOL_DOWN " Z-":LV_SYMBOL_UP " Z+")));
    char expected[128];snprintf(expected,sizeof(expected),"G91\nG1 Z%.3f F600\nG90",(sign?-1:1)*atof(z_values[i]));assert(!strcmp(sent,expected));
   }
  }
  click(s_tab_buttons[2]);
  const char *adjustments[]={"-0.005","+0.005","-0.01","+0.01","-0.025","+0.025","-0.05","+0.05","-0.10","+0.10"};
  for(unsigned i=0;i<10;i++) {
   lv_tick_inc(350);click(lv_obj_get_parent(find_label(s_toolhead_popup,adjustments[i])));
   char expected[128];snprintf(expected,sizeof(expected),"SET_GCODE_OFFSET Z_ADJUST=%+.3f MOVE=1",atof(adjustments[i]));assert(!strcmp(sent,expected));
  }
  lv_tick_inc(350);click(lv_obj_get_parent(last));assert(!strcmp(sent,"SET_GCODE_OFFSET Z=0 MOVE=1"));
  click(s_tab_buttons[1]);
  for(unsigned i=0;i<5;i++) {
   click(s_extrusion_buttons[i]);assert(s_extrusion_step==atof(s_extrusion_values[i]));
   for(int retract=0;retract<2;retract++) {
    lv_tick_inc(350);click(lv_obj_get_parent(find_label(s_toolhead_popup,retract?"Retract":"Extrude")));
    char expected[128];snprintf(expected,sizeof(expected),"M83\nG1 E%.0f F300",(retract?-1:1)*s_extrusion_step);assert(!strcmp(sent,expected));
   }
  }
  click(s_extrusion_buttons[2]);
  lv_tick_inc(350);click(lv_obj_get_parent(find_label(s_toolhead_popup,"Extrude")));assert(!strcmp(sent,"M83\nG1 E10 F300"));
  click(s_tab_buttons[0]);assert(s_extrusion_step==10&&fabs(s_z_step-25.0)<0.0001);
  motion_open();assert(s_active_tab==0&&s_refresh_timer==baseline_timer);
  click(s_tab_buttons[2]);motion_open();assert(s_active_tab==2&&s_refresh_timer==baseline_timer);
  click(lv_obj_get_child(footer,0));assert(!s_toolhead_popup&&!s_refresh_timer&&!steps[0]&&!s_tab_panels[0]&&!s_tab_buttons[0]);
 }
}
int main(void)
{
    lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t buffer[1024*40*2];
    lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,buffer,NULL,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
    for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
        ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});reset();unsigned baseline=timers();
        motion_open();lv_obj_t *popup=s_toolhead_popup;lv_timer_t*timer=s_refresh_timer;
        motion_open();assert(s_toolhead_popup==popup&&s_refresh_timer==timer);
        unsigned before=sends;during_send=1;toolhead_send("G28");assert(sends==before+1);
        toolhead_send("G28");assert(sends==before+1);lv_tick_inc(350);toolhead_send("G28");assert(sends==before+2);
        lv_tick_inc(350);accept=false;toolhead_send("G28");assert(sends==before+3);accept=true;toolhead_send("G28");assert(sends==before+4);
        lv_tick_inc(350);fixture.moonraker_ok=false;toolhead_send("G28");assert(sends==before+4);fixture.moonraker_ok=true;
        ++generation;toolhead_send("G28");assert(sends==before+4&&!s_toolhead_popup&&!s_refresh_timer&&!steps[0]&&!s_xy_step);
        motion_open();++generation;refresh_timer_cb(NULL);assert(!s_toolhead_popup&&timers()==baseline);
        motion_open();lv_obj_delete(s_toolhead_popup);assert(!s_toolhead_popup&&!s_refresh_timer&&!s_send_gcode&&!s_xy_step1_btn&&timers()==baseline);
        motion_open();during_send=2;toolhead_send("G28");assert(!s_toolhead_popup&&!s_toolhead_dispatching&&timers()==baseline);
        motion_open();before=sends;toolhead_send("G28");assert(sends==before);lv_tick_inc(350);during_send=3;toolhead_send("G28");assert(!s_toolhead_popup&&!s_toolhead_dispatching&&timers()==baseline);
        reset();hotend_open();before=sends;accept=false;confirm_hotend_activate_cb(NULL);assert(sends==before+1&&s_hotend_activate_popup&&!s_hotend_pending&&!lv_obj_has_state(s_hotend_confirm_button,LV_STATE_DISABLED));
        accept=true;during_send=4;confirm_hotend_activate_cb(NULL);assert(sends==before+2&&s_hotend_pending);confirm_hotend_activate_cb(NULL);assert(sends==before+2);
        refresh_hotend_list(NULL);assert(lv_obj_has_state(s_hotend_confirm_button,LV_STATE_DISABLED));
        lv_tick_inc(5000);refresh_hotend_list(NULL);assert(!s_hotend_pending&&!lv_obj_has_state(s_hotend_confirm_button,LV_STATE_DISABLED));confirm_hotend_activate_cb(NULL);assert(sends==before+3);
        fixture.hotends[1].active=true;refresh_hotend_list(NULL);assert(!s_hotend_activate_popup&&!s_hotend_confirm_status);close_hotend_list_cb(NULL);assert(timers()==baseline);
        reset();hotend_open();moonraker_hotend_t swap=fixture.hotends[1];fixture.hotends[1]=fixture.hotends[2];fixture.hotends[2]=swap;confirm_hotend_activate_cb(NULL);assert(!strcmp(sent,"ACTIVATE_EXTRUDER EXTRUDER=extruder1"));
        close_hotend_list_cb(NULL);assert(!s_hotend_activate_popup&&!s_hotend_refresh_timer&&!s_hotend_send&&timers()==baseline);
        reset();hotend_open();before=sends;++generation;refresh_hotend_list(NULL);assert(!s_hotend_list_popup&&!s_hotend_activate_popup);confirm_hotend_activate_cb(NULL);assert(sends==before&&timers()==baseline);
        reset();hotend_open();before=sends;fixture.hotend_count=1;confirm_hotend_activate_cb(NULL);assert(sends==before&&s_hotend_activate_popup);close_hotend_list_cb(NULL);
        reset();hotend_open();before=sends;strcpy(fixture.printer_state,"printing");confirm_hotend_activate_cb(NULL);assert(sends==before&&s_hotend_activate_popup);close_hotend_list_cb(NULL);
        reset();hotend_open();during_send=2;confirm_hotend_activate_cb(NULL);assert(!s_hotend_activate_popup&&s_hotend_pending&&!s_hotend_dispatching&&timers()==baseline);
        ui_printer_popups_show_hotends(NULL,&fixture);before=sends;click(s_hotend_activate_buttons[1]);assert(!s_hotend_activate_popup&&sends==before);lv_tick_inc(5000);refresh_hotend_list(NULL);assert(!s_hotend_pending);close_hotend_list_cb(NULL);
        reset();hotend_open();during_send=5;confirm_hotend_activate_cb(NULL);assert(!s_hotend_list_popup&&!s_hotend_activate_popup&&!s_hotend_confirm_status&&!s_hotend_dispatching);lv_tick_inc(5000);hotend_open();close_hotend_list_cb(NULL);
        for(int i=0;i<10;i++){motion_open();ui_printer_popups_show_hotends(NULL,&fixture);lv_obj_delete(s_toolhead_popup);lv_obj_delete(s_hotend_list_popup);assert(timers()==baseline&&!s_xy_step&&!s_hotend_refresh_timer);}
    }
    lv_indev_t *pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_touch);native_layout_checks(d,pointer);
    puts("PASS: Tabbed Toolhead: all controls fit without scrolling at 1024x600, button fit/centering, pinned Close and smaller-view touch fallback; Toolhead/hotend lifecycle, duplicate suppression, rejected-send retries, telemetry/timeout, printer switching, reentrant cleanup, all five themes/densities/text sizes");return 0;
}
