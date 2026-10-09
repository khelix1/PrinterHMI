#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_printer_chooser.c"

bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static moonraker_profile_t profiles[4];
static moonraker_state_t state;
static bool known[4],online[4],fresh[4];
static char health[4][32];
static int active;
static bool preview_ready;
static uint32_t revision;
static uint16_t pixels[16];
static lv_image_dsc_t image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=4,.h=4,.stride=8},.data_size=32,.data=(const uint8_t*)pixels};
const moonraker_profile_t *moonraker_config_profile(int i){return &profiles[i];}
int moonraker_config_active_profile_index(void){return active;}
void moonraker_state_snapshot(moonraker_state_t *out){*out=state;}
bool printer_profile_health_get(int i,bool *out){*out=known[i];return online[i];}
bool printer_profile_health_get_live_state(int i,char *out,size_t n){snprintf(out,n,"%.31s",health[i]);return health[i][0]!=0;}
bool printer_profile_health_live_state_fresh(int i,int64_t age){assert(age==5000000LL);return fresh[i];}
const lv_image_dsc_t *printer_preview_cache_image(int i,const char **file,uint32_t *rev){
 if(i!=0 || !preview_ready)return NULL;
 *file="print.gcode";*rev=revision;return &image;
}
static int selected_index=-1,managed_index=-1;
static void selected(int index){selected_index=index;}
static void managed(int index){managed_index=index;}
static unsigned flushes,invalidations,notifications;
static void notified(lv_observer_t *o,lv_subject_t *subject){(void)o;(void)subject;notifications++;}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){(void)a;memcpy(raster,p,sizeof(raster));flushes++;lv_display_flush_ready(d);}
static void snapshot(int theme){const char*folder=getenv("CHOOSER_TEXT_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/chooser-theme%d.ppm",folder,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}

static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static void settle(lv_display_t *d){lv_refr_now(d);invalidations=0;}

static void contained(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);}
static bool overlapping(lv_obj_t*a,lv_obj_t*b){lv_area_t x,y;lv_obj_get_coords(a,&x);lv_obj_get_coords(b,&y);return x.x1<=y.x2&&y.x1<=x.x2&&x.y1<=y.y2&&y.y1<=x.y2;}
static void geometry(void){lv_obj_update_layout(s_root);for(int i=0;i<4;i++){chooser_card_t*c=&s_cards[i];contained(c->root,s_root);lv_obj_t*labels[]={c->name,c->endpoint,c->status,c->active,c->hint};for(unsigned j=0;j<5;j++){contained(labels[j],c->root);assert(lv_obj_get_height(labels[j])==lv_obj_get_style_text_font(labels[j],0)->line_height);if(j>=2){lv_point_t n;lv_text_get_size(&n,lv_label_get_text(labels[j]),lv_obj_get_style_text_font(labels[j],0),lv_obj_get_style_text_letter_space(labels[j],0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);assert(n.x<=lv_obj_get_width(labels[j]));}}assert(!overlapping(c->hint,c->active));assert(!overlapping(c->name,c->active));assert(!overlapping(c->name,c->endpoint));assert(!overlapping(c->endpoint,c->status));contained(c->preview,c->preview_box);contained(c->preview_icon,c->preview_box);assert(!overlapping(c->preview,c->preview_icon));}}

int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);
 static uint8_t buffers[2][1024*600*2];
 lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(d,buffers[0],buffers[1],sizeof(buffers[0]),LV_DISPLAY_RENDER_MODE_DIRECT);
 lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 profiles[0]=(moonraker_profile_t){.configured=true,.name="Printer A",.host="printer-a.local",.port=7125};
 profiles[1]=(moonraker_profile_t){.configured=true,.name="Printer B",.host="printer-b.local",.port=7126};
 known[1]=online[1]=fresh[1]=true;snprintf(health[1],32,"printing");
 for(int theme=0;theme<4;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  ui_theme_set_density((ui_density_id_t)density);
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  state=(moonraker_state_t){0};active=0;preview_ready=false;revision=1;
  unsigned events=lv_display_get_event_count(d);
  ui_printer_chooser_show(selected,managed);assert(s_root && s_timer);
  assert(!strcmp(lv_label_get_text(s_cards[1].status),"printing"));
  assert(!strcmp(lv_label_get_text(s_cards[2].status),"NOT CONFIGURED"));
  assert(!strcmp(lv_label_get_text(s_cards[2].hint),"TAP TO ADD"));
  geometry();if(density==1&&large)snapshot(theme);
  for(int i=0;i<4;i++){
   assert(s_cards[i].status_bound && s_cards[i].name_bound && s_cards[i].endpoint_bound);
   assert(!strcmp(lv_subject_get_string(&s_cards[i].name_subject),lv_label_get_text(s_cards[i].name)));
   assert(!strcmp(lv_subject_get_string(&s_cards[i].endpoint_subject),lv_label_get_text(s_cards[i].endpoint)));
   lv_subject_add_observer(&s_cards[i].name_subject,notified,NULL);
   lv_subject_add_observer(&s_cards[i].endpoint_subject,notified,NULL);
   assert(!strcmp(lv_subject_get_string(&s_cards[i].status_subject),lv_label_get_text(s_cards[i].status)));
   lv_subject_add_observer(&s_cards[i].status_subject,notified,NULL);
  }
  unsigned notified_before=notifications;
  settle(d);unsigned before=flushes;
  for(int i=0;i<6;i++){lv_tick_inc(500);lv_timer_handler();}
  assert(invalidations==0 && flushes==before && notifications==notified_before);
  state.moonraker_ok=true;snprintf(state.printer_state,sizeof(state.printer_state),"paused");
  ui_printer_chooser_refresh();assert(invalidations>0);
  assert(!strcmp(lv_label_get_text(s_cards[0].status),"paused"));assert(notifications==notified_before+1);settle(d);
  ui_printer_chooser_refresh();assert(!invalidations);
  preview_ready=true;ui_printer_chooser_refresh();
  assert(s_cards[0].preview_image && !lv_obj_has_flag(s_cards[0].preview_image,LV_OBJ_FLAG_HIDDEN));
  settle(d);before=flushes;
  for(int i=0;i<6;i++){lv_tick_inc(500);lv_timer_handler();}
  assert(!invalidations && flushes==before);
  revision++;ui_printer_chooser_refresh();assert(invalidations>0);settle(d);
  preview_ready=false;ui_printer_chooser_refresh();assert(lv_obj_has_flag(s_cards[0].preview_image,LV_OBJ_FLAG_HIDDEN));settle(d);
  ui_printer_chooser_refresh();assert(!invalidations);
  fresh[1]=false;ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[1].status),"printing"));
  assert(lv_color_eq(lv_obj_get_style_text_color(s_cards[1].status,0),UI_TEXT_DIM));
  online[1]=false;ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[1].status),"OFFLINE"));
  assert(lv_color_eq(lv_obj_get_style_text_color(s_cards[1].status,0),
   theme==UI_THEME_CLASSIC?UI_TEXT_ERROR:UI_DANGER_BRIGHT));
  known[1]=false;ui_printer_chooser_refresh();assert(!strcmp(lv_label_get_text(s_cards[1].status),"VERIFYING..."));
  known[1]=online[1]=fresh[1]=true;snprintf(health[1],32,"paused");ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[1].status),"paused"));
  snprintf(health[1],32,"printing");ui_printer_chooser_refresh();
  moonraker_profile_t original=profiles[0];
  snprintf(profiles[0].name,sizeof(profiles[0].name),"Renamed P4");
  snprintf(profiles[0].host,sizeof(profiles[0].host),"changed.local");profiles[0].port=7443;
  ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[0].name),"Renamed P4"));
  assert(!strcmp(lv_label_get_text(s_cards[0].endpoint),"changed.local:7443"));
  settle(d);before=flushes;notified_before=notifications;
  for(int i=0;i<6;i++){lv_tick_inc(500);lv_timer_handler();}
  assert(!invalidations && flushes==before && notifications==notified_before);
  lv_obj_send_event(s_cards[0].root,LV_EVENT_CLICKED,NULL);assert(selected_index==0);
  profiles[0].configured=false;ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[0].name),"ADD PRINTER 1"));
  assert(!strcmp(lv_label_get_text(s_cards[0].endpoint),"EMPTY PROFILE SLOT"));
  lv_obj_send_event(s_cards[0].root,LV_EVENT_CLICKED,NULL);assert(managed_index==0);
  profiles[0]=original;ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[0].name),original.name));
  /* Previously empty slots must bind their newly configured profile. */
  profiles[2]=(moonraker_profile_t){.configured=true,.name="New Printer",.host="new.local",.port=7127};
  ui_printer_chooser_refresh();assert(!strcmp(lv_label_get_text(s_cards[2].name),"New Printer"));
  assert(!strcmp(lv_label_get_text(s_cards[2].endpoint),"new.local:7127"));
  lv_obj_send_event(s_cards[2].root,LV_EVENT_CLICKED,NULL);assert(selected_index==2);
  profiles[2]=(moonraker_profile_t){0};ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[2].name),"ADD PRINTER 3"));
  active=1;ui_printer_chooser_refresh();assert(lv_obj_has_flag(s_cards[0].active,LV_OBJ_FLAG_HIDDEN));
  assert(!lv_obj_has_flag(s_cards[1].active,LV_OBJ_FLAG_HIDDEN));
  /* Direct/fallback writes must repair without requiring a subject change. */
  set_status_text(&s_cards[0],"READY");
  char long_status[160];memset(long_status,'Z',sizeof(long_status)-1);long_status[159]=0;
  set_name_text(&s_cards[0],"short");set_endpoint_text(&s_cards[0],"short.local:7125");
  set_name_text(&s_cards[0],long_status);set_endpoint_text(&s_cards[0],long_status);
  assert(!strcmp(lv_label_get_text(s_cards[0].name),long_status));
  assert(!strcmp(lv_label_get_text(s_cards[0].endpoint),long_status));
  set_name_text(&s_cards[0],"short");set_endpoint_text(&s_cards[0],"short.local:7125");
  assert(!strcmp(lv_label_get_text(s_cards[0].name),"short"));
  assert(!strcmp(lv_label_get_text(s_cards[0].endpoint),"short.local:7125"));
  s_cards[0].name_bound=false;s_cards[0].endpoint_bound=false;
  set_name_text(&s_cards[0],"fallback name");set_endpoint_text(&s_cards[0],"fallback.local:7125");
  assert(!strcmp(lv_label_get_text(s_cards[0].name),"fallback name"));
  assert(!strcmp(lv_label_get_text(s_cards[0].endpoint),"fallback.local:7125"));
  s_cards[0].name_bound=true;s_cards[0].endpoint_bound=true;
  set_status_text(&s_cards[0],long_status);assert(!strcmp(lv_label_get_text(s_cards[0].status),long_status));
  set_status_text(&s_cards[0],"READY");assert(!strcmp(lv_label_get_text(s_cards[0].status),"READY"));
  lv_label_set_text(s_cards[0].status,"external");set_status_text(&s_cards[0],"READY");
  assert(!strcmp(lv_label_get_text(s_cards[0].status),"READY"));
  s_cards[0].status_bound=false;set_status_text(&s_cards[0],"fallback");
  assert(!strcmp(lv_label_get_text(s_cards[0].status),"fallback"));s_cards[0].status_bound=true;
  set_name_text(&s_cards[0],"Workshop printer with long name");set_endpoint_text(&s_cards[0],"workshop-printer-long-hostname.local:7125");set_status_text(&s_cards[0],"OFFLINE / RETRYING");ui_value_set_text(s_cards[0].preview,"a_very_long_print_filename_with_details.gcode");geometry();
  /* A card may be deleted independently before the chooser itself closes. */
  lv_obj_delete(s_cards[3].root);assert(!s_cards[3].root && !s_cards[3].status_bound && !s_cards[3].name_bound && !s_cards[3].endpoint_bound);
  ui_printer_chooser_refresh();
  ui_printer_chooser_hide();assert(!s_root && !s_timer && !s_cards[0].root && !s_cards[0].status_bound);
  assert(lv_display_get_event_count(d)==events);
 }
 lv_display_delete(d);lv_deinit();
 puts("PASS: 500ms chooser timers stay redraw-free when unchanged, all four themes/densities/text sizes, card/label bounds and active/inactive state lifetime, live status/preview/revision/profile updates, silent name/endpoint/status subjects, profile edits/empty slots/click routing, fallback repair and card/chooser teardown");
}
