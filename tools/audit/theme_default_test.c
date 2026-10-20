/* Real theme manager/dispatcher with a transactional NVS boundary. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "theme_manager.h"
#include "nvs.h"
static esp_err_t open_error,read_error,commit_error;
static bool stored,staged,custom_active;
static uint8_t saved,staged_theme;
static unsigned writes,removals;
static bool secondary;
const char *esp_err_to_name(esp_err_t error){(void)error;return "fixture";}
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h){assert(!strcmp(ns,"ui_theme"));(void)mode;*h=1;staged=stored;staged_theme=saved;return open_error;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char *key,uint8_t *out){assert(h==1);
 if(!strcmp(key,"active")){if(read_error)return read_error;if(!stored)return ESP_ERR_NVS_NOT_FOUND;*out=saved;return ESP_OK;}
 if(!secondary)return ESP_ERR_NVS_NOT_FOUND;
 if(!strcmp(key,"accent"))*out=UI_ACCENT_ID_AMBER;
 else if(!strcmp(key,"density"))*out=UI_DENSITY_COMPACT;
 else *out=1;
 return ESP_OK;
}
esp_err_t nvs_get_str(nvs_handle_t h,const char*k,char*v,size_t*n){(void)h;(void)k;(void)v;(void)n;return ESP_ERR_NVS_NOT_FOUND;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char*k,uint8_t v){assert(h==1&&!strcmp(k,"active"));++writes;staged=true;staged_theme=v;return ESP_OK;}
esp_err_t nvs_set_str(nvs_handle_t h,const char*k,const char*v){(void)h;(void)k;(void)v;return ESP_OK;}
esp_err_t nvs_erase_key(nvs_handle_t h,const char*k){(void)h;(void)k;return ESP_ERR_NVS_NOT_FOUND;}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);if(commit_error)return commit_error;stored=staged;saved=staged_theme;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1);}
void custom_theme_deactivate(void){custom_active=false;}
bool custom_theme_is_active(void){return custom_active;}
const char *custom_theme_active_id(void){return "fixture";}
const custom_theme_summary_t *custom_theme_summary(size_t i){static custom_theme_summary_t s={.id="fixture",.base_theme=UI_THEME_OPERATOR};return i==0?&s:NULL;}
bool custom_theme_remove(size_t i){assert(i==0);++removals;return true;}
int main(void){
 assert(ui_theme_get_active()==UI_THEME_STUDIO_DARK);
 assert(UI_THEME_DEFAULT==UI_THEME_STUDIO_DARK);
 theme_manager_init();assert(theme_manager_active()==UI_THEME_STUDIO_DARK && !writes && !stored);
 secondary=true;theme_manager_init();assert(theme_manager_active()==UI_THEME_STUDIO_DARK);
 assert(theme_manager_accent()==UI_ACCENT_ID_AMBER && theme_manager_density()==UI_DENSITY_COMPACT);
 ui_accessibility_t a=theme_manager_accessibility();assert(a.large_text&&a.high_contrast&&a.reduced_transparency&&a.reduced_motion);
 for(unsigned i=UI_THEME_CLASSIC;i<=UI_THEME_STUDIO_DARK;i++){stored=true;saved=i;theme_manager_init();assert((unsigned)theme_manager_active()==i && !writes);}
 saved=255;theme_manager_init();assert(theme_manager_active()==UI_THEME_STUDIO_DARK && saved==255 && !writes);
 saved=UI_THEME_CLASSIC;read_error=ESP_FAIL;theme_manager_init();assert(theme_manager_active()==UI_THEME_STUDIO_DARK);read_error=ESP_OK;
 open_error=ESP_FAIL;theme_manager_init();assert(theme_manager_active()==UI_THEME_STUDIO_DARK);open_error=ESP_OK;
 assert(theme_manager_select(UI_THEME_GLASS));assert(saved==UI_THEME_GLASS);theme_manager_init();assert(theme_manager_active()==UI_THEME_GLASS);
 commit_error=ESP_FAIL;assert(!theme_manager_select(UI_THEME_CLASSIC));assert(saved==UI_THEME_GLASS&&theme_manager_active()==UI_THEME_GLASS);commit_error=ESP_OK;
 unsigned before=writes;assert(!theme_manager_select((ui_theme_id_t)255)&&writes==before);
 ui_theme_set_active((ui_theme_id_t)255);assert(ui_theme_get_active()==UI_THEME_STUDIO_DARK);
 custom_active=true;assert(theme_manager_remove_custom(0));assert(removals==1 && saved==UI_THEME_STUDIO_DARK && !custom_active);
 custom_active=true;commit_error=ESP_FAIL;assert(!theme_manager_remove_custom(0));assert(removals==1&&custom_active);
 puts("PASS: Studio startup/defaults, all saved themes, retained preferences, save/reload/failure and custom-removal fallback using real theme manager/dispatcher");
 return 0;
}
