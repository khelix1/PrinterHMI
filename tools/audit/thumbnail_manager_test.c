#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thumbnail_manager.c"
static size_t live_bytes;static bool fail_alloc,fail_task,fail_http;
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
bool moonraker_fetch_thumbnail_encoded_quiet(const char *host,int port,const char *path,uint8_t **bytes,size_t *length){
 assert(!strcmp(host,"printer.local")&&port==7125&&!strcmp(path,".thumbs/job%20name.png"));fetches++;
 if(fail_http)return false;
 *bytes=heap_caps_malloc(8,MALLOC_CAP_SPIRAM);if(!*bytes)return false;memset(*bytes,0xab,8);*length=8;return true;
}
bool thumbnail_cache_write(const char *path,const uint8_t *bytes,size_t length){(void)path;(void)bytes;(void)length;assert(!"test unexpectedly wrote persistent storage");return false;}
int main(void){
 assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_IDLE&&!thumbnail_manager_has_png());
 uint8_t *first=heap_caps_malloc(16,MALLOC_CAP_SPIRAM);thumbnail_manager_take_png(first,16);thumbnail_manager_prepare_raw_image();
 assert(thumbnail_manager_png_data()==first&&thumbnail_manager_png_size()==16&&thumbnail_manager_image_dsc()->data==first&&thumbnail_manager_image_dsc()->data_size==16);
 thumbnail_manager_mark_ready();assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_READY);
 uint8_t *second=heap_caps_malloc(8,MALLOC_CAP_SPIRAM);thumbnail_manager_take_png(second,8);thumbnail_manager_prepare_raw_image();assert(live_bytes==8&&thumbnail_manager_image_dsc()->data==second);
 thumbnail_manager_mark_pending();assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_IDLE);
 thumbnail_manager_mark_failed();assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_FAILED);
 thumbnail_manager_mark_result(true);assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_READY);
 assert(thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));
 assert(thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_LOADING&&!thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));
 worker(worker_arg);assert(fetches==1&&!delays&&deletes==1&&live_bytes==8&&thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_READY&&!thumbnail_manager_force_refresh());
 assert(thumbnail_manager_image_dsc()->data==thumbnail_manager_png_data()&&thumbnail_manager_image_dsc()->data_size==8);
 fail_http=true;assert(thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false));worker(worker_arg);
 assert(fetches==4&&delays==2&&deletes==2&&live_bytes==8&&thumbnail_manager_result()==THUMBNAIL_MANAGER_RESULT_FAILED&&!thumbnail_manager_task_running());
 fail_http=false;fail_task=true;assert(!thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false)&&!thumbnail_manager_task_running()&&live_bytes==8);
 fail_task=false;fail_alloc=true;assert(!thumbnail_manager_start_download_task("printer.local",7125,"job.gcode",".thumbs/job name.png",true,false)&&live_bytes==8);fail_alloc=false;
 thumbnail_manager_clear_png();assert(!thumbnail_manager_has_png()&&!thumbnail_manager_png_data()&&!thumbnail_manager_png_size()&&!live_bytes);
 thumbnail_manager_take_png(NULL,0);assert(!live_bytes);
 assert(!thumbnail_manager_start_download_task("",7125,"job.gcode","path",false,false));
 puts("PASS: production PNG ownership/replacement/raw descriptor, result transitions, duplicate task gate, actual worker success/retries and allocation/task-failure cleanup");return 0;
}
