/* Backend adapters and owner storage; page assembly comes from main.c. */
#include <stdio.h>
#include <string.h>
#include "ui_printer.h"
#include "ui_printer_layout.h"
#include "ui_printer_live_status.h"
#include "ui_printer_actions.h"
#include "ui_printer_info_cards.h"
#include "ui_printer_banner.h"
#include "printer_ui_controller.h"
#include "printer_controller.h"
#include "ui_printer_motion.h"
#include "ui_printer_toolhead.h"
#include "ui_printer_popups.h"
#include "ui_page_geometry.h"
#include "ui_page_title.h"
#include "ui_page_layout_profile.h"
#include "ui_shell.h"
#include "ui_theme.h"
#include "ui_theme_studio.h"
static const char *TAG="native-printer";
#include "ui_filament_recovery.h"
#include "macro_controller.h"
#include "esp_log.h"
#include "ui_form_dialog.h"
#include "ui_text.h"
#include "lightbox_local.inc"
#include "ui_dashboard.h"
static lv_obj_t *printer_panel, *printer_banner_label, *printer_active_file_label;
static lv_obj_t *printer_fan_label,*printer_speed_label,*printer_flow_label,*printer_filament_label;
static lv_obj_t *printer_progress_label,*printer_nozzle_label,*printer_bed_label,*printer_part_fan_label;
static lv_obj_t *printer_elapsed_label,*printer_remaining_label,*printer_eta_label;
static lv_obj_t *printer_home_btn,*printer_pause_btn,*printer_resume_btn,*printer_object_btn,*printer_cancel_btn;
static lv_obj_t *printer_state_label,*printer_file_label,*printer_tuning_label;
static lv_obj_t *motion_step1_btn,*motion_step10_btn,*motion_step50_btn;
static double printer_jog_step=1;
static ui_printer_layout_t printer_layout;
static ui_printer_info_cards_t printer_info_cards;
static ui_printer_actions_t printer_actions;
static bool (*host_send)(const char *);
static char selected_file[256]="Spool holder.gcode",thumbnail_path[256];
static void safe_copy(char *out,size_t size,const char *text){snprintf(out,size,"%s",text);}
static char *thumbnail_session_selected_file(void){return selected_file;}
static size_t thumbnail_session_selected_file_size(void){return sizeof(selected_file);}
static char *thumbnail_session_selected_thumbnail_path(void){return thumbnail_path;}
static void thumbnail_preview_coordinator_reset(void){}
static void thumbnail_session_clear_png_buffer(void){}
#include "ui_drybox.h"
static void ui_network_refresh_bridge(void){}
static const char *printer_banner_text(void){return "Simulated printer";}
static bool moonraker_send_gcode(const char *cmd){return host_send && host_send(cmd);}
static void printer_popup_send_gcode_bridge(const char *cmd){moonraker_send_gcode(cmd);}
static bool action_send_gcode_inline(const char *cmd){return moonraker_send_gcode(cmd);}
#include "printer_builder.inc"
void host_printer_close(void){if(!printer_panel)return;ui_preview_lightbox_close();ui_filament_recovery_close();ui_printer_popups_close_all();ui_printer_destroy();}
void host_printer_show(bool (*send)(const char *)){host_send=send;ui_printer_create();}
void host_printer_refresh(void){
 if(!printer_panel)return;
 moonraker_state_t m;moonraker_state_snapshot(&m);moonraker_filament_state_t filament;moonraker_filament_state_snapshot(&filament);
 printer_ui_controller_refresh_ctx_t ctx={
 .printer_panel=printer_panel,.banner_label=printer_banner_label,
 .active_file_label=printer_active_file_label,.speed_label=printer_fan_label,.flow_label=printer_speed_label,.layer_label=printer_flow_label,.filament_label=printer_filament_label,
 .motion_button=printer_actions.motion,.home_button=printer_home_btn,.pause_button=printer_pause_btn,.resume_button=printer_resume_btn,.object_button=printer_object_btn,.cancel_button=printer_cancel_btn,
 .info_cards=&printer_info_cards,.banner_text="Simulated printer",.printer_state=m.printer_state,.printer_file=m.printer_file,.selected_preview_file=selected_file,
 .live_velocity=m.live_velocity,.live_flow=m.live_flow,.speed_factor=m.speed_factor,.flow_factor=m.flow_factor,.current_layer=m.current_layer,.total_layer=m.total_layer,.progress=m.progress,
 .nozzle_temp=m.nozzle_temp,.nozzle_target=m.nozzle_target,.bed_temp=m.bed_temp,.bed_target=m.bed_target,.part_fan_speed=m.part_fan_speed,.print_duration=m.print_duration,
 .moonraker_ok=m.moonraker_ok,.live_data_ok=m.live_data_ok,.exclude_objects_available=true,.capabilities=&m.capabilities,.filament_state=&filament};
 printer_ui_controller_refresh(&ctx);
}
/* Sample data has no transport. The real thumbnail and lightbox builders own
 * sizing, fitting, click handling and cleanup. */
int moonraker_config_active_profile_index(void){return 0;}
const lv_image_dsc_t *printer_preview_cache_image(int profile,const char **file,uint32_t *revision){
 (void)profile;static uint16_t pixels[300*200];static lv_image_dsc_t image;static bool ready;
 if(!ready){for(int y=0;y<200;y++)for(int x=0;x<300;x++){
  bool left=x>=90&&x<=150&&y>=45+(150-x)/3&&y<=150+(x-150)/3;
  bool right=x>150&&x<=210&&y>=45+(x-150)/3&&y<=150-(x-150)/3;
  pixels[y*300+x]=left?0x8c3f:right?0x5ad8:0x18e3;
 }image.header.magic=LV_IMAGE_HEADER_MAGIC;image.header.cf=LV_COLOR_FORMAT_RGB565;image.header.w=300;image.header.h=200;image.header.stride=600;image.data_size=sizeof(pixels);image.data=(const uint8_t *)pixels;ready=true;}
 *file="Spool holder.gcode";*revision=1;return &image;
}
void macro_controller_status(macro_controller_status_t *out){*out=(macro_controller_status_t){.discovered=true,.count=4,.total_count=4,.generation=1};}
bool macro_controller_get(size_t index,char *out,size_t size){const char *names[]={"HMI_FILAMENT_HEAT","HMI_FILAMENT_UNLOAD","HMI_FILAMENT_LOAD","HMI_FILAMENT_PURGE"};if(index>=4)return false;snprintf(out,size,"%s",names[index]);return true;}
