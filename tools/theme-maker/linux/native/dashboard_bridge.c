/* Host-only fixtures; Dashboard widgets and live formatting come from main/. */
#include <stdio.h>
#include <string.h>
#include "ui_dashboard.c"
#include "ui_active_print.h"
#include "ui_command_bar.h"
#include "ui_filament_recovery.h"
#include "ui_printer_popups.h"
#include "ui_preview_lightbox.h"
#include "dashboard_live_controller.h"
#include "printer_controller.h"
#include "printer_connection_policy.h"
#include "printer_preview_cache.h"
#include "thumbnail_render.h"
#include "camera_catalog_controller.h"
#include "camera_stream_controller.h"
static bool dashboard_open;
static bool (*dashboard_send)(const char *);
static void send_popup(const char *command){if(dashboard_send)dashboard_send(command);}
void ui_command_bar_action(const char *action){
 if(!dashboard_open || !action)return;
 moonraker_state_t state;moonraker_state_snapshot(&state);
 if(printer_connection_reason(&state)[0])return;
 if(!strcmp(action,"RESUME")){if(!strcmp(state.printer_state,"paused"))ui_filament_recovery_show(dashboard_send);}
 else if(!strcmp(action,"CANCEL_OBJECT"))ui_printer_popups_show_cancel_object(send_popup);
 else if(!strcmp(action,"CANCEL_PRINT"))ui_printer_popups_show_cancel(send_popup);
 else if(!strcmp(action,"PAUSE") && !strcmp(state.printer_state,"printing") && dashboard_send)dashboard_send(action);
}
void host_dashboard_close(void){
 if(!dashboard_open)return;
 dashboard_open=false;dashboard_send=NULL;
 ui_preview_lightbox_close();ui_filament_recovery_close();ui_printer_popups_close_all();ui_dashboard_status_popup_close();ui_dashboard_destroy();
}
void host_dashboard_refresh(void){
 if(!dashboard_open)return;
 moonraker_state_t m;moonraker_state_snapshot(&m);
 dashboard_live_controller_push_banner(m.moonraker_ok);dashboard_live_controller_push_machine();
 char layer[32],elapsed[32],remaining[32];
 snprintf(layer,sizeof(layer),"%d/%d",m.current_layer,m.total_layer);
 printer_controller_format_hhmm(elapsed,sizeof(elapsed),m.print_duration);
 printer_controller_format_remaining(remaining,sizeof(remaining),m.progress,m.print_duration);
 ui_dashboard_set_active_print(layer,elapsed,remaining);
 ui_command_bar_update(printer_connection_reason(&m)[0]?"offline":m.printer_state,moonraker_exclude_objects_available());
}
void host_dashboard_show(bool (*send)(const char *)){
 dashboard_send=send;ui_dashboard_create();dashboard_open=true;
 const char *file=NULL;uint32_t revision=0;const lv_image_dsc_t *sample=printer_preview_cache_image(0,&file,&revision);
 int packed_width=0,packed_height=0;
 if(sample && ui_dashboard_thumb_ensure_canvas_buffer(THUMBNAIL_PREVIEW_WIDTH*THUMBNAIL_PREVIEW_HEIGHT) && thumbnail_render_to_rgb565_aspect(sample,*ui_dashboard_thumb_canvas_buf_ref(),THUMBNAIL_PREVIEW_WIDTH,THUMBNAIL_PREVIEW_HEIGHT,&packed_width,&packed_height)){
  moonraker_state_t m;moonraker_state_snapshot(&m);ui_dashboard_thumb_show_canvas_from_buffer(packed_width,packed_height,m.printer_file);
 }
 host_dashboard_refresh();
}

/* The fixture provider installs image objects after page construction. Keep
 * the firmware-owned controls in front of those sample images. */
void host_dashboard_raise_controls(void){
 if(!dashboard_open)return;
 if(dash32_camera_status && !lv_obj_has_flag(dash32_camera_status,LV_OBJ_FLAG_HIDDEN))lv_obj_move_foreground(dash32_camera_status);
 if(dash32_camera_toggle && !lv_obj_has_flag(dash32_camera_toggle,LV_OBJ_FLAG_HIDDEN))lv_obj_move_foreground(dash32_camera_toggle);
}
