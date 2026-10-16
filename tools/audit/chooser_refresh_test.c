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
static uint32_t revision,generation=1;
static unsigned profile_reads,select_calls,manage_calls;
static void (*read_hook)(void),(*snapshot_hook)(void),(*health_hook)(void),(*preview_hook)(void),(*select_hook)(void),(*manage_hook)(void);
uint32_t moonraker_config_generation(void){return generation;}
static uint16_t pixels[216*108];
static lv_image_dsc_t image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=216,.h=108,.stride=432},.data_size=216*108*2,.data=(const uint8_t*)pixels};
const moonraker_profile_t *moonraker_config_profile(int i){profile_reads++;if(read_hook){void(*hook)(void)=read_hook;read_hook=NULL;hook();}return &profiles[i];}
int moonraker_config_active_profile_index(void){return active;}
void moonraker_state_snapshot(moonraker_state_t *out){if(snapshot_hook){void(*hook)(void)=snapshot_hook;snapshot_hook=NULL;hook();}*out=state;}
bool printer_profile_health_get(int i,bool *out){if(health_hook){void(*hook)(void)=health_hook;health_hook=NULL;hook();}*out=known[i];return online[i];}
bool printer_profile_health_get_live_state(int i,char *out,size_t n){snprintf(out,n,"%.31s",health[i]);return health[i][0]!=0;}
bool printer_profile_health_live_state_fresh(int i,int64_t age){assert(age==5000000LL);return fresh[i];}
const lv_image_dsc_t *printer_preview_cache_image(int i,const char **file,uint32_t *rev){
 if(preview_hook){void(*hook)(void)=preview_hook;preview_hook=NULL;hook();}
 if(i!=0 || !preview_ready)return NULL;
 *file="print.gcode";*rev=revision;return &image;
}
static int selected_index=-1,managed_index=-1;
static void selected(int index){select_calls++;selected_index=index;if(select_hook)select_hook();}
static void managed(int index){manage_calls++;managed_index=index;if(manage_hook)manage_hook();}
static unsigned flushes,invalidations,notifications;
static void notified(lv_observer_t *o,lv_subject_t *subject){(void)o;(void)subject;notifications++;}
static uint16_t raster[1024*600];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){(void)a;memcpy(raster,p,sizeof(raster));flushes++;lv_display_flush_ready(d);}
static void snapshot(int theme){const char*folder=getenv("CHOOSER_TEXT_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/chooser-theme%d.ppm",folder,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(int i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}

static void invalidated(lv_event_t *e){(void)e;invalidations++;}
static void settle(lv_display_t *d){lv_refr_now(d);invalidations=0;}

static void contained(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2))fprintf(stderr,"outside theme=%d child=(%d,%d,%d,%d) parent=(%d,%d,%d,%d) text=%s\n",ui_theme_get_active(),a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2,lv_obj_check_type(c,&lv_label_class)?lv_label_get_text(c):"object");assert(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2);}
static bool overlapping(lv_obj_t*a,lv_obj_t*b){lv_area_t x,y;lv_obj_get_coords(a,&x);lv_obj_get_coords(b,&y);return x.x1<=y.x2&&y.x1<=x.x2&&x.y1<=y.y2&&y.y1<=x.y2;}
static void geometry(void)
{
 lv_obj_update_layout(s_root);
 assert(!lv_obj_has_flag(s_root,LV_OBJ_FLAG_SCROLLABLE));
 unsigned separators=0;
 for(unsigned i=0;i<lv_obj_get_child_count(s_root);i++){
  lv_obj_t *child=lv_obj_get_child(s_root,i);
  if(lv_obj_get_width(child)!=1)continue;
  separators++;contained(child,s_root);
  assert(!lv_obj_has_flag(child,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
  for(int j=0;j<4;j++)assert(!overlapping(child,s_cards[j].root));
  assert(lv_obj_get_y(child)>lv_obj_get_y(s_cards[0].root));
  assert(lv_obj_get_y(child)+lv_obj_get_height(child)<lv_obj_get_y(s_cards[3].root)+lv_obj_get_height(s_cards[3].root));
 }
 assert(separators==1);
 for(int i=0;i<4;i++){
  chooser_card_t*c=&s_cards[i];contained(c->root,s_root);
  assert(lv_obj_get_style_radius(c->root,0)==0);
  for(int j=0;j<i;j++)assert(!overlapping(c->root,s_cards[j].root));
  assert(lv_obj_get_x(c->root)==lv_obj_get_x(s_cards[i%2].root));
  assert(lv_obj_get_y(c->root)==lv_obj_get_y(s_cards[(i/2)*2].root));
  assert(lv_obj_get_width(c->preview_box)==116 && lv_obj_get_height(c->preview_box)==116);
  assert(lv_obj_get_y(c->hint)==lv_obj_get_y(c->active));
  assert(lv_obj_get_y(c->hint)==lv_obj_get_y(s_cards[i%2].hint));
  lv_obj_t *lower=lv_obj_get_child(c->root,0);
  assert(lv_obj_get_height(lower)==1 && lv_obj_get_width(lower)==lv_obj_get_width(c->root)-24);
  assert(lv_obj_get_y(lower)==lv_obj_get_height(c->root)-1);
  lv_obj_t*labels[]={c->name,c->endpoint,c->status,c->active,c->hint};
  for(unsigned j=0;j<5;j++){
   contained(labels[j],c->root);
   assert(lv_obj_get_height(labels[j])==lv_obj_get_style_text_font(labels[j],0)->line_height);
   if(j>=2){lv_point_t n;lv_text_get_size(&n,lv_label_get_text(labels[j]),lv_obj_get_style_text_font(labels[j],0),lv_obj_get_style_text_letter_space(labels[j],0),0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);assert(n.x<=lv_obj_get_width(labels[j]));}
   for(unsigned k=j+1;k<5;k++)assert(!overlapping(labels[j],labels[k]));
   assert(!overlapping(labels[j],c->preview_box));
  }
  contained(c->preview_box,c->root);contained(c->preview_icon,c->preview_box);
  assert(lv_obj_get_style_text_font(c->preview_icon,0)->line_height==lv_font_montserrat_32.line_height);
  assert(lv_label_get_long_mode(c->name)==LV_LABEL_LONG_DOT);
  assert(lv_label_get_long_mode(c->endpoint)==LV_LABEL_LONG_DOT);
 }
}


static lv_indev_t *pointer;
static lv_point_t touch;
static bool pressed;
static void read_touch(lv_indev_t *input,lv_indev_data_t *data){(void)input;data->point=touch;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_obj_t *object){lv_obj_update_layout(s_root);lv_area_t area;lv_obj_get_coords(object,&area);touch=(lv_point_t){(area.x1+area.x2)/2,(area.y1+area.y2)/2};pressed=true;lv_indev_read(pointer);pressed=false;lv_indev_read(pointer);}
static void native_card_taps(void)
{
 unsigned before=select_calls,managed_before=manage_calls;
 lv_obj_t *targets[]={s_cards[0].preview_box,s_cards[0].name,s_cards[0].endpoint,s_cards[0].status,s_cards[0].hint,s_cards[0].preview_icon,s_cards[0].root};
 for(unsigned i=0;i<sizeof(targets)/sizeof(targets[0]);++i){tap(targets[i]);assert(select_calls==++before&&selected_index==0);}
 preview_ready=true;ui_printer_chooser_refresh();assert(s_cards[0].preview_image);tap(s_cards[0].preview_image);assert(select_calls==++before&&selected_index==0);
 preview_ready=false;ui_printer_chooser_refresh();
 tap(s_cards[2].preview_box);assert(manage_calls==++managed_before&&managed_index==2);
 tap(s_manage_action);assert(manage_calls==++managed_before&&managed_index==-1);
}

static void click(lv_obj_t *object){lv_obj_send_event(object,LV_EVENT_CLICKED,NULL);}
static void wrong_selected(int index){(void)index;assert(!"Repeated Show replaced Select");}
static void wrong_managed(int index){(void)index;assert(!"Repeated Show replaced Manage");}
static void nested_actions(void){click(s_cards[0].root);manage_clicked_cb(NULL);}
static void reopen_chooser(void){ui_printer_chooser_hide();ui_printer_chooser_show(selected,managed);}
static void reopen_deleted(lv_event_t *event){(void)event;ui_printer_chooser_show(selected,managed);}
static void change_generation(void){generation++;}
static void delete_status(void){lv_obj_delete(s_cards[0].status);}
static bool arm_replace_observer,arm_delete_observer;
static void replacing_observer(lv_observer_t *observer,lv_subject_t *subject)
{
 (void)observer;(void)subject;
 static bool in_callback;
 if(arm_replace_observer&&!in_callback){arm_replace_observer=false;in_callback=true;reopen_chooser();in_callback=false;}
}
static void deleting_observer(lv_observer_t *observer,lv_subject_t *subject)
{
 (void)observer;(void)subject;if(arm_delete_observer&&s_cards[0].status){arm_delete_observer=false;lv_obj_delete(s_cards[0].status);}
}
static void chooser_safety(void)
{
 ui_printer_chooser_show(selected,managed);ui_printer_chooser_show(wrong_selected,wrong_managed);
 assert(s_select_cb==selected&&s_manage_cb==managed);
 native_card_taps();
 unsigned before=select_calls,managed_before=manage_calls;
 lv_obj_add_state(s_cards[0].root,LV_STATE_DISABLED);click(s_cards[0].root);assert(select_calls==before);lv_obj_remove_state(s_cards[0].root,LV_STATE_DISABLED);
 lv_obj_t *fake=lv_button_create(s_root);lv_obj_add_event_cb(fake,card_clicked_cb,LV_EVENT_CLICKED,(void*)0);click(fake);assert(select_calls==before);lv_obj_delete(fake);
 fake=lv_obj_create(s_cards[0].root);lv_obj_add_flag(fake,LV_OBJ_FLAG_EVENT_BUBBLE);click(fake);assert(select_calls==before);lv_obj_delete(fake);
 lv_obj_add_state(s_manage_action,LV_STATE_DISABLED);manage_clicked_cb(NULL);assert(manage_calls==managed_before);lv_obj_remove_state(s_manage_action,LV_STATE_DISABLED);
 fake=lv_button_create(s_root);lv_obj_add_event_cb(fake,manage_clicked_cb,LV_EVENT_CLICKED,NULL);click(fake);assert(manage_calls==managed_before);lv_obj_delete(fake);
 select_hook=nested_actions;click(s_cards[0].root);select_hook=NULL;assert(select_calls==before+1&&manage_calls==managed_before);
 manage_hook=nested_actions;manage_clicked_cb(NULL);manage_hook=NULL;assert(manage_calls==managed_before+1&&select_calls==before+1);
 before=select_calls;read_hook=reopen_chooser;click(s_cards[0].root);assert(s_root&&s_timer&&select_calls==before);
 select_hook=reopen_chooser;click(s_cards[0].root);select_hook=NULL;assert(s_root&&s_timer&&select_calls==before+1);
 lv_timer_t *timer=s_timer,*other=lv_timer_create(chooser_timer_cb,60000,NULL);unsigned reads=profile_reads;
 chooser_timer_cb(NULL);chooser_timer_cb(other);assert(s_timer==timer&&profile_reads==reads);lv_timer_delete(other);
 lv_obj_delete(s_manage_action);manage_clicked_cb(NULL);assert(!s_manage_action&&s_root);
 ui_printer_chooser_hide();ui_printer_chooser_show(selected,managed);
 lv_obj_t **fields[]={&s_cards[0].name,&s_cards[0].endpoint,&s_cards[0].status,&s_cards[0].hint,&s_cards[0].active,&s_cards[0].preview_icon};
 for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);++i){lv_obj_delete(*fields[i]);assert(!*fields[i]);ui_printer_chooser_refresh();}
 preview_ready=true;ui_printer_chooser_refresh();assert(s_cards[0].preview_image);
 lv_obj_delete(s_cards[0].preview_image);assert(!s_cards[0].preview_image);revision=0;ui_printer_chooser_refresh();assert(s_cards[0].preview_image&&lv_image_get_src(s_cards[0].preview_image)==&image);
 lv_obj_delete(s_cards[0].preview_box);ui_printer_chooser_refresh();assert(!s_cards[0].preview_box&&!s_cards[0].preview_image);
 ui_printer_chooser_hide();ui_printer_chooser_show(selected,managed);
 image.header.w=0;ui_printer_chooser_refresh();image.header.w=216;ui_printer_chooser_refresh();assert(s_cards[0].preview_image);
 for(unsigned phase=0;phase<4;++phase){
  if(phase==0)snapshot_hook=reopen_chooser;
  if(phase==1)read_hook=reopen_chooser;
  if(phase==2)health_hook=reopen_chooser;
  if(phase==3)preview_hook=reopen_chooser;
  ui_printer_chooser_refresh();assert(s_root&&s_timer&&s_cards[0].status);
 }
 snapshot_hook=reopen_chooser;read_hook=reopen_chooser;ui_printer_chooser_refresh();
 lv_timer_handler();assert(s_root&&s_timer&&!s_show_pending&&!s_flush_queued);
 snapshot_hook=reopen_chooser;read_hook=reopen_chooser;ui_printer_chooser_refresh();ui_printer_chooser_hide();
 lv_timer_handler();assert(!s_root&&!s_timer&&!s_show_pending&&!s_flush_queued);ui_printer_chooser_show(selected,managed);
 read_hook=change_generation;ui_printer_chooser_refresh();assert(s_root&&s_timer);ui_printer_chooser_refresh();
 health_hook=delete_status;ui_printer_chooser_refresh();assert(!s_cards[0].status);ui_printer_chooser_hide();
 ui_printer_chooser_show(selected,managed);lv_obj_add_event_cb(s_root,reopen_deleted,LV_EVENT_DELETE,NULL);ui_printer_chooser_hide();assert(s_root&&s_timer&&s_cards[0].name&&s_cards[0].name_bound);
 lv_obj_delete(s_root);assert(!s_root&&!s_timer&&!s_select_cb&&!s_manage_cb&&!s_cards[0].root&&!s_cards[0].subjects_ready);
 ui_printer_chooser_show(selected,managed);
 lv_subject_add_observer(&s_cards[0].status_subject,replacing_observer,NULL);
 arm_replace_observer=true;set_status_text(&s_cards[0],"Replace from observer");assert(s_root&&s_timer&&s_cards[0].status_bound&&!s_show_pending);
 lv_subject_add_observer(&s_cards[0].status_subject,deleting_observer,NULL);
 arm_delete_observer=true;set_status_text(&s_cards[0],"Delete feedback");assert(!s_cards[0].status&&!s_cards[0].status_bound);
 ui_printer_chooser_hide();assert(!s_root&&!s_timer&&!s_show_pending);
 preview_ready=false;revision=1;
}

