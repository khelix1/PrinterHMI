#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TEST_CAMERA
#include "camera_stream_controller.c"
static camera_frame_result_t queued;
static bool has_result, shared, stop_on_delay, stop_on_open;
static unsigned delays, elapsed, allocations, frees;
static void (*worker)(void *);
void *heap_caps_malloc(size_t n,uint32_t caps){(void)caps;allocations++;return malloc(n);}
void heap_caps_free(void *p){if(p){frees++;free(p);}}
size_t strlcpy(char *d,const char *s,size_t n){size_t len=strlen(s);if(n){size_t count=len<n-1?len:n-1;memcpy(d,s,count);d[count]=0;}return len;}
QueueHandle_t xQueueCreate(unsigned n,size_t size){assert(n==1 && size==sizeof(queued));return &queued;}
int xQueueReceive(QueueHandle_t q,void *p,unsigned ticks){assert(q==&queued && ticks==0);if(!has_result)return 0;memcpy(p,&queued,sizeof(queued));has_result=false;return pdPASS;}
int xQueueSend(QueueHandle_t q,const void *p,unsigned ticks){assert(q==&queued && ticks==0);if(has_result)return 0;memcpy(&queued,p,sizeof(queued));has_result=true;return pdPASS;}
int xTaskCreatePinnedToCoreWithCaps(void (*fn)(void*),const char *name,unsigned stack,void *arg,unsigned priority,TaskHandle_t *task,int core,unsigned caps){(void)name;(void)stack;(void)arg;(void)priority;(void)core;(void)caps;worker=fn;*task=(void*)1;return pdPASS;}
void vTaskDelete(void *task){assert(!task && !s_task);}
void vTaskDelay(unsigned ticks){delays++;elapsed+=ticks;if(stop_on_delay){camera_stream_request_stop();stop_on_delay=false;}else if(camera_stop_requested())s_task=NULL;}
bool network_activity_controller_acquire_shared(uint32_t ms){assert(ms==3000);if(!stop_on_open)return false;assert(!shared);shared=true;return true;}
void network_activity_controller_end_shared(void){assert(shared);shared=false;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c){assert(c->timeout_ms==2500);return (void*)2;}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int n){assert(c==(void*)2 && n==0);unsigned before=delays;camera_stream_request_stop();assert(delays==before && shared && camera_stream_busy());return ESP_FAIL;}
int esp_http_client_fetch_headers(esp_http_client_handle_t c){(void)c;return 0;}
int esp_http_client_read(esp_http_client_handle_t c,char *p,int n){(void)c;(void)p;(void)n;return 0;}
esp_err_t esp_http_client_close(esp_http_client_handle_t c){(void)c;return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c){(void)c;return ESP_OK;}
bool camera_jpeg_decode_rgb565(const uint8_t *p,size_t n,uint16_t **out,int *w,int *h){(void)p;(void)n;(void)out;(void)w;(void)h;return false;}
static void offer(void){camera_frame_result_t f={.pixels=heap_caps_malloc(2,0),.pixel_size=2,.width=1,.height=1,.ok=true};assert(xQueueSend(s_result_queue,&f,0)==pdPASS);}
int main(void){
 assert(camera_stream_start("http://offline/stream"));assert(camera_stream_busy());offer();unsigned before=delays;camera_stream_request_stop();assert(delays==before && camera_stream_busy() && !has_result);
 assert(!camera_stream_start("http://other/stream"));assert(strcmp(s_url,"http://offline/stream")==0);
 /* A decode finishing after retirement cannot publish into the next page. */
 offer();uint8_t *p=NULL;assert(!camera_stream_take_result(&p,NULL,NULL,NULL,NULL) && !p && !has_result);
 worker(NULL);assert(!camera_stream_busy() && !has_result);
 assert(camera_stream_start("http://other/stream"));stop_on_delay=true;before=elapsed;worker(NULL);assert(elapsed-before<=50 && !camera_stream_busy());
 assert(camera_stream_start("http://stalled/stream"));stop_on_open=true;worker(NULL);assert(!camera_stream_busy() && !shared && !has_result);stop_on_open=false;
 /* Explicit OTA quiescence still waits for worker ownership to end. */
 assert(camera_stream_start("http://ota/stream"));before=delays;camera_stream_stop();assert(delays>before && !camera_stream_busy());
 assert(allocations==frees);puts("PASS: camera retirement is nonblocking, rejects late frames, prevents overlapping workers, interrupts retry backoff and preserves explicit transport quiescence");
}
#else
#include "offline_ws_extracted.inc"
static unsigned stopped, created, elapsed;
static uint32_t selected=1;
static bool switch_during_stop;
uint32_t moonraker_config_generation(void){return selected;}
int64_t esp_timer_get_time(void){return 10000;}
static void destroy_client(void){assert(in_runtime);stopped++;elapsed+=5000;s_client=NULL;s_connected=false;s_subscribed=false;if(switch_during_stop){selected++;moonraker_live_websocket_prepare_profile_change(selected);switch_during_stop=false;}}
static bool create_client(const char *host,int port,const char *key,uint32_t g){(void)port;(void)key;assert(in_runtime);created++;s_client=(void*)2;strcpy(s_host,host);s_generation=g;__atomic_store_n(&s_accepted_generation,g,__ATOMIC_RELEASE);return true;}
static bool send_identify(void){return true;}static bool send_object_list(void){return true;}static bool send_subscription(void){return true;}
int esp_websocket_client_send_text(void *c,const char *s,int n,unsigned ticks){(void)c;(void)s;(void)ticks;return n;}
static void tick(uint32_t generation){in_runtime=true;moonraker_live_websocket_tasklet(true,"new",7125,"",generation);in_runtime=false;}
int main(void){
 s_client=(void*)1;s_generation=s_accepted_generation=1;s_connected=s_subscribed=true;s_last_status_update_us=9999;s_file_change_pending=true;strcpy(s_host,"old");assert(moonraker_live_websocket_fresh(1000));
 selected=2;moonraker_live_websocket_prepare_profile_change(2);assert(stopped==0 && elapsed==0 && s_client);assert(!moonraker_live_websocket_connected() && !moonraker_live_websocket_subscribed() && !moonraker_live_websocket_fresh(1000));assert(!moonraker_live_websocket_take_file_change());
 tick(2);assert(stopped==1 && created==1 && s_generation==2);
 s_connected=s_subscribed=true;selected=3;moonraker_live_websocket_prepare_profile_change(3);switch_during_stop=true;tick(3);assert(stopped==2 && created==1 && selected==4 && !s_client);tick(4);assert(created==2 && s_generation==4);
 selected=5;moonraker_live_websocket_prepare_profile_change(5);tick(4);assert(created==2 && stopped==2);tick(5);assert(created==3 && stopped==3);
 puts("PASS: printer selection fences old connection without transport waits; runtime owns teardown/rebind and rapid selections reject stale snapshots");
}
#endif
