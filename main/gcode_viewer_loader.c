#include "gcode_viewer_loader.h"
#include "gcode_surface.h"
#include "gcode_cache.h"
#include "gcode_volume.h"
#include <stdio.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "moonraker_config_controller.h"
#include "moonraker_transport_security_controller.h"
#include "network_activity_controller.h"

typedef struct {
    uint32_t request, profile;
    char url[1024], key[MOONRAKER_CONFIG_API_KEY_LENGTH], ca[MOONRAKER_TLS_CA_PEM_MAX];
    gcode_viewer_ready_cb_t ready;
    struct gcode_cache *cache;
    gcode_toolpath_t snapshot;
    unsigned layer;
    gcode_viewer_refined_cb refined;
    int64_t deadline,yield_due;
    gcode_segment_t *replay_segments;size_t replay_count;
    gcode_volume_t *volume;bool feed_ok;
} job_t;
/* Explicit volatile stores keep key erasure from being optimized away before free. */
static void wipe_key(job_t *job){volatile unsigned char *p=(volatile unsigned char *)job->key;for(size_t i=0;i<sizeof(job->key);i++)p[i]=0;}
static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_request;
static bool s_running;
static uint32_t s_progress_request;
static gcode_viewer_progress_t s_progress;
gcode_viewer_progress_t gcode_viewer_loader_progress(void)
{
    taskENTER_CRITICAL(&s_lock);
    gcode_viewer_progress_t value=s_progress_request==s_request?s_progress:(gcode_viewer_progress_t){0};
    value.running=s_running&&s_progress_request==s_request;
    taskEXIT_CRITICAL(&s_lock);return value;
}
static void progress(const job_t *job,const gcode_toolpath_t *model)
{
    taskENTER_CRITICAL(&s_lock);
    if(job->request==s_request){s_progress.bytes=model->bytes;s_progress.paths=model->input_segments;s_progress.layers=model->layer_count;}
    taskEXIT_CRITICAL(&s_lock);
}
static bool current(const job_t *job)
{
    taskENTER_CRITICAL(&s_lock);bool same=s_request==job->request;taskEXIT_CRITICAL(&s_lock);
    return same&&job->profile==moonraker_config_generation();
}
void gcode_viewer_loader_cancel(void){taskENTER_CRITICAL(&s_lock);s_request++;taskEXIT_CRITICAL(&s_lock);}
bool gcode_viewer_loader_running(void){taskENTER_CRITICAL(&s_lock);bool running=s_running;taskEXIT_CRITICAL(&s_lock);return running;}
static void cache_drop(struct gcode_cache *cache){
    if(!cache)return;
    taskENTER_CRITICAL(&s_lock);bool last=!--cache->refs;taskEXIT_CRITICAL(&s_lock);
    if(last){gcode_cache_destroy(cache);heap_caps_free(cache);}
}
static void volume_release(void *p);
void gcode_viewer_loader_free(gcode_toolpath_t *path){if(path){cache_drop(path->cache);mesh_preview_free(path->overview_mesh,volume_release);heap_caps_free(path->surface);heap_caps_free(path);}}
static void segment_sink(void *context,const gcode_segment_t *segment){
    gcode_toolpath_t *m=context;if(m->cache)gcode_cache_feed(m->cache,segment);if(m->surface)gcode_surface_feed(m->surface,segment);
}
static gcode_toolpath_t *compact_overview(gcode_toolpath_t *old){
    if(!old->cache||!old->cache->complete||old->capacity<=GCODE_TOOLPATH_SEGMENTS_MIN)return old;
    gcode_toolpath_t *small=heap_caps_calloc(1,sizeof(*small)+GCODE_TOOLPATH_SEGMENTS_MIN*sizeof(gcode_segment_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!small)return old;
    *small=*old;small->segments=(gcode_segment_t *)(small+1);small->capacity=GCODE_TOOLPATH_SEGMENTS_MIN;small->count=0;small->input_segments=0;
    for(size_t i=0;i<old->count;i++)gcode_toolpath_collect(small,old->segments[i]);
    small->input_segments=old->input_segments;small->last_segment=old->last_segment;
    old->cache=NULL;old->surface=NULL;old->overview_mesh=NULL;gcode_viewer_loader_free(old);return small;
}
static void worker(void *argument)
{
    job_t *job=argument;gcode_toolpath_t *model=NULL;gcode_parser_t *parser=NULL;
    esp_http_client_handle_t client=NULL;bool shared=false,ok=false;
    const char *error="Unable to load G-code. Close and retry.";
    struct gcode_cache *cache=heap_caps_calloc(1,sizeof(*cache),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(cache&&!gcode_cache_begin(cache,job->request)){heap_caps_free(cache);cache=NULL;}
    size_t capacity=GCODE_TOOLPATH_SEGMENTS_MAX;
    for(;;){
        model=heap_caps_calloc(1,sizeof(*model)+capacity*sizeof(gcode_segment_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(model||capacity==GCODE_TOOLPATH_SEGMENTS_MIN)break;
        capacity/=2;
    }
    parser=heap_caps_calloc(1,sizeof(*parser),MALLOC_CAP_8BIT);
    if(!model||!parser){cache_drop(cache);error="Not enough memory for the 3D preview.";goto done;}
    gcode_parser_init(parser,model,(gcode_segment_t *)(model+1),capacity);
    model->cache=cache;model->profile=job->profile;
    model->surface=heap_caps_calloc(1,sizeof(struct gcode_surface)+GCODE_SURFACE_PANELS_MAX*sizeof(gcode_panel_t)+GCODE_SURFACE_SLOTS*sizeof(uint32_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(model->surface){
        gcode_panel_t *panels=(gcode_panel_t *)(model->surface+1);
        gcode_surface_init(model->surface,panels,GCODE_SURFACE_PANELS_MAX,(uint32_t *)(panels+GCODE_SURFACE_PANELS_MAX));
    }
    parser->segment_sink=segment_sink;parser->sink_context=model;
    if(!current(job))goto done;
    if(!network_activity_controller_acquire_shared(1500)){error="Network busy. Close and retry after the current operation.";goto done;}
    shared=true;
    esp_http_client_config_t config={.url=job->url,.cert_pem=job->ca[0]?job->ca:NULL,
        .disable_auto_redirect=true,.method=HTTP_METHOD_GET,.timeout_ms=3000,.buffer_size=2048};
    client=esp_http_client_init(&config);if(!client)goto done;
    if(job->key[0]&&esp_http_client_set_header(client,"X-Api-Key",job->key)!=ESP_OK)goto done;
    if(esp_http_client_open(client,0)!=ESP_OK)goto done;
    if(esp_http_client_fetch_headers(client)<0||esp_http_client_get_status_code(client)!=200){error="G-code request failed. Check connection and file access.";goto done;}
    int64_t deadline=esp_timer_get_time()+60000000;
    char chunk[2048];unsigned chunks=0;model->source_hash=14695981039346656037ull;
    while(current(job)&&!network_activity_controller_exclusive_requested()){
        if(esp_timer_get_time()>deadline){gcode_parser_limit(parser,"60 second load budget");ok=true;break;}
        int n=esp_http_client_read(client,chunk,sizeof(chunk));
        if(n<0)goto done;
        if(n==0){ok=esp_http_client_is_complete_data_received(client);break;}
        size_t before=model->bytes;bool more=gcode_parser_feed(parser,chunk,(size_t)n);for(size_t i=0;i<model->bytes-before;i++){model->source_hash^=(unsigned char)chunk[i];model->source_hash*=1099511628211ull;}progress(job,model);if(!more){ok=true;break;}
        if(++chunks%16==0)vTaskDelay(1);
    }
    if(!current(job)||network_activity_controller_exclusive_requested()){ok=false;error="Preview cancelled for a printer or network operation.";}
    if(ok){
        gcode_parser_finish(parser);model->source_hash_valid=true;if(model->surface)gcode_surface_finish(model->surface);
        if(model->cache&&!gcode_cache_finish(model->cache)){cache_drop(model->cache);model->cache=NULL;}
        model=compact_overview(model);progress(job,model);
    }
done:
    if(client){esp_http_client_close(client);esp_http_client_cleanup(client);}
    if(shared)network_activity_controller_end_shared();
    heap_caps_free(parser);
    if(current(job)&&bsp_display_lock(1500)){
        if(current(job)&&job->ready(ok?model:NULL,ok?NULL:error))model=NULL;
        bsp_display_unlock();
    }
    gcode_viewer_loader_free(model);wipe_key(job);heap_caps_free(job);
    taskENTER_CRITICAL(&s_lock);s_running=false;taskEXIT_CRITICAL(&s_lock);
    vTaskDelete(NULL);
}
static bool endpoint(job_t *job,const char *file){
    if(!file||!*file||strlen(file)>255||file[0]=='/'||strchr(file,'\\'))return false;
    for(const char *part=file;*part;){const char *slash=strchr(part,'/');size_t n=slash?(size_t)(slash-part):strlen(part);if(!n||(n==1&&part[0]=='.')||(n==2&&!strncmp(part,"..",2)))return false;if(!slash)break;part=slash+1;if(!*part)return false;}
    char path[800]="/server/files/gcodes/";size_t used=strlen(path);const char hex[]="0123456789ABCDEF";
    for(const unsigned char *s=(const unsigned char *)file;*s;s++){
        unsigned char c=*s;bool plain=(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/';
        if(used+(plain?1:3)>=sizeof(path)){return false;}
        if(plain)path[used++]=(char)c;else{path[used++]='%';path[used++]=hex[c>>4];path[used++]=hex[c&15];}
    }path[used]=0;
    const char *ca=NULL;
    if(!moonraker_transport_security_build_http_url_for_endpoint(moonraker_config_host(),moonraker_config_port(),path,job->url,sizeof(job->url),&ca)){return false;}
    if(ca){if(strlen(ca)>=sizeof(job->ca)){return false;}memcpy(job->ca,ca,strlen(ca)+1);}
    const char *key=moonraker_config_api_key();if(key&&strlen(key)>=sizeof(job->key)){return false;}if(key)memcpy(job->key,key,strlen(key)+1);
    return true;
}
bool gcode_viewer_loader_start(const char *file,gcode_viewer_ready_cb_t ready)
{
    if(!file||!*file||strlen(file)>255||file[0]=='/'||strchr(file,'\\')||!ready)return false;
    /* Reject traversal before percent encoding; filenames cannot escape gcode root. */
    for(const char *part=file;*part;){const char *slash=strchr(part,'/');size_t n=slash?(size_t)(slash-part):strlen(part);if(!n||(n==1&&part[0]=='.')||(n==2&&!strncmp(part,"..",2)))return false;if(!slash)break;part=slash+1;if(!*part)return false;}
    taskENTER_CRITICAL(&s_lock);bool busy=s_running;taskEXIT_CRITICAL(&s_lock);if(busy)return false;
    job_t *job=heap_caps_calloc(1,sizeof(*job),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!job)return false;
    job->profile=moonraker_config_generation();
    if(!endpoint(job,file)||job->profile!=moonraker_config_generation()){wipe_key(job);heap_caps_free(job);return false;}
    job->ready=ready;
    taskENTER_CRITICAL(&s_lock);job->request=++s_request;s_running=true;s_progress_request=s_request;s_progress=(gcode_viewer_progress_t){0};taskEXIT_CRITICAL(&s_lock);
    if(xTaskCreatePinnedToCore(worker,"gcode_view",8192,job,3,NULL,0)!=pdPASS){taskENTER_CRITICAL(&s_lock);s_running=false;taskEXIT_CRITICAL(&s_lock);wipe_key(job);heap_caps_free(job);return false;}
    return true;
}

static void detail_worker(void *argument){
    job_t *job=argument;gcode_toolpath_t *model=NULL;FILE *file=NULL;bool ok=false;
    const char *error="SD layer unavailable. Reload the preview to rebuild its cache.";
    uint32_t count=0;file=gcode_cache_open_layer(job->cache,job->layer,&count);if(!file)goto done;
    size_t capacity=GCODE_TOOLPATH_SEGMENTS_MIN;
    while(capacity<count&&capacity<GCODE_TOOLPATH_SEGMENTS_MAX)capacity*=2;
    for(;;){
        model=heap_caps_calloc(1,sizeof(*model)+capacity*sizeof(gcode_segment_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(model||capacity==GCODE_TOOLPATH_SEGMENTS_MIN)break;
        capacity/=2;
    }
    if(!model){error="Not enough memory for SD layer detail.";goto done;}
    *model=job->snapshot;model->segments=(gcode_segment_t *)(model+1);model->capacity=capacity;model->count=0;
    model->cache=NULL;model->surface=NULL;model->overview_mesh=NULL;model->sample_stride=1;model->input_segments=0;model->cached_layer=(int)job->layer;
    int64_t deadline=esp_timer_get_time()+15000000;
    for(uint32_t i=0;i<count;i++){
        if(i%256==0){if(!current(job)||network_activity_controller_exclusive_requested()||esp_timer_get_time()>deadline)goto done;vTaskDelay(1);}
        gcode_segment_t s;if(!gcode_cache_read(file,job->cache->start[job->layer]+i,job->layer,&s))goto done;
        gcode_toolpath_collect(model,s);
    }
    ok=true;
done:
    if(file)fclose(file);
    if(current(job)&&bsp_display_lock(1500)){
        if(current(job)&&job->ready(ok?model:NULL,ok?NULL:error))model=NULL;
        bsp_display_unlock();
    }
    gcode_viewer_loader_free(model);cache_drop(job->cache);heap_caps_free(job);
    taskENTER_CRITICAL(&s_lock);s_running=false;taskEXIT_CRITICAL(&s_lock);vTaskDelete(NULL);
}
bool gcode_viewer_loader_detail(const gcode_toolpath_t *model,unsigned layer,gcode_viewer_ready_cb_t ready){
    if(!model||!model->cache||!model->cache->complete||layer>=model->cache->layers||!ready||model->profile!=moonraker_config_generation())return false;
    taskENTER_CRITICAL(&s_lock);bool busy=s_running;taskEXIT_CRITICAL(&s_lock);if(busy)return false;
    job_t *job=heap_caps_calloc(1,sizeof(*job),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!job)return false;
    job->snapshot=*model;job->profile=model->profile;job->ready=ready;job->cache=model->cache;job->layer=layer;
    taskENTER_CRITICAL(&s_lock);job->cache->refs++;job->request=++s_request;s_running=true;taskEXIT_CRITICAL(&s_lock);
    if(xTaskCreatePinnedToCore(detail_worker,"gcode_layer",8192,job,3,NULL,0)!=pdPASS){
        taskENTER_CRITICAL(&s_lock);s_running=false;taskEXIT_CRITICAL(&s_lock);cache_drop(job->cache);heap_caps_free(job);return false;
    }
    return true;
}

static bool volume_cancel(void *context){
    job_t *j=context;int64_t now=esp_timer_get_time();
    if(!current(j)||network_activity_controller_exclusive_requested()||now>j->deadline)return true;
    /* Cancellation probes are frequent inside volume construction. Keep them
     * cheap; yield for other tasks at a wall-time interval, not at every probe. */
    if(now>=j->yield_due){vTaskDelay(1);j->yield_due=esp_timer_get_time()+20000;}
    return !current(j)||network_activity_controller_exclusive_requested()||esp_timer_get_time()>j->deadline;
}
static void volume_progress(void *context,unsigned percent,float cell){job_t *j=context;taskENTER_CRITICAL(&s_lock);if(j->request==s_request){s_progress.phase=1;s_progress.percent=percent;s_progress.cell_mm=cell;}taskEXIT_CRITICAL(&s_lock);}
static void *volume_allocate(size_t n){return heap_caps_calloc(1,n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
static void volume_release(void *p){heap_caps_free(p);}
static void volume_segment(void *context,const gcode_segment_t *s){job_t *j=context;if(j->feed_ok)j->feed_ok=gcode_volume_feed(j->volume,s);}
static void refine_worker(void *argument){
    job_t *j=argument;mesh_preview_t *mesh=NULL;FILE *file=NULL;esp_http_client_handle_t client=NULL;bool shared=false,ok=false;
    gcode_toolpath_t *model=NULL;gcode_parser_t *parser=NULL;const char *error="Solid refinement unavailable; path preview remains available.";
    static const unsigned resolutions[]={512,384,256,192,128};j->deadline=esp_timer_get_time()+120000000;
    if(!current(j))goto done;
    for(unsigned i=0;i<sizeof(resolutions)/sizeof(resolutions[0]);i++){j->volume=gcode_volume_create(&j->snapshot,resolutions[i],volume_allocate,volume_release,volume_cancel,volume_progress,j);if(j->volume)break;}
    if(!j->volume){error="Not enough memory for solid refinement. Path preview remains available.";goto done;}
    j->feed_ok=true;
    if(j->replay_segments){
        for(size_t i=0;i<j->replay_count;i++){
            if(i%256==0){if(volume_cancel(j))goto done;volume_progress(j,(unsigned)(50ull*i/j->replay_count),0);}
            if(!gcode_volume_feed(j->volume,&j->replay_segments[i]))goto done;
        }
        ok=true;goto prepared;
    }
    if(j->cache){
        uint32_t ignored;file=gcode_cache_open_layer(j->cache,0,&ignored);if(!file)goto replay_http;
        for(uint32_t i=0;i<j->cache->count;i++){
            if(i%256==0){if(volume_cancel(j))goto done;volume_progress(j,50ull*i/j->cache->count,0);}
            gcode_segment_t s;if(!gcode_cache_read(file,i,0,&s))goto replay_http;if(!gcode_volume_feed(j->volume,&s))goto done;
        }
        ok=true;goto prepared;
    }
replay_http:
    if(file){fclose(file);file=NULL;}
    if(!j->url[0])goto done;
    {
        model=volume_allocate(sizeof(*model)+sizeof(gcode_segment_t));parser=volume_allocate(sizeof(*parser));if(!model||!parser)goto done;
        gcode_parser_init(parser,model,(gcode_segment_t *)(model+1),1);parser->segment_sink=volume_segment;parser->sink_context=j;model->source_hash=14695981039346656037ull;
        if(!network_activity_controller_acquire_shared(1500))goto done;
        shared=true;
        esp_http_client_config_t config={.url=j->url,.cert_pem=j->ca[0]?j->ca:NULL,.disable_auto_redirect=true,.method=HTTP_METHOD_GET,.timeout_ms=3000,.buffer_size=2048};
        client=esp_http_client_init(&config);if(!client)goto done;if(j->key[0]&&esp_http_client_set_header(client,"X-Api-Key",j->key)!=ESP_OK)goto done;
        if(esp_http_client_open(client,0)!=ESP_OK||esp_http_client_fetch_headers(client)<0||esp_http_client_get_status_code(client)!=200)goto done;
        char chunk[2048];unsigned chunks=0;
        while(!volume_cancel(j)){
            int n=esp_http_client_read(client,chunk,sizeof(chunk));if(n<0)goto done;if(!n){ok=esp_http_client_is_complete_data_received(client);break;}
            size_t before=model->bytes;bool more=gcode_parser_feed(parser,chunk,(size_t)n);for(size_t i=0;i<model->bytes-before;i++){model->source_hash^=(unsigned char)chunk[i];model->source_hash*=1099511628211ull;}
            if(!j->feed_ok)goto done;
            volume_progress(j,(unsigned)(50ull*model->bytes/(j->snapshot.bytes?j->snapshot.bytes:1)),0);
            if(!more){ok=true;break;}if(++chunks%16==0)vTaskDelay(1);
        }
        if(ok){gcode_parser_finish(parser);ok=j->feed_ok&&model->bytes==j->snapshot.bytes&&model->source_hash==j->snapshot.source_hash&&model->input_segments==j->snapshot.input_segments;}
        if(!ok){error="File changed or replay failed. Reload to rebuild the preview.";goto done;}
    }
prepared:
    if(file){fclose(file);file=NULL;}if(client){esp_http_client_close(client);esp_http_client_cleanup(client);client=NULL;}if(shared){network_activity_controller_end_shared();shared=false;}
    if(ok&&!volume_cancel(j)){mesh=gcode_volume_finish(j->volume);j->volume=NULL;}
    if(mesh)mesh_preview_prepare_interaction(mesh,volume_allocate,volume_release,volume_cancel,j);
done:
    if(file)fclose(file);
    if(client){esp_http_client_close(client);esp_http_client_cleanup(client);}if(shared)network_activity_controller_end_shared();
    gcode_volume_destroy(j->volume);heap_caps_free(j->replay_segments);heap_caps_free(parser);gcode_viewer_loader_free(model);
    if(current(j)&&bsp_display_lock(1500)){if(current(j)&&j->refined(mesh,mesh?NULL:error))mesh=NULL;bsp_display_unlock();}
    mesh_preview_free(mesh,volume_release);cache_drop(j->cache);wipe_key(j);heap_caps_free(j);
    taskENTER_CRITICAL(&s_lock);s_running=false;taskEXIT_CRITICAL(&s_lock);vTaskDelete(NULL);
}
bool gcode_viewer_loader_refine(const char *file,const gcode_toolpath_t *model,gcode_viewer_refined_cb ready){
    if(!model||!model->extrusion_seen||!model->source_hash_valid||!ready||model->profile!=moonraker_config_generation())return false;
    taskENTER_CRITICAL(&s_lock);bool busy=s_running;taskEXIT_CRITICAL(&s_lock);if(busy)return false;
    job_t *j=volume_allocate(sizeof(*j));if(!j)return false;j->snapshot=*model;j->profile=model->profile;j->refined=ready;
    if(model->cache&&model->cache->complete){j->cache=model->cache;if(!endpoint(j,file)){j->url[0]=0;wipe_key(j);}}else if(!endpoint(j,file)){wipe_key(j);heap_caps_free(j);return false;}
    if(j->profile!=moonraker_config_generation()){wipe_key(j);heap_caps_free(j);return false;}
    if(model->segments&&model->count&&model->count<=model->capacity&&model->count==model->input_segments&&model->sample_stride==1&&model->count<=GCODE_TOOLPATH_SEGMENTS_MIN){
        j->replay_segments=heap_caps_calloc(model->count,sizeof(*j->replay_segments),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(j->replay_segments){memcpy(j->replay_segments,model->segments,model->count*sizeof(*j->replay_segments));j->replay_count=model->count;}
    }
    taskENTER_CRITICAL(&s_lock);if(j->cache)j->cache->refs++;j->request=++s_request;s_running=true;s_progress_request=s_request;s_progress=(gcode_viewer_progress_t){.phase=1};taskEXIT_CRITICAL(&s_lock);
    if(xTaskCreatePinnedToCore(refine_worker,"gcode_solid",8192,j,3,NULL,0)!=pdPASS){taskENTER_CRITICAL(&s_lock);s_running=false;taskEXIT_CRITICAL(&s_lock);cache_drop(j->cache);heap_caps_free(j->replay_segments);wipe_key(j);heap_caps_free(j);return false;}return true;
}
