/* Real LVGL cache/descriptor lifetime with ESP allocator and lock fixtures. */
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "esp_heap_caps.h"
#include "bsp/esp-bsp.h"
#include "freertos/semphr.h"
#include "custom_theme.h"
#include "moonraker_config_controller.h"
static size_t owned_bytes,allocations;static bool fail_psram;static int display_locked;
typedef struct { size_t size; max_align_t alignment; } allocation_t;
void *heap_caps_malloc(size_t size,uint32_t caps){if(fail_psram && (caps&MALLOC_CAP_SPIRAM))return NULL;allocation_t *a=malloc(sizeof(*a)+size);assert(a);a->size=size;owned_bytes+=size;allocations++;return a+1;}
void *heap_caps_calloc(size_t n,size_t size,uint32_t caps){void *p=heap_caps_malloc(n*size,caps);if(p)memset(p,0,n*size);return p;}
void heap_caps_free(void *p){if(!p)return;allocation_t *a=(allocation_t*)p-1;assert(owned_bytes>=a->size);owned_bytes-=a->size;free(a);}
bool bsp_display_lock(unsigned timeout){assert(timeout==0||timeout==2500);assert(!display_locked);display_locked=1;return true;}
void bsp_display_unlock(void){assert(display_locked);display_locked=0;}
int xSemaphoreTake(SemaphoreHandle_t lock,unsigned timeout){(void)lock;(void)timeout;return 1;}
int xSemaphoreGive(SemaphoreHandle_t lock){(void)lock;return 1;}
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t *d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t *b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t *a){(void)a;return false;}
static int active;static moonraker_profile_t profiles[MOONRAKER_CONFIG_MAX_PROFILES];
int moonraker_config_active_profile_index(void){return active;}
const moonraker_profile_t *moonraker_config_profile(int i){return i>=0 && i<MOONRAKER_CONFIG_MAX_PROFILES?&profiles[i]:NULL;}
static uint32_t profile_generation=1;
uint32_t moonraker_config_generation(void){return profile_generation;}
size_t strlcpy(char *d,const char *s,size_t n){size_t len=strlen(s);if(n){size_t c=len<n-1?len:n-1;memcpy(d,s,c);d[c]=0;}return len;}
#if defined(TEST_PROFILE)
#include "printer_preview_cache.c"
static void run(void){
 static uint16_t pixels[286*215];pixels[0]=0x1234;assert(printer_preview_cache_publish_active("one.gcode",pixels,286,215));
 uint32_t revision;const lv_image_dsc_t *image=printer_preview_cache_image(0,NULL,&revision);assert(image && *(const uint16_t*)image->data==0x1234);const uint8_t *old_pixels=image->data;
 lv_image_header_t header;assert(lv_image_decoder_get_info(image,&header)==LV_RESULT_OK && header.w==286);
 size_t before=allocations;fail_psram=true;pixels[0]=0x5678;assert(printer_preview_cache_publish_active("two.gcode",pixels,286,215));assert(allocations==before && image->data==old_pixels && *(const uint16_t*)image->data==0x5678);
 assert(!printer_preview_cache_publish_active("large.gcode",pixels,900,520));assert(printer_preview_cache_matches(0,"two.gcode"));
 assert(!printer_preview_cache_publish_active("small.gcode",pixels,20,10));assert(printer_preview_cache_matches(0,"two.gcode"));fail_psram=false;
 assert(printer_preview_cache_publish_active("small.gcode",pixels,20,10));assert(lv_image_decoder_get_info(image,&header)==LV_RESULT_OK && header.w==20 && header.h==10);
 uint8_t bad_png[8]={0};assert(!printer_preview_cache_publish_png(0,"stale.local",7125,"other",bad_png,sizeof(bad_png),20,10));assert(!printer_preview_cache_publish_png(0,"printer.local",7125,"other",bad_png,sizeof(bad_png),20,10));assert(printer_preview_cache_matches(0,"small.gcode"));
 printer_preview_cache_image(0,NULL,&revision);printer_preview_cache_invalidate(0);assert(!printer_preview_cache_image(0,NULL,NULL));assert(image->data);assert(printer_preview_cache_publish_active("three.gcode",pixels,20,10));uint32_t next;printer_preview_cache_image(0,NULL,&next);assert(next>revision);
 for(active=1;active<MOONRAKER_CONFIG_MAX_PROFILES;active++)assert(printer_preview_cache_publish_active("profile.gcode",pixels,286,215));
 assert(owned_bytes<=sizeof(preview_slot_t)*MOONRAKER_CONFIG_MAX_PROFILES+(size_t)286*215*2*MOONRAKER_CONFIG_MAX_PROFILES);
 active=0;printer_preview_cache_image(0,NULL,&revision);printer_preview_cache_reset();assert(owned_bytes==sizeof(preview_slot_t)*MOONRAKER_CONFIG_MAX_PROFILES);assert(printer_preview_cache_publish_active("reset.gcode",pixels,20,10));printer_preview_cache_image(0,NULL,&next);assert(next>revision);printer_preview_cache_reset();
 puts("PASS: profile buffer reuse, dimension/header invalidation, PSRAM/budget failures, stale/bad PNG rejection and revisions");
}
#elif defined(TEST_ROWS)
#include "files_row_preview.c"

