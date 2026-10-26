#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_client.h"
#include "gcode_viewer_loader.h"
static unsigned generation=1,allocations,callbacks,errors,requests,reads,delays;
static bool fail_replay_copy,cancel_during_delay,switch_during_endpoint,verify_key_erasure;
static const char key_marker[]="PRIVATE_API_KEY_ERASURE_CHECK_7123";
static struct {void *p;size_t bytes;} heap_blocks[64];
static bool shared_allowed=true,exclusive,locked,display_allowed=true;
static int fail,code=200;
static char api[160]="secret",ca[4096]="original-ca";
static char url_seen[1024],key_seen[160],ca_seen[4096];
static void (*task)(void *);static void *job;
static gcode_toolpath_t *adopted;
static const char *body="G90\nM83\nG1 X10 Y10 Z.2\nG1 X20 E1\nG1 Y20 E1\n";
static size_t offset;
void *test_calloc(size_t n,size_t s){if(fail_replay_copy&&n==2&&s==sizeof(gcode_segment_t))return NULL;if(fail==1||(fail==14&&s>900000)||(fail==15&&s>sizeof(gcode_toolpath_t)+65535*sizeof(gcode_segment_t))||(fail==16&&s>sizeof(gcode_toolpath_t)+131072*sizeof(gcode_segment_t))||(fail==17&&s>sizeof(gcode_toolpath_t)+65536*sizeof(gcode_segment_t)))return NULL;void *p=calloc(n,s);if(p){allocations++;unsigned i;for(i=0;i<64&&heap_blocks[i].p;i++);assert(i<64);heap_blocks[i].p=p;heap_blocks[i].bytes=n*s;}return p;}
void test_free(void *p){if(p){assert(allocations);unsigned i;for(i=0;i<64&&heap_blocks[i].p!=p;i++);assert(i<64);if(verify_key_erasure){size_t n=sizeof(key_marker)-1;for(size_t k=0;k+n<=heap_blocks[i].bytes;k++)assert(memcmp((unsigned char *)p+k,key_marker,n));}heap_blocks[i].p=NULL;allocations--;free(p);}}
uint32_t moonraker_config_generation(void){return generation;}
const char *moonraker_config_host(void){return "printer.local";}
int moonraker_config_port(void){return 443;}
const char *moonraker_config_api_key(void){if(switch_during_endpoint){switch_during_endpoint=false;generation++;}return api;}
bool moonraker_transport_security_build_http_url_for_endpoint(const char *h,int port,const char *path,char *out,size_t n,const char **pem){assert(!strcmp(h,"printer.local")&&port==443);snprintf(out,n,"https://printer.local%s",path);*pem=ca;return fail!=2;}
bool network_activity_controller_acquire_shared(uint32_t timeout){assert(timeout==1500);if(!shared_allowed)return false;assert(!locked);locked=true;return true;}
void network_activity_controller_end_shared(void){assert(locked);locked=false;}
bool network_activity_controller_exclusive_requested(void){return exclusive;}
bool bsp_display_lock(unsigned timeout){assert(timeout==1500);return display_allowed;}
void bsp_display_unlock(void){}
int64_t esp_timer_get_time(void){return fail==13&&reads?65000000:0;}
int xTaskCreatePinnedToCore(void (*f)(void *),const char *name,unsigned stack,void *p,int priority,void *handle,int core){(void)name;(void)priority;(void)handle;(void)core;assert(stack==8192);if(fail==3)return 0;assert(!task);task=f;job=p;return 1;}
void vTaskDelete(void *p){assert(!p);}
void vTaskDelay(unsigned n){(void)n;delays++;if(cancel_during_delay)gcode_viewer_loader_cancel();}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config){assert(config->disable_auto_redirect&&config->method==HTTP_METHOD_GET);assert(config->timeout_ms==3000);strcpy(url_seen,config->url);strcpy(ca_seen,config->cert_pem);requests++;return fail==4?NULL:(void*)1;}
int esp_http_client_set_header(esp_http_client_handle_t c,const char *name,const char *value){(void)c;assert(!strcmp(name,"X-Api-Key"));strcpy(key_seen,value);return fail==5?-1:0;}
int esp_http_client_open(esp_http_client_handle_t c,int n){(void)c;assert(!n);return fail==6?-1:0;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t c){(void)c;return fail==7?-1:0;}
int esp_http_client_get_status_code(esp_http_client_handle_t c){(void)c;return code;}
int esp_http_client_read(esp_http_client_handle_t c,char *out,int n){(void)c;reads++;if(fail==8)return -1;if(fail==9)generation++;if(fail==10)gcode_viewer_loader_cancel();if(fail==11)exclusive=true;size_t remaining=strlen(body)-offset;if(remaining>(size_t)n)remaining=(size_t)n;if(remaining>7)remaining=7;memcpy(out,body+offset,remaining);offset+=remaining;return (int)remaining;}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c){(void)c;return fail!=12&&offset==strlen(body);}
int esp_http_client_close(esp_http_client_handle_t c){(void)c;return 0;}
int esp_http_client_cleanup(esp_http_client_handle_t c){(void)c;return 0;}
static bool ready(gcode_toolpath_t *model,const char *error){callbacks++;if(error){assert(!model);errors++;return false;}assert(model);if(fail==13)assert(model->bounded&&strstr(model->stop_reason,"60 second"));else assert(model->count==2);if(fail==15)assert(model->capacity==32768);if(fail==16)assert(model->capacity==131072);if(fail==17)assert(model->capacity==65536);if(!fail)assert(model->capacity==262144);adopted=model;return true;}
static void run(void){assert(task);void (*f)(void*)=task;void *p=job;task=NULL;job=NULL;f(p);assert(!locked&&!gcode_viewer_loader_running());if(adopted){gcode_viewer_loader_free(adopted);adopted=NULL;}assert(!allocations);offset=0;reads=0;}
int main(void){
 assert(gcode_viewer_loader_start("folder/a b#?.gcode",ready));assert(!gcode_viewer_loader_start("second.gcode",ready));strcpy(api,"changed");strcpy(ca,"changed-ca");run();assert(callbacks==1&&!errors);gcode_viewer_progress_t snapshot=gcode_viewer_loader_progress();assert(!snapshot.running&&snapshot.bytes==strlen(body)&&snapshot.paths==2&&snapshot.layers==1);assert(!strcmp(url_seen,"https://printer.local/server/files/gcodes/folder/a%20b%23%3F.gcode"));assert(!strcmp(key_seen,"secret")&&!strcmp(ca_seen,"original-ca"));
 const char *bad[]={"","/file.gcode","../file.gcode","folder/../file.gcode","a//b","a/","a\\b","."};for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)assert(!gcode_viewer_loader_start(bad[i],ready));assert(!allocations);
 for(fail=1;fail<=14;fail++){
  unsigned prior=callbacks,prior_errors=errors;bool started=gcode_viewer_loader_start("file.gcode",ready);
  if(fail<=3){assert(!started&&!allocations);continue;}assert(started);run();
  if(fail==9||fail==10)assert(callbacks==prior);else if(fail==13)assert(callbacks==prior+1&&errors==prior_errors);else assert(callbacks==prior+1&&errors==prior_errors+1);
  exclusive=false;
 }
 for(fail=15;fail<=17;fail++){unsigned prior=errors;assert(gcode_viewer_loader_start("file.gcode",ready));run();assert(errors==prior);}
 fail=0;unsigned before=requests;assert(gcode_viewer_loader_start("file.gcode",ready));generation++;run();assert(requests==before);
 before=callbacks;assert(gcode_viewer_loader_start("file.gcode",ready));gcode_viewer_loader_cancel();assert(!gcode_viewer_loader_progress().running&&!gcode_viewer_loader_progress().bytes);run();assert(callbacks==before);
 shared_allowed=false;assert(gcode_viewer_loader_start("file.gcode",ready));run();shared_allowed=true;
 code=302;assert(gcode_viewer_loader_start("file.gcode",ready));run();code=200;
 display_allowed=false;before=callbacks;assert(gcode_viewer_loader_start("file.gcode",ready));run();assert(callbacks==before);
 assert(!allocations&&!locked);puts("PASS: snapshot TLS/API key, exact encoded path, GET/no redirect, single flight, traversal, allocation/task/HTTP failures, incomplete response, cancellation, profile changes, exclusivity and complete cleanup");
 return 0;
}
