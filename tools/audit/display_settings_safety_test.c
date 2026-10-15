#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ui_settings.c"
static bool fail_brightness,fail_off,fail_open,fail_set,fail_commit;
static unsigned hardware_sets,hardware_offs,commits,warnings,reboot_reviews;
static int hardware_brightness=100;
static uint8_t stored_brightness=100,stored_sleep,staged_brightness,staged_sleep;
static void (*hardware_hook)(void),(*storage_hook)(void);
static void hook(void (**slot)(void)){if(*slot){void(*call)(void)=*slot;*slot=NULL;call();}}
const char *esp_err_to_name(esp_err_t error){return error==ESP_OK?"OK":"FAIL";}
esp_err_t bsp_display_brightness_set(int value){hardware_sets++;hook(&hardware_hook);if(fail_brightness)return ESP_FAIL;hardware_brightness=value;return ESP_OK;}
esp_err_t bsp_display_backlight_off(void){hardware_offs++;hook(&hardware_hook);if(fail_off)return ESP_FAIL;hardware_brightness=0;return ESP_OK;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle){assert(!strcmp(name,"display"));if(fail_open)return ESP_FAIL;*handle=mode;staged_brightness=stored_brightness;staged_sleep=stored_sleep;return ESP_OK;}
esp_err_t nvs_get_u8(nvs_handle_t handle,const char *key,uint8_t *value){assert(handle==NVS_READONLY);*value=!strcmp(key,"brightness")?stored_brightness:stored_sleep;return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t handle,const char *key,uint8_t value){assert(handle==NVS_READWRITE);if(fail_set)return ESP_FAIL;if(!strcmp(key,"brightness"))staged_brightness=value;else{assert(!strcmp(key,"sleep_min"));staged_sleep=value;}return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t handle){assert(handle==NVS_READWRITE);commits++;hook(&storage_hook);if(fail_commit)return ESP_FAIL;stored_brightness=staged_brightness;stored_sleep=staged_sleep;return ESP_OK;}
void nvs_close(nvs_handle_t handle){(void)handle;}
void ui_toast_show(ui_status_kind_t kind,const char *title,const char *detail){assert(kind==UI_STATUS_WARNING&&title&&detail);warnings++;}
void ui_apply_surface_role(lv_obj_t *object,ui_surface_role_t role){(void)object;(void)role;}
void ui_toast_preferences_close(void){}
void ui_appearance_popups_close_all(void){}
void ui_settings_popups_close_all(void){}
void ui_settings_popups_show_reboot(void){reboot_reviews++;}
void ui_settings_backup_popup_close(void){}
void ui_about_popup_close(void){}
void settings_system_info_unbind(void){}
void settings_system_info_refresh(void){}
void ui_shell_raise(void){}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels){(void)area;(void)pixels;lv_display_flush_ready(display);}
static void build_controls(void)
{
    assert(!settings_panel);
    settings_panel=lv_obj_create(lv_screen_active());++s_settings_page_epoch;
    lv_obj_add_event_cb(settings_panel,settings_page_deleted,LV_EVENT_DELETE,NULL);
    s_brightness_slider=lv_slider_create(settings_panel);lv_slider_set_range(s_brightness_slider,10,100);
    lv_slider_set_value(s_brightness_slider,s_display_brightness_percent,LV_ANIM_OFF);
    s_brightness_value=lv_label_create(settings_panel);lv_label_set_text(s_brightness_value,"CURRENT");
    lv_obj_add_event_cb(s_brightness_slider,settings_brightness_changed_cb,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_add_event_cb(s_brightness_slider,settings_brightness_save_cb,LV_EVENT_RELEASED,NULL);
    settings_display_field_own(s_brightness_slider);settings_display_field_own(s_brightness_value);
    s_reboot_action=lv_button_create(settings_panel);settings_display_field_own(s_reboot_action);lv_obj_add_event_cb(s_reboot_action,settings_reboot_cb,LV_EVENT_CLICKED,NULL);
    s_sleep_action=lv_obj_create(settings_panel);settings_sleep_label=lv_label_create(s_sleep_action);
    lv_label_set_text(settings_sleep_label,settings_sleep_timeout_text());
    settings_display_field_own(s_sleep_action);settings_display_field_own(settings_sleep_label);
    lv_obj_add_event_cb(s_sleep_action,settings_sleep_card_cb,LV_EVENT_CLICKED,NULL);
}
static void preview(int value){lv_slider_set_value(s_brightness_slider,value,LV_ANIM_OFF);lv_obj_send_event(s_brightness_slider,LV_EVENT_VALUE_CHANGED,NULL);}
static void release(void){lv_obj_send_event(s_brightness_slider,LV_EVENT_RELEASED,NULL);}
static void sleep_tap(void){lv_obj_send_event(s_sleep_action,LV_EVENT_CLICKED,NULL);}
static void replace_page(void){hide_settings_tab();build_controls();}
static void nested_preview(void){preview(20);sleep_tap();assert(!ui_settings_restore_display_preferences(25,30));}
static void nested_save(void){release();sleep_tap();}
static void retire_overlay(void){lv_obj_delete(s_sleep_wake_overlay);}
static void pump(void){lv_tick_inc(20);lv_timer_handler();}
int main(void)
{
    lv_init();lv_display_t *display=lv_display_create(1024,600);static uint8_t pixels[1024*20*2];
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);lv_display_set_buffers(display,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(display,flush);
    ui_settings_module_init();lv_timer_t *timer=s_sleep_timer;ui_settings_module_init();assert(s_sleep_timer==timer&&hardware_brightness==100);
    for(unsigned width=0;width<3;width++){
        lv_display_set_resolution(display,(int[]){1024,640,480}[width],width==2?400:600);
        assert(ui_settings_restore_display_preferences(100,0));build_controls();unsigned reviews=reboot_reviews;lv_obj_add_state(s_reboot_action,LV_STATE_DISABLED);lv_obj_send_event(s_reboot_action,LV_EVENT_CLICKED,NULL);assert(reboot_reviews==reviews);lv_obj_remove_state(s_reboot_action,LV_STATE_DISABLED);lv_obj_send_event(s_reboot_action,LV_EVENT_CLICKED,NULL);assert(reboot_reviews==reviews+1);lv_obj_delete(s_reboot_action);assert(!s_reboot_action);settings_reboot_cb(NULL);assert(reboot_reviews==reviews+1);
        fail_brightness=true;unsigned before=warnings;preview(40);assert(s_display_brightness_percent==100&&hardware_brightness==100&&lv_slider_get_value(s_brightness_slider)==100&&warnings==before+1);fail_brightness=false;
        unsigned sets=hardware_sets;hardware_hook=nested_preview;preview(40);assert(hardware_sets==sets+1&&s_display_brightness_percent==40&&s_sleep_timeout_minutes==0);
        fail_open=true;before=commits;release();assert(commits==before&&s_saved_display_brightness_percent==100);fail_open=false;
        fail_set=true;release();assert(commits==before&&s_saved_display_brightness_percent==100);fail_set=false;
        fail_commit=true;release();assert(s_saved_display_brightness_percent==100&&s_display_brightness_percent==40);fail_commit=false;
        before=commits;storage_hook=nested_save;release();assert(commits==before+1&&stored_brightness==40&&s_saved_display_brightness_percent==40);
        release();assert(commits==before+1);
        hardware_hook=replace_page;preview(60);assert(s_brightness_value&&!strcmp(lv_label_get_text(s_brightness_value),"CURRENT")&&s_display_brightness_percent==60);
        storage_hook=replace_page;release();assert(stored_brightness==60&&s_saved_display_brightness_percent==60&&!strcmp(lv_label_get_text(s_brightness_value),"CURRENT"));ui_settings_refresh();assert(lv_slider_get_value(s_brightness_slider)==60&&!strcmp(lv_label_get_text(s_brightness_value),"60%"));
        lv_obj_delete(s_brightness_value);assert(!s_brightness_value);preview(70);release();assert(stored_brightness==70);
        lv_obj_delete(s_brightness_slider);assert(!s_brightness_slider);settings_brightness_changed_cb(NULL);settings_brightness_save_cb(NULL);
        fail_commit=true;before=warnings;sleep_tap();assert(s_sleep_timeout_minutes==0&&stored_sleep==0&&warnings==before+1&&!strcmp(lv_label_get_text(settings_sleep_label),"OFF"));fail_commit=false;
        sleep_tap();assert(s_sleep_timeout_minutes==5&&stored_sleep==5);
        storage_hook=replace_page;sleep_tap();assert(s_sleep_timeout_minutes==15&&stored_sleep==15&&settings_sleep_label);ui_settings_refresh();assert(!strcmp(lv_label_get_text(settings_sleep_label),"15 MIN"));
        sleep_tap();assert(s_sleep_timeout_minutes==30);sleep_tap();assert(s_sleep_timeout_minutes==0);
        lv_obj_delete(settings_sleep_label);assert(!settings_sleep_label);before=commits;settings_sleep_card_cb(NULL);assert(commits==before);
        lv_obj_delete(settings_panel);assert(!settings_panel&&!s_brightness_slider&&!s_sleep_action&&!s_settings_content);
        assert(ui_settings_restore_display_preferences(70,5));
        before=hardware_offs;settings_sleep_timer_cb(NULL);assert(hardware_offs==before);
        fail_off=true;settings_enter_display_sleep();assert(!s_display_sleeping&&!s_sleep_wake_overlay);fail_off=false;
        lv_display_trigger_activity(display);lv_tick_inc(300000);settings_sleep_timer_cb(s_sleep_timer);assert(s_display_sleeping&&s_sleep_wake_overlay&&hardware_brightness==0);
        fail_brightness=true;lv_obj_send_event(s_sleep_wake_overlay,LV_EVENT_PRESSED,NULL);assert(s_display_sleeping&&s_sleep_wake_overlay&&hardware_brightness==0);fail_brightness=false;
        sets=hardware_sets;lv_obj_send_event(s_sleep_wake_overlay,LV_EVENT_PRESSED,NULL);assert(!s_display_sleeping&&!s_sleep_wake_overlay&&hardware_brightness==70&&hardware_sets==sets+1);settings_wake_display_cb(NULL);assert(hardware_sets==sets+1);pump();
        settings_enter_display_sleep();lv_obj_delete(s_sleep_wake_overlay);assert(!s_display_sleeping&&!s_sleep_wake_overlay&&hardware_brightness==70);
        settings_enter_display_sleep();fail_brightness=true;lv_obj_delete(s_sleep_wake_overlay);assert(s_display_sleeping&&s_sleep_wake_overlay);fail_brightness=false;settings_wake_display_cb(NULL);pump();
        hardware_hook=retire_overlay;settings_enter_display_sleep();assert(!s_display_sleeping&&!s_sleep_wake_overlay&&hardware_brightness==70);pump();
        settings_enter_display_sleep();sets=hardware_sets;assert(ui_settings_restore_display_preferences(80,15));assert(s_display_sleeping&&hardware_sets==sets&&hardware_brightness==0);settings_wake_display_cb(NULL);pump();assert(hardware_brightness==80);
        settings_enter_display_sleep();assert(ui_settings_restore_display_preferences(90,0));pump();assert(!s_display_sleeping&&!s_sleep_wake_overlay&&hardware_brightness==90);
        assert(!ui_settings_restore_display_preferences(9,0)&&!ui_settings_restore_display_preferences(100,1));
    }
    lv_timer_delete(s_sleep_timer);s_sleep_timer=NULL;lv_display_delete(display);lv_deinit();
    puts("PASS: real Settings brightness/sleep callbacks, captured persistence, SDK/NVS faults, retry, nested dispatch, page/field replacement, owned timer, wake failure shield, external overlay deletion and restored sleep preferences at three viewports");
}
