#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "files_page_controller.c"
static bool in_worker,success=true,detail,embedded,task_fail,body_fail;
static unsigned allocations,frees,http_calls,delays,rendered,preview_begin;
static uint32_t generation=1;
static lv_obj_t *page=(void*)1;
static char status[160],last_path[160];
static void (*worker_fn)(void*);
static void *worker_arg,*result;
void *heap_caps_malloc(size_t n,uint32_t c){(void)c;if(body_fail && n==FILES_PAGE_LIST_CAPACITY)return NULL;allocations++;return malloc(n);}
void *heap_caps_calloc(size_t n,size_t size,uint32_t c){void *p=heap_caps_malloc(n*size,c);if(p)memset(p,0,n*size);return p;}
void heap_caps_free(void *p){if(p){frees++;free(p);}}
size_t strlcpy(char *d,const char *s,size_t n){size_t len=strlen(s);if(n)snprintf(d,n,"%s",s);return len;}
size_t strlcat(char *d,const char *s,size_t n){size_t used=strlen(d);if(used<n)strlcpy(d+used,s,n-used);return used+strlen(s);}
uint32_t moonraker_config_generation(void){return generation;}
const char *esp_err_to_name(esp_err_t e){(void)e;return "TIMEOUT";}
QueueHandle_t xQueueCreate(unsigned count,size_t size){assert(count==1 && size==sizeof(result));return &result;}
int xQueueReceive(QueueHandle_t q,void *p,unsigned ticks){assert(q==&result && ticks==0);if(!result)return 0;*(void**)p=result;result=NULL;return pdPASS;}
int xQueueSend(QueueHandle_t q,const void *p,unsigned ticks){assert(in_worker && q==&result && ticks==portMAX_DELAY && !result);result=*(void*const*)p;return pdPASS;}
int xTaskCreatePinnedToCoreWithCaps(void (*fn)(void*),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *task,int core,unsigned caps){(void)name;(void)stack;(void)priority;(void)task;(void)core;(void)caps;assert(!in_worker && !worker_arg);if(task_fail)return 0;worker_fn=fn;worker_arg=arg;return pdPASS;}
void vTaskDelete(void *t){assert(in_worker && !t);}
void vTaskDelay(unsigned ticks){assert(in_worker);delays+=ticks;}
bool moonraker_fetch_file_list(const char *h,int port,const char *key,char *body,size_t n,int *code,esp_err_t *e){assert(in_worker && port==7125 && strcmp(key,"key")==0);http_calls++;*code=success?200:0;*e=success?ESP_OK:ESP_FAIL;snprintf(body,n,"%s",h);return success;}
void ui_files_set_status(const char *s){assert(!in_worker);snprintf(status,sizeof(status),"%s",s);}
lv_obj_t *ui_files_get_popup(void){assert(!in_worker);return page;}
bool ui_files_can_refresh_rows(void){assert(!in_worker);return !detail||embedded;}
void ui_files_set_browser_callbacks(ui_files_search_cb_t a,ui_files_action_cb_t b,ui_files_folder_cb_t c,ui_files_action_cb_t d){(void)a;(void)b;(void)c;(void)d;assert(!in_worker);}
void ui_files_set_file_thumbnail(const char *p,const lv_image_dsc_t *i){(void)p;(void)i;assert(!in_worker);}
void ui_files_clear_rows(void){assert(!in_worker);rendered++;}
void ui_files_set_breadcrumb(const char *s){(void)s;assert(!in_worker);}
void ui_files_set_sort_text(const char *s){(void)s;assert(!in_worker);}
void ui_files_set_search_text(const char *s){(void)s;assert(!in_worker);}
void ui_files_add_file_entry(const char *p,double size,double modified,int y){(void)size;(void)modified;(void)y;assert(!in_worker);snprintf(last_path,sizeof(last_path),"%s",p);}
void ui_files_add_folder_button(const char *n,const char *p,int y){(void)n;(void)p;(void)y;assert(!in_worker);}
int32_t ui_theme_density_metric(int32_t a,int32_t b,int32_t c){(void)b;(void)c;return a;}
void files_row_preview_begin(const char *h,int p,const char *k,bool sd,files_row_preview_ready_cb_t cb){(void)h;(void)p;(void)k;(void)sd;(void)cb;assert(!in_worker);preview_begin++;}
int printer_files_parse_entries(const char *body,printer_file_entry_t *entries,size_t capacity){assert(!in_worker && capacity);snprintf(entries[0].path,sizeof(entries[0].path),"%s.gcode",body);return 1;}
int printer_files_for_each_path(const char *body,void (*cb)(const char*,void*),void *arg){(void)body;(void)cb;(void)arg;return 0;}
static void run_worker(void){assert(worker_arg);void *arg=worker_arg;worker_arg=NULL;in_worker=true;worker_fn(arg);in_worker=false;}
static void poll(void){assert(s_load_timer);files_load_poll(s_load_timer);}
static void load(const char *host){files_page_controller_reload(true,true,true,host,7125,"key");}
int main(void){
 lv_init();load("offline");assert(http_calls==0 && delays==0 && s_load_busy);success=false;run_worker();assert(http_calls==3 && delays==360 && rendered==0);poll();assert(!s_load_busy && !s_load_timer && strstr(status,"TIMEOUT"));
 success=true;load("hidden");run_worker();page=NULL;poll();assert(!rendered && !preview_begin);page=(void*)2;
 load("old");generation=2;load("new");load("newest");assert(s_pending_load && s_load_busy);unsigned before=http_calls;run_worker();assert(http_calls==before);poll();assert(worker_arg && rendered==0);run_worker();poll();assert(rendered==1 && preview_begin==1 && strcmp(last_path,"newest.gcode")==0);
 load("detail");run_worker();detail=true;poll();assert(result && rendered==1);detail=false;poll();assert(!result && rendered==2);
 detail=embedded=true;load("inspector");run_worker();poll();assert(!result && rendered==3);detail=embedded=false;
 load("stale");run_worker();files_page_controller_reload(true,false,true,"offline",7125,"key");poll();assert(rendered==3 && strstr(status,"offline"));
 task_fail=true;load("failed");assert(!s_load_busy && strstr(status,"worker"));poll();task_fail=false;
 body_fail=true;load("no_memory");run_worker();poll();assert(strstr(status,"allocate"));body_fail=false;
 heap_caps_free(s_entries);s_entries=NULL;assert(allocations==frees && !s_pending_load && !s_load_busy && !s_load_timer);lv_deinit();puts("PASS: file-list HTTP/retries run outside LVGL, one worker/coalesced latest job, profile/page/serial fencing, deferred detail publication and allocation/task-failure cleanup");
}
