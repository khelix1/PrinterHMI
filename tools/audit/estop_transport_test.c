#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "moonraker_command_route.h"
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define HTTP_METHOD_POST 1
#define WS_COMMAND_ID_FIRST 2000U
#define pdMS_TO_TICKS(n) (n)
#define ESP_LOGE(t,...) do{(void)(t);}while(0)
#define ESP_LOGW ESP_LOGE
#define ESP_LOGI ESP_LOGE
static const char TAG[]="fixture";
typedef int esp_err_t;
typedef void *esp_http_client_handle_t;
typedef struct {const char *url,*cert_pem;int method,timeout_ms;} esp_http_client_config_t;
static char storage[1024],wire[1024],url[256],body[640];
static char *s_command_buffer=storage;
static size_t s_command_capacity=sizeof(storage);
static void *s_client=(void*)1;
static uint32_t s_command_request_id=WS_COMMAND_ID_FIRST;
static bool connected=true,short_write,shared=true,init_ok=true,secure,api_header;
static int http_status=200,http_error,performs,cleanups,releases,timeout;
static bool ensure_command_buffer(void){return true;}
static bool moonraker_live_websocket_connected(void){return connected;}
static int esp_websocket_client_send_text(void*c,const char*t,int n,unsigned ticks){assert(c&&ticks==1000);assert(n<(int)sizeof(wire));memcpy(wire,t,n);wire[n]=0;return short_write?n-1:n;}
static bool moonraker_transport_security_build_http_url_for_endpoint(const char*h,int p,const char*path,char*out,size_t n,const char**ca){snprintf(out,n,"%s://%s:%d%s",secure?"https":"http",h,p,path);*ca=secure?"test-ca":NULL;return true;}
static void *esp_http_client_init(const esp_http_client_config_t*c){assert(c->method==HTTP_METHOD_POST);strcpy(url,c->url);timeout=c->timeout_ms;assert((c->cert_pem!=NULL)==secure);api_header=false;return init_ok?(void*)2:NULL;}
static int esp_http_client_set_header(void*c,const char*k,const char*v){assert(c);if(!strcmp(k,"X-Api-Key")){assert(!strcmp(v,"test-key"));api_header=true;}return ESP_OK;}
static int esp_http_client_set_post_field(void*c,const char*b,int n){assert(c&&n<(int)sizeof(body));memcpy(body,b,n);body[n]=0;return ESP_OK;}
static bool network_activity_controller_try_begin_shared(void){return shared;}
static void network_activity_controller_end_shared(void){assert(shared);releases++;}
static int esp_http_client_perform(void*c){assert(c);performs++;return http_error;}
static int esp_http_client_get_status_code(void*c){assert(c);return http_status;}
static int esp_http_client_cleanup(void*c){assert(c);cleanups++;return ESP_OK;}
#include "actual_estop_transport.c"
int main(void){
 const char *stop[]={"M112"," m112 \t","M112 ; stop","G4 P600\nM112\nG28"};
 for(unsigned i=0;i<4;i++){
  assert(!strcmp(moonraker_command_admin_method(stop[i]),"printer.emergency_stop"));
  assert(moonraker_live_websocket_send_gcode(stop[i]));assert(strstr(wire,"\"method\":\"printer.emergency_stop\""));assert(!strstr(wire,"params")&&!strstr(wire,"gcode.script"));
  for(unsigned tls=0;tls<2;tls++){secure=tls;shared=false;int code;esp_err_t err;int before=releases;
   assert(moonraker_send_gcode_script("printer.local",7126,"test-key",stop[i],&code,&err));
   assert(strstr(url,"/printer/emergency_stop")&&strstr(url,":7126/")&&!strcmp(body,"{}")&&api_header&&timeout==1500&&code==200&&err==ESP_OK&&releases==before);
  }
 }
 const char *ordinary[]={"G28","RESPOND MSG=\"M112\"",";M112","M1120","G28\n; M112"};
 shared=true;for(unsigned i=0;i<5;i++){assert(!moonraker_command_admin_method(ordinary[i]));assert(moonraker_live_websocket_send_gcode(ordinary[i]));assert(strstr(wire,"printer.gcode.script")&&strstr(wire,"params"));assert(moonraker_send_gcode_script("p",7125,"test-key",ordinary[i],NULL,NULL));assert(strstr(url,"/printer/gcode/script")&&strstr(body,"script"));}
 assert(moonraker_live_websocket_send_gcode("FIRMWARE_RESTART"));assert(strstr(wire,"printer.firmware_restart")&&!strstr(wire,"script"));
 assert(moonraker_send_gcode_script("p",7125,"test-key","FIRMWARE_RESTART",NULL,NULL));assert(strstr(url,"/printer/firmware_restart")&&!strcmp(body,"{}"));
 shared=false;int before=performs;assert(!moonraker_send_gcode_script("p",7125,"test-key","G28",NULL,NULL)&&performs==before);
 shared=true;http_status=401;assert(!moonraker_send_gcode_script("p",7125,"test-key","M112",NULL,NULL));http_status=200;http_error=ESP_FAIL;assert(!moonraker_send_gcode_script("p",7125,"test-key","M112",NULL,NULL));http_error=ESP_OK;
 init_ok=false;assert(!moonraker_send_gcode_script("p",7125,"test-key","M112",NULL,NULL));init_ok=true;
 connected=false;assert(!moonraker_live_websocket_send_gcode("M112"));connected=true;short_write=true;assert(!moonraker_live_websocket_send_gcode("M112"));
 assert(!moonraker_send_gcode_script("",7125,"test-key","M112",NULL,NULL));assert(cleanups==performs+1);
 puts("PASS: actual WS/HTTP emergency/recovery endpoints, M112 console scripts, ordinary-command isolation, TLS/auth/selected port, busy-slot bypass and transport failures");return 0;
}
