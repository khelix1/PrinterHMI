/* Local sample releases; no HTTP client, OTA writer or reboot is linked. */
#include "ota_release_catalog.h"
#include "firmware_identity.h"
#include "ui_ota_release_browser.h"
#include "ui_ota_popup.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#ifdef PRINTERHMI_DEMO_RUNTIME
#include "demo_ota.h"
bool ota_manager_is_running(void){return demo_ota_running();}
bool ota_release_catalog_start(void){return demo_ota_catalog_start();}
void ota_release_catalog_snapshot(ota_release_catalog_snapshot_t *out){demo_ota_catalog_snapshot(out);}
bool ota_release_catalog_entry(size_t i,ota_release_entry_t *out){return demo_ota_catalog_entry(i,out);}
void host_ota_show(void){demo_ota_open();}
void host_ota_close(void){demo_ota_stop();ui_ota_popup_close();ui_ota_progress_close();}
void host_ota_fixture(int value){(void)value;}
#else
static bool ready;
static lv_timer_t *progress;
static int percent;
bool ota_manager_is_running(void){return progress!=NULL;}
static int mode;
static const ota_release_entry_t samples[]={
 {.version="v7.2.0",.name="Stable sample",.published="2026-10-29",.asset_url="https://example.invalid/stable.bin",.notes="Local simulation. No firmware will be downloaded or installed.",.asset_size=3180000,.channel=OTA_RELEASE_CHANNEL_STABLE},
 {.version="nightly-2026-10-30-61266380e005",.name="Nightly sample",.published="2026-10-30",.asset_url="https://example.invalid/nightly.bin",.notes="Local development sample. This simulation cannot flash firmware.",.asset_size=3190000,.channel=OTA_RELEASE_CHANNEL_NIGHTLY}
};
bool ota_release_catalog_start(void){ready=true;return mode!=1;}
void ota_release_catalog_snapshot(ota_release_catalog_snapshot_t *out){*out=(ota_release_catalog_snapshot_t){.state=mode==1?OTA_RELEASE_CATALOG_ERROR:ready?OTA_RELEASE_CATALOG_READY:OTA_RELEASE_CATALOG_IDLE,.count=ready&&mode!=1?2:0};if(mode==1)snprintf(out->error,sizeof(out->error),"Sample release catalog unavailable. Retry.");}
bool ota_release_catalog_entry(size_t index,ota_release_entry_t *out){if(!out||index>=2||!ready||mode==1)return false;*out=samples[index];firmware_identity_t installed;firmware_identity_get(&installed);out->relation=(ota_release_relation_t)firmware_identity_compare(&installed,index?FIRMWARE_CHANNEL_NIGHTLY:FIRMWARE_CHANNEL_STABLE,out->version);return true;}
static void cancel(void){if(progress)lv_timer_delete(progress);progress=NULL;ui_ota_progress_pump("Simulation cancelled. No firmware was changed.",percent,percent*31900,3190000,false);}
static void advance(lv_timer_t *timer){(void)timer;percent+=10;if(percent>100)percent=100;ui_ota_progress_pump(percent==100?"Simulation complete. No firmware was changed.":"Simulating firmware transfer...",percent,percent*31900,3190000,percent<100);if(percent==100){lv_timer_delete(progress);progress=NULL;}}
static void start(const char *url){(void)url;percent=0;ui_ota_progress_show(cancel);ui_ota_progress_pump("Simulating firmware transfer...",0,0,3190000,true);progress=lv_timer_create(advance,200,NULL);}
void host_ota_show(void);
static void custom(void){char identity[160];firmware_identity_format(identity,sizeof(identity));ui_ota_popup_show("https://example.invalid/sample.bin",identity,192,start,host_ota_show);}
void host_ota_show(void){ui_ota_release_browser_show(start,custom);}
void host_ota_close(void){if(progress)lv_timer_delete(progress);progress=NULL;ui_ota_popup_close();ui_ota_progress_close();}
void host_ota_fixture(int value){mode=value;ready=false;}

#endif
