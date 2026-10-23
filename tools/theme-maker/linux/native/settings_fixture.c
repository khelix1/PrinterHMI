/* Hardware, custom-theme media and backup service fixtures. Actual Settings
 * layouts, preference controllers, validation and confirmation flows are shared. */
#include <stdio.h>
#include <string.h>
#include "ui_settings.h"
#include "custom_theme.h"
#include "settings_backup.h"
#include "settings_backup_encrypted_worker.h"
#include "ota_manager.h"
#include "nvs.h"
#include "operator_event_log.h"
void host_page_emit(const char *event);
bool host_backup_save(bool,const char *);
bool host_backup_exists(bool,const char *);
bool host_backup_restore(bool,const char *);
void host_backup_remove(void);
static bool display_fail;
static int display_percent=100;
int host_display_percent(void){return display_percent;}
void host_display_fail(bool fail){display_fail=fail;}
esp_err_t bsp_display_brightness_set(int percent){if(display_fail)return ESP_FAIL;display_percent=percent;return ESP_OK;}
esp_err_t bsp_display_backlight_off(void){return bsp_display_brightness_set(0);}
bool host_storage_erased(void);
static bool restart_pending;
void esp_restart(void){if(host_storage_erased())host_page_emit("@native-reset");else restart_pending=true;}
bool host_settings_take_restart(void){bool value=restart_pending;restart_pending=false;return value;}
bool ota_manager_is_running(void){return false;}
size_t custom_theme_scan_sd(void){return 0;}
size_t custom_theme_count(void){return 0;}
const custom_theme_summary_t *custom_theme_summary(size_t index){(void)index;return NULL;}
bool custom_theme_activate(size_t index){(void)index;return false;}
bool custom_theme_activate_id(const char *id){(void)id;return false;}
void custom_theme_deactivate(void){}
bool custom_theme_is_active(void){return false;}
const char *custom_theme_active_id(void){return "";}
const char *custom_theme_active_name(void){return "";}
bool custom_theme_remove(size_t index){(void)index;return false;}
static bool result(char *out,size_t size,bool ok,const char *good,const char *bad){snprintf(out,size,"%s",ok?good:bad);return ok;}
bool settings_backup_export_with_progress(settings_backup_progress_cb_t progress,void *user,char *out,size_t size){if(progress)progress(SETTINGS_BACKUP_STAGE_WRITING,user);return result(out,size,host_backup_save(false,NULL),"Local sample backup saved in memory.","Sample backup failed.");}
bool settings_backup_preflight(char *out,size_t size){return result(out,size,host_backup_exists(false,NULL),"Local sample configuration verified.","No sample plain backup is available. Create one first.");}
static bool restore(bool secret,const char *password,settings_backup_progress_cb_t progress,void *user,char *out,size_t size){if(progress)progress(SETTINGS_BACKUP_STAGE_APPLYING,user);return result(out,size,host_backup_restore(secret,password),"Sample settings restored. Restart the simulation to apply all preferences.","Sample restore failed or passphrase mismatch.");}
bool settings_backup_restore_with_progress(settings_backup_progress_cb_t cb,void *user,char *out,size_t size){return restore(false,NULL,cb,user,out,size);}
bool settings_backup_restore_encrypted_with_progress(const char *password,settings_backup_progress_cb_t cb,void *user,char *out,size_t size){return restore(true,password,cb,user,out,size);}
bool settings_backup_remove_all(char *out,size_t size){host_backup_remove();return result(out,size,true,"Local sample backups removed.","");}
static settings_backup_encrypted_worker_state_t worker_state;
static uint32_t worker_started;
static char worker_status[256];
bool settings_backup_encrypted_worker_start_export(const char *password,char *out,size_t size){bool ok=host_backup_save(true,password);worker_started=lv_tick_get();worker_state=ok?SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING:SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED;result(worker_status,sizeof(worker_status),ok,"Local sample password-protected backup saved in memory. No encryption is performed.","Sample backup failed.");snprintf(out,size,"%s",worker_status);return true;}
bool settings_backup_encrypted_worker_start_verify(const char *password,char *out,size_t size){bool ok=host_backup_exists(true,password);worker_started=lv_tick_get();worker_state=ok?SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING:SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED;result(worker_status,sizeof(worker_status),ok,"Local sample passphrase matched.","Sample backup unavailable or passphrase mismatch.");snprintf(out,size,"%s",worker_status);return true;}
settings_backup_encrypted_worker_state_t settings_backup_encrypted_worker_state(void){if((worker_state==SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING||worker_state==SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING)&&lv_tick_elaps(worker_started)>350)worker_state=SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED;return worker_state;}
void settings_backup_encrypted_worker_status(char *out,size_t size){snprintf(out,size,"%s",worker_status);}
