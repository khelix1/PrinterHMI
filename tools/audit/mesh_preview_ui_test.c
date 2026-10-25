#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "custom_theme.h"
#include "ui_mesh_preview.c"
static uint32_t generation=1;
static bool pressed,running,fail_canvas;
static lv_point_t touch;
static mesh_preview_ready_cb pending;
static uint16_t screen[1024*600];
void *gcode_test_calloc(size_t n,size_t size){if(fail_canvas&&(n==(size_t)WIDTH*HEIGHT))return NULL;return calloc(n,size);}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
int64_t esp_timer_get_time(void){return 0;}
uint32_t moonraker_config_generation(void){return generation;}
void mesh_preview_loader_cancel(void){running=false;pending=NULL;}
bool mesh_preview_loader_running(void){return running;}
void mesh_preview_loader_free(mesh_preview_t *m){free(m);}
bool mesh_preview_loader_start(const char *name,mesh_preview_ready_cb cb){assert(mesh_preview_name_valid(name));if(running)return false;running=true;pending=cb;return true;}
static void tick_frame(void){lv_tick_inc(40);lv_timer_handler();}
static void finish_render(void){for(unsigned i=0;(s.dirty||s.rendering)&&i<1024;i++)tick_frame();assert(!s.dirty&&!s.rendering);}
static void finish_load(void){assert(pending);char path[256],error[128];snprintf(path,sizeof(path),"%s/%s",MESH_PREVIEW_DIRECTORY,s.selected);FILE *file=fopen(path,"rb");assert(file);mesh_preview_t *m=mesh_preview_read(file,malloc,free,NULL,NULL,error,sizeof(error));fclose(file);assert(m);if(!pending(m,NULL))free(m);pending=NULL;running=false;finish_render();}
static void read_touch(lv_indev_t *i,lv_indev_data_t *d){(void)i;d->point=touch;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *p){uint16_t *pixels=(uint16_t *)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)screen[y*1024+x]=*pixels++;lv_display_flush_ready(d);}
static lv_obj_t *find(lv_obj_t *root,const char *name){for(unsigned i=0;i<lv_obj_get_child_count(root);i++){lv_obj_t *c=lv_obj_get_child(root,i);if(lv_obj_check_type(c,&lv_button_class)&&lv_obj_get_child_count(c)&&lv_obj_check_type(lv_obj_get_child(c,0),&lv_label_class)&&!strcmp(lv_label_get_text(lv_obj_get_child(c,0)),name))return c;lv_obj_t *b=find(c,name);if(b)return b;}return NULL;}
static void tap(lv_indev_t *i,lv_obj_t *b){assert(b);lv_obj_update_layout(lv_layer_top());lv_area_t a;lv_obj_get_coords(b,&a);touch=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};pressed=true;lv_indev_read(i);pressed=false;lv_indev_read(i);tick_frame();}
static void capture(void){const char *path=getenv("MESH_PREVIEW_SCREENSHOT");if(!path)return;finish_render();lv_refr_now(NULL);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t p=screen[i];unsigned char c[3]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
int main(void){
 lv_init();lv_display_t *d=lv_display_create(1024,600);static uint8_t draw[1024*50*2];lv_display_set_buffers(d,draw,NULL,sizeof(draw),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_flush_cb(d,flush);lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_touch);
 for(unsigned theme=0;theme<5;theme++){
  ui_theme_set_active(theme);ui_mesh_preview_open();assert(ui_mesh_preview_active()&&s.chooser&&s.file_count==1&&!s.mesh);assert(lv_obj_has_state(s.controls[0],LV_STATE_DISABLED));
  tap(input,find(s.chooser,"demo.phm"));assert(!s.chooser&&s.loading&&pending);fail_canvas=true;finish_load();assert(!s.mesh&&strstr(lv_label_get_text(s.status),"Not enough PSRAM"));fail_canvas=false;
  tap(input,find(s.popup,"Models"));tap(input,find(s.chooser,"demo.phm"));finish_load();assert(s.mesh&&s.mesh->triangles==6144&&!s.loading&&s.solid);if(theme==4)capture();
  size_t bytes=(size_t)WIDTH*HEIGHT*2;uint16_t *baseline=malloc(bytes);memcpy(baseline,s.pixels,bytes);
  float yaw=s.view.yaw;touch=(lv_point_t){450,250};pressed=true;lv_indev_read(input);touch.x+=40;lv_indev_read(input);tick_frame();assert(s.drag&&s.view.yaw<yaw);pressed=false;lv_indev_read(input);finish_render();assert(memcmp(baseline,s.pixels,bytes));
  tap(input,find(s.popup,"Reset"));finish_render();assert(!memcmp(baseline,s.pixels,bytes));
  tap(input,find(s.popup,"Top"));finish_render();assert(s.view.pitch>1.57f);tap(input,find(s.popup,"Front"));finish_render();assert(s.view.pitch==0);tap(input,find(s.popup,"3D"));finish_render();assert(s.view.yaw==yaw);
  lv_obj_update_layout(s.popup);lv_area_t canvas;lv_obj_get_coords(s.canvas,&canvas);int x=(canvas.x1+canvas.x2)/2,y=(canvas.y1+canvas.y2)/2;float zoom=s.view.zoom;
  ui_mesh_preview_multitouch_update(2,x-40,y,x+40,y);ui_mesh_preview_multitouch_update(2,x-100,y,x+100,y);tick_frame();assert(s.pinch&&s.block&&s.view.zoom>zoom);
  ui_mesh_preview_multitouch_update(1,x,y,0,0);assert(s.block);ui_mesh_preview_multitouch_update(0,0,0,0,0);finish_render();assert(!s.block&&!s.pinch);
  tap(input,find(s.popup,"Details"));assert(s.details);zoom=s.view.zoom;ui_mesh_preview_multitouch_update(2,x-40,y,x+40,y);ui_mesh_preview_multitouch_update(2,x-100,y,x+100,y);assert(!s.pinch&&s.view.zoom==zoom);ui_mesh_preview_multitouch_update(0,0,0,0,0);
  lv_obj_t *close=find(s.details,"Close");lv_obj_update_layout(s.details);lv_area_t before,after;lv_obj_get_coords(close,&before);lv_obj_scroll_to_y(lv_obj_get_child(s.details,1),2000,LV_ANIM_OFF);lv_obj_update_layout(s.details);lv_obj_get_coords(close,&after);assert(!memcmp(&before,&after,sizeof(before)));tap(input,close);
  tap(input,find(s.popup,"Solid"));finish_render();assert(!s.solid&&find(s.popup,"Edges"));tap(input,find(s.popup,"Edges"));finish_render();assert(s.solid);
  tap(input,find(s.popup,"Models"));tap(input,find(s.chooser,"demo.phm"));mesh_preview_t *old=s.mesh;assert(pending(NULL,"Invalid mesh test")==false);pending=NULL;running=false;assert(s.mesh==old&&!strcmp(lv_label_get_text(s.filename),"demo.phm"));
  tap(input,find(s.popup,"Models"));tap(input,find(s.chooser,"demo.phm"));assert(pending);tap(input,find(s.popup,"Close"));assert(!s.popup&&!s.mesh&&!s.pixels&&!s.depth&&!pending);
  free(baseline);
  for(unsigned i=0;i<10;i++){ui_mesh_preview_open();tap(input,find(s.chooser,"demo.phm"));finish_load();tap(input,find(s.popup,"Details"));lv_obj_delete(s.popup);assert(!s.popup&&!s.mesh&&!s.details&&!s.chooser&&!s.pixels&&!s.depth);}
  ui_mesh_preview_open();generation++;tick_frame();assert(!s.popup&&!s.chooser);
 }
 lv_indev_delete(input);lv_display_delete(d);lv_deinit();puts("PASS: five-theme native SD mesh chooser, OOM/retry, adoption/error identity, shaded/edge rendering, orbit direction, presets, pinch/modal/lift guards, pinned Details Close, cancellation, profile retirement and 50 nested cleanups");return 0;
}
