#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "custom_theme.h"
#include "ui_gcode_viewer.c"
#include "ui_tools.c"
static uint32_t generation=1;
static uint16_t raster[1024*600];
static size_t flushed_pixels;
static lv_point_t touch;
static bool pressed,fail_depth,fail_staging,fail_section;
static int64_t clock_step,clock_us;
void *gcode_test_calloc(size_t n,size_t size){if(fail_section&&n==30*332&&size==sizeof(uint32_t))return NULL;if(n==952*332&&((fail_depth&&size==sizeof(float))||(fail_staging&&size==sizeof(uint16_t))))return NULL;return calloc(n,size);}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
int64_t esp_timer_get_time(void){clock_us+=clock_step;return clock_us;}
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t*out){memset(out,0,sizeof(*out));}
void ui_shell_raise_topbar(void){}
void ui_shell_raise_nav(void){}
static bool accept_load,fake_running,accept_detail,accept_refine;
static gcode_viewer_refined_cb pending_refined;
static unsigned refinement_starts;
static unsigned requested_detail;
static gcode_viewer_progress_t fake_progress;
gcode_viewer_progress_t gcode_viewer_loader_progress(void){gcode_viewer_progress_t out=fake_progress;out.running=fake_running;return out;}
static gcode_viewer_ready_cb_t pending_ready;
void gcode_viewer_loader_cancel(void){fake_running=false;pending_ready=NULL;pending_refined=NULL;}
bool gcode_viewer_loader_running(void){return fake_running;}
bool gcode_viewer_loader_start(const char*f,gcode_viewer_ready_cb_t cb){(void)f;if(!accept_load)return false;fake_running=true;pending_ready=cb;return true;}
void gcode_viewer_loader_free(gcode_toolpath_t*p){if(p){mesh_preview_free(p->overview_mesh,free);free(p->surface);free(p->cache);free(p);}}
bool gcode_viewer_loader_detail(const gcode_toolpath_t *m,unsigned layer,gcode_viewer_ready_cb_t cb){assert(m->cache);if(!accept_detail)return false;requested_detail=layer;fake_running=true;pending_ready=cb;return true;}
bool gcode_viewer_loader_refine(const char *file,const gcode_toolpath_t *model,gcode_viewer_refined_cb cb){(void)file;(void)model;if(!accept_refine)return false;pending_refined=cb;fake_running=true;refinement_starts++;return true;}
static void read_touch(lv_indev_t*i,lv_indev_data_t*d){(void)i;d->point=touch;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){uint16_t*v=(uint16_t*)p;flushed_pixels+=(size_t)(a->x2-a->x1+1)*(a->y2-a->y1+1);for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static lv_obj_t *find(lv_obj_t *root,const char *name){
 for(unsigned i=0;i<lv_obj_get_child_count(root);i++){lv_obj_t*c=lv_obj_get_child(root,i);if(lv_obj_check_type(c,&lv_button_class)&&lv_obj_get_child_count(c)&&lv_obj_check_type(lv_obj_get_child(c,0),&lv_label_class)&&!strcmp(lv_label_get_text(lv_obj_get_child(c,0)),name))return c;lv_obj_t *b=find(c,name);if(b)return b;}return NULL;
}
static bool contains_text(lv_obj_t *root,const char *needle){
 if(lv_obj_check_type(root,&lv_label_class)&&strstr(lv_label_get_text(root),needle))return true;
 for(unsigned i=0;i<lv_obj_get_child_count(root);i++)if(contains_text(lv_obj_get_child(root,i),needle))return true;
 return false;
}
static void tap(lv_indev_t*i,lv_obj_t*b){assert(b);lv_obj_update_layout(lv_screen_active());lv_obj_update_layout(lv_layer_top());lv_area_t a;lv_obj_get_coords(b,&a);touch=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};pressed=true;lv_indev_read(i);pressed=false;lv_indev_read(i);lv_tick_inc(40);lv_timer_handler();}
static void scrub(lv_indev_t *input,int percent){
 lv_obj_update_layout(s.popup);lv_area_t a;lv_obj_get_coords(s.slider,&a);
 int max=lv_slider_get_max_value(s.slider);
 touch=(lv_point_t){a.x1+(a.x2-a.x1)*lv_slider_get_value(s.slider)/max,(a.y1+a.y2)/2};
 pressed=true;lv_indev_read(input);touch.x=a.x1+(a.x2-a.x1)*percent/100;lv_indev_read(input);
 pressed=false;lv_indev_read(input);lv_tick_inc(40);lv_timer_handler();
}
static void pinch_checks(lv_indev_t *input){
 lv_obj_update_layout(s.popup);lv_area_t a;lv_obj_get_coords(s.canvas,&a);
 int x=(a.x1+a.x2)/2,y=(a.y1+a.y2)/2;
 float yaw=s.view.yaw,pitch=s.view.pitch,zoom=s.view.zoom;
 printerhmi_gcode_viewer_multitouch_update(2,x-50,y,x+50,y);
 assert(s.pinch_active&&s.view.zoom==zoom);
 printerhmi_gcode_viewer_multitouch_update(2,x-100,y,x+100,y);
 assert(s.view.zoom>zoom&&s.view.zoom<=GCODE_VIEW_ZOOM_MAX&&s.view.yaw==yaw&&s.view.pitch==pitch);
 float expanded=s.view.zoom;
 for(int i=0;i<12;i++)printerhmi_gcode_viewer_multitouch_update(2,x-25,y,x+25,y);
 assert(s.view.zoom<expanded&&s.view.zoom<zoom&&s.view.zoom>=.25f);
 touch=(lv_point_t){x,y};pressed=true;lv_indev_read(input);touch.x+=30;lv_indev_read(input);
 assert(s.view.yaw==yaw);printerhmi_gcode_viewer_multitouch_update(1,touch.x,y,0,0);
 touch.x+=30;lv_indev_read(input);assert(s.block_one_finger&&s.view.yaw==yaw);
 pressed=false;lv_indev_read(input);printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);
 touch=(lv_point_t){x,y};pressed=true;lv_indev_read(input);touch.x+=30;lv_indev_read(input);pressed=false;lv_indev_read(input);
 assert(s.view.yaw<yaw);printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);
 /* Tiny spans, controls and covering modals cannot start a zoom. */
 zoom=s.view.zoom;printerhmi_gcode_viewer_multitouch_update(2,x,y,x,y);assert(s.view.zoom==zoom);
 printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);
 printerhmi_gcode_viewer_multitouch_update(2,x,550,x+100,550);assert(!s.pinch_active&&s.view.zoom==zoom);
 printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);
 lv_obj_t *overlay=lv_obj_create(lv_layer_top());lv_obj_set_pos(overlay,0,0);lv_obj_set_size(overlay,1024,600);lv_obj_update_layout(overlay);
 printerhmi_gcode_viewer_multitouch_update(2,x-50,y,x+50,y);printerhmi_gcode_viewer_multitouch_update(2,x-100,y,x+100,y);
 assert(!s.pinch_active&&s.view.zoom==zoom);lv_obj_delete(overlay);printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);
 s.view.zoom=GCODE_VIEW_ZOOM_MAX;printerhmi_gcode_viewer_multitouch_update(2,x-6,y,x+6,y);printerhmi_gcode_viewer_multitouch_update(2,x-400,y,x+400,y);assert(s.view.zoom==GCODE_VIEW_ZOOM_MAX);
 printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);s.view.zoom=.25f;
 printerhmi_gcode_viewer_multitouch_update(2,x-100,y,x+100,y);printerhmi_gcode_viewer_multitouch_update(2,x-6,y,x+6,y);assert(s.view.zoom==.25f);
 printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);
}
static uint64_t canvas_hash(void){uint64_t h=14695981039346656037ull;for(size_t i=0;i<(size_t)s.width*s.height;i++){h^=s.pixels[i];h*=1099511628211ull;}return h;}
static uint64_t display_hash(void){lv_refr_now(NULL);lv_area_t a;lv_obj_get_coords(s.canvas,&a);uint64_t h=14695981039346656037ull;for(int y=a.y1;y<=a.y2;y++)for(int x=a.x1;x<=a.x2;x++){assert(x>=0&&x<1024&&y>=0&&y<600);h^=raster[y*1024+x];h*=1099511628211ull;}return h;}
static void finish_frame(void){for(unsigned i=0;(s.solid_rendering||s.dirty)&&i<1024;i++){lv_tick_inc(40);lv_timer_handler();}assert(!s.solid_rendering&&!s.dirty);}
static void pinch_anchor_checks(void){
 lv_obj_update_layout(s.popup);lv_area_t a;lv_obj_get_coords(s.canvas,&a);gcode_view_t saved=s.view;s.view.zoom=1;s.view.pan_x=s.view.pan_y=0;int x=a.x1+s.width*3/4,y=a.y1+s.height*2/3;float ax=(float)(x-a.x1)/s.width-.5f,ay=(float)(y-a.y1)/s.height-.5f;
 printerhmi_gcode_viewer_multitouch_update(2,x-20,y,x+20,y);printerhmi_gcode_viewer_multitouch_update(2,x-40,y,x+40,y);assert(s.view.zoom>1&&fabsf((ax-s.view.pan_x)/s.view.zoom-ax)<.00001f&&fabsf((ay-s.view.pan_y)/s.view.zoom-ay)<.00001f);printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);s.view=saved;s.dirty=true;
}
static void open_viewer(void){ui_gcode_viewer_open(NULL);}
static void capture_env(const char *env){const char *name=getenv(env);if(!name)return;for(unsigned i=0;s.solid_rendering&&i<1024;i++){lv_tick_inc(40);lv_timer_handler();}lv_tick_inc(400);lv_timer_handler();lv_refr_now(NULL);FILE*f=fopen(name,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
int main(void){
 lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_flush_cb(d,flush);
 lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_touch);
 for(unsigned theme=0;theme<5;theme++){
  ui_theme_set_active(theme);ui_tools_set_viewer_callback(open_viewer);ui_tools_show();tap(input,find(lv_screen_active(),"3D G-code Viewer"));assert(s.popup);assert(!find(s.popup,"Mesh"));
  assert(lv_obj_has_state(find(s.popup,"Reload"),LV_STATE_DISABLED));assert(lv_obj_has_state(find(s.popup,"+"),LV_STATE_DISABLED));
  tap(input,find(s.popup,"Sample"));assert(s.model&&s.model->layer_count==60);assert(strstr(lv_label_get_text(s.filename),"Offline sample"));
  assert(s.model->arcs==3&&s.model->tool_mask==3);if(theme==4)capture_env("GCODE_VIEWER_ALL_SCREENSHOT");
  assert(!s.solid&&!s.depth);fail_depth=true;tap(input,find(s.popup,"Lines"));assert(!s.solid&&!s.depth&&strstr(lv_label_get_text(s.status),"Not enough PSRAM"));fail_depth=false;
  fail_staging=true;tap(input,find(s.popup,"Lines"));assert(!s.solid&&!s.depth&&!s.solid_pixels);fail_staging=false;
  fail_section=true;tap(input,find(s.popup,"Lines"));assert(s.solid&&s.depth&&s.solid_pixels&&!s.section_mask);fail_section=false;
  tap(input,find(s.popup,"Solid"));tap(input,find(s.popup,"Lines"));assert(s.solid&&s.section_mask);capture_env("GCODE_SOLID_CAPTURE");
  touch=(lv_point_t){420,240};pressed=true;lv_indev_read(input);touch.x+=20;lv_indev_read(input);lv_tick_inc(40);lv_timer_handler();assert(s.solid&&s.drag);
  pressed=false;lv_indev_read(input);lv_tick_inc(40);lv_timer_handler();assert(s.solid&&!s.drag);
  pinch_checks(input);pinch_anchor_checks();assert(s.solid);tap(input,find(s.popup,"Reset"));
  tap(input,find(s.popup,"Solid"));assert(!s.solid&&s.depth);
  tap(input,find(s.popup,"Details"));assert(s.details&&contains_text(s.details,"Tools: T0 T1")&&contains_text(s.details,"Arcs: 3"));
  if(theme==4)capture_env("GCODE_VIEWER_DETAILS_SCREENSHOT");
  assert(lv_obj_get_style_bg_opa(lv_obj_get_child(s.details,1),LV_PART_MAIN)==LV_OPA_TRANSP);
  lv_obj_t *detail_close=find(s.details,"Close");lv_area_t close_before,close_after;
  lv_obj_update_layout(s.details);lv_obj_get_coords(detail_close,&close_before);
  lv_obj_scroll_to_y(lv_obj_get_child(s.details,1),2000,LV_ANIM_OFF);lv_obj_update_layout(s.details);lv_obj_get_coords(detail_close,&close_after);assert(!memcmp(&close_before,&close_after,sizeof(close_before)));
  tap(input,detail_close);assert(!s.details&&s.popup);
  tap(input,find(s.popup,"Details"));lv_obj_delete(s.details);assert(!s.details);
  size_t size=(size_t)s.width*s.height*sizeof(uint16_t);uint16_t *baseline=malloc(size);memcpy(baseline,s.pixels,size);
  tap(input,find(s.popup,"+"));assert(memcmp(baseline,s.pixels,size));tap(input,find(s.popup,"Reset"));assert(!memcmp(baseline,s.pixels,size));
  float initial_yaw=s.view.yaw,initial_pitch=s.view.pitch;
  touch=(lv_point_t){420,240};pressed=true;lv_indev_read(input);touch.x+=80;touch.y+=30;lv_indev_read(input);pressed=false;lv_indev_read(input);lv_tick_inc(40);lv_timer_handler();assert(memcmp(baseline,s.pixels,size));
  assert(s.view.yaw<initial_yaw&&s.view.pitch>initial_pitch);
  float right_yaw=s.view.yaw;pressed=true;lv_indev_read(input);touch.x-=80;lv_indev_read(input);pressed=false;lv_indev_read(input);
  assert(s.view.yaw>right_yaw&&fabsf(s.view.yaw-initial_yaw)<.00001f);
  tap(input,find(s.popup,"Top"));assert(fabsf(s.view.pitch-1.5707963f)<.00001f&&s.view.yaw==0&&s.view.zoom==1);
  tap(input,find(s.popup,"Front"));assert(s.view.pitch==0&&s.view.yaw==0);
  tap(input,find(s.popup,"3D"));assert(fabsf(s.view.yaw-initial_yaw)<.00001f&&fabsf(s.view.pitch-initial_pitch)<.00001f);assert(!memcmp(baseline,s.pixels,size));
  pinch_checks(input);
  tap(input,find(s.popup,"Reset"));tap(input,find(s.popup,"Layer +"));assert(!strcmp(lv_label_get_text(s.layer),"1/60  Z 0.20"));assert(memcmp(baseline,s.pixels,size));tap(input,find(s.popup,"All"));assert(!memcmp(baseline,s.pixels,size));tap(input,find(s.popup,"Travel off"));assert(!strcmp(lv_label_get_text(s.travel),"Travel on"));
  tap(input,find(s.popup,"Layer +"));int chosen_layer=s.view.layer;
  tap(input,find(s.popup,"Top"));assert(s.view.layer==chosen_layer&&s.view.travel);
  tap(input,find(s.popup,"Front"));assert(s.view.layer==chosen_layer&&s.view.travel);
  tap(input,find(s.popup,"3D"));assert(s.view.layer==chosen_layer&&s.view.travel);
  tap(input,find(s.popup,"All"));
  scrub(input,0);assert(s.view.layer==0&&lv_obj_has_state(find(s.popup,"Layer -"),LV_STATE_DISABLED));
  tap(input,find(s.popup,"Layer -"));assert(s.view.layer==0);
  scrub(input,100);assert(s.view.layer==59&&lv_obj_has_state(find(s.popup,"Layer +"),LV_STATE_DISABLED));
  tap(input,find(s.popup,"Layer +"));assert(s.view.layer==59);
  scrub(input,50);assert(s.view.layer>=28&&s.view.layer<=31);int middle=s.view.layer;
  tap(input,find(s.popup,"Single"));assert(s.view.cumulative&&s.view.layer==middle);assert(strstr(lv_label_get_text(s.layer),"Up to "));
  tap(input,find(s.popup,"Front"));assert(s.view.cumulative&&s.view.layer==middle);
  tap(input,find(s.popup,"Up to"));assert(!s.view.cumulative&&s.view.layer==middle);
  tap(input,find(s.popup,"All"));tap(input,find(s.popup,"Layer -"));assert(s.view.layer==59);
  tap(input,find(s.popup,"Reset"));assert(s.view.layer==-1&&!s.view.cumulative&&lv_slider_get_value(s.slider)==59);
  scrub(input,50);tap(input,find(s.popup,"Single"));
  if(theme==4)capture_env("GCODE_VIEWER_SCREENSHOT");
  free(baseline);
  lv_obj_update_layout(s.popup);lv_area_t parent;lv_obj_get_coords(s.popup,&parent);
  for(unsigned i=0;i<lv_obj_get_child_count(s.popup);i++){lv_obj_t *c=lv_obj_get_child(s.popup,i);lv_area_t a;lv_obj_get_coords(c,&a);assert(a.x1>=parent.x1&&a.x2<=parent.x2&&a.y1>=parent.y1&&a.y2<=parent.y2);}
  gcode_toolpath_t *reduced=calloc(1,sizeof(*reduced)+8*sizeof(gcode_segment_t));gcode_parser_t reduced_parser;
  gcode_parser_init(&reduced_parser,reduced,(gcode_segment_t *)(reduced+1),8);
  const char *start_reduced="M83\nG1 X0 Y0 Z.2\n";assert(gcode_parser_feed(&reduced_parser,start_reduced,strlen(start_reduced)));
  for(unsigned level=0;level<80;level++){
   char code[128];int n=snprintf(code,sizeof(code),"G1 X10 Y10 Z%.2f E1\nG1 X20 E1\n",.2+level*.2);
   assert(gcode_parser_feed(&reduced_parser,code,(size_t)n));
  }
  assert(ready(reduced,NULL)&&s.model->sample_stride>1&&s.model->layer_count==80);
  assert(strstr(lv_label_get_text(s.status),"REDUCED DETAIL"));scrub(input,50);assert(s.view.layer>=38&&s.view.layer<=41);
  tap(input,find(s.popup,"Details"));assert(contains_text(s.details,"Sampling stride")&&contains_text(s.details,"Layer anchors: 80"));
  if(theme==4)capture_env("GCODE_VIEWER_REDUCED_SCREENSHOT");
  tap(input,find(s.details,"Close"));
  gcode_toolpath_t *empty=calloc(1,sizeof(*empty));empty->unsupported=true;empty->stop_line=7;strcpy(empty->stop_command,"G5");strcpy(empty->stop_reason,"Unsupported command");
  assert(ready(empty,NULL));assert(strstr(lv_label_get_text(s.status),"NO PATHS")&&strstr(lv_label_get_text(s.status),"Line 7 G5"));
  for(unsigned i=0;i<11;i++)assert(lv_obj_has_state(s.controls[i],LV_STATE_DISABLED));
  tap(input,find(s.popup,"Details"));assert(contains_text(s.details,"Line 7 G5"));tap(input,find(s.details,"Close"));
  gcode_toolpath_t *travel_only=calloc(1,sizeof(*travel_only)+sizeof(gcode_segment_t));
  travel_only->segments=(gcode_segment_t *)(travel_only+1);travel_only->count=1;travel_only->segments[0].b.x=1;travel_only->max.x=1;
  assert(ready(travel_only,NULL));assert(lv_obj_has_state(s.slider,LV_STATE_DISABLED)&&lv_obj_has_state(find(s.popup,"Single"),LV_STATE_DISABLED));
  for(unsigned i=4;i<=6;i++)assert(lv_obj_has_state(s.controls[i],LV_STATE_DISABLED));
  gcode_toolpath_t *one=calloc(1,sizeof(*one)+8*sizeof(gcode_segment_t));gcode_parser_t one_parser;
  gcode_parser_init(&one_parser,one,(gcode_segment_t *)(one+1),8);
  const char *one_code="G90\nM83\nG1 X10 Y10 Z.2\nG1 X20 E1\n";
  assert(gcode_parser_feed(&one_parser,one_code,strlen(one_code)));assert(ready(one,NULL));
  assert(s.model->layer_count==1&&lv_obj_has_state(s.slider,LV_STATE_DISABLED));
  assert(!lv_obj_has_state(find(s.popup,"Single"),LV_STATE_DISABLED));
  tap(input,find(s.popup,"Single"));assert(s.view.cumulative&&s.view.layer==0);
  assert(lv_obj_has_state(find(s.popup,"Layer -"),LV_STATE_DISABLED)&&lv_obj_has_state(find(s.popup,"Layer +"),LV_STATE_DISABLED));
  tap(input,find(s.popup,"Reset"));assert(s.view.layer==-1&&!s.view.cumulative);
  ui_gcode_viewer_open("part.gcode");tap(input,find(s.popup,"Sample"));assert(s.model&&strstr(lv_label_get_text(s.filename),"Offline sample"));
  accept_load=true;tap(input,find(s.popup,"Reload"));assert(s.loading&&!s.model&&pending_ready&&lv_obj_has_state(s.reload_button,LV_STATE_DISABLED));assert(!strcmp(lv_label_get_text(s.filename),"part.gcode"));
  fake_progress=(gcode_viewer_progress_t){.bytes=1024,.paths=5000,.layers=7};lv_tick_inc(300);lv_timer_handler();assert(strstr(lv_label_get_text(s.status),"Reading 1 KiB")&&strstr(lv_label_get_text(s.status),"7 layers"));
  fake_running=false;pending_ready(NULL,"Network failure");assert(!s.model&&!s.loading&&strstr(lv_label_get_text(s.status),"Network failure"));
  assert(!lv_obj_has_state(s.reload_button,LV_STATE_DISABLED));tap(input,find(s.popup,"Sample"));assert(s.model&&strstr(lv_label_get_text(s.filename),"Offline sample"));accept_load=false;
  tap(input,find(s.popup,"Close"));assert(!s.popup&&!s.pixels&&!s.model);
  ui_tools_hide();
  ui_gcode_viewer_open(NULL);
  gcode_toolpath_t *dense=calloc(1,sizeof(*dense)+5000*sizeof(gcode_segment_t));
  dense->segments=(gcode_segment_t *)(dense+1);dense->count=dense->capacity=dense->input_segments=5000;dense->sample_stride=1;dense->layer_count=100;dense->max=(gcode_point_t){100,50,20};
  for(unsigned i=0;i<5000;i++){unsigned level=i/50;dense->layer_z[level]=.2f*(level+1);dense->segments[i]=(gcode_segment_t){.a={0,(float)(i%50),dense->layer_z[level]},.b={100,(float)(i%50),dense->layer_z[level]},.extrusion=true,.layer=level};}
  assert(ready(dense,NULL));lv_tick_inc(40);lv_timer_handler();uint64_t lines=canvas_hash();clock_step=10000;
  tap(input,find(s.popup,"Lines"));assert(s.solid_rendering&&s.solid_cursor==2048&&strstr(lv_label_get_text(s.status),"Rendering solid"));assert(canvas_hash()==lines);
  lv_tick_inc(6);lv_timer_handler();assert(s.solid_rendering&&s.solid_cursor==4096&&canvas_hash()==lines);
  /* Moving input cannot restart/starve an in-flight frame or expose its partial fill. */
  gcode_view_t snapshot=s.render_view;unsigned frames=s.frames;
  touch=(lv_point_t){420,240};pressed=true;lv_indev_read(input);touch.x+=20;lv_indev_read(input);
  lv_tick_inc(40);lv_timer_handler();assert(s.drag&&s.solid&&s.frames>frames&&!s.solid_rendering&&s.dirty);assert(s.render_view.yaw==snapshot.yaw&&s.view.yaw!=snapshot.yaw);
  lv_tick_inc(40);lv_timer_handler();assert(s.solid_rendering&&!s.dirty&&s.render_view.yaw==s.view.yaw);
  finish_frame();uint64_t solid=canvas_hash();assert(solid!=lines&&strstr(lv_label_get_text(s.status),"SOLID PATHS"));
  frames=s.frames;for(unsigned i=0;i<10;i++){lv_indev_read(input);lv_tick_inc(40);lv_timer_handler();}assert(s.frames==frames&&!s.dirty&&!s.solid_rendering&&canvas_hash()==solid);
  pressed=false;lv_indev_read(input);printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);lv_tick_inc(40);lv_timer_handler();assert(!s.drag&&s.frames==frames&&canvas_hash()==solid);
  tap(input,find(s.popup,"Solid"));uint64_t moved_lines=canvas_hash();assert(!s.solid&&moved_lines!=solid&&strstr(lv_label_get_text(s.status),"LINES"));
  tap(input,find(s.popup,"Lines"));assert(s.solid_rendering&&canvas_hash()==moved_lines);finish_frame();assert(canvas_hash()==solid);
  tap(input,find(s.popup,"Layer +"));finish_frame();assert(s.view.layer==0);
  tap(input,find(s.popup,"All"));assert(s.solid_rendering);tap(input,find(s.popup,"Close"));assert(!s.popup&&!s.model&&!s.depth&&!s.solid_pixels&&!s.section_mask);clock_step=0;
  ui_gcode_viewer_open(NULL);tap(input,find(s.popup,"Sample"));
  s.model->cache=calloc(1,sizeof(struct gcode_cache));s.model->cache->complete=true;s.model->cache->layers=60;accept_detail=true;
  tap(input,find(s.popup,"Layer +"));lv_tick_inc(220);lv_timer_handler();assert(s.detail_loading&&requested_detail==0&&pending_ready);
  gcode_toolpath_t *fine=calloc(1,sizeof(*fine)+sizeof(gcode_segment_t));*fine=*s.model;fine->surface=NULL;fine->cache=NULL;fine->segments=(gcode_segment_t *)(fine+1);fine->segments[0]=s.model->segments[0];fine->count=fine->input_segments=fine->capacity=1;fine->sample_stride=1;fine->cached_layer=0;
  assert(pending_ready(fine,NULL));fake_running=false;pending_ready=NULL;lv_tick_inc(40);lv_timer_handler();assert(s.detail==fine&&strstr(lv_label_get_text(s.status),"SD layer: 1 paths"));
  tap(input,find(s.popup,"Layer +"));assert(!s.detail);lv_tick_inc(220);lv_timer_handler();assert(s.detail_loading&&requested_detail==1);
  tap(input,find(s.popup,"All"));assert(!s.detail_loading&&!fake_running&&!pending_ready);accept_detail=false;tap(input,find(s.popup,"Close"));assert(!s.detail);
  ui_gcode_viewer_open(NULL);accept_refine=true;strcpy(s.file,"normal.gcode");s.loading=true;
  gcode_toolpath_t *automatic=calloc(1,sizeof(*automatic)+8*sizeof(gcode_segment_t));gcode_parser_t ap;gcode_parser_init(&ap,automatic,(gcode_segment_t *)(automatic+1),8);assert(gcode_parser_feed(&ap,one_code,strlen(one_code)));automatic->source_hash_valid=true;automatic->profile=generation;assert(ready(automatic,NULL));lv_tick_inc(40);lv_timer_handler();assert(s.refining&&pending_refined&&s.solid);tap(input,find(s.popup,"Single"));assert(s.view.cumulative&&s.refining&&pending_refined);tap(input,find(s.popup,"Up to"));assert(!s.view.cumulative&&s.refining&&pending_refined&&s.solid);tap(input,find(s.popup,"Single"));
  mesh_preview_t *auto_mesh=calloc(1,sizeof(*auto_mesh)+3*(sizeof(mesh_vertex_t)+sizeof(mesh_projected_t))+sizeof(mesh_triangle_t));auto_mesh->vertices=3;auto_mesh->triangles=1;auto_mesh->cell_mm=.5f;auto_mesh->vertex=(mesh_vertex_t *)(auto_mesh+1);auto_mesh->projected=(mesh_projected_t *)(auto_mesh->vertex+3);auto_mesh->triangle=(mesh_triangle_t *)(auto_mesh->projected+3);auto_mesh->vertex[0]=(mesh_vertex_t){.nz=1};auto_mesh->vertex[1]=(mesh_vertex_t){.x=10,.nz=1};auto_mesh->vertex[2]=(mesh_vertex_t){.y=10,.nz=1};auto_mesh->triangle[0]=(mesh_triangle_t){0,1,2};auto_mesh->max[0]=auto_mesh->max[1]=10;
  assert(pending_refined(auto_mesh,NULL));pending_refined=NULL;fake_running=false;lv_tick_inc(40);lv_timer_handler();assert(s.model->overview_mesh==auto_mesh&&strstr(lv_label_get_text(s.status),"SOLID MESH"));tap(input,find(s.popup,"Up to"));tap(input,find(s.popup,"All"));finish_frame();
  /* A visibly different gesture child proves geometry selection, quarter-pixel
   * rasterization and exact full-detail restoration after one/two-finger input. */
  mesh_preview_t *gesture_mesh=calloc(1,sizeof(*gesture_mesh)+3*(sizeof(mesh_vertex_t)+sizeof(mesh_projected_t))+sizeof(mesh_triangle_t));assert(gesture_mesh);*gesture_mesh=*auto_mesh;gesture_mesh->interaction=NULL;gesture_mesh->vertex=(mesh_vertex_t *)(gesture_mesh+1);gesture_mesh->projected=(mesh_projected_t *)(gesture_mesh->vertex+3);gesture_mesh->triangle=(mesh_triangle_t *)(gesture_mesh->projected+3);memcpy(gesture_mesh->vertex,auto_mesh->vertex,3*sizeof(mesh_vertex_t));memcpy(gesture_mesh->triangle,auto_mesh->triangle,sizeof(mesh_triangle_t));for(unsigned i=0;i<3;i++){gesture_mesh->vertex[i].x=1.5f+.7f*gesture_mesh->vertex[i].x;gesture_mesh->vertex[i].y=1.5f+.7f*gesture_mesh->vertex[i].y;}auto_mesh->interaction=gesture_mesh;
  touch=(lv_point_t){420,240};pressed=true;lv_indev_read(input);float first_pixel_yaw=s.view.yaw,orbit_zoom=s.view.zoom;touch.x+=1;lv_indev_read(input);assert(s.view.yaw<first_pixel_yaw&&s.view.zoom==orbit_zoom);lv_tick_inc(6);lv_timer_handler();assert(s.display_interactive&&s.render_mesh==gesture_mesh&&s.render_width==476&&s.render_height==166);
  display_hash();size_t flushed_before=flushed_pixels;touch.x+=1;lv_indev_read(input);lv_tick_inc(6);lv_timer_handler();display_hash();assert(s.view.zoom==orbit_zoom);assert(s.damaged_pixels<(size_t)s.width*s.height&&flushed_pixels-flushed_before<(size_t)s.width*s.height);if(theme==0)printf("PASS: held-finger mesh redraw damaged %lu and flushed %lu pixels vs %lu full canvas\n",(unsigned long)s.damaged_pixels,(unsigned long)(flushed_pixels-flushed_before),(unsigned long)((size_t)s.width*s.height));
  for(int y=0;y<s.height;y+=2)for(int x=0;x<s.width;x+=2){size_t i=(size_t)y*s.width+x;assert(s.pixels[i]==s.pixels[i+1]&&s.pixels[i]==s.pixels[i+s.width]&&s.pixels[i]==s.pixels[i+s.width+1]);}
  uint16_t *expected=calloc((size_t)s.width*s.height,sizeof(uint16_t));assert(expected);mesh_preview_render_step(auto_mesh,&s.view,expected,s.depth,s.width,s.height,0,SIZE_MAX);assert(memcmp(expected,s.pixels,(size_t)s.width*s.height*sizeof(uint16_t)));
  pressed=false;lv_indev_read(input);printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);finish_frame(); /* The next timer must request the full-detail settle frame. */
  lv_tick_inc(40);lv_timer_handler();finish_frame();assert(!s.display_interactive&&s.render_mesh==auto_mesh&&!memcmp(expected,s.pixels,(size_t)s.width*s.height*sizeof(uint16_t)));free(expected);
  printerhmi_gcode_viewer_multitouch_update(2,400,240,500,240);printerhmi_gcode_viewer_multitouch_update(2,370,240,530,240);lv_tick_inc(40);lv_timer_handler();assert(s.display_interactive&&s.render_mesh==gesture_mesh);printerhmi_gcode_viewer_multitouch_update(1,370,240,0,0);lv_tick_inc(40);lv_timer_handler();assert(s.display_interactive&&s.block_one_finger);printerhmi_gcode_viewer_multitouch_update(0,0,0,0,0);lv_tick_inc(40);lv_timer_handler();finish_frame();assert(!s.display_interactive&&s.render_mesh==auto_mesh);
  uint64_t mesh_solid=canvas_hash(),mesh_display=display_hash();tap(input,find(s.popup,"Solid"));assert(canvas_hash()!=mesh_solid&&display_hash()!=mesh_display);tap(input,find(s.popup,"Lines"));finish_frame();assert(canvas_hash()==mesh_solid&&display_hash()==mesh_display);unsigned starts=refinement_starts;
  for(unsigned scope=0;scope<3;scope++){
   tap(input,find(s.popup,"Reset"));if(scope==1)tap(input,find(s.popup,"Layer +"));if(scope==2)tap(input,find(s.popup,"Single"));
   for(unsigned travel=0;travel<2;travel++){
    if(travel)tap(input,find(s.popup,"Travel off"));
    const char *presets[]={"Top","Front","3D"};for(unsigned camera=0;camera<3;camera++){tap(input,find(s.popup,presets[camera]));finish_frame();assert(s.solid&&s.render_mesh==auto_mesh&&s.render_view.travel==(travel!=0)&&strstr(lv_label_get_text(s.status),"SOLID MESH"));if(scope==1)assert(s.render_low>-FLT_MAX&&s.render_low<s.render_cut);else assert(s.render_low==-FLT_MAX);}
   }
  }
  tap(input,find(s.popup,"Reset"));finish_frame();tap(input,find(s.popup,"Single"));finish_frame();assert(s.solid&&s.view.cumulative&&s.render_mesh==auto_mesh&&s.render_cut==s.model->layer_z[s.view.layer]&&strstr(lv_label_get_text(s.status),"SOLID MESH"));
  /* Up to also requests/adopts exact cached layer paths for visible infill. */
  s.model->cache=calloc(1,sizeof(struct gcode_cache));s.model->cache->complete=true;s.model->cache->layers=1;accept_detail=true;lv_tick_inc(40);lv_timer_handler();lv_tick_inc(220);lv_timer_handler();assert(s.detail_loading&&pending_ready&&requested_detail==0);uint64_t reduced_layer=canvas_hash();
  gcode_toolpath_t *infill_detail=calloc(1,sizeof(*infill_detail)+4*sizeof(gcode_segment_t));*infill_detail=*s.model;infill_detail->surface=NULL;infill_detail->cache=NULL;infill_detail->overview_mesh=NULL;infill_detail->segments=(gcode_segment_t *)(infill_detail+1);infill_detail->count=infill_detail->capacity=infill_detail->input_segments=4;infill_detail->sample_stride=1;infill_detail->cached_layer=0;for(unsigned i=0;i<4;i++)infill_detail->segments[i]=(gcode_segment_t){.a={1,1+2*i,.2f},.b={9,1+2*i,.2f},.extrusion=true,.layer=0,.width=.5f,.height=.2f};infill_detail->layer_anchor[0]=infill_detail->segments[0];assert(pending_ready(infill_detail,NULL));pending_ready=NULL;fake_running=false;finish_frame();assert(s.render_layer_model==infill_detail&&canvas_hash()!=reduced_layer&&s.render_mesh==auto_mesh);
  /* Exposed cached interior uses solid material, while explicit Lines retains
   * cyan tool paths. Exercise the actual UI routing, not only a direct raster. */
  tap(input,find(s.popup,"Top"));finish_frame();unsigned interior_gold=0,interior_cyan=0;
  for(size_t i=0;i<(size_t)s.width*s.height;i++){uint16_t color=s.pixels[i];if(color==0x0863||color==0x52aa)continue;if((color>>11)>(color&31)*2)interior_gold++;if((color&31)>(color>>11)*2)interior_cyan++;}
  assert(s.solid&&interior_gold>100&&!interior_cyan);uint64_t material_frame=canvas_hash();
  tap(input,find(s.popup,"Solid"));finish_frame();interior_cyan=0;
  for(size_t i=0;i<(size_t)s.width*s.height;i++){uint16_t color=s.pixels[i];if(color!=0x0863&&color!=0x52aa&&(color&31)>(color>>11)*2)interior_cyan++;}
  assert(!s.solid&&interior_cyan>10&&canvas_hash()!=material_frame);
  tap(input,find(s.popup,"Lines"));finish_frame();assert(s.solid&&canvas_hash()==material_frame);
  tap(input,find(s.popup,"Up to"));assert(s.detail==infill_detail&&s.render_layer_model==infill_detail);tap(input,find(s.popup,"All"));tap(input,find(s.popup,"Layer +"));tap(input,find(s.popup,"All"));assert(refinement_starts==starts&&s.model->overview_mesh==auto_mesh);tap(input,find(s.popup,"Close"));accept_refine=false;accept_detail=false;
  for(unsigned i=0;i<20;i++){ui_gcode_viewer_open(NULL);tap(input,find(s.popup,"Sample"));tap(input,find(s.popup,"Lines"));assert(s.depth);tap(input,find(s.popup,"Details"));lv_obj_delete(s.popup);assert(!s.popup&&!s.details&&!s.model&&!s.pixels&&!s.depth);}
  ui_gcode_viewer_open(NULL);tap(input,find(s.popup,"Sample"));generation++;lv_tick_inc(40);lv_timer_handler();assert(!s.popup&&!s.model);
 }
 lv_indev_delete(input);lv_display_delete(d);lv_deinit();puts("PASS: five themes, real pointer controls, sample, both drag directions, Top/Front/3D presets, slider endpoints/midpoint, cumulative/single layers, pinch damping/bounds/modal and lift guards, zoom/reset/travel, disabled empty actions, bounds, Details/no-path/reload identity, 100 nested external deletions, complete-frame publication, solid/line pixel differences, stationary-touch stability, camera coalescing, staging OOM, accelerated render scheduling, gesture LOD/half-resolution selection, exact settle restoration, 16x pinch anchoring, immediate one-pixel orbit, solid All/Single/Up-to x Travel x camera matrix, reduced display damage, Up-to/Single cached infill adoption and profile retirement");
}