static bool metadata_fail,download_fail,bump_profile_in_fetch;
static unsigned metadata_requests,thumbnail_requests;
static char downloaded[768];
static const char *metadata_reply;
void thumbnail_manager_url_encode(const char *in,char *out,size_t size){assert(strlen(in)<size);snprintf(out,size,"%s",in);}
bool moonraker_fetch_file_metadata(const char *host,int port,const char *key,const char *file,char *out,size_t size,int *code,esp_err_t *error){
 assert(!strcmp(host,"printer.local")&&port==7125&&!strcmp(key,"key")&&!strcmp(file,"file.gcode"));metadata_requests++;
 *code=metadata_fail?500:200;*error=metadata_fail?ESP_FAIL:ESP_OK;
 snprintf(out,size,"%s",metadata_reply);if(bump_profile_in_fetch){bump_profile_in_fetch=false;profile_generation++;}return !metadata_fail;
}
bool moonraker_fetch_thumbnail_encoded(const char *host,int port,const char *path,uint8_t **out,size_t *size){
 assert(!strcmp(host,"printer.local")&&port==7125);thumbnail_requests++;snprintf(downloaded,sizeof(downloaded),"%s",path);
 if(download_fail)return false;
 *out=heap_caps_malloc(4,MALLOC_CAP_SPIRAM);memset(*out,0,4);*size=4;return true;
}
static void source_checks(void){
 char path[192],tiny[4];
 metadata_reply="{\"result\":{\"thumbnails\":[{\"width\":900,\"height\":520,\"relative_path\":\"large.png\"},{\"width\":32,\"height\":32,\"relative_path\":\"tiny.png\"},{\"width\":96,\"height\":56,\"relative_path\":\"row.png\"},{\"width\":300,\"height\":175,\"relative_path\":\"medium.png\"}]}}";
 assert(row_thumbnail_path(metadata_reply,path,sizeof(path))&&!strcmp(path,"row.png"));
 assert(!row_thumbnail_path(metadata_reply,tiny,sizeof(tiny))&&!tiny[0]);
 assert(row_thumbnail_path("{\"result\":{\"thumbnails\":[{\"width\":32,\"height\":32,\"relative_path\":\"32.png\"},{\"width\":64,\"height\":64,\"relative_path\":\"64.png\"}]}}",path,sizeof(path))&&!strcmp(path,"64.png"));
 assert(row_thumbnail_path("{\"result\":{\"thumbnails\":[{\"relative_path\":\"unknown.png\"}]}}",path,sizeof(path))&&!strcmp(path,"unknown.png"));
 assert(row_thumbnail_path("{\"result\":{\"thumbnails\":[{\"width\":900,\"height\":520,\"relative_path\":\"only-large.png\"}]}}",path,sizeof(path))&&!strcmp(path,"only-large.png"));
 assert(row_thumbnail_path("{\"result\":{\"thumbnails\":[{\"width\":1e309,\"height\":999,\"relative_path\":\"invalid-size.png\"},{\"width\":0,\"height\":96,\"relative_path\":\"zero.png\"},{\"width\":56,\"height\":96,\"relative_path\":\"portrait.png\"}]}}",path,sizeof(path))&&!strcmp(path,"portrait.png"));
 const char *bad[]={"bad json","{}","{\"result\":{\"thumbnails\":[]}}","{\"result\":{\"thumbnails\":[{\"relative_path\":\"\"},{\"relative_path\":42}]}}"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){assert(!row_thumbnail_path(bad[i],path,sizeof(path))&&!path[0]);}
 uint8_t *png=NULL;size_t size=0;
 assert(fetch_preview_png("printer.local",7125,"key","file.gcode",&png,&size));heap_caps_free(png);assert(metadata_requests==1&&thumbnail_requests==1&&!strcmp(downloaded,"row.png"));
 metadata_fail=true;assert(!fetch_preview_png("printer.local",7125,"key","file.gcode",&png,&size)&&!png&&!size);metadata_fail=false;
 download_fail=true;assert(!fetch_preview_png("printer.local",7125,"key","file.gcode",&png,&size)&&!png&&!size);download_fail=false;
 assert(!owned_bytes);assert(ROW_PREVIEW_SLOT_COUNT>=64&&ROW_PREVIEW_QUEUE_LENGTH>=64&&ROW_PREVIEW_WIDTH==96&&ROW_PREVIEW_HEIGHT==96);
 puts("PASS: smallest adequate source, portrait/missing/invalid dimensions, large-only fallback, malformed metadata and fresh fetch failure cleanup");
}
static row_preview_job_t queued[ROW_PREVIEW_QUEUE_LENGTH];static unsigned queued_count;static bool run_worker;static jmp_buf worker_end;
QueueHandle_t xQueueCreate(unsigned count,size_t size){assert(count==64&&size==sizeof(row_preview_job_t));return queued;}
int xQueueReset(QueueHandle_t q){assert(q==queued);queued_count=0;return pdTRUE;}
int xQueueSend(QueueHandle_t q,const void *job,unsigned timeout){assert(q==queued&&timeout==0);if(queued_count==64)return 0;queued[queued_count++]=*(const row_preview_job_t *)job;return pdTRUE;}
int xQueueReceive(QueueHandle_t q,void *job,unsigned timeout){assert(q==queued&&timeout==portMAX_DELAY);if(!queued_count){assert(run_worker);longjmp(worker_end,1);}*(row_preview_job_t *)job=queued[0];memmove(queued,queued+1,--queued_count*sizeof(*queued));return pdTRUE;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void *)1;}
int xTaskCreatePinnedToCore(void (*fn)(void*),const char *name,unsigned stack,void *arg,unsigned priority,void *task,int core){(void)name;(void)stack;(void)arg;(void)priority;(void)task;(void)core;assert(fn==preview_worker);return pdPASS;}
static void drain_worker(void){run_worker=true;if(!setjmp(worker_end))preview_worker(NULL);run_worker=false;}
static int delivered;static lv_obj_t *consumer;
static void ready(const char *file,const lv_image_dsc_t *image){assert(display_locked && !strcmp(file,"file.gcode"));delivered++;lv_image_set_src(consumer,image);}
static void run(void){
 source_checks();
 s_slots=heap_caps_calloc(ROW_PREVIEW_SLOT_COUNT,sizeof(*s_slots),MALLOC_CAP_SPIRAM);s_lock=(void*)1;s_generation=1;s_profile_generation=profile_generation;s_slots[0].generation=1;strcpy(s_slots[0].file,"file.gcode");s_slots[0].state=ROW_PREVIEW_LOADING;s_ready_cb=ready;consumer=lv_image_create(lv_screen_active());
 row_preview_job_t job={.slot=0,.generation=1};uint16_t *pixels=heap_caps_malloc(96*96*2,MALLOC_CAP_SPIRAM);pixels[0]=0x1234;uint16_t *expected=pixels;publish_preview_result(&job,"file.gcode",&pixels,true,96,55);assert(!pixels && s_slots[0].pixels==expected && delivered==1 && !display_locked);
 size_t stable_bytes=owned_bytes;lv_image_header_t header;assert(lv_image_decoder_get_info(&s_slots[0].image,&header)==LV_RESULT_OK);assert(header.w==96 && header.h==55 && header.stride==192 && s_slots[0].image.data_size==96*55*2);
 pixels=heap_caps_malloc(96*96*2,MALLOC_CAP_SPIRAM);pixels[0]=0x5678;publish_preview_result(&job,"file.gcode",&pixels,true,55,96);assert(!pixels && owned_bytes==stable_bytes && delivered==2 && *(const uint16_t*)s_slots[0].image.data==0x5678);
 assert(lv_image_decoder_get_info(&s_slots[0].image,&header)==LV_RESULT_OK && header.w==55 && header.h==96 && header.stride==110);
 s_generation=2;pixels=heap_caps_malloc(96*96*2,MALLOC_CAP_SPIRAM);expected=pixels;publish_preview_result(&job,"file.gcode",&pixels,true,96,55);assert(pixels==expected && delivered==2);heap_caps_free(pixels);pixels=NULL;
 s_generation=1;publish_preview_result(&job,"file.gcode",&pixels,false,0,0);assert(s_slots[0].state==ROW_PREVIEW_FAILED && delivered==2 && owned_bytes==stable_bytes);
 /* Unchanged decoded images must survive a new list generation with no queue,
  * HTTP or new pixel allocation. Changed revisions must not reuse them. */
 printer_file_entry_t entry={.path="file.gcode",.size=100,.modified=200};
 s_queue=queued;s_worker_started=true;strlcpy(s_host,"printer.local",sizeof(s_host));strlcpy(s_api_key,"key",sizeof(s_api_key));s_port=7125;
 s_slots[0].state=ROW_PREVIEW_READY;s_slots[0].revision=files_row_rgb_key(s_host,s_port,entry.path,entry.size,entry.modified);
 expected=s_slots[0].pixels;size_t allocated_before=allocations;unsigned reads_before=metadata_requests;int delivered_before=delivered;
 bsp_display_lock(0);files_row_preview_begin("printer.local",7125,"key",true,&entry,1,ready);files_row_preview_request(entry.path);bsp_display_unlock();
 assert(!queued_count&&delivered==delivered_before+1&&allocations==allocated_before&&s_slots[0].pixels==expected&&metadata_requests==reads_before);
 /* Changed file revision misses RAM, then a valid RGB SD entry bypasses PNG,
  * metadata and thumbnail HTTP in the actual worker. */
 entry.modified++;uint64_t next_revision=files_row_rgb_key(s_host,s_port,entry.path,entry.size,entry.modified);
 uint16_t rgb[96*55];for(unsigned i=0;i<96*55;i++)rgb[i]=0x4567;
 assert(files_row_rgb_store(next_revision,rgb,96,55));
 bsp_display_lock(0);files_row_preview_begin("printer.local",7125,"key",true,&entry,1,ready);assert(s_slots[0].state==ROW_PREVIEW_IDLE);files_row_preview_request(entry.path);bsp_display_unlock();assert(queued_count==1);
 delivered_before=delivered;drain_worker();assert(delivered==delivered_before+1&&metadata_requests==reads_before&&*(const uint16_t *)s_slots[0].image.data==0x4567);
 /* Credential/profile changes must invalidate the in-memory fast path. */
 bsp_display_lock(0);files_row_preview_begin("printer.local",7125,"different-key",true,&entry,1,ready);assert(s_slots[0].state==ROW_PREVIEW_IDLE);bsp_display_unlock();
 s_slots[0].state=ROW_PREVIEW_READY;profile_generation++;delivered_before=delivered;
 bsp_display_lock(0);files_row_preview_request(entry.path);bsp_display_unlock();assert(!queued_count&&delivered==delivered_before);
 bsp_display_lock(0);files_row_preview_begin("printer.local",7125,"different-key",true,&entry,1,ready);assert(s_slots[0].state==ROW_PREVIEW_IDLE);bsp_display_unlock();
 /* A profile change during HTTP retires the fetched PNG before decode/publish. */
 entry.modified+=100;bsp_display_lock(0);files_row_preview_begin("printer.local",7125,"key",true,&entry,1,ready);files_row_preview_request(entry.path);bsp_display_unlock();
 size_t stable_allocated=owned_bytes;delivered_before=delivered;bump_profile_in_fetch=true;drain_worker();
 assert(delivered==delivered_before&&owned_bytes==stable_allocated);
 /* Queueing the entire catalogue cannot strand later rows. */
 printer_file_entry_t *catalog=calloc(64,sizeof(*catalog));assert(catalog);
 for(unsigned i=0;i<64;i++){snprintf(catalog[i].path,sizeof(catalog[i].path),"job%u.gcode",i);catalog[i].size=100+i;catalog[i].modified=500+i;}
 bsp_display_lock(0);files_row_preview_begin("printer.local",7125,"key",false,catalog,64,ready);
 for(unsigned i=0;i<64;i++)files_row_preview_request(catalog[i].path);
 bsp_display_unlock();assert(queued_count==64);free(catalog);
 puts("PASS: decoded RAM reuse without allocations/HTTP/queue, file revision invalidation, actual RGB-hit worker, credential/profile fences and all 64 requests queued");
 lv_obj_delete(consumer);lv_image_cache_drop(&s_slots[0].image);heap_caps_free(s_slots[0].pixels);heap_caps_free(s_slots);s_slots=NULL;assert(!owned_bytes);
 puts("PASS: Files display-first publication, single-buffer transfer, replacement lifetime, stale generation discard and failed result retention");
}
#else
#include "ui_preview_lightbox.c"
static void run(void){
 uint16_t *source=malloc(286*215*2);for(int i=0;i<286*215;i++)source[i]=0x1234;
 lv_image_dsc_t image={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=286,.h=215,.stride=572},.data_size=286*215*2,.data=(uint8_t*)source};
 for(int i=0;i<12;i++){
  ui_preview_lightbox_show(&image);assert(s_preview_lightbox && s_preview_image && s_fallback_pixels);
  const lv_image_dsc_t *snapshot=lv_image_get_src(s_preview_image);assert(snapshot!=&image && snapshot->data!=(uint8_t*)source);assert(((const uint16_t*)snapshot->data)[0]==0x1234);
  source[0]=0xabcd;assert(((const uint16_t*)snapshot->data)[0]==0x1234);source[0]=0x1234;
  assert(owned_bytes==286*215*2);ui_preview_lightbox_close();assert(!s_fallback_pixels && !s_fullscreen_pixels && !owned_bytes);
 }
 fail_psram=true;ui_preview_lightbox_show(&image);assert(s_preview_lightbox && !s_fallback_pixels);assert(!lv_image_get_src(s_preview_image));ui_preview_lightbox_close();fail_psram=false;free(source);
 uint16_t *large=calloc(900*520,2);image.header.w=900;image.header.h=520;image.header.stride=1800;image.data_size=900*520*2;image.data=(uint8_t*)large;ui_preview_lightbox_show(&image);assert(owned_bytes<=286*215*2);free(large);assert(s_fallback_image.data);ui_preview_lightbox_close();assert(!owned_bytes);
 puts("PASS: fullscreen owned fallback, immutable snapshot, bounded large-source fit, allocation failure and repeated close cleanup");
}
#endif
int main(void){lv_init();lv_display_create(1024,600);for(int i=0;i<MOONRAKER_CONFIG_MAX_PROFILES;i++){profiles[i].configured=true;strcpy(profiles[i].host,"printer.local");profiles[i].port=7125;}run();lv_deinit();}
