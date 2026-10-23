/* Process-local camera frame/test providers. Catalog and profile persistence
 * now use their unchanged firmware controllers against the NVS fixture. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "camera_catalog_controller.h"
#include "camera_stream_controller.h"
#include "camera_discovery_controller.h"
#include "camera_test_controller.h"
#include "moonraker_config_controller.h"
static bool streaming,stream_failed,discovery_pending,test_pending,test_ok;
static unsigned stream_camera;
static int discovery_profile;
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
bool camera_test_start(const char *url){if(!url||!url[0])return false;test_pending=true;test_ok=!strstr(url,"fail")&&!strstr(url,"unavailable");return true;}
bool camera_test_busy(void){return false;}
bool camera_test_take_result(bool *ok,int *w,int *h,size_t *bytes){if(!test_pending)return false;test_pending=false;*ok=test_ok;*w=480;*h=320;*bytes=480*320*2;return true;}

bool camera_discovery_start_for_profile(int p,const char *host,int port,const char *key){(void)host;(void)port;(void)key;if(p<0||p>=MOONRAKER_CONFIG_MAX_PROFILES)return false;discovery_profile=p;discovery_pending=true;return true;}
bool camera_discovery_busy(void){return false;}
bool camera_discovery_take_result(moonraker_webcam_t *out,bool *found,size_t *count){if(!discovery_pending)return false;discovery_pending=false;camera_catalog_set(discovery_profile,0,"Front camera","http://simulation.invalid/camera/1");camera_catalog_set(discovery_profile,1,"Side camera","http://simulation.invalid/camera/2");snprintf(out->name,sizeof(out->name),"Front camera");snprintf(out->stream_url,sizeof(out->stream_url),"http://simulation.invalid/camera/1");*found=true;*count=2;return true;}
