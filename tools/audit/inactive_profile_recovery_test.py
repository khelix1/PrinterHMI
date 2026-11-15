#!/usr/bin/env python3
"""Exercise production inactive-profile polling while the selected printer is offline."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--source", type=Path, default=root/"main/printer_profile_preview_worker.c")
parser.add_argument("--cjson-dir", type=Path, required=True)
args = parser.parse_args()
source = args.source.read_text()
def function(signature):
    start = source.index(signature)
    return source[start:source.index("\n}", start)+2]

fixture = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#define MOONRAKER_CONFIG_MAX_PROFILES 4
#define MOONRAKER_CONFIG_HOST_LENGTH 128
#define PROFILE_PREVIEW_CONFIRM_RETRY_US 3000000LL
#define PROFILE_PREVIEW_OFFLINE_RETRY_US 20000000LL
#define PROFILE_PREVIEW_FILENAME_LENGTH 160
#define PROFILE_PREVIEW_IMAGE_RETRY_US 15000000LL
#define PROFILE_PREVIEW_STATUS_AGE_US 2500000LL
#define PROFILE_PREVIEW_WIDTH 286
#define PROFILE_PREVIEW_HEIGHT 215
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE -2
#define ESP_LOGD(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
typedef int esp_err_t;
typedef struct {bool configured;char host[128];int port;char api_key[160];bool secure_transport;} moonraker_profile_t;
static moonraker_profile_t profiles[4]={{.configured=true,.host="offline.local",.port=7125},{.configured=true,.host="online.local",.port=7126},{.configured=true,.host="paused.local",.port=7127},{.configured=true,.host="idle.local",.port=7128}};
static int64_t now=1000000,s_preview_retry_after_us[4],s_image_retry_after_us[4];
typedef struct { int index; char file[PROFILE_PREVIEW_FILENAME_LENGTH]; } preview_candidate_t;
static uint32_t generation=1, revisions[4], s_retry_revision[4];
static uint32_t moonraker_config_connection_revision(int i){return revisions[i];}
static int index_to_poll=1,edit_during_request,cursor;
static bool sweep,incomplete_reply,drop_network,restore_network;
static uint32_t s_network_epoch=1,s_last_polled_network_epoch;
static unsigned requests_before_metadata;
static int reply_http_error;
static bool lane_busy;
static bool ws_connected,reply_ok=true,change_generation,health_known=true;
static unsigned requests,reports,metadata_requests;
static bool online[4];
static char states[4][32];
static const char *reply_state="printing";
static const char *klippy="ready";
static bool moonraker_live_websocket_running(void){return true;}
static bool moonraker_live_websocket_connected(void){return ws_connected;}
static int printer_profile_health_take_next_index(void){return sweep ? (index_to_poll=cursor++%4) : index_to_poll;}
static int64_t esp_timer_get_time(void){return now;}
static uint32_t moonraker_config_generation(void){return generation;}
static int moonraker_config_active_profile_index(void){return 0;}
static const moonraker_profile_t *moonraker_config_profile(int i){return &profiles[i];}
static size_t strlcpy(char*d,const char*s,size_t n){size_t len=strlen(s);if(n)snprintf(d,n,"%s",s);return len;}
static bool printer_profile_health_live_state_fresh(int i,int64_t age){(void)i;assert(age==2500000);return false;}
void printer_profile_preview_worker_set_network_ready(bool ready);
static bool moonraker_fetch_print_stats(const char*h,int port,const char*k,char*out,size_t n,int*code,esp_err_t*err){
 assert(!strcmp(h,profiles[index_to_poll].host)&&port==profiles[index_to_poll].port&&!strcmp(k,profiles[index_to_poll].api_key));
 requests++;
 if(drop_network)printer_profile_preview_worker_set_network_ready(false);
 if(restore_network)printer_profile_preview_worker_set_network_ready(true);
 if(change_generation)generation++;
 if(edit_during_request==1)strcpy(profiles[index_to_poll].host,"replacement.local");
 if(edit_during_request==2)profiles[index_to_poll].port++;
 if(edit_during_request==3)strcpy(profiles[index_to_poll].api_key,"new-key");
 if(edit_during_request==4)profiles[index_to_poll].secure_transport=true;
 if(edit_during_request==5)profiles[index_to_poll].configured=false;
 if(reply_http_error || lane_busy){*code=reply_http_error;*err=lane_busy?ESP_ERR_INVALID_STATE:ESP_FAIL;return false;}
 if(edit_during_request==6)revisions[index_to_poll]+=2;
 if(!reply_ok){*code=0;*err=ESP_FAIL;return false;}
 *code=200;
 if(incomplete_reply){snprintf(out,n,"{}");return true;}
 snprintf(out,n,"{\"result\":{\"status\":{\"webhooks\":{\"state\":\"%s\"},\"print_stats\":{\"state\":\"%s\",\"filename\":\"job.gcode\"}}}}",klippy,sweep && index_to_poll != 1 ? "standby" : reply_state);
 return true;
}
static void printer_profile_health_report_live_state(int i,bool up,const char*s){
 reports++;online[i]=up;snprintf(states[i],sizeof(states[i]),"%s",up&&s?s:"");
}
static bool printer_profile_health_get(int i,bool*known){if(known)*known=health_known;return online[i];}
static void printer_profile_health_set_live_state(int i,bool known,bool up,const char*s){assert(known);printer_profile_health_report_live_state(i,up,s);}
static void printer_profile_health_set_verifying(int i){online[i]=false;states[i][0]=0;}
static cJSON *moonraker_http_objects_parse(const char*text,size_t n,size_t max,const cJSON**status){
 assert(n<=max);cJSON*root=cJSON_Parse(text);*status=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(root,"result"),"status");return root;
}
static bool printer_preview_cache_matches(int i,const char*f){(void)i;(void)f;return false;}
static void thumbnail_manager_url_encode(const char*s,char*out,size_t n){strlcpy(out,s,n);}
static void *heap_caps_calloc(size_t n,size_t s,unsigned caps){(void)caps;return calloc(n,s);}
static void heap_caps_free(void*p){free(p);}
static bool moonraker_fetch_file_metadata(const char*h,int p,const char*k,const char*f,char*out,size_t n,int*c,esp_err_t*e){
 (void)h;(void)p;(void)f;(void)out;(void)n;(void)c;(void)e;assert(!strcmp(k,profiles[1].api_key));requests_before_metadata=requests;metadata_requests++;return false;
}
static bool moonraker_find_thumbnail_path(const char*m,const char*f,char*out,size_t n){(void)m;(void)f;(void)out;(void)n;return false;}
static bool moonraker_fetch_thumbnail_encoded_with_api_key(const char*h,int p,const char*k,const char*f,uint8_t**png,size_t*n){(void)h;(void)p;(void)f;(void)png;(void)n;assert(!strcmp(k,profiles[index_to_poll].api_key));return false;}
static bool bsp_display_lock(unsigned t){(void)t;return true;}
static void bsp_display_unlock(void){}
static bool printer_preview_cache_publish_png(int i,const char*h,int p,const char*f,uint8_t*png,size_t n,int w,int ht){
 (void)i;(void)h;(void)p;(void)f;(void)png;(void)n;(void)w;(void)ht;return false;
}
static void printer_preview_store_store_png(int i,const char*h,int p,const char*f,uint8_t*png,size_t n){(void)i;(void)h;(void)p;(void)f;(void)png;(void)n;}
'''
tests = r'''
int main(void){
 /* Activating an offline profile must not stop observations of its siblings. */
 strcpy(profiles[1].api_key,"printer-one-key");strcpy(profiles[2].api_key,"different-key");
 const char *expected[]={"","printing","paused","standby"};
 for(int i=1;i<4;i++){
  index_to_poll=i;reply_state=expected[i];printer_profile_preview_worker_poll_cycle();
  assert(requests==(unsigned)i&&online[i]&&!strcmp(states[i],expected[i]));
 }
 assert(!metadata_requests&&reports==3);
 index_to_poll=1;reply_ok=false;printer_profile_preview_worker_poll_cycle();
 assert(!online[1]&&s_preview_retry_after_us[1]>now);
 unsigned before=requests;printer_profile_preview_worker_poll_cycle();assert(requests==before);
 now+=21000000;reply_ok=true;printer_profile_preview_worker_poll_cycle();assert(online[1]);
 /* Existing generation fence discards an in-flight result after another switch. */
 unsigned old=reports;change_generation=true;index_to_poll=2;
 printer_profile_preview_worker_poll_cycle();assert(reports==old);change_generation=false;
 for(int edit=1;edit<=6;edit++){
  index_to_poll=2;moonraker_profile_t original=profiles[2];old=reports;
  edit_during_request=edit;printer_profile_preview_worker_poll_cycle();
  assert(reports==old);
  profiles[2]=original;edit_during_request=0;
 }
 index_to_poll=2;s_preview_retry_after_us[2]=now+20000000;health_known=false;
 unsigned old_requests=requests;printer_profile_preview_worker_poll_cycle();assert(requests==old_requests+1);
 health_known=true;
 ws_connected=true;index_to_poll=1;reply_state="printing";
 printer_profile_preview_worker_poll_cycle();assert(metadata_requests==1);
 index_to_poll=0;before=requests;printer_profile_preview_worker_poll_cycle();assert(requests==before);
 /* All eligible rows publish before the slow printing row's thumbnail. */
 now+=16000000;sweep=true;cursor=0;before=requests;
 printer_profile_preview_worker_poll_cycle();
 assert(requests==before+3&&requests_before_metadata==requests&&metadata_requests==2);
 before=requests;printer_profile_preview_worker_poll_cycle();
 assert(requests==before+3&&metadata_requests==2);
 sweep=false;index_to_poll=1;incomplete_reply=true;old=reports;
 printer_profile_preview_worker_poll_cycle();assert(reports==old&&online[1]);
 incomplete_reply=false;drop_network=true;
 printer_profile_preview_worker_poll_cycle();assert(reports==old&&online[1]);
 before=requests;printer_profile_preview_worker_poll_cycle();assert(requests==before);
 /* A failed request from the previous link must not overwrite live health. */
 printer_profile_preview_worker_set_network_ready(true);
 reply_ok=false;restore_network=true;
 printer_profile_preview_worker_poll_cycle();assert(reports==old&&online[1]);
 drop_network=restore_network=false;reply_ok=true;
 s_preview_retry_after_us[1]=now+20000000;
 before=requests;printer_profile_preview_worker_poll_cycle();
 assert(requests==before+1&&reports==old+1&&online[1]);
 /* An HTTP reply without usable status cannot prove a disconnect. */
 const int rejected[]={401,403,429,502,503,200};
 for(unsigned i=0;i<sizeof(rejected)/sizeof(rejected[0]);++i){
  now+=4000000;old=reports;before=requests;reply_http_error=rejected[i];
  printer_profile_preview_worker_poll_cycle();
  assert(requests==before+1&&reports==old&&online[1]);
  assert(s_preview_retry_after_us[1]==now+PROFILE_PREVIEW_CONFIRM_RETRY_US);
 }
 reply_http_error=0;lane_busy=true;now+=4000000;
 printer_profile_preview_worker_poll_cycle();assert(reports==old&&online[1]);
 lane_busy=false;reply_ok=false;now+=4000000;
 printer_profile_preview_worker_poll_cycle();assert(reports==old+1&&!online[1]);
 /* An edited slot must not inherit status or image backoff. */
 old=reports;before=requests;reply_ok=true;revisions[1]++;
 s_preview_retry_after_us[1]=s_image_retry_after_us[1]=now+20000000;
 unsigned images=metadata_requests;
 printer_profile_preview_worker_poll_cycle();
 assert(requests==before+1&&reports==old+1&&metadata_requests==images+1);
 puts("PASS: per-profile credentials, inactive-edit response fences, new-endpoint retry and offline-active sibling polling with normal preview behavior");
}
'''
with tempfile.TemporaryDirectory(prefix="inactive-profile-") as directory:
    tmp = Path(directory)
    src = tmp/"test.c"
    src.write_text(fixture+"\n"+function("static bool state_has_current_print(")+"\n"+
                   function("void printer_profile_preview_worker_reset(")+"\n"+
                   function("void printer_profile_preview_worker_set_network_ready(")+"\n"+
                   function("static bool profile_request_current(")+"\n"+
                   function("static void poll_profile(")+"\n"+
                   function("static void fetch_preview(")+"\n"+
                   function("void printer_profile_preview_worker_poll_cycle(")+"\n"+tests)
    exe = tmp/"test"
    subprocess.run(["cc", "-std=c11", "-D_POSIX_C_SOURCE=200809L", "-O1", "-g",
                    "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-pie", "-no-pie", "-I", str(args.cjson_dir), str(src),
                    str(args.cjson_dir/"cJSON.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True,
                   env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
