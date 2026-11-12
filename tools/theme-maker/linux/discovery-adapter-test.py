#!/usr/bin/env python3
"""Check the local scan timer adapter's ownership and running-state lifecycle."""
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[3]
adapter=root/'tools/theme-maker/linux/native/discovery_start.inc'
fixture=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct timer {void(*callback)(struct timer*);bool live;} lv_timer_t;
typedef struct {uint32_t addr;} esp_ip4_addr_t;
typedef struct {bool reachable,klippy_ready;char identity[64];} moonraker_probe_result_t;
static bool s_scan_running,s_cancelled,s_discovery_dispatching,s_has_scan_ip,fail_timer;
static uint32_t s_owner_generation=1,s_scan_epoch,s_discovery_epoch=1,generation=1;
static void *s_popup=(void*)1,*s_results_list=(void*)1;
static esp_ip4_addr_t s_scan_ip,s_retry_ip;
static unsigned candidates,timers;
static int mode;
static lv_timer_t pool[32];
static uint32_t moonraker_config_generation(void){return generation;}
static bool moonraker_discovery_is_cancelled(void){return s_cancelled||s_scan_epoch!=s_discovery_epoch||s_owner_generation!=generation;}
static int host_discovery_mode(void){return mode;}
static void moonraker_discovery_set_status(const char*s){assert(s&&*s);}
static void copy_text(char*out,size_t n,const char*s){snprintf(out,n,"%s",s);}
static bool moonraker_discovery_add_candidate(const char*h,int port,const moonraker_probe_result_t*p){assert(h&&port>0&&p->reachable);candidates++;return true;}
static lv_timer_t *lv_timer_create(void(*callback)(lv_timer_t*),unsigned delay,void*data){assert(delay==300&&!data);if(fail_timer)return NULL;for(unsigned i=0;i<32;i++)if(!pool[i].live){pool[i]=(lv_timer_t){callback,true};timers++;return &pool[i];}assert(false);return NULL;}
static void lv_timer_delete(lv_timer_t*t){assert(t&&t->live&&timers);t->live=false;timers--;}
/* ADAPTER */
static void complete(void){assert(host_scan_timer&&host_scan_timer->live);host_scan_timer->callback(host_scan_timer);}
int main(void){
 esp_ip4_addr_t ip={1};
 assert(moonraker_discovery_start(&ip)&&s_scan_running&&timers==1);
 assert(!moonraker_discovery_start(&ip)&&timers==1&&s_scan_running);
 complete();assert(!s_scan_running&&!timers&&candidates==2);
 mode=1;assert(moonraker_discovery_start(&ip));complete();assert(!s_scan_running&&!timers&&candidates==2);
 mode=2;assert(!moonraker_discovery_start(&ip)&&!s_scan_running&&!timers);
 mode=0;fail_timer=true;assert(!moonraker_discovery_start(&ip)&&!s_scan_running&&!timers);fail_timer=false;
 assert(moonraker_discovery_start(&ip));s_cancelled=true;s_discovery_epoch++;complete();assert(!s_scan_running&&!timers&&candidates==2);
 assert(moonraker_discovery_start(&ip));s_cancelled=true;s_discovery_epoch++;
 assert(moonraker_discovery_start(&ip)&&timers==1&&s_scan_running);complete();assert(!timers&&!s_scan_running&&candidates==4);
 assert(moonraker_discovery_start(&ip));generation++;complete();assert(!timers&&!s_scan_running&&candidates==4);
 s_owner_generation=generation;assert(moonraker_discovery_start(&ip));complete();assert(candidates==6&&!timers&&!s_scan_running);
 assert(!moonraker_discovery_start(NULL)&&!s_has_scan_ip&&!timers);
 puts("PASS: native discovery timer ownership, running flag, duplicate start, failure/retry, canceled close/reopen and stale generation");
}
'''.replace('/* ADAPTER */',adapter.read_text())
with tempfile.TemporaryDirectory(prefix='native-discovery-') as directory:
 p=Path(directory);(p/'test.c').write_text(fixture)
 subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-no-pie',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
