#include "mesh_preview_loader.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "bsp/esp-bsp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "moonraker_config_controller.h"
#include <string.h>
#include <sys/stat.h>
typedef struct {char name[81];uint32_t request,profile;int64_t deadline;mesh_preview_ready_cb ready;} job_t;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static uint32_t request;
static bool running;
static bool current(const job_t *j){taskENTER_CRITICAL(&lock);bool same=j->request==request;taskEXIT_CRITICAL(&lock);return same&&j->profile==moonraker_config_generation();}
void mesh_preview_loader_cancel(void){taskENTER_CRITICAL(&lock);request++;taskEXIT_CRITICAL(&lock);}
bool mesh_preview_loader_running(void){taskENTER_CRITICAL(&lock);bool value=running;taskEXIT_CRITICAL(&lock);return value;}
void mesh_preview_loader_free(mesh_preview_t *m){heap_caps_free(m);}
static void *allocate(size_t n){return heap_caps_calloc(1,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
static void release(void *m){heap_caps_free(m);}
static bool cancelled(void *ctx){job_t *j=ctx;vTaskDelay(1);return !current(j)||esp_timer_get_time()>j->deadline;}
static void worker(void *argument){
    job_t *j=argument;mesh_preview_t *m=NULL;FILE *f=NULL;char path[256],error[120]="Unable to open SD mesh. Check the card and file.";
    snprintf(path,sizeof(path),"%s/%s",MESH_PREVIEW_DIRECTORY,j->name);
    struct stat info;
    if(!current(j)||stat(path,&info)||!S_ISREG(info.st_mode))goto done;
    f=fopen(path,"rb");if(!f)goto done;
    j->deadline=esp_timer_get_time()+30000000;
    m=mesh_preview_read(f,allocate,release,cancelled,j,error,sizeof(error));
done:
    if(f)fclose(f);
    if(current(j)&&bsp_display_lock(1500)){if(current(j)&&j->ready(m,m?NULL:error))m=NULL;bsp_display_unlock();}
    mesh_preview_loader_free(m);heap_caps_free(j);
    taskENTER_CRITICAL(&lock);running=false;taskEXIT_CRITICAL(&lock);vTaskDelete(NULL);
}
bool mesh_preview_loader_start(const char *name,mesh_preview_ready_cb ready){
    if(!mesh_preview_name_valid(name)||!ready)return false;
    taskENTER_CRITICAL(&lock);bool busy=running;taskEXIT_CRITICAL(&lock);if(busy)return false;
    job_t *j=allocate(sizeof(*j));if(!j)return false;
    strcpy(j->name,name);j->ready=ready;j->profile=moonraker_config_generation();
    taskENTER_CRITICAL(&lock);j->request=++request;running=true;taskEXIT_CRITICAL(&lock);
    if(xTaskCreatePinnedToCore(worker,"mesh_preview",6144,j,3,NULL,0)!=pdPASS){taskENTER_CRITICAL(&lock);running=false;taskEXIT_CRITICAL(&lock);heap_caps_free(j);return false;}
    return true;
}