int main(void){
 assert(!strcmp(chooser_display_status("ready"),"Ready"));
 assert(!strcmp(chooser_display_status("standby"),"Standby"));
 assert(!strcmp(chooser_display_status("ONLINE"),"Online"));
 assert(!strcmp(chooser_display_status("custom_state"),"custom_state"));
 lv_init();lv_display_t *d=lv_display_create(1024,600);
 static uint8_t buffers[2][1024*600*2];
 lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(d,buffers[0],buffers[1],sizeof(buffers[0]),LV_DISPLAY_RENDER_MODE_DIRECT);
 lv_display_set_flush_cb(d,flush);lv_display_add_event_cb(d,invalidated,LV_EVENT_INVALIDATE_AREA,NULL);
 pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_touch);
 profiles[0]=(moonraker_profile_t){.configured=true,.name="Printer A",.host="printer-a.local",.port=7125};
 profiles[1]=(moonraker_profile_t){.configured=true,.name="Printer B",.host="printer-b.local",.port=7126};
 known[1]=online[1]=fresh[1]=true;snprintf(health[1],32,"printing");
 for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  ui_theme_set_density((ui_density_id_t)density);
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  known[0]=online[0]=false;state=(moonraker_state_t){0};active=0;preview_ready=false;revision=1;
  unsigned events=lv_display_get_event_count(d);
  ui_printer_chooser_show(selected,managed);assert(s_root && s_timer);
  assert(!strcmp(lv_label_get_text(s_cards[0].status),"Checking..."));
  assert(!strcmp(lv_label_get_text(s_cards[1].status),"Printing"));
  assert(!strcmp(lv_label_get_text(s_cards[2].status),"Not configured"));
  assert(!strcmp(lv_label_get_text(s_cards[2].hint),"Set up " LV_SYMBOL_RIGHT));
  assert(lv_color_eq(lv_obj_get_style_text_color(s_cards[2].name,0),UI_TEXT_DIM));
  assert(lv_obj_get_style_text_font(s_cards[2].status,0)->line_height<=UI_FONT_CAPTION->line_height);
  assert(lv_obj_has_state(s_cards[0].root,LV_STATE_USER_1));
  assert(!lv_obj_has_state(s_cards[1].root,LV_STATE_USER_1));
  for(int i=0;i<4;i++)assert(!strcmp(lv_label_get_text(s_cards[i].preview_icon),i>=2?LV_SYMBOL_PLUS:LV_SYMBOL_FILE));
  geometry();if(ui_theme_is_studio()){assert(lv_obj_get_x(s_cards[1].root)+lv_obj_get_width(s_cards[1].root)==976);assert(lv_obj_get_y(s_cards[3].root)+lv_obj_get_height(s_cards[3].root)==424);}if(density==1&&large)snapshot(theme);
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
  known[0]=online[0]=true;ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[0].status),"Connecting..."));
  online[0]=false;ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[0].status),"Offline / retrying"));
  notified_before=notifications;
  state.moonraker_ok=state.live_data_ok=true;snprintf(state.printer_state,sizeof(state.printer_state),"paused");
  ui_printer_chooser_refresh();assert(invalidations>0);
  assert(!strcmp(lv_label_get_text(s_cards[0].status),"Paused"));assert(notifications==notified_before+1);settle(d);
  ui_printer_chooser_refresh();assert(!invalidations);
  state.live_data_ok=false;strcpy(state.printer_state,"shutdown");
  strcpy(health[0],"printing");known[0]=online[0]=true;
  ui_printer_chooser_refresh();assert(!strcmp(lv_label_get_text(s_cards[0].status),"Klipper shutdown"));
  assert(!lv_color_eq(lv_obj_get_style_text_color(s_cards[0].status,0),UI_OK_BRIGHT));
  state.moonraker_ok=false;strcpy(state.printer_state,"printing");
  ui_printer_chooser_refresh();assert(!strcmp(lv_label_get_text(s_cards[0].status),"Offline / retrying"));
  state.moonraker_ok=state.live_data_ok=true;strcpy(state.printer_state,"paused");
  ui_printer_chooser_refresh();assert(!strcmp(lv_label_get_text(s_cards[0].status),"Paused"));settle(d);
  preview_ready=true;ui_printer_chooser_refresh();
  assert(s_cards[0].preview_image && !lv_obj_has_flag(s_cards[0].preview_image,LV_OBJ_FLAG_HIDDEN));
  assert(lv_image_get_scale(s_cards[0].preview_image)==128);
  assert(image.header.w*lv_image_get_scale(s_cards[0].preview_image)/256==108);
  assert(image.header.h*lv_image_get_scale(s_cards[0].preview_image)/256==54);
  settle(d);before=flushes;
  for(int i=0;i<6;i++){lv_tick_inc(500);lv_timer_handler();}
  assert(!invalidations && flushes==before);
  revision++;ui_printer_chooser_refresh();assert(invalidations>0);settle(d);
  preview_ready=false;ui_printer_chooser_refresh();assert(lv_obj_has_flag(s_cards[0].preview_image,LV_OBJ_FLAG_HIDDEN));settle(d);
  ui_printer_chooser_refresh();assert(!invalidations);
  fresh[1]=false;ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[1].status),"Printing"));
  assert(lv_color_eq(lv_obj_get_style_text_color(s_cards[1].status,0),UI_TEXT_DIM));
  online[1]=false;ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[1].status),"Offline"));
  assert(lv_color_eq(lv_obj_get_style_text_color(s_cards[1].status,0),
   theme==UI_THEME_CLASSIC?UI_TEXT_ERROR:UI_DANGER_BRIGHT));
  known[1]=false;ui_printer_chooser_refresh();assert(!strcmp(lv_label_get_text(s_cards[1].status),"Verifying..."));
  known[1]=online[1]=fresh[1]=true;snprintf(health[1],32,"paused");ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[1].status),"Paused"));
  snprintf(health[1],32,"printing");ui_printer_chooser_refresh();
  moonraker_profile_t original=profiles[0];
  memset(profiles[0].name,'W',sizeof(profiles[0].name)-1);profiles[0].name[sizeof(profiles[0].name)-1]=0;
  memset(profiles[0].host,'W',sizeof(profiles[0].host)-1);profiles[0].host[sizeof(profiles[0].host)-1]=0;
  ui_printer_chooser_refresh();geometry();settle(d);before=flushes;notified_before=notifications;
  for(int i=0;i<6;i++){lv_tick_inc(500);lv_timer_handler();}
  if(invalidations || flushes!=before || notifications!=notified_before)fprintf(stderr,"long refresh theme=%d density=%d large=%d invalid=%u flush=%u notify=%u name=%s rendered=%s endpoint=%s rendered=%s\n",theme,density,large,invalidations,flushes-before,notifications-notified_before,lv_label_get_text(s_cards[0].name),s_cards[0].name_rendered,lv_label_get_text(s_cards[0].endpoint),s_cards[0].endpoint_rendered);
  assert(!invalidations && flushes==before && notifications==notified_before);
  profiles[0]=original;
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
  assert(!strcmp(lv_label_get_text(s_cards[0].name),"Add printer 1"));
  assert(!strcmp(lv_label_get_text(s_cards[0].endpoint),"Available profile slot"));
  lv_obj_send_event(s_cards[0].root,LV_EVENT_CLICKED,NULL);assert(managed_index==0);
  profiles[0]=original;ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[0].name),original.name));
  assert(lv_color_eq(lv_obj_get_style_text_color(s_cards[0].name,0),UI_TEXT_BRIGHT));
  /* Previously empty slots must bind their newly configured profile. */
  profiles[2]=(moonraker_profile_t){.configured=true,.name="New Printer",.host="new.local",.port=7127};
  ui_printer_chooser_refresh();assert(!strcmp(lv_label_get_text(s_cards[2].name),"New Printer"));
  assert(!strcmp(lv_label_get_text(s_cards[2].endpoint),"new.local:7127"));
  lv_obj_send_event(s_cards[2].root,LV_EVENT_CLICKED,NULL);assert(selected_index==2);
  profiles[2]=(moonraker_profile_t){0};ui_printer_chooser_refresh();
  assert(!strcmp(lv_label_get_text(s_cards[2].name),"Add printer 3"));
  active=1;ui_printer_chooser_refresh();assert(lv_obj_has_flag(s_cards[0].active,LV_OBJ_FLAG_HIDDEN));
  assert(!lv_obj_has_flag(s_cards[1].active,LV_OBJ_FLAG_HIDDEN));
  assert(!lv_obj_has_state(s_cards[0].root,LV_STATE_USER_1));
  assert(lv_obj_has_state(s_cards[1].root,LV_STATE_USER_1));
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
  set_name_text(&s_cards[0],"Workshop printer with long name");set_endpoint_text(&s_cards[0],"workshop-printer-long-hostname.local:7125");set_status_text(&s_cards[0],"OFFLINE / RETRYING");geometry();
  /* A card may be deleted independently before the chooser itself closes. */
  lv_obj_delete(s_cards[3].root);assert(!s_cards[3].root && !s_cards[3].status_bound && !s_cards[3].name_bound && !s_cards[3].endpoint_bound);
  ui_printer_chooser_refresh();
  ui_printer_chooser_hide();assert(!s_root && !s_timer && !s_cards[0].root && !s_cards[0].status_bound);
  assert(lv_display_get_event_count(d)==events);
  chooser_safety();assert(lv_display_get_event_count(d)==events);
 }
 lv_indev_delete(pointer);lv_display_delete(d);lv_deinit();
 puts("PASS: Two-column chooser separators, aligned actions/previews, readable status/empty slots, active transitions and static long labels; 500ms chooser timers stay redraw-free when unchanged, all five themes/densities/text sizes, section/label bounds and active/inactive state lifetime, live status/preview/revision/profile updates, silent name/endpoint/status subjects, profile edits/empty slots/click routing, fallback repair and section/chooser teardown");
}
