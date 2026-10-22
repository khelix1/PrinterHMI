/* Expose the native Printer page's private Tune dialog to the host runtime.
 * Include the implementation so the dialog is never reimplemented here. */
#include "ui_printer_live_status.c"
static lv_obj_t *host_tuning_owner;
void host_tuning_close(void){studio_tuning_close(NULL);if(host_tuning_owner)lv_obj_delete(host_tuning_owner);host_tuning_owner=NULL;s_tuning_owner=NULL;}
void host_tuning_show(bool (*send)(const char *)){
 host_tuning_close();host_tuning_owner=lv_obj_create(lv_screen_active());lv_obj_add_flag(host_tuning_owner,LV_OBJ_FLAG_HIDDEN);
 s_tuning_owner=host_tuning_owner;s_tuning_generation=moonraker_config_generation();s_send_gcode_cb=send;studio_tuning_open(NULL);
}
void ui_toast_show(ui_status_kind_t kind,const char *title,const char *body){(void)kind;(void)title;(void)body;}
