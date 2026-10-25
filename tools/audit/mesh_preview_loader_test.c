#include "mesh_preview_loader.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static unsigned allocations,callbacks,errors,generation=1,delay_calls;
static int fail;static bool display=true;
static void (*task)(void *);static void *argument;
void *test_calloc(size_t n,size_t s){if(fail==1||(fail==2&&s>1000))return NULL;void *p=calloc(n,s);if(p)allocations++;return p;}
void test_free(void *p){if(p){assert(allocations);allocations--;free(p);}}
uint32_t moonraker_config_generation(void){return generation;}
int64_t esp_timer_get_time(void){return fail==6&&delay_calls?31000000:0;}
bool bsp_display_lock(unsigned n){assert(n==1500);return display;}
void bsp_display_unlock(void){}
int xTaskCreatePinnedToCore(void (*f)(void *),const char *name,unsigned stack,void *p,int priority,void *handle,int core){(void)name;(void)priority;(void)handle;(void)core;assert(stack==6144&&!task);if(fail==3)return 0;task=f;argument=p;return 1;}
void vTaskDelete(void *p){assert(!p);}
void vTaskDelay(unsigned n){(void)n;delay_calls++;if(fail==4)generation++;if(fail==5)mesh_preview_loader_cancel();}
static bool ready(mesh_preview_t *m,const char *error){callbacks++;if(error){assert(!m);errors++;return false;}assert(m&&m->triangles==6144);mesh_preview_loader_free(m);return true;}
static bool reject(mesh_preview_t *m,const char *error){assert(m&&!error);return false;}
static void execute(void){assert(task);void (*f)(void*)=task;void *p=argument;task=NULL;argument=NULL;f(p);assert(!allocations&&!mesh_preview_loader_running());delay_calls=0;}
int main(void){
 assert(mesh_preview_loader_start("demo.phm",ready));assert(!mesh_preview_loader_start("demo.phm",ready));execute();assert(callbacks==1&&!errors);
 const char *bad[]={"../demo.phm","/demo.phm","x/demo.phm","demo.stl",".hidden.phm","demo\\a.phm",""};for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!mesh_preview_loader_start(bad[i],ready));
 for(fail=1;fail<=6;fail++){unsigned before=callbacks,prior_errors=errors;bool start=mesh_preview_loader_start("demo.phm",ready);if(fail==1||fail==3){assert(!start&&!allocations);continue;}assert(start);execute();if(fail==4||fail==5)assert(callbacks==before);else assert(callbacks==before+1&&errors==prior_errors+1);}
 fail=0;unsigned before=callbacks;assert(mesh_preview_loader_start("demo.phm",ready));mesh_preview_loader_cancel();execute();assert(callbacks==before);
 assert(mesh_preview_loader_start("missing.phm",ready));execute();assert(errors);
 assert(mesh_preview_loader_start("demo.phm",reject));execute();
 display=false;before=callbacks;assert(mesh_preview_loader_start("demo.phm",ready));execute();assert(callbacks==before);display=true;
 puts("PASS: local mesh worker, single flight, safe names, allocation/task/deadline failures, request/profile cancellation, display-lock failure, rejected adoption and complete cleanup");return 0;
}
