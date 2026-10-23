/* Process-local camera catalog and RGB565 sample frames. No sockets or files. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "camera_catalog_controller.h"
#include "camera_stream_controller.h"
#include "camera_discovery_controller.h"
#include "camera_test_controller.h"
#include "moonraker_config_controller.h"
#include "moonraker_endpoint_test.h"
#include "moonraker_transport_security_controller.h"
#include "printer_profile_health.h"
#include "printer_preview_cache.h"
#include "printer_preview_store.h"
static camera_catalog_entry_t cameras[CAMERA_CATALOG_MAX_CAMERAS];
static size_t default_camera;
static bool initialized,streaming,stream_failed,discovery_pending,test_pending,test_ok;
static unsigned stream_camera;
static moonraker_profile_t profile={.configured=true,.name="Sermoon D1",.host="simulation.invalid",.port=7125};
static void init(void){if(initialized)return;initialized=true;for(unsigned i=0;i<2;i++){
 snprintf(cameras[i].name,sizeof(cameras[i].name),i?"Side camera":"Front camera");
 snprintf(cameras[i].stream_url,sizeof(cameras[i].stream_url),"http://simulation.invalid/camera/%u",i+1);cameras[i].configured=true;
 }snprintf(profile.camera_stream_url,sizeof(profile.camera_stream_url),"%s",cameras[0].stream_url);}
size_t camera_catalog_count(int p){(void)p;init();size_t count=0;for(unsigned i=0;i<CAMERA_CATALOG_MAX_CAMERAS;i++)count+=cameras[i].configured;return count;}
bool camera_catalog_get(int p,size_t i,camera_catalog_entry_t *out){init();if(p!=0||i>=CAMERA_CATALOG_MAX_CAMERAS||!out)return false;*out=cameras[i];return cameras[i].configured;}
bool camera_catalog_set(int p,size_t i,const char *name,const char *url){init();if(p!=0||i>=CAMERA_CATALOG_MAX_CAMERAS||!name||!url||strlen(name)>=sizeof(cameras[i].name)||strlen(url)>=sizeof(cameras[i].stream_url))return false;snprintf(cameras[i].name,sizeof(cameras[i].name),"%s",name);snprintf(cameras[i].stream_url,sizeof(cameras[i].stream_url),"%s",url);cameras[i].configured=url[0]!=0;if(!i)snprintf(profile.camera_stream_url,sizeof(profile.camera_stream_url),"%s",url);return true;}
bool camera_catalog_clear(int p,size_t i){if(p!=0||i>=CAMERA_CATALOG_MAX_CAMERAS)return false;init();memset(&cameras[i],0,sizeof(cameras[i]));if(!i)profile.camera_stream_url[0]=0;if(default_camera==i){default_camera=0;for(unsigned j=0;j<CAMERA_CATALOG_MAX_CAMERAS;j++)if(cameras[j].configured){default_camera=j;break;}}return true;}
size_t camera_catalog_default(int p){(void)p;init();return default_camera;}
bool camera_catalog_set_default(int p,size_t i){init();if(p!=0||i>=CAMERA_CATALOG_MAX_CAMERAS||!cameras[i].configured)return false;default_camera=i;return true;}
bool camera_catalog_set_view(int p,size_t i,unsigned rotation,bool h,bool v){init();if(p!=0||i>=CAMERA_CATALOG_MAX_CAMERAS||rotation>270||rotation%90)return false;cameras[i].rotation=rotation;cameras[i].mirror_horizontal=h;cameras[i].mirror_vertical=v;return true;}
bool camera_stream_start(const char *url){if(!url||!url[0]||streaming)return false;streaming=true;stream_failed=strstr(url,"fail")||strstr(url,"unavailable");stream_camera=strstr(url,"/2")?1:0;return true;}
bool camera_stream_busy(void){return streaming;}
void camera_stream_request_stop(void){streaming=false;}
void camera_stream_stop(void){streaming=false;}
bool camera_stream_take_result(uint8_t **pixels,size_t *size,int *width,int *height,bool *ok){
 *pixels=NULL;*size=0;*width=*height=0;*ok=false;if(!streaming)return false;if(stream_failed)return true;
 *width=480;*height=320;*size=480*320*2;uint16_t *frame=malloc(*size);if(!frame)return true;
 for(int y=0;y<320;y++)for(int x=0;x<480;x++){
  uint16_t color=((x/40+y/40)%2)?0x2104:0x2945;
  if(y>210&&y<290&&x>55&&x<425)color=0x52aa;
  if(x>80&&x<98&&y>30&&y<270)color=0x7bef;
  if(x>382&&x<400&&y>30&&y<270)color=0x7bef;
  if(y>40&&y<55&&x>80&&x<400)color=0x7bef;
  if(x>=180&&x<240&&y>110+(240-x)/3&&y<235+(x-240)/3)color=stream_camera?0x4fdf:0xac3f;
  if(x>=240&&x<=300&&y>110+(x-240)/3&&y<235-(x-240)/3)color=stream_camera?0x2494:0x5ad8;
  if(x<55&&y<35)color=0xf800;
  if(x>420&&y>285)color=0x07e0;
  frame[y*480+x]=color;
 }*pixels=(uint8_t *)frame;*ok=true;return true;
}
const moonraker_profile_t *moonraker_config_profile(int p){init();return p==0?&profile:NULL;}
size_t moonraker_config_profile_capacity(void){return MOONRAKER_CONFIG_MAX_PROFILES;}
size_t moonraker_config_profile_count(void){return 1;}
const char *moonraker_config_host(void){return profile.host;}
int moonraker_config_port(void){return profile.port;}
const char *moonraker_config_api_key(void){return "";}
const char *moonraker_config_camera_stream_url(int p){init();return p==0?profile.camera_stream_url:"";}
bool moonraker_config_set_camera_stream_url(int p,const char *url){init();if(p!=0||!url||strlen(url)>=sizeof(profile.camera_stream_url))return false;snprintf(profile.camera_stream_url,sizeof(profile.camera_stream_url),"%s",url);return true;}
bool camera_test_start(const char *url){if(!url||!url[0])return false;test_pending=true;test_ok=!strstr(url,"fail")&&!strstr(url,"unavailable");return true;}
bool camera_test_busy(void){return false;}
bool camera_test_take_result(bool *ok,int *w,int *h,size_t *bytes){if(!test_pending)return false;test_pending=false;*ok=test_ok;*w=480;*h=320;*bytes=480*320*2;return true;}
bool camera_discovery_start_for_profile(int p,const char *host,int port,const char *key){(void)host;(void)port;(void)key;if(p!=0)return false;discovery_pending=true;return true;}
bool camera_discovery_busy(void){return false;}
bool camera_discovery_take_result(moonraker_webcam_t *out,bool *found,size_t *count){if(!discovery_pending)return false;discovery_pending=false;camera_catalog_set(0,0,"Front camera","http://simulation.invalid/camera/1");camera_catalog_set(0,1,"Side camera","http://simulation.invalid/camera/2");snprintf(out->name,sizeof(out->name),"Front camera");snprintf(out->stream_url,sizeof(out->stream_url),"http://simulation.invalid/camera/1");*found=true;*count=2;return true;}
/* Profile manager APIs are outside this host's entry points. */
bool moonraker_config_select_profile(int p){return p==0;}
bool moonraker_config_save_profile(int p,const char *n,const char *h,int port,const char *key){(void)p;(void)n;(void)h;(void)port;(void)key;return false;}
bool moonraker_config_delete_profile(int p){(void)p;return false;}
bool moonraker_config_set_transport_security(int p,bool secure){(void)p;return !secure;}
const char *moonraker_transport_security_ca_pem_for_profile(int p){(void)p;return NULL;}
bool moonraker_transport_security_import_ca_file_for_profile(int p,const char *path){(void)p;(void)path;return false;}
bool moonraker_endpoint_test_start(const char *h,int port,const char *key){(void)h;(void)port;(void)key;return false;}
bool moonraker_endpoint_test_busy(void){return false;}
bool moonraker_endpoint_test_take_result(moonraker_probe_result_t *out,bool *verified){(void)out;*verified=false;return false;}
void printer_preview_cache_invalidate(int p){(void)p;}
void printer_preview_store_invalidate(int p){(void)p;}
void printer_profile_health_reconcile(void){}
bool printer_profile_health_get(int p,bool *known){*known=p==0;return p==0;}

void printer_profile_health_set(int p,bool known,bool online){(void)p;(void)known;(void)online;}
void ui_printer_chooser_refresh(void){}
