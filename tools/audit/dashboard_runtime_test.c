/* Actual runtime pipeline after retirement of unbound metric-label slots. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard_runtime_controller.c"
static moonraker_state_t live;
static char layer[32],elapsed[32],remaining[32],selected[160],path[160],metadata[256],dashboard_file[160],printer_file[160];
static lv_obj_t *printer_canvas,*printer_image,*dashboard_canvas,*dashboard_image;
static unsigned preview_calls,cleanups,placeholders,deletions;
void moonraker_state_snapshot(moonraker_state_t *out){*out=live;}
const char *moonraker_config_host(void){return "printer.local";}
int moonraker_config_port(void){return 7125;}
char *thumbnail_session_selected_file(void){return selected;}
size_t thumbnail_session_selected_file_size(void){return sizeof(selected);}
char *thumbnail_session_selected_thumbnail_path(void){return path;}
char *thumbnail_session_metadata_info(void){return metadata;}
size_t thumbnail_session_metadata_info_size(void){return sizeof(metadata);}
char *ui_dashboard_thumb_canvas_file(void){return dashboard_file;}
char *ui_printer_preview_canvas_file(void){return printer_file;}
lv_obj_t **ui_printer_preview_canvas_ref(void){return &printer_canvas;}
lv_obj_t **ui_printer_preview_image_ref(void){return &printer_image;}
void ui_dashboard_set_active_print(const char *l,const char *e,const char *r){snprintf(layer,sizeof(layer),"%s",l);snprintf(elapsed,sizeof(elapsed),"%s",e);snprintf(remaining,sizeof(remaining),"%s",r);}
void ui_dashboard_thumb_set_placeholder(const char *text){assert(!strcmp(text,"PRINT\nTHUMBNAIL"));placeholders++;}
static void free_thumbnails(void){cleanups++;}
static void image_deleted(lv_event_t *e){(void)e;deletions++;}
void thumbnail_preview_coordinator_update(thumbnail_preview_coordinator_context_t *context){
 assert(!strcmp(context->moonraker_host,"printer.local")&&context->moonraker_port==7125);
 assert(context->selected_print_file==selected&&context->selected_print_file_size==sizeof(selected));
 assert(context->dashboard_canvas==&dashboard_canvas&&context->dashboard_image==&dashboard_image);
 assert(context->printer_canvas==&printer_canvas&&context->printer_image==&printer_image);
 assert(context->free_thumbnail==free_thumbnails&&context->metadata_info==metadata);
 assert(!strcmp(context->printer_state,live.printer_state)&&!strcmp(context->printer_file,live.printer_file));
 preview_calls++;
}
int main(void){
 lv_init();lv_display_t *display=lv_display_create(1024,600);
 char last[32]="";
 dashboard_runtime_context_t context={.meta_object_height=20,.meta_layer_height=.5,.dashboard_canvas=&dashboard_canvas,.dashboard_image=&dashboard_image,.last_print_state=last,.last_print_state_size=sizeof(last),.preview_network_ready=true,.free_thumbnail=free_thumbnails};
 live=(moonraker_state_t){.moonraker_ok=true,.live_data_ok=true,.progress=.5,.print_duration=1200,.current_layer=12,.total_layer=40,.current_gcode_z=6};
 strcpy(live.printer_state,"printing");strcpy(live.printer_file,"job.gcode");strcpy(selected,"job.gcode");strcpy(path,"job.png");
 dashboard_runtime_controller_tick(&context);
 assert(!strcmp(layer,"12/40")&&!strcmp(elapsed,"00:20")&&!strcmp(remaining,"0:20")&&!strcmp(last,"PRINTING")&&preview_calls==1&&!cleanups);
 /* Metadata fallback remains active when slicer layer counts are absent. */
 live.current_layer=live.total_layer=-1;dashboard_runtime_controller_tick(&context);assert(!strcmp(layer,"12/40"));
 strcpy(live.printer_state,"paused");dashboard_runtime_controller_tick(&context);assert(!strcmp(last,"PAUSED")&&!cleanups);
 live.live_data_ok=false;dashboard_runtime_controller_tick(&context);assert(!strcmp(elapsed,"--:--")&&!strcmp(last,"WAITING FOR KLIPPER")&&!cleanups);
 live.moonraker_ok=false;context.preview_network_ready=false;unsigned before=preview_calls;dashboard_runtime_controller_tick(&context);assert(!strcmp(last,"OFFLINE")&&preview_calls==before&&!cleanups);
 /* Reconnection to an active print is not print completion. */
 live.live_data_ok=live.moonraker_ok=true;strcpy(live.printer_state,"printing");context.preview_network_ready=true;dashboard_runtime_controller_tick(&context);assert(!strcmp(last,"PRINTING")&&!cleanups);
 dashboard_image=lv_image_create(lv_screen_active());lv_obj_add_event_cb(dashboard_image,image_deleted,LV_EVENT_DELETE,NULL);
 strcpy(live.printer_state,"ready");dashboard_runtime_controller_tick(&context);
 assert(!dashboard_image&&deletions==1&&cleanups==1&&placeholders==1&&!selected[0]&&!path[0]);
 dashboard_runtime_controller_tick(&context);assert(cleanups==1&&placeholders==1);
 dashboard_runtime_controller_tick(NULL);
 lv_display_delete(display);lv_deinit();
 puts("PASS: actual Dashboard runtime preserves reported/fallback layers, timing, paused/offline/reconnect truth, preview routing and one-shot completion cleanup after legacy metric retirement");
}
