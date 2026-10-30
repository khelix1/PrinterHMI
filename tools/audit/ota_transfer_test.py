#!/usr/bin/env python3
"""Exercise the actual OTA worker with deterministic SDK/network faults.
No firmware is fetched or flashed. Target throughput must be measured separately.
"""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
stubs={
'lvgl.h':'#pragma once\n#include <stddef.h>\n',
'esp_err.h':'#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_TIMEOUT -2\nconst char *esp_err_to_name(esp_err_t);\n',
'esp_log.h':'#pragma once\n#include <stdio.h>\nvoid fixture_log(const char*,...);\n#define ESP_LOGI(t,...) do { (void)(t); fixture_log(__VA_ARGS__); } while(0)\n#define ESP_LOGW ESP_LOGI\n#define ESP_LOGE ESP_LOGI\n',
'esp_timer.h':'#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\n',
'esp_crt_bundle.h':'#pragma once\n#include "esp_err.h"\nesp_err_t esp_crt_bundle_attach(void*);\n',
'esp_http_client.h':'#pragma once\n#include <stdbool.h>\n#include "esp_err.h"\ntypedef void *esp_http_client_handle_t;\nesp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t,int);\ntypedef struct { const char *url; int timeout_ms; bool keep_alive_enable; esp_err_t(*crt_bundle_attach)(void*); int buffer_size; int buffer_size_tx; } esp_http_client_config_t;\n',
'esp_https_ota.h':'#pragma once\n#include "esp_http_client.h"\ntypedef void *esp_https_ota_handle_t;\ntypedef struct {const esp_http_client_config_t *http_config; esp_err_t(*http_client_init_cb)(esp_http_client_handle_t);} esp_https_ota_config_t;\n#define ESP_ERR_HTTPS_OTA_IN_PROGRESS 1\nesp_err_t esp_https_ota_begin(const esp_https_ota_config_t*,esp_https_ota_handle_t*);\nesp_err_t esp_https_ota_perform(esp_https_ota_handle_t);\nesp_err_t esp_https_ota_finish(esp_https_ota_handle_t);\nesp_err_t esp_https_ota_abort(esp_https_ota_handle_t);\nbool esp_https_ota_is_complete_data_received(esp_https_ota_handle_t);\nint esp_https_ota_get_image_len_read(esp_https_ota_handle_t);\nint esp_https_ota_get_image_size(esp_https_ota_handle_t);\n',
'esp_ota_ops.h':'#pragma once\n',
'esp_system.h':'#pragma once\nvoid esp_restart(void);\n',
'nvs.h':'#pragma once\n#include <stddef.h>\n#include "esp_err.h"\ntypedef unsigned nvs_handle_t;\n#define NVS_READONLY 0\n#define NVS_READWRITE 1\nesp_err_t nvs_open(const char*,int,nvs_handle_t*);\nesp_err_t nvs_get_str(nvs_handle_t,const char*,char*,size_t*);\nesp_err_t nvs_set_str(nvs_handle_t,const char*,const char*);\nesp_err_t nvs_commit(nvs_handle_t);\nvoid nvs_close(nvs_handle_t);\n',
'freertos/FreeRTOS.h':'#pragma once\n#include <stdint.h>\ntypedef int BaseType_t;\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define pdPASS 1\n#define pdMS_TO_TICKS(x) (x)\nvoid fixture_lock(portMUX_TYPE*);\nvoid fixture_unlock(portMUX_TYPE*);\n#define portENTER_CRITICAL fixture_lock\n#define portEXIT_CRITICAL fixture_unlock\n',
'freertos/task.h':'#pragma once\n#include "FreeRTOS.h"\nvoid vTaskDelay(unsigned);\nvoid vTaskDelete(void*);\nBaseType_t xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,void*);\n',
}
fixture=r'''
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include "ota_manager.c"
void fixture_log(const char *format,...){
 char rendered[1024];va_list args;va_start(args,format);vsnprintf(rendered,sizeof(rendered),format,args);va_end(args);
 assert(!strstr(rendered,"https://fixture/")&&!strstr(rendered,"?token="));
}
static int64_t clock_us;
static bool locked,task_accept=true,exclusive=true,ready=true,complete=true;
static int mode,reads,aborts,finishes,releases,reboots,pumps,closes,read_timeout;
static char status[160],transfer_text[112];
static int pumped_bytes,pumped_total,pumped_percent;
static jmp_buf reboot;
void fixture_lock(portMUX_TYPE *p){assert(p==&s_progress_lock&&!locked);locked=true;}
void fixture_unlock(portMUX_TYPE *p){assert(p==&s_progress_lock&&locked);locked=false;}
int64_t esp_timer_get_time(void){assert(!locked);return clock_us;}
void vTaskDelay(unsigned n){assert(!locked);clock_us+=n*1000;}
void vTaskDelete(void *p){(void)p;assert(!locked);}
BaseType_t xTaskCreate(void(*f)(void*),const char*n,unsigned stack,void*a,unsigned priority,void*out){(void)a;(void)out;assert(f==update_task&&stack==12288&&priority==5&&!strcmp(n,"ota_update"));return task_accept?pdPASS:0;}
const char *esp_err_to_name(esp_err_t e){(void)e;return "fixture failure";}
esp_err_t esp_crt_bundle_attach(void *p){(void)p;return ESP_OK;}
esp_err_t nvs_open(const char*n,int mode,nvs_handle_t*out){(void)n;(void)mode;(void)out;return ESP_FAIL;}
esp_err_t nvs_get_str(nvs_handle_t h,const char*k,char*v,size_t*n){(void)h;(void)k;(void)v;(void)n;return ESP_FAIL;}
esp_err_t nvs_set_str(nvs_handle_t h,const char*k,const char*v){(void)h;(void)k;(void)v;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
void operator_event_log_add(operator_event_level_t level,const char *format,...){(void)level;(void)format;assert(!locked);}
bool network_activity_controller_request_exclusive(void){return exclusive;}
bool network_activity_controller_exclusive_ready(void){return ready;}
void network_activity_controller_release_exclusive(void){releases++;}
void ota_release_catalog_snapshot(ota_release_catalog_snapshot_t *out){*out=(ota_release_catalog_snapshot_t){.state=OTA_RELEASE_CATALOG_READY};}
void ui_ota_progress_show(ui_ota_cancel_cb_t f){assert(f==ota_manager_cancel);}
void ui_ota_progress_close(void){closes++;}
void ui_ota_progress_pump(const char *s,int p,int b,int t,bool cancel){(void)cancel;assert(!locked);pumps++;snprintf(status,sizeof(status),"%s",s);pumped_percent=p;pumped_bytes=b;pumped_total=t;}
void ui_ota_progress_set_transfer(const char *s){assert(!locked);snprintf(transfer_text,sizeof(transfer_text),"%s",s);}
esp_err_t esp_https_ota_begin(const esp_https_ota_config_t *config,esp_https_ota_handle_t *out){
 assert(!locked&&config->http_config->buffer_size==4096&&config->http_config->buffer_size_tx==2048);
 assert(config->http_config->crt_bundle_attach==esp_crt_bundle_attach&&config->http_config->keep_alive_enable&&config->http_config->timeout_ms==15000);
 assert(!strcmp(config->http_config->url,"https://fixture/fw.bin"));
 assert(config->http_client_init_cb&&config->http_client_init_cb((void*)2)==ESP_OK);
 clock_us+=2000000;
 if(mode==1)return ESP_FAIL;
 *out=(void*)1;return ESP_OK;
}
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t h,int ms){assert(h==(void*)2&&!locked&&ms==1000&&reads==1);read_timeout=ms;return mode==10?ESP_FAIL:ESP_OK;}
esp_err_t esp_https_ota_perform(esp_https_ota_handle_t h){assert(h==(void*)1&&!locked);clock_us+=1000000;reads++;
 if(reads>1)assert(read_timeout==1000);
 if((mode==2||mode==7)&&reads==2)ota_manager_cancel();
 if(mode==9)return ESP_ERR_HTTPS_OTA_IN_PROGRESS;
 if(mode==7)return ESP_ERR_HTTPS_OTA_IN_PROGRESS;
 if(mode==8)return reads<6?ESP_ERR_HTTPS_OTA_IN_PROGRESS:ESP_OK;
 if(mode==3&&reads==2)return ESP_FAIL;
 return reads<4?ESP_ERR_HTTPS_OTA_IN_PROGRESS:ESP_OK;
}
int esp_https_ota_get_image_len_read(esp_https_ota_handle_t h){assert(h==(void*)1&&!locked);return (mode==7||mode==9?1:mode==8?(reads<4?1:reads-2):reads)*4096;}
int esp_https_ota_get_image_size(esp_https_ota_handle_t h){assert(h==(void*)1&&!locked);return mode==4?-1:16384;}
bool esp_https_ota_is_complete_data_received(esp_https_ota_handle_t h){assert(h==(void*)1);return complete;}
esp_err_t esp_https_ota_finish(esp_https_ota_handle_t h){assert(h==(void*)1&&!s_cancel_allowed);finishes++;ota_manager_cancel();assert(!s_cancel_requested);return mode==5?ESP_FAIL:ESP_OK;}
esp_err_t esp_https_ota_abort(esp_https_ota_handle_t h){assert(h==(void*)1);aborts++;return ESP_OK;}
void esp_restart(void){reboots++;longjmp(reboot,1);}
static void reset(int m){mode=m;read_timeout=0;reads=aborts=finishes=releases=reboots=pumps=closes=0;clock_us=1000000;complete=ready=exclusive=task_accept=true;s_task_running=s_cancel_requested=s_cancel_complete=false;}
int main(void){
 reset(0);char too_long[sizeof(s_task_url)+1];memset(too_long,'x',sizeof(too_long)-1);too_long[sizeof(too_long)-1]=0;
 assert(!ota_manager_start(too_long)&&!s_task_running&&!progress_snapshot().started_us);
 too_long[sizeof(s_task_url)-1]=0;assert(ota_manager_start(too_long)&&!strcmp(s_task_url,too_long));
 for(volatile int m=0;m<=6;m++){
  reset(m);if(m==6)complete=false;
  assert(ota_manager_start("https://fixture/fw.bin"));
  assert(!ota_manager_start("https://fixture/duplicate.bin"));
  assert(progress_snapshot().bytes==0&&!progress_snapshot().ended);
  if(!setjmp(reboot))update_task(NULL);
  ota_progress_snapshot_t p=progress_snapshot();assert(p.ended&&!locked);
  if(m==0||m==4){assert(reboots==1&&finishes==1&&!aborts&&p.bytes==16384);s_task_running=false;}
  else {assert(!reboots&&releases==1&&!s_task_running);}
  if(m==1)assert(!reads&&!aborts&&!finishes);
  if(m==2){assert(reads==2&&aborts==1&&!finishes&&s_cancel_complete);ota_manager_pump_ui();assert(closes==1&&!s_cancel_complete);}
  if(m==3||m==6)assert(aborts==1&&!finishes);
  if(m==5)assert(finishes==1&&!aborts);
  clock_us+=9000000;ota_manager_pump_ui();assert(pumped_bytes==p.bytes);
  assert(pumped_total==(m==4||m==1?0:16384));
  if(m==0||m==4)assert(strstr(transfer_text,"Average: 4.0 KiB/s | Elapsed: 6 s"));
  char frozen[112];snprintf(frozen,sizeof(frozen),"%s",transfer_text);clock_us+=5000000;ota_manager_pump_ui();assert(!strcmp(frozen,transfer_text));
 }
 reset(7);assert(ota_manager_start("https://fixture/fw.bin"));update_task(NULL);assert(reads==2&&aborts==1&&!finishes&&s_cancel_complete&&releases==1&&s_worker_http==NULL);
 reset(8);assert(ota_manager_start("https://fixture/fw.bin"));if(!setjmp(reboot))update_task(NULL);assert(reboots==1&&finishes==1&&reads==6&&!aborts&&s_worker_http==NULL);s_task_running=false;
 reset(10);assert(ota_manager_start("https://fixture/fw.bin"));update_task(NULL);assert(reads==1&&aborts==1&&!finishes&&!s_task_running&&releases==1&&s_worker_http==NULL);
 reset(9);assert(ota_manager_start("https://fixture/fw.bin"));update_task(NULL);assert(reads==16&&aborts==1&&!finishes&&!s_task_running&&releases==1&&s_worker_http==NULL);
 reset(0);task_accept=false;assert(!ota_manager_start("https://fixture/fw.bin"));assert(progress_snapshot().ended);ota_manager_pump_ui();assert(strstr(status,"Unable to start"));
 reset(0);exclusive=false;assert(ota_manager_start("https://fixture/fw.bin"));update_task(NULL);assert(!reads&&!aborts&&!releases&&!s_task_running);
 reset(0);ready=false;assert(ota_manager_start("https://fixture/fw.bin"));update_task(NULL);assert(!reads&&!aborts&&releases==1&&!s_task_running);
 reset(0);assert(ota_manager_start("https://fixture/fw.bin"));ota_manager_cancel();update_task(NULL);assert(!reads&&!reboots);ota_manager_pump_ui();assert(closes==1);
 puts("PASS: actual OTA worker buffer/TLS configuration, firmware counters, known/unknown size, transfer rate/frozen elapsed, restart, cancellation during a stalled short read, transient stalls, 15s no-progress deadline, timeout-setting failure, SDK failures, incomplete image, task failure, lane timeouts and snapshot lock boundaries");
}
'''
with tempfile.TemporaryDirectory(prefix='ota-transfer-') as directory:
    tmp=Path(directory)
    for name,text in stubs.items():
        p=tmp/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
    (tmp/'fixture.c').write_text(fixture)
    common=['cc','-D_POSIX_C_SOURCE=200809L','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wformat-truncation=2','-Wrestrict','-I',str(tmp),'-I',str(root/'main')]
    subprocess.run([*common,str(tmp/'fixture.c'),'-o',str(tmp/'worker')],check=True)
    subprocess.run([str(tmp/'worker')],check=True)
    subprocess.run([*common,'-c',str(root/'main/ota_manager.c'),'-o',str(tmp/'owner.o')],check=True)
