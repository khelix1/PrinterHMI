/* Process-local camera frame/test providers. Catalog and profile persistence
 * now use their unchanged firmware controllers against the NVS fixture. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "demo_camera.h"
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
 demo_camera_draw(frame,stream_camera != 0);
 *pixels=(uint8_t *)frame;*ok=true;return true;
}
bool camera_test_start(const char *url){if(!url||!url[0])return false;test_pending=true;test_ok=!strstr(url,"fail")&&!strstr(url,"unavailable");return true;}
bool camera_test_busy(void){return false;}
bool camera_test_take_result(bool *ok,int *w,int *h,size_t *bytes){if(!test_pending)return false;test_pending=false;*ok=test_ok;*w=480;*h=320;*bytes=480*320*2;return true;}

bool camera_discovery_start_for_profile(int p,const char *host,int port,const char *key){(void)host;(void)port;(void)key;if(p<0||p>=MOONRAKER_CONFIG_MAX_PROFILES)return false;discovery_profile=p;discovery_pending=true;return true;}
bool camera_discovery_busy(void){return false;}
bool camera_discovery_take_result(moonraker_webcam_t *out,bool *found,size_t *count){if(!discovery_pending)return false;discovery_pending=false;camera_catalog_set(discovery_profile,0,"Front camera","http://simulation.invalid/camera/1");camera_catalog_set(discovery_profile,1,"Side camera","http://simulation.invalid/camera/2");snprintf(out->name,sizeof(out->name),"Front camera");snprintf(out->stream_url,sizeof(out->stream_url),"http://simulation.invalid/camera/1");*found=true;*count=2;return true;}
