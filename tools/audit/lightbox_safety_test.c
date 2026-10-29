/* Real owner/LVGL; typed network, queue, allocator and render-boundary fixtures. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thumbnail_render.h"
static bool test_render(const lv_image_dsc_t *,uint16_t *,int,int);
static void test_set_src(lv_obj_t *,const void *);
#define thumbnail_render_to_rgb565_fit test_render
#define lv_image_set_src test_set_src
#include "ui_preview_lightbox.c"
#undef thumbnail_render_to_rgb565_fit
#undef lv_image_set_src
#include "custom_theme.h"

static unsigned source_boundary;
static uint32_t generation=1;
static unsigned queued,fetches,delays;
static bool fail_memory,fail_queue,fail_task,fail_fetch,fail_lock;
static unsigned receive_count;
static size_t owned_bytes;
static fullscreen_job_t queued_job;
static jmp_buf worker_end;
static void (*host_hook)(void),(*key_hook)(void),(*port_hook)(void),(*queue_hook)(void),(*task_hook)(void),(*fetch_hook)(void),(*render_hook)(void);
static uint16_t source_pixels[4]={0x1234,0x1234,0x1234,0x1234};
static lv_image_dsc_t source={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=2,.h=2,.stride=4},.data_size=8,.data=(uint8_t*)source_pixels};
static void hook(void (**slot)(void)){if(*slot){void(*callback)(void)=*slot;*slot=NULL;callback();}}
static void reopen(void){ui_preview_lightbox_close();preview_lightbox_show_image(&source);}
static void test_set_src(lv_obj_t *image,const void *source){lv_image_set_src(image,source);if(source_boundary&&!--source_boundary)reopen();}
static void changed(void){generation++;}
static void delete_image(void){lv_obj_delete(s_preview_image);}
static void delete_hint(void){lv_obj_delete(s_preview_hint);}
static void reopened(lv_event_t *event){(void)event;preview_lightbox_show_image(&source);}
uint32_t moonraker_config_generation(void){return generation;}
const char *moonraker_config_host(void){hook(&host_hook);return "printer.local";}
const char *moonraker_config_api_key(void){hook(&key_hook);return "test-key";}
int moonraker_config_port(void){hook(&port_hook);return 7125;}
typedef struct {size_t bytes;max_align_t alignment;} allocation_t;
void *heap_caps_malloc(size_t size,uint32_t caps){assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(fail_memory)return NULL;allocation_t*a=malloc(sizeof(*a)+size);assert(a);a->bytes=size;owned_bytes+=size;return a+1;}
void *heap_caps_calloc(size_t n,size_t size,uint32_t caps){void*p=heap_caps_malloc(n*size,caps);if(p)memset(p,0,n*size);return p;}
void heap_caps_free(void *p){if(!p)return;allocation_t*a=(allocation_t*)p-1;assert(owned_bytes>=a->bytes);owned_bytes-=a->bytes;free(a);}
bool bsp_display_lock(unsigned timeout){assert(timeout==1000||timeout==2500);return !fail_lock;}
void bsp_display_unlock(void){}
QueueHandle_t xQueueCreate(unsigned count,size_t size){assert(count==1&&size==sizeof(fullscreen_job_t));hook(&queue_hook);return fail_queue?NULL:(void*)1;}
int xQueueOverwrite(QueueHandle_t queue,const void *job){assert(queue==(void*)1);queued++;queued_job=*(const fullscreen_job_t*)job;return pdTRUE;}
int xQueueReceive(QueueHandle_t queue,void *job,unsigned timeout){assert(queue==(void*)1&&timeout==portMAX_DELAY);if(receive_count++)longjmp(worker_end,1);*(fullscreen_job_t*)job=queued_job;return pdTRUE;}
int xTaskCreatePinnedToCore(void(*worker)(void*),const char*name,unsigned stack,void*arg,unsigned priority,void*handle,int core){assert(worker==fullscreen_preview_worker&&!strcmp(name,"preview_full")&&stack==8192&&!arg&&priority==3&&!handle&&core==0);hook(&task_hook);return fail_task?0:pdPASS;}
void vTaskDelay(unsigned delay){assert(delay==500);delays++;}
void thumbnail_manager_url_encode(const char *in,char *out,size_t n){snprintf(out,n,"%s",in);}
bool moonraker_fetch_file_metadata(const char*host,int port,const char*key,const char*file,char*body,size_t n,int*code,esp_err_t*error){assert(!strcmp(host,"printer.local")&&port==7125&&!strcmp(key,"test-key")&&!strcmp(file,"part.gcode"));fetches++;snprintf(body,n,"metadata");*code=200;*error=ESP_OK;hook(&fetch_hook);return !fail_fetch;}
bool json_find_best_thumbnail_path(const char *json,char *out,size_t n){assert(!strcmp(json,"metadata"));snprintf(out,n,"thumb.png");return true;}
bool moonraker_fetch_thumbnail_encoded(const char*host,int port,const char*file,uint8_t**out,size_t*n){assert(!strcmp(host,"printer.local")&&port==7125&&!strcmp(file,"thumb.png"));*n=8;*out=heap_caps_calloc(1,*n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);return *out!=NULL;}
static bool test_render(const lv_image_dsc_t *image,uint16_t *out,int width,int height){bool ok;if(image->header.cf==LV_COLOR_FORMAT_RAW){for(int i=0;i<width*height;i++)out[i]=0xabcd;ok=true;}else ok=thumbnail_render_to_rgb565_fit(image,out,width,height);hook(&render_hook);return ok;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
static lv_point_t point;static bool pressed;
static void read_touch(lv_indev_t *input,lv_indev_data_t *data){(void)input;data->point=point;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t *input,lv_obj_t *object){lv_obj_update_layout(s_preview_lightbox);lv_area_t area;lv_obj_get_coords(object,&area);point=(lv_point_t){(area.x1+area.x2)/2,(area.y1+area.y2)/2};pressed=true;lv_indev_read(input);pressed=false;lv_indev_read(input);}
static void run_worker(void){receive_count=0;if(!setjmp(worker_end))fullscreen_preview_worker(NULL);}
static void launch(lv_obj_t *image){unsigned before=queued;ui_preview_lightbox_show_file_object(image,"part.gcode");assert(queued==before+1&&fullscreen_job_is_current(&queued_job));}
static void closed(void){ui_preview_lightbox_close();assert(!s_preview_lightbox&&!s_preview_image&&!s_preview_hint&&!s_fallback_pixels&&!s_fullscreen_pixels&&!owned_bytes);}
static void checks(lv_indev_t *input)
{
 lv_obj_t *image=lv_image_create(lv_screen_active());lv_image_set_src(image,&source);
 lv_obj_t *canvas=lv_canvas_create(lv_screen_active());static uint8_t canvas_pixels[8];lv_canvas_set_buffer(canvas,canvas_pixels,2,2,LV_COLOR_FORMAT_RGB565);preview_lightbox_show_image_object(canvas);assert(s_preview_lightbox&&s_fallback_pixels);closed();lv_obj_delete(canvas);
 for(int action=0;action<3;action++){preview_lightbox_show_image(&source);if(action==0)tap(input,s_preview_image);else if(action==1)tap(input,s_preview_hint);else {lv_obj_update_layout(s_preview_lightbox);point=(lv_point_t){2,2};pressed=true;lv_indev_read(input);pressed=false;lv_indev_read(input);}assert(!s_preview_lightbox&&!owned_bytes);}
 preview_lightbox_show_image(&source);lv_obj_t *root=s_preview_lightbox;
 lv_obj_add_state(s_preview_image,LV_STATE_DISABLED);lv_obj_send_event(s_preview_image,LV_EVENT_CLICKED,NULL);assert(s_preview_lightbox==root);lv_obj_remove_state(s_preview_image,LV_STATE_DISABLED);
 lv_obj_t *fake=lv_button_create(root);lv_obj_add_event_cb(fake,preview_lightbox_close_cb,LV_EVENT_CLICKED,NULL);lv_obj_add_flag(fake,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_send_event(fake,LV_EVENT_CLICKED,NULL);assert(s_preview_lightbox==root);lv_obj_delete(fake);
 unsigned before=queued;uint32_t request=s_request_id;
 ui_preview_lightbox_show_file_object(NULL,"part.gcode");ui_preview_lightbox_show_file_object(root,"part.gcode");assert(s_preview_lightbox==root&&request==s_request_id&&queued==before);
 lv_obj_t *symbol=lv_image_create(lv_screen_active());lv_image_set_src(symbol,LV_SYMBOL_OK);ui_preview_lightbox_show_file_object(symbol,"part.gcode");assert(s_preview_lightbox==root&&queued==before);lv_obj_delete(symbol);
 /* Aliased descriptors and object sources must be copied before old cleanup. */
 preview_lightbox_show_image(&s_fallback_image);assert(s_fallback_pixels&&s_fallback_pixels[0]==0x1234&&owned_bytes==8);
 preview_lightbox_show_image_object(s_preview_image);assert(s_fallback_pixels&&s_fallback_pixels[0]==0x1234&&owned_bytes==8);
 root=s_preview_lightbox;lv_obj_add_event_cb(root,reopened,LV_EVENT_DELETE,NULL);preview_lightbox_show_image(&source);assert(s_preview_lightbox&&s_fallback_pixels&&owned_bytes==8);closed();
 preview_lightbox_show_image(&source);lv_obj_add_event_cb(s_preview_lightbox,reopened,LV_EVENT_DELETE,NULL);ui_preview_lightbox_close();assert(s_preview_lightbox&&owned_bytes==8);closed();
 preview_lightbox_show_image(&source);lv_obj_add_event_cb(s_preview_lightbox,reopened,LV_EVENT_DELETE,NULL);lv_obj_delete(s_preview_lightbox);assert(s_preview_lightbox&&owned_bytes==8);closed();
 preview_lightbox_show_image(&source);lv_obj_delete(s_preview_hint);assert(!s_preview_hint);lv_obj_delete(s_preview_image);assert(!s_preview_image);lv_obj_delete(s_preview_lightbox);assert(!s_preview_lightbox&&!owned_bytes);
 render_hook=reopen;preview_lightbox_show_image(&source);assert(s_preview_lightbox&&owned_bytes==8);closed();
 for(unsigned stage=0;stage<5;stage++){
  before=queued;s_preview_queue=stage<3?(void*)1:NULL;s_preview_worker_started=stage!=4;
  if(stage==0)host_hook=reopen;
  if(stage==1)key_hook=reopen;
  if(stage==2)port_hook=reopen;
  if(stage==3)queue_hook=reopen;
  if(stage==4)task_hook=reopen;
  ui_preview_lightbox_show_file_object(image,"part.gcode");assert(queued==before&&s_preview_lightbox&&owned_bytes==8);closed();
 }
 s_preview_queue=NULL;fail_queue=true;before=queued;ui_preview_lightbox_show_file_object(image,"part.gcode");assert(queued==before&&s_preview_lightbox);closed();fail_queue=false;
 s_preview_worker_started=false;fail_task=true;ui_preview_lightbox_show_file_object(image,"part.gcode");assert(queued==before&&s_preview_lightbox);closed();fail_task=false;
 host_hook=changed;ui_preview_lightbox_show_file_object(image,"part.gcode");assert(queued==before&&s_preview_lightbox);closed();
 preview_lightbox_show_image(&source);lv_label_set_text(s_preview_hint,"part.gcode");const char *aliased_file=lv_label_get_text(s_preview_hint);before=queued;ui_preview_lightbox_show_file_object(s_preview_image,aliased_file);assert(queued==before+1&&!strcmp(queued_job.file,"part.gcode"));closed();
 launch(image);uint16_t *fallback=s_fallback_pixels;run_worker();assert(s_fullscreen_pixels&&s_fullscreen_pixels[0]==0xabcd&&!s_fallback_pixels&&owned_bytes==(size_t)900*520*2);assert(lv_image_get_src(s_preview_image)==&s_fullscreen_image);closed();
 launch(image);fallback=s_fallback_pixels;fail_fetch=true;unsigned reads=fetches;run_worker();assert(fetches==reads+3&&s_fallback_pixels==fallback&&!s_fullscreen_pixels&&strstr(lv_label_get_text(s_preview_hint),"ORIGINAL"));fail_fetch=false;closed();
 launch(image);fail_lock=true;reads=fetches;run_worker();assert(fetches==reads&&s_fallback_pixels);fail_lock=false;closed();
 launch(image);changed();reads=fetches;run_worker();assert(fetches==reads&&!s_fullscreen_pixels);closed();
 launch(image);ui_preview_lightbox_close();preview_lightbox_show_image(&source);request=s_request_id;reads=fetches;run_worker();assert(fetches==reads&&s_request_id==request&&owned_bytes==8);closed();
 launch(image);fetch_hook=reopen;run_worker();assert(s_preview_lightbox&&owned_bytes==8&&!s_fullscreen_pixels);closed();
 launch(image);render_hook=reopen;run_worker();assert(s_preview_lightbox&&owned_bytes==8&&!s_fullscreen_pixels);closed();
 launch(image);render_hook=delete_image;run_worker();assert(s_preview_lightbox&&!s_preview_image&&!s_fullscreen_pixels&&owned_bytes==8);closed();
 launch(image);render_hook=delete_hint;run_worker();assert(s_fullscreen_pixels&&!s_preview_hint);closed();
 for(unsigned boundary=1;boundary<=2;boundary++){
  launch(image);uint16_t *pending=heap_caps_malloc(900*520*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);source_boundary=boundary;
  assert(!fullscreen_publish(&queued_job,&pending));if(boundary==1)assert(pending);else assert(!pending);
  heap_caps_free(pending);assert(s_preview_lightbox&&owned_bytes==8&&!s_fullscreen_pixels);closed();
 }
 /* Direct transfer must leave stale jobs' buffers with their caller. */
 launch(image);uint16_t *pixels=heap_caps_malloc(900*520*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);changed();assert(!fullscreen_publish(&queued_job,&pixels)&&pixels);heap_caps_free(pixels);closed();
 fail_memory=true;preview_lightbox_show_image(&source);assert(s_preview_lightbox&&!s_fallback_pixels);closed();fail_memory=false;
 lv_obj_delete(image);
}
int main(void)
{
 lv_init();lv_display_t *display=lv_display_create(1024,600);lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,read_touch);
 for(int theme=0;theme<5;theme++)for(int density=0;density<3;density++)for(int large=0;large<2;large++){
  ui_theme_set_active((ui_theme_id_t)theme);ui_theme_set_density((ui_density_id_t)density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});checks(input);
 }
 lv_indev_delete(input);lv_display_delete(display);lv_deinit();puts("PASS: fullscreen native taps, owned action/field retirement, aliased snapshots, replacement cleanup, guarded queue launch, actual worker success/retry/stale render publication and bounded buffer cleanup across five themes/densities/text sizes");
}
