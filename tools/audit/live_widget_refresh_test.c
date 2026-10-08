#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_drybox_page.c"
/* Host archive omits LVGL float printf. Use libc for this fixture only;
 * production formatting code is unchanged and separately compiled below. */
#undef lv_snprintf
#define lv_snprintf snprintf
#include "ui_devices_live_values.c"
#undef lv_snprintf
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_state_t snapshot;
static device_descriptor_t devices[3];
static bool present=true, expects=true;
void moonraker_state_snapshot(moonraker_state_t *s){*s=snapshot;}
void moonraker_filament_state_snapshot(moonraker_filament_state_t *s){memset(s,0,sizeof(*s));}
bool device_catalog_controller_get(size_t i,device_descriptor_t *d){if(!present || i>=3)return false;*d=devices[i];return true;}
bool device_catalog_controller_has_live_value_source(const char *n){(void)n;return expects;}
static unsigned invalidations;
static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){(void)a;(void)p;lv_display_flush_ready(d);}
static void same(const lv_obj_t *l,const char *s){if(strcmp(lv_label_get_text(l),s))fprintf(stderr,"Expected [%s], received [%s]\n",s,lv_label_get_text(l));assert(!strcmp(lv_label_get_text(l),s));}
static void color(lv_obj_t *l,lv_color_t c){assert(lv_color_eq(lv_obj_get_style_text_color(l,0),c));}
static ui_drybox_page_t fixture(void){
 ui_drybox_page_t p={.panel=lv_obj_create(lv_screen_active())};
 lv_obj_t *banner=ui_status_banner_create(p.panel,0,0,854,54);p.banner_label=ui_status_banner_state_label(banner);
 p.humidity_label=lv_label_create(p.panel);p.air_label=lv_label_create(p.panel);
 p.center_label=lv_label_create(p.panel);p.target_label=lv_label_create(p.panel);
 p.heater_label=lv_label_create(p.panel);p.fan_label=lv_label_create(p.panel);
 s_program_status_label=lv_label_create(p.panel);s_humidity_condition_label=lv_label_create(p.panel);
 s_heater_dot=lv_obj_create(p.panel);s_fan_dot=lv_obj_create(p.panel);s_active_dot=lv_obj_create(p.panel);
 s_humidity_bar=lv_bar_create(p.panel);s_heater_activity_bar=lv_bar_create(p.panel);
 for(int i=DRYBOX_PROGRAM_PLA;i<DRYBOX_PROGRAM_COUNT;i++)s_program_buttons[i]=lv_button_create(p.panel);
 return p;
}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];
 lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);
 lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 for(unsigned theme=0;theme<4;theme++)for(unsigned large=0;large<2;large++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  for(int cycle=0;cycle<3;cycle++){
   ui_drybox_page_t p=fixture();
   ui_drybox_page_state_t s={.banner_text="READY",.air_temp=32.4f,.center_temp=35.2f,.humidity=22.5f,.heater_target=45,.heater_on=true,.fan_speed=50,.active_program=UI_DRYBOX_PROGRAM_PLA};
   ui_drybox_page_refresh(&p,&s);same(p.air_label,"32.4 C");same(p.humidity_label,"22.5");color(p.humidity_label,UI_ACCENT_CYAN);
   same(s_program_status_label,"PLA ACTIVE");assert(lv_bar_get_value(s_humidity_bar)==22);
   assert(lv_bar_get_value(s_heater_activity_bar)==100);assert(!lv_obj_has_state(s_program_buttons[DRYBOX_PROGRAM_PLA],LV_STATE_DISABLED));
   lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++)ui_drybox_page_refresh(&p,&s);assert(!invalidations);
   s.banner_text="DISCONNECTED";ui_drybox_page_refresh(&p,&s);same(p.humidity_label,"--.-");color(p.humidity_label,UI_TEXT_DIM);
   assert(lv_obj_has_state(s_program_buttons[DRYBOX_PROGRAM_PLA],LV_STATE_DISABLED));assert(!lv_bar_get_value(s_humidity_bar));
   lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++)ui_drybox_page_refresh(&p,&s);assert(!invalidations);
   s.banner_text="READY";s.active_program=UI_DRYBOX_PROGRAM_PETG;s.heater_on=false;s.fan_speed=0;s.heater_target=0;
   ui_drybox_page_refresh(&p,&s);color(p.humidity_label,UI_ACCENT_CYAN);same(s_program_status_label,"PETG ACTIVE");same(p.heater_label,"OFF");same(p.fan_label,"0 %");color(p.target_label,UI_TEXT_DIM);
   const float humidity[]={9,10,25,40,120,-4};const char *conditions[]={"VERY DRY","IDEAL","MODERATE","HIGH","HIGH","VERY DRY"};
   for(unsigned i=0;i<6;i++){s.humidity=humidity[i];ui_drybox_page_refresh(&p,&s);same(s_humidity_condition_label,conditions[i]);assert(lv_bar_get_value(s_humidity_bar)==(i==4?100:i==5?0:(int)humidity[i]));}
   ui_drybox_page_cleanup(&p);assert(!p.panel && !s_humidity_bar && !s_program_buttons[DRYBOX_PROGRAM_PLA]);
   lv_obj_t *owner=lv_obj_create(lv_screen_active());ui_devices_live_values_init(owner);
   memset(devices,0,sizeof(devices));strcpy(devices[0].object_name,"heater_bed");strcpy(devices[1].object_name,"generic");strcpy(devices[2].object_name,"other");
   devices[1].live_value_valid=true;strcpy(devices[1].live_value,"ON");snapshot.bed_temp=60;snapshot.bed_target=60;present=true;expects=true;
   lv_obj_t *labels[3];for(unsigned i=0;i<3;i++){labels[i]=lv_label_create(owner);ui_devices_live_values_register(i,labels[i],i);}
   ui_devices_live_values_update();same(labels[0],"60.0 / 60.0 C");same(labels[1],"ON");same(labels[2],"WAITING FOR DATA");color(labels[2],UI_TEXT_DIM);
   lv_refr_now(d);invalidations=0;for(int i=0;i<100;i++)ui_devices_live_values_update();assert(!invalidations);
   snapshot.bed_temp=61;devices[2].live_value_valid=true;strcpy(devices[2].live_value,"25 %");ui_devices_live_values_update();same(labels[0],"61.0 / 60.0 C");same(labels[2],"25 %");color(labels[2],UI_TEXT_BRIGHT);
   devices[2].live_value_valid=false;expects=false;ui_devices_live_values_update();same(labels[2],"DISCOVERED");
   present=false;ui_devices_live_values_update();same(labels[0],"--");
   ui_devices_live_values_clear();lv_obj_t *rebuilt=lv_label_create(owner);ui_devices_live_values_register(0,rebuilt,1);present=true;ui_devices_live_values_update();same(rebuilt,"ON");
   ui_devices_live_values_close();lv_obj_delete(owner);ui_devices_live_values_update();
  }
 }
 lv_display_delete(d);lv_deinit();puts("PASS: Devices/Drybox including shared banner: silent repeated refresh, live updates, thresholds, offline recovery, disabled controls, rebuilds and cleanup across themes/text sizes");
}
