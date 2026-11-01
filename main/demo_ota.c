#include "demo_ota.h"
#include "firmware_identity.h"
#include "ui_ota_popup.h"
#include "ui_ota_release_browser.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>
static bool ready;
static lv_timer_t *progress;
static int percent;
static const ota_release_entry_t samples[]={
 {.name="Stable sample",.published="2026-10-29",.asset_url="https://example.invalid/stable.bin",.notes="Offline demo: no firmware is downloaded or installed.",.asset_size=3180000,.channel=OTA_RELEASE_CHANNEL_STABLE},
 {.version="nightly-2026-10-30-61266380e005",.name="Nightly sample",.published="2026-10-30",.asset_url="https://example.invalid/nightly.bin",.notes="Offline demo: no firmware is downloaded or installed.",.asset_size=3190000,.channel=OTA_RELEASE_CHANNEL_NIGHTLY}
};
bool demo_ota_catalog_start(void){ready=true;return true;}
void demo_ota_catalog_snapshot(ota_release_catalog_snapshot_t *out){if(out)*out=(ota_release_catalog_snapshot_t){.state=ready?OTA_RELEASE_CATALOG_READY:OTA_RELEASE_CATALOG_IDLE,.count=ready?2:0};}
bool demo_ota_catalog_entry(size_t i,ota_release_entry_t *out){
 if(!out || !ready || i>=2)return false;
 *out=samples[i];if(i==0)snprintf(out->version,sizeof(out->version),"v%u.%u.%u",7u,2u,0u);firmware_identity_t installed;firmware_identity_get(&installed);
 out->relation=(ota_release_relation_t)firmware_identity_compare(&installed,i?FIRMWARE_CHANNEL_NIGHTLY:FIRMWARE_CHANNEL_STABLE,out->version);return true;
}
bool demo_ota_running(void){return progress!=NULL;}
void demo_ota_stop(void){if(progress)lv_timer_delete(progress);progress=NULL;}
static void cancel(void){demo_ota_stop();ui_ota_progress_pump("Simulation cancelled. No firmware was changed.",percent,percent*31900,3190000,false);}
static void advance(lv_timer_t *timer){
 percent+=10;if(percent>100)percent=100;
 if(percent==100){progress=NULL;lv_timer_delete(timer);}
 ui_ota_progress_pump(percent==100?"Simulation complete. No firmware was changed.":"Simulating firmware transfer...",percent,percent*31900,3190000,percent<100);
}
static void start(const char *url){
 if(!url || !url[0] || progress)return;
 percent=0;progress=lv_timer_create(advance,200,NULL);
 if(!progress){ui_ota_start_failure("Unable to start simulation.");return;}
 ui_ota_progress_show(cancel);ui_ota_progress_pump("Simulating firmware transfer...",0,0,3190000,true);
}
static void custom(void){char identity[160];firmware_identity_format(identity,sizeof(identity));ui_ota_popup_show("https://example.invalid/sample.bin",identity,192,start,demo_ota_open);}
void demo_ota_open(void){if(progress)return;ui_ota_popup_quiesce_camera();ui_ota_release_browser_show(start,custom);}
