#!/usr/bin/env python3
"""Compile production endpoint verification and live HTTP polling with fault injection."""
from pathlib import Path
import argparse, os, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--probe-source',type=Path,default=root/'main/moonraker_probe.c')
parser.add_argument('--live-source',type=Path,default=root/'main/moonraker_live_transport.c')
args=parser.parse_args()
headers={
'esp_err.h':'#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_INVALID_STATE -2\nconst char *esp_err_to_name(int);\n',
'esp_heap_caps.h':'#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\nvoid *heap_caps_malloc(size_t,unsigned);\n',
'esp_log.h':'#pragma once\n#define ESP_LOGE(t,...) ((void)(t))\n#define ESP_LOGW ESP_LOGE\n',
'lwip/inet.h':'#pragma once\n#include <arpa/inet.h>\n',
'lwip/sockets.h':'#pragma once\n#include <sys/socket.h>\n#include <sys/select.h>\n#include <netinet/in.h>\n',
'esp_http_client.h':'''#pragma once
#include <stdbool.h>
#include "esp_err.h"
typedef void *esp_http_client_handle_t;
#define HTTP_EVENT_ON_DATA 1
#define HTTP_METHOD_GET 0
typedef struct {void *user_data;int event_id;const char *data;int data_len;} esp_http_client_event_t;
typedef struct {bool disable_auto_redirect;const char *url,*cert_pem;int method,timeout_ms;esp_err_t (*event_handler)(esp_http_client_event_t*);void *user_data;} esp_http_client_config_t;
void *esp_http_client_init(const esp_http_client_config_t*);
int esp_http_client_set_header(void*,const char*,const char*);
int esp_http_client_perform(void*);
int esp_http_client_get_status_code(void*);
int esp_http_client_cleanup(void*);
'''}
fixture=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
size_t strlcpy(char *d,const char *s,size_t n){size_t z=strlen(s);if(n){size_t c=z<n-1?z:n-1;memcpy(d,s,c);d[c]=0;}return z;}
#include "probe_under_test.c"
#include "live_under_test.c"
static unsigned created,cleaned,performed,begun,ended;
static bool fail_header,fail_init,fail_alloc,busy,secure=true;
static int status=200,perform_error;
static const char *server="{\"result\":{\"klippy_state\":\"ready\"}}";
static const char *printer="{\"result\":{\"state\":\"ready\",\"hostname\":\"Workshop\\\"printer\"}}";
static const char *live="{\"result\":{}}";
static size_t server_size,live_size;
static int printer_status=200;
static esp_http_client_config_t request;
static char request_url[1024];
void *heap_caps_malloc(size_t n,unsigned caps){(void)caps;return fail_alloc?NULL:malloc(n);}
const char *esp_err_to_name(int e){(void)e;return "fixture";}
bool moonraker_transport_security_build_http_url_for_endpoint(const char *h,int p,const char *path,char *out,size_t n,const char **ca){*ca=secure?"test-ca":NULL;int z=snprintf(out,n,"%s://%s:%d%s",secure?"https":"http",h,p,path);return z>0&&(size_t)z<n;}
bool network_activity_controller_acquire_shared(uint32_t ms){assert(ms==900);if(busy)return false;begun++;return true;}
bool network_activity_controller_try_begin_shared(void){if(busy)return false;begun++;return true;}
void network_activity_controller_end_shared(void){ended++;}
void *esp_http_client_init(const esp_http_client_config_t *c){assert(c->disable_auto_redirect&&c->method==HTTP_METHOD_GET);assert((c->cert_pem!=NULL)==secure);if(fail_init)return NULL;created++;request=*c;strlcpy(request_url,c->url,sizeof(request_url));return (void*)1;}
int esp_http_client_set_header(void *c,const char *k,const char *v){assert(c&&!strcmp(k,"X-Api-Key")&&!strcmp(v,"owner-key"));return fail_header?ESP_FAIL:ESP_OK;}
int esp_http_client_perform(void *c){assert(c);performed++;const char *body;size_t n;
 if(strstr(request_url,"/server/info")){body=server;n=server_size?server_size:strlen(body);}
 else if(strstr(request_url,"/printer/info")){body=printer;n=strlen(body);}
 else {body=live;n=live_size?live_size:strlen(body);}
 /* Split response bytes across callbacks, including NUL/limit boundaries. */
 for(size_t i=0;i<n;){size_t count=n-i;if(count>37)count=37;esp_http_client_event_t e={request.user_data,HTTP_EVENT_ON_DATA,body+i,(int)count};assert(request.event_handler(&e)==ESP_OK);i+=count;}
 return perform_error;
}
int esp_http_client_get_status_code(void *c){assert(c);return strstr(request_url,"/printer/info")?printer_status:status;}
int esp_http_client_cleanup(void *c){assert(c);cleaned++;return ESP_OK;}
static bool probe(moonraker_probe_result_t *r){return moonraker_probe_endpoint_with_api_key("printer.local",7126,"owner-key",r);}
int main(void){
 moonraker_probe_result_t r;
 assert(probe(&r)&&r.reachable&&r.klippy_ready&&!strcmp(r.identity,"Workshop\"printer"));
 assert(strstr(request_url,":7126/"));
 printer="{\"result\":{\"state\":\"shutdown\",\"hostname\":\"Stopped\"}}";
 assert(probe(&r)&&!r.klippy_ready&&!strcmp(r.identity,"Stopped"));
 printer_status=503;assert(probe(&r)&&r.klippy_ready&&!strcmp(r.identity,"Moonraker"));printer_status=200;
 server="{\"result\":{\"klippy_state\":\"disconnected\"}}";
 assert(probe(&r)&&!r.klippy_ready);
 printer="{\"result\":{\"state\":\"ready\",\"hostname\":\"Recovered\"}}";
 assert(probe(&r)&&r.klippy_ready&&!strcmp(r.identity,"Recovered"));
 const char *bad[]={"<html>moonraker klippy \"result\"</html>","{\"result\":{}}","{\"result\":[]}","{\"result\":{\"klippy_state\":\"\"}}","{\"result\":{\"klippy_state\":true}}","{\"result\":{\"klippy_state\":\"ready\"}} junk","{\"error\":{},\"result\":{\"klippy_state\":\"ready\"}}"};
 for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++){server=bad[i];assert(!probe(&r));}

 char nested[256];const char *prefix="{\"result\":{\"klippy_state\":\"ready\",\"nested\":";
 size_t at=strlen(prefix);memcpy(nested,prefix,at);for(unsigned i=0;i<17;i++)nested[at++]='[';
 for(unsigned i=0;i<17;i++){nested[at++]=']';}
 nested[at++]='}';nested[at++]='}';nested[at]=0;
 server=nested;assert(!probe(&r));
 char large[9000];memset(large,' ',sizeof(large));const char *valid="{\"result\":{\"klippy_state\":\"ready\"}}";memcpy(large,valid,strlen(valid));
 server=large;server_size=1000;assert(probe(&r)); /* Ordinary server info exceeds the old 384-byte prefix. */
 server_size=sizeof(large);assert(!probe(&r));
 server=valid;server_size=strlen(valid)+1;assert(!probe(&r));server_size=0;
 fail_header=true;unsigned sent=performed,owned=begun;assert(!probe(&r)&&performed==sent&&begun==owned);fail_header=false;
 fail_alloc=true;sent=performed;assert(!probe(&r)&&performed==sent);fail_alloc=false;
 fail_init=true;assert(!probe(&r));fail_init=false;
 busy=true;sent=performed;assert(!probe(&r)&&performed==sent);busy=false;
 status=401;assert(!probe(&r));status=200;perform_error=ESP_FAIL;assert(!probe(&r));perform_error=ESP_OK;

 moonraker_webcam_t cameras[2]={0};
 live="{\"result\":{\"webcams\":[{\"name\":\"Workshop\",\"enabled\":true,\"stream_url\":\"/webcam/stream\"}]}}";
 assert(moonraker_probe_webcams_with_api_key("printer.local",7126,"owner-key",cameras,2)==1);
 assert(!strcmp(cameras[0].stream_url,"http://printer.local/webcam/stream"));
 live_size=strlen(live)+1;assert(!moonraker_probe_webcams_with_api_key("printer.local",7126,"owner-key",cameras,2));
 live_size=0;live="{\"result\":{\"webcams\":[]}} junk";
 assert(!moonraker_probe_webcams_with_api_key("printer.local",7126,"owner-key",cameras,2));

 /* A reused output must contain only this scan's accepted camera fields. */
 memset(cameras,0x55,sizeof(cameras));
 live="{\"result\":{\"webcams\":[{\"stream_url\":\"rtsp://camera/stream\"},{\"stream_url\":\"/good\"}]}}";
 assert(moonraker_probe_webcams_with_api_key("printer.local",7126,"owner-key",cameras,2)==1);
 assert(!strcmp(cameras[0].stream_url,"http://printer.local/good")&&!cameras[0].snapshot_url[0]);
 char camera_json[2048],camera_url[256];memset(camera_url,'a',sizeof(camera_url));
 memcpy(camera_url,"http://camera/",14);camera_url[191]=0;
 snprintf(camera_json,sizeof(camera_json),"{\"result\":{\"webcams\":[{\"stream_url\":\"%s\"}]}}",camera_url);live=camera_json;
 assert(moonraker_probe_webcams_with_api_key("printer.local",7126,"owner-key",cameras,2)==1);
 assert(strlen(cameras[0].stream_url)==191&&!strcmp(cameras[0].stream_url,camera_url));
 camera_url[191]='a';camera_url[192]=0;
 snprintf(camera_json,sizeof(camera_json),"{\"result\":{\"webcams\":[{\"stream_url\":\"%s\"},{\"stream_url\":\"/good\",\"snapshot_url\":\"%s\"}]}}",camera_url,camera_url);
 assert(moonraker_probe_webcams_with_api_key("printer.local",7126,"owner-key",cameras,2)==1);
 assert(!strcmp(cameras[0].stream_url,"http://printer.local/good")&&!cameras[0].snapshot_url[0]);
 memset(camera_url,'a',230);camera_url[0]='/';camera_url[230]=0;
 snprintf(camera_json,sizeof(camera_json),"{\"webcams\":[{\"stream_url\":\"/ok\",\"snapshot_url\":\"%s\"}]}",camera_url);
 assert(moonraker_probe_webcams_with_api_key("printer.local",7126,"owner-key",cameras,2)==1&&!cameras[0].snapshot_url[0]);
 live="{\"error\":{},\"result\":{\"webcams\":[{\"stream_url\":\"/bad\"}]}}";
 assert(!moonraker_probe_webcams_with_api_key("printer.local",7126,"owner-key",cameras,2));
 live="{\"result\":{\"webcams\":[{\"enabled\":false,\"stream_url\":\"/disabled\"},{\"stream_url\":\"file:///tmp/f\"},{\"stream_url\":\"/stream?action=snapshot&next=http://local\"}]}}";
 assert(moonraker_probe_webcams_with_api_key("printer.local",7126,"owner-key",cameras,1)==1);
 assert(strstr(cameras[0].stream_url,"next=http://local"));
 live="{\"result\":{}}";
 char out[128];size_t len;int code;
 for(unsigned tls=0;tls<2;tls++){secure=tls;assert(moonraker_live_transport_fetch("printer.local",7126,"owner-key",out,sizeof(out),&len,&code)&&code==200&&len==strlen(live));}
 fail_header=true;sent=performed;owned=begun;strcpy(out,"stale");len=99;code=200;
 assert(!moonraker_live_transport_fetch("p",7125,"owner-key",out,sizeof(out),&len,&code));assert(!out[0]&&!len&&!code&&performed==sent&&begun==owned);fail_header=false;
 live="{}\0junk";live_size=7;assert(!moonraker_live_transport_fetch("p",7125,NULL,out,sizeof(out),&len,&code)&&!len);
 live=large;live_size=sizeof(large);assert(!moonraker_live_transport_fetch("p",7125,NULL,out,sizeof(out),&len,&code)&&!len);
 live="{}";live_size=0;assert(moonraker_live_transport_fetch("p",7125,NULL,out,sizeof(out),&len,&code)&&len==2);
 assert(created==cleaned&&begun==ended);
 puts("PASS: camera URL boundaries/scheme rejection/clean result slots; production endpoint schema/identity/readiness, full bounded JSON, truncation/NUL/header refusal, live capture/auth, selected port/TLS, cleanup and retries");
}
'''
with tempfile.TemporaryDirectory(prefix='probe-transport-') as directory:
 p=Path(directory)
 for name,data in headers.items():
  f=p/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text(data)
 (p/'probe_under_test.c').write_text(args.probe_source.read_text())
 (p/'live_under_test.c').write_text(args.live_source.read_text())
 (p/'test.c').write_text(fixture)
 cjson=root/'tools/theme-maker/linux/native/vendor/cjson'
 subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-I',str(p),'-I',str(root/'main'),'-I',str(cjson),str(p/'test.c'),str(cjson/'cJSON.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
