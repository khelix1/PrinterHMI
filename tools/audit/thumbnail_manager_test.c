#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thumbnail_manager.c"
static size_t live_bytes;static bool fail_alloc,fail_task,fail_http;
static unsigned invalidate_during_fetch;
static uint32_t generation=1;
uint32_t moonraker_config_generation(void){return generation;}
static unsigned fetches,delays,deletes;static void (*worker)(void *);static void *worker_arg;
void *heap_caps_malloc(size_t n,uint32_t flags){(void)flags;if(fail_alloc)return NULL;size_t *p=malloc(sizeof(size_t)+n);assert(p);*p=n;live_bytes+=n;return p+1;}
void *heap_caps_calloc(size_t n,size_t s,uint32_t flags){void *p=heap_caps_malloc(n*s,flags);if(p)memset(p,0,n*s);return p;}
void heap_caps_free(void *p){if(p){size_t *base=(size_t *)p-1;assert(live_bytes>=*base);live_bytes-=*base;free(base);}}
size_t strlcpy(char *out,const char *in,size_t capacity){size_t n=strlen(in);if(capacity)snprintf(out,capacity,"%s",in);return n;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return (void *)1;}
int xSemaphoreTake(SemaphoreHandle_t mutex,unsigned ticks){(void)ticks;assert(mutex);return pdTRUE;}
int xSemaphoreGive(SemaphoreHandle_t mutex){assert(mutex);return pdTRUE;}
void vSemaphoreDelete(SemaphoreHandle_t mutex){assert(mutex);}
void vTaskDelay(unsigned ticks){assert(ticks==500);delays++;}
void vTaskDelete(void *task){assert(!task);deletes++;}
int xTaskCreatePinnedToCore(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle,int core){
 assert(!strcmp(name,"thumb_dl")&&stack==8192&&priority==4&&!handle&&core==0);if(fail_task)return 0;worker=fn;worker_arg=arg;return pdPASS;
}
const char *moonraker_config_host(void){return "printer.local";}
int moonraker_config_port(void){return 7125;}
const char *moonraker_config_api_key(void){return "preview-key";}
bool moonraker_fetch_thumbnail_encoded_quiet_with_api_key(const char *host,int port,const char *key,const char *path,uint8_t **bytes,size_t *length){assert(!strcmp(key,"preview-key"));
 assert(!strcmp(host,"printer.local")&&port==7125&&!strcmp(path,".thumbs/job%20name.png"));fetches++;
 if(invalidate_during_fetch==1)generation++;
 if(invalidate_during_fetch==2)thumbnail_manager_mark_pending();
 if(fail_http)return false;
 *bytes=heap_caps_malloc(8,MALLOC_CAP_SPIRAM);if(!*bytes)return false;memset(*bytes,0xab,8);*length=8;return true;
}
bool thumbnail_cache_write(const char *path,const uint8_t *bytes,size_t length){(void)path;(void)bytes;(void)length;assert(!"test unexpectedly wrote persistent storage");return false;}
static void check_cache_identity(void)
{
 char original[64], other[64], tiny[8];
 assert(cache_path_for_thumbnail("printer.local",7125,"folder/job.gcode",".thumbs/job.png",original,sizeof(original)));
 assert(cache_path_for_thumbnail("printer.local",7125,"folder/job.gcode",".thumbs/job.png",other,sizeof(other))&&!strcmp(original,other));
 assert(cache_path_for_thumbnail("printer.local",7125,"folder_job.gcode",".thumbs/job.png",other,sizeof(other))&&strcmp(original,other));
 assert(cache_path_for_thumbnail("other.local",7125,"folder/job.gcode",".thumbs/job.png",other,sizeof(other))&&strcmp(original,other));
 assert(cache_path_for_thumbnail("printer.local",7126,"folder/job.gcode",".thumbs/job.png",other,sizeof(other))&&strcmp(original,other));
 assert(cache_path_for_thumbnail("printer.local",7125,"folder/job.gcode",".thumbs/new.png",other,sizeof(other))&&strcmp(original,other));
 assert(!cache_path_for_thumbnail("printer.local",7125,"job.gcode","thumb.png",tiny,sizeof(tiny))&&!tiny[0]);
 assert(!cache_path_for_thumbnail("printer.local",65536,"job.gcode","thumb.png",other,sizeof(other))&&!other[0]);
}
int main(void){
 check_cache_identity();
 char long_text[193];memset(long_text,'a',sizeof(long_text)-1);long_text[192]=0;
 assert(!thumbnail_manager_start_download_task(long_text,7125,"job.gcode","thumb.png",false,false));
 assert(!thumbnail_manager_start_download_task("printer.local",7125,long_text,"thumb.png",false,false));
 assert(!thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",long_text,false,false));
 assert(!thumbnail_manager_start_download_task("printer.local",65536,"job.gcode","thumb.png",false,false));
 assert(!worker&&!live_bytes);
 assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_IDLE&&!thumbnail_manager_has_png());
 uint8_t *first=heap_caps_malloc(16,MALLOC_CAP_SPIRAM);thumbnail_manager_take_png(first,16);thumbnail_manager_prepare_raw_image();
 assert(thumbnail_manager_png_data()==first&&thumbnail_manager_png_size()==16&&thumbnail_manager_image_dsc()->data==first&&thumbnail_manager_image_dsc()->data_size==16);
 uint8_t *second=heap_caps_malloc(8,MALLOC_CAP_SPIRAM);thumbnail_manager_take_png(second,8);thumbnail_manager_prepare_raw_image();assert(live_bytes==8&&thumbnail_manager_image_dsc()->data==second);
 thumbnail_manager_mark_pending();assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_IDLE);
 thumbnail_manager_mark_failed();assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_FAILED);
 assert(thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));
 assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_LOADING&&!thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));
 worker(worker_arg);assert(fetches==1&&!delays&&deletes==1&&live_bytes==16&&thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_READY&&!thumbnail_manager_force_refresh());
 assert(thumbnail_manager_png_data()==second&&thumbnail_manager_image_dsc()->data==second);
 assert(thumbnail_manager_publish_download("job.gcode")&&live_bytes==8);
 assert(thumbnail_manager_image_dsc()->data==thumbnail_manager_png_data()&&thumbnail_manager_image_dsc()->data_size==8);
 fail_http=true;assert(thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));worker(worker_arg);
 assert(fetches==4&&delays==2&&deletes==2&&live_bytes==8&&thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_FAILED&&!thumbnail_manager_task_running());
 fail_http=false;fail_task=true;assert(!thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false)&&!thumbnail_manager_task_running()&&live_bytes==8);
 fail_task=false;fail_alloc=true;assert(!thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false)&&live_bytes==8);fail_alloc=false;
 uint8_t *published=thumbnail_manager_png_data();
 for(unsigned mode=1;mode<=2;mode++){
  assert(thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));
  unsigned before=fetches;
  invalidate_during_fetch=mode;worker(worker_arg);invalidate_during_fetch=0;
  assert(fetches==before+1&&thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_IDLE);
  assert(!thumbnail_manager_publish_download("job.gcode")&&thumbnail_manager_png_data()==published&&live_bytes==8);
 }
 assert(thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));
 worker(worker_arg);
 assert(!thumbnail_manager_publish_download("other.gcode")&&thumbnail_manager_png_data()==published&&live_bytes==8);
 assert(thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));
 unsigned before=fetches;generation++;worker(worker_arg);
 assert(fetches==before&&!thumbnail_manager_task_running()&&live_bytes==8);
 assert(thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));
 worker(worker_arg);generation++;
 assert(!thumbnail_manager_publish_download("job.gcode")&&thumbnail_manager_png_data()==published&&live_bytes==8);
 thumbnail_manager_clear_png();assert(!thumbnail_manager_has_png()&&!thumbnail_manager_png_data()&&!thumbnail_manager_png_size()&&!live_bytes);
 thumbnail_manager_take_png(NULL,0);assert(!live_bytes);
 assert(!thumbnail_manager_start_download_task("",7125,"job.gcode","path",false,false));
 puts("PASS: production PNG ownership/replacement/raw descriptor, result transitions, duplicate task gate, actual worker success/retries and allocation/task-failure cleanup");return 0;
}
