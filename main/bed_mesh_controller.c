#include "bed_mesh_controller.h"
#include <float.h>
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    bool valid, truncated;
    uint16_t rows, cols;
    double mesh_min_x, mesh_min_y, mesh_max_x, mesh_max_y;
    double minimum, maximum, average, range;
    char profile_name[BED_MESH_PROFILE_NAME_MAX];
    float *values;
    size_t capacity;
} mesh_state_t;
static mesh_state_t s;
static SemaphoreHandle_t mutex;
static void lock(void){ if(mutex) xSemaphoreTake(mutex, portMAX_DELAY); }
static void unlock(void){ if(mutex) xSemaphoreGive(mutex); }
static bool ensure_capacity(size_t count){
    if(s.values && s.capacity >= count) return true;
    float *p = heap_caps_calloc(count,sizeof(float),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!p) p = heap_caps_calloc(count,sizeof(float),MALLOC_CAP_8BIT);
    if(!p) return false;
    if(s.values) heap_caps_free(s.values);
    s.values=p; s.capacity=count; return true;
}
static bool read_xy(cJSON *a,double *x,double *y){
    if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)<2) return false;
    cJSON *jx=cJSON_GetArrayItem(a,0), *jy=cJSON_GetArrayItem(a,1);
    if(!cJSON_IsNumber(jx)||!cJSON_IsNumber(jy)) return false;
    *x=jx->valuedouble; *y=jy->valuedouble; return true;
}
void bed_mesh_controller_init(void)
{
    if (!mutex) {
        mutex = xSemaphoreCreateMutex();
    }

    if (!mutex) {
        return;
    }

    lock();
    memset(&s, 0, sizeof(s));
    unlock();
}
void bed_mesh_controller_reset(void){
    lock(); s.valid=false; s.rows=s.cols=0; s.profile_name[0]=0; unlock();
}
bool bed_mesh_controller_merge_status(cJSON *status){
    if(!cJSON_IsObject(status)) return false;
    cJSON *bm=cJSON_GetObjectItemCaseSensitive(status,"bed_mesh");
    if(!cJSON_IsObject(bm)) return false;
    cJSON *m=cJSON_GetObjectItemCaseSensitive(bm,"mesh_matrix");
    if(!cJSON_IsArray(m)||cJSON_GetArraySize(m)<=0) m=cJSON_GetObjectItemCaseSensitive(bm,"probed_matrix");
    if(!cJSON_IsArray(m)||cJSON_GetArraySize(m)<=0){bed_mesh_controller_reset();return true;}
    int sr=cJSON_GetArraySize(m); cJSON *r0=cJSON_GetArrayItem(m,0);
    int sc=cJSON_IsArray(r0)?cJSON_GetArraySize(r0):0; if(sc<=0)return false;
    uint16_t rows=sr>BED_MESH_MAX_ROWS?BED_MESH_MAX_ROWS:sr;
    uint16_t cols=sc>BED_MESH_MAX_COLS?BED_MESH_MAX_COLS:sc;
    size_t count=(size_t)rows*cols;
    lock();
    if(!ensure_capacity(count)){unlock();return false;}
    double lo=DBL_MAX, hi=-DBL_MAX, sum=0; size_t n=0;
    for(uint16_t y=0;y<rows;y++){
        cJSON *row=cJSON_GetArrayItem(m,y);
        for(uint16_t x=0;x<cols;x++){
            cJSON *v=cJSON_IsArray(row)?cJSON_GetArrayItem(row,x):NULL;
            double z=cJSON_IsNumber(v)?v->valuedouble:0.0;
            s.values[(size_t)y*cols+x]=(float)z;
            if(cJSON_IsNumber(v)){if(z<lo)lo=z;if(z>hi)hi=z;sum+=z;n++;}
        }
    }
    cJSON *profile=cJSON_GetObjectItemCaseSensitive(bm,"profile_name");
    snprintf(s.profile_name,sizeof(s.profile_name),"%s",cJSON_IsString(profile)&&profile->valuestring?profile->valuestring:"active");
    read_xy(cJSON_GetObjectItemCaseSensitive(bm,"mesh_min"),&s.mesh_min_x,&s.mesh_min_y);
    read_xy(cJSON_GetObjectItemCaseSensitive(bm,"mesh_max"),&s.mesh_max_x,&s.mesh_max_y);
    s.rows=rows;s.cols=cols;s.truncated=sr>BED_MESH_MAX_ROWS||sc>BED_MESH_MAX_COLS;
    s.minimum=n?lo:0;s.maximum=n?hi:0;s.average=n?sum/n:0;s.range=s.maximum-s.minimum;s.valid=n>0;
    unlock();return true;
}
bool bed_mesh_controller_snapshot(bed_mesh_snapshot_t *out,float *values,size_t cap){
    if (!out) {
        return false;
    }

    lock();

    size_t count = (size_t)s.rows * s.cols;
    if(!s.valid||!s.values||!values||cap<count){memset(out,0,sizeof(*out));unlock();return false;}
    out->valid=s.valid;out->truncated=s.truncated;out->rows=s.rows;out->cols=s.cols;
    out->mesh_min_x=s.mesh_min_x;out->mesh_min_y=s.mesh_min_y;out->mesh_max_x=s.mesh_max_x;out->mesh_max_y=s.mesh_max_y;
    out->minimum=s.minimum;out->maximum=s.maximum;out->average=s.average;out->range=s.range;
    snprintf(out->profile_name,sizeof(out->profile_name),"%s",s.profile_name);
    memcpy(values,s.values,count*sizeof(float));out->values=values;unlock();return true;
}
