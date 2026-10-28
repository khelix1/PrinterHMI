/* Actual onboarding persistence with transactional NVS fault/reboot fixtures. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "onboarding_controller.c"
static bool has_complete,has_steps,fail_open,fail_set,fail_commit;
static uint8_t disk_complete,disk_steps,stage_complete,stage_steps;
static bool stage_has_complete,stage_has_steps;
static unsigned opens,writes,commits;
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle)
{
 assert(!strcmp(name,"onboard"));(void)mode;opens++;
 if(fail_open)return ESP_FAIL;
 *handle=1;stage_complete=disk_complete;stage_steps=disk_steps;
 stage_has_complete=has_complete;stage_has_steps=has_steps;return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t handle,const char *key,uint8_t *value)
{
 assert(handle==1);
 if(!strcmp(key,"complete")){if(!has_complete)return ESP_ERR_NVS_NOT_FOUND;*value=disk_complete;}
 else {assert(!strcmp(key,"steps"));if(!has_steps)return ESP_ERR_NVS_NOT_FOUND;*value=disk_steps;}
 return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t handle,const char *key,uint8_t value)
{
 assert(handle==1);writes++;if(fail_set)return ESP_FAIL;
 if(!strcmp(key,"complete")){stage_complete=value;stage_has_complete=true;}
 else {assert(!strcmp(key,"steps"));stage_steps=value;stage_has_steps=true;}
 return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t handle)
{
 assert(handle==1);commits++;if(fail_commit)return ESP_FAIL;
 disk_complete=stage_complete;disk_steps=stage_steps;has_complete=stage_has_complete;has_steps=stage_has_steps;return ESP_OK;
}
void nvs_close(nvs_handle_t handle){assert(handle==1);}
static void reboot(void){s_initialized=false;s_complete=false;s_steps=0;}
int main(void)
{
 assert(onboarding_controller_should_show());unsigned reads=opens;
 for(unsigned i=0;i<ONBOARDING_STEP_COUNT;i++)assert(!onboarding_controller_step_complete(i));
 assert(opens==reads);
 assert(onboarding_controller_mark_step_complete(ONBOARDING_STEP_TIMEZONE));
 assert(disk_steps==(1U<<ONBOARDING_STEP_TIMEZONE)&&!has_complete);
 unsigned before=writes;assert(onboarding_controller_mark_step_complete(ONBOARDING_STEP_TIMEZONE)&&writes==before);
 reboot();assert(onboarding_controller_step_complete(ONBOARDING_STEP_TIMEZONE)&&onboarding_controller_should_show());
 for(unsigned fault=0;fault<3;fault++){
  fail_open=fault==0;fail_set=fault==1;fail_commit=fault==2;
  assert(!onboarding_controller_mark_step_complete(ONBOARDING_STEP_DISPLAY));
  assert(!onboarding_controller_step_complete(ONBOARDING_STEP_DISPLAY));
  fail_open=fail_set=fail_commit=false;reboot();assert(!onboarding_controller_step_complete(ONBOARDING_STEP_DISPLAY));
 }
 assert(onboarding_controller_mark_step_complete(ONBOARDING_STEP_DISPLAY));
 reboot();assert(onboarding_controller_step_complete(ONBOARDING_STEP_DISPLAY));
 fail_commit=true;assert(!onboarding_controller_mark_complete()&&onboarding_controller_should_show());fail_commit=false;
 assert(onboarding_controller_mark_complete()&&!onboarding_controller_should_show());
 reboot();for(unsigned i=0;i<ONBOARDING_STEP_COUNT;i++)assert(onboarding_controller_step_complete(i));
 before=writes;assert(onboarding_controller_mark_step_complete(ONBOARDING_STEP_WELCOME)&&writes==before);
 /* Existing completed installations need no new progress key or migration. */
 has_steps=false;disk_complete=1;reboot();
 for(unsigned i=0;i<ONBOARDING_STEP_COUNT;i++)assert(onboarding_controller_step_complete(i));
 assert(!onboarding_controller_should_show());
 disk_complete=2;has_steps=true;disk_steps=0x80;reboot();assert(onboarding_controller_should_show());
 for(unsigned i=0;i<ONBOARDING_STEP_COUNT;i++)assert(!onboarding_controller_step_complete(i));
 before=writes;assert(!onboarding_controller_mark_step_complete((onboarding_step_t)-1));
 assert(!onboarding_controller_mark_step_complete(ONBOARDING_STEP_COUNT));
 assert(!onboarding_controller_step_complete((onboarding_step_t)-1)&&writes==before);
 assert(onboarding_controller_mark_step_complete(ONBOARDING_STEP_WELCOME)&&disk_steps==1);
 puts("PASS: real onboarding progress, reboot persistence, missing/corrupt keys, legacy completion, invalid steps, deduplicated writes and NVS open/set/commit faults");
}
