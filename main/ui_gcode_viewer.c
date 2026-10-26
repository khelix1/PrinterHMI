#include "ui_gcode_viewer.h"
#include "mesh_preview.h"
#include "gcode_viewer_loader.h"
#include "gcode_surface.h"
#include "gcode_cache.h"
#include "ui_popup.h"
#include "ui_button.h"
#include "ui_widgets.h"
#include "moonraker_config_controller.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static struct {
    lv_obj_t *popup,*canvas,*status,*layer,*travel,*filename,*slider,*mode,*details,*details_button,*reload_button,*solid_button;
    lv_obj_t *controls[11];
    lv_timer_t *timer;
    lv_display_t *display;
    int64_t display_started,flush_started,display_us,flush_us,cycle_flush_us;
    gcode_toolpath_t *model,*detail;
    bool detail_loading,detail_failed,rendering_surface,refine_pending,refining,refine_failed;
    int detail_wanted;
    uint32_t detail_due;
    uint16_t *pixels,*solid_pixels;
    uint32_t *section_mask;
    gcode_view_t render_view;
    const gcode_toolpath_t *render_model,*render_layer_model;
    mesh_preview_t *render_mesh;
    float *depth;
    bool solid,solid_rendering;
    size_t solid_cursor,travel_cursor,layer_cursor;
    bool travel_overlay,layer_overlay;
    gcode_view_t view;
    lv_point_t pointer;
    uint32_t owner,frames,progress_tick;
    int64_t render_started,raster_us,last_raster_us,publish_us,frame_us;
    int64_t body_us,material_us,travel_us,last_body_us,last_material_us,last_travel_us;
    int64_t gesture_frame_us,gesture_body_us,gesture_material_us,gesture_travel_us;
    int gesture_layer;bool gesture_cumulative;uint32_t gesture_frames;
    uint32_t render_slices,last_slices,gesture_slices;int64_t gesture_publish_us;
    float render_cut,render_low;
    bool displayed_mesh;
    int displayed_bounds[4],layer_bounds[4];
    size_t damaged_pixels;
    size_t progress_bytes;
    int width,height,render_width,render_height;
    bool render_interactive,display_interactive;
    bool dirty,drag,loading,pinch_active,block_one_finger;
    float pinch_distance,pinch_base_zoom;
    char file[256],refine_error[128];
} s;
static void wake_render(void){s.dirty=true;if(s.timer){lv_timer_set_period(s.timer,5);lv_timer_ready(s.timer);}}
static void apply_zoom(float zoom,float anchor_x,float anchor_y){
    float ratio=zoom/s.view.zoom;s.view.pan_x=fmaxf(-32,fminf(32,anchor_x-(anchor_x-s.view.pan_x)*ratio));s.view.pan_y=fmaxf(-32,fminf(32,anchor_y-(anchor_y-s.view.pan_y)*ratio));s.view.zoom=zoom;
}
static void display_event(lv_event_t *e){
    lv_event_code_t code=lv_event_get_code(e);if(code==LV_EVENT_DELETE){s.display=NULL;return;}
    if(!s.popup)return;
    int64_t now=esp_timer_get_time();
    if(code==LV_EVENT_RENDER_START){s.display_started=now;s.cycle_flush_us=0;}
    if(code==LV_EVENT_FLUSH_START)s.flush_started=now;
    if(code==LV_EVENT_FLUSH_FINISH)s.cycle_flush_us+=now-s.flush_started;
    if(code==LV_EVENT_RENDER_READY){s.display_us=now-s.display_started;s.flush_us=s.cycle_flush_us;}
}
static void publish_canvas(bool mesh){
    int bounds[4];
    if(mesh&&mesh_preview_screen_bounds(s.render_mesh,&s.render_view,s.width,s.height,bounds)){
        if(s.render_view.layer>=0&&s.layer_bounds[2]>=s.layer_bounds[0]){int scale=s.render_interactive?2:1;int rect[4]={s.layer_bounds[0]*scale,s.layer_bounds[1]*scale,(s.layer_bounds[2]+1)*scale-1,(s.layer_bounds[3]+1)*scale-1};if(rect[0]<bounds[0])bounds[0]=rect[0];if(rect[1]<bounds[1])bounds[1]=rect[1];if(rect[2]>bounds[2])bounds[2]=rect[2];if(rect[3]>bounds[3])bounds[3]=rect[3];}
        if(s.displayed_mesh){
            int dirty[4]={bounds[0]<s.displayed_bounds[0]?bounds[0]:s.displayed_bounds[0],bounds[1]<s.displayed_bounds[1]?bounds[1]:s.displayed_bounds[1],bounds[2]>s.displayed_bounds[2]?bounds[2]:s.displayed_bounds[2],bounds[3]>s.displayed_bounds[3]?bounds[3]:s.displayed_bounds[3]};
            lv_area_t canvas;lv_obj_get_coords(s.canvas,&canvas);lv_area_t area={canvas.x1+dirty[0],canvas.y1+dirty[1],canvas.x1+dirty[2],canvas.y1+dirty[3]};lv_obj_invalidate_area(s.canvas,&area);s.damaged_pixels=(size_t)(dirty[2]-dirty[0]+1)*(dirty[3]-dirty[1]+1);
        }else{lv_obj_invalidate(s.canvas);s.damaged_pixels=(size_t)s.width*s.height;}
        memcpy(s.displayed_bounds,bounds,sizeof(bounds));s.displayed_mesh=true;
    }else{lv_obj_invalidate(s.canvas);s.displayed_mesh=false;s.damaged_pixels=(size_t)s.width*s.height;}
}
static void release_mesh(void *p){heap_caps_free(p);}
static void close_details(void)
{
    lv_obj_t *popup=s.details;s.details=NULL;if(popup)lv_obj_delete(popup);
}
static void details_closed(lv_event_t *e){if(lv_event_get_target_obj(e)==s.details)s.details=NULL;}
static void close_details_event(lv_event_t *e){(void)e;close_details();}
static void reason(const gcode_toolpath_t *m,char *text,size_t size)
{
    if(m->stop_reason[0]){
        if(m->stop_command[0])snprintf(text,size,"Line %lu %s: %s",(unsigned long)m->stop_line,m->stop_command,m->stop_reason);
        else snprintf(text,size,"Line %lu: %s",(unsigned long)m->stop_line,m->stop_reason);
    }
    else if(m->omitted_count)snprintf(text,size,"%s omitted at line %lu",m->omitted_command,(unsigned long)m->omitted_line);
    else if(m->tool_changes)snprintf(text,size,"Tool offsets/docking omitted (line %lu)",(unsigned long)m->tool_line);
    else if(m->homings)snprintf(text,size,"Homing motion omitted (line %lu)",(unsigned long)m->homing_line);
    else if(m->retracts)snprintf(text,size,"Firmware retract geometry omitted");
    else snprintf(text,size,"%s",m->malformed?"Invalid input":m->bounded?"Resource limit":m->unsupported?"Unmodelled commands":"Supported paths loaded");
}
static void open_details(void)
{
    if(s.details||!s.model)return;
    s.details=ui_popup_create(lv_layer_top(),760,440,UI_POPUP_STANDARD);if(!s.details)return;
    lv_obj_set_style_pad_all(s.details,0,0);lv_obj_remove_flag(s.details,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s.details,details_closed,LV_EVENT_DELETE,NULL);
    lv_obj_t *title=lv_label_create(s.details);lv_label_set_text(title,"Preview details");
    ui_apply_custom_label_style(title,UI_FONT_TITLE,UI_TEXT);lv_obj_set_pos(title,20,12);lv_obj_set_width(title,720);
    lv_obj_t *body=lv_obj_create(s.details);ui_apply_surface_role(body,UI_SURFACE_TRANSPARENT);lv_obj_set_pos(body,20,60);lv_obj_set_size(body,720,300);
    lv_obj_set_style_pad_all(body,8,0);lv_obj_set_scroll_dir(body,LV_DIR_VER);
    char why[180],text[2600],tools[80]="",omitted[100]="None";const gcode_toolpath_t *m=s.model;reason(m,why,sizeof(why));
    if(m->omitted_count)snprintf(omitted,sizeof(omitted),"%s at line %lu",m->omitted_command,(unsigned long)m->omitted_line);
    size_t used=0;for(unsigned i=0;i<GCODE_TOOLPATH_TOOLS_MAX;i++)if(m->tool_mask&(1u<<i))used+=(size_t)snprintf(tools+used,sizeof(tools)-used,"T%u ",i);
    snprintf(text,sizeof(text),"File: %s\n\n%s\n%s\nRead: %lu bytes | %lu physical lines\nGenerated: %lu paths | %u extrusion Z levels\nMain samples: %lu / %lu | Sampling stride: 1/%lu\nLayer anchors: %u | Coarser arcs: %s\nBounds: %.2f x %.2f x %.2f mm\n\nOmitted start/end macros: %lu\nFirst omitted: %s\nTools: %s | Tool changes: %lu | Arcs: %lu\nHoming moves omitted: %lu | Firmware retracts omitted: %lu\nTool offsets/docking and printer macros are not expanded.\n\nG0/G1 and tessellated G2/G3 paths; explicit offsets and saved states. Reduced detail keeps independent paths and layer/tool anchors; gaps can appear, especially in isolated layers. Solid uses an approximate mesh or deposited paths with supported width tags and nominal defaults, not recovered CAD surfaces. This is file inspection, not a printer simulation. No printer command was sent.",
        lv_label_get_text(s.filename),m->bounded||m->unsupported||m->malformed?"PARTIAL PREVIEW":m->sample_stride>1||m->coarse_arcs?"REDUCED DETAIL":"Supported paths loaded",why,
        (unsigned long)m->bytes,(unsigned long)m->lines,(unsigned long)(m->input_segments?m->input_segments:m->count),m->layer_count,(unsigned long)m->count,(unsigned long)m->capacity,(unsigned long)(m->sample_stride?m->sample_stride:1),m->layer_count,m->coarse_arcs?"Yes":"No",
        (double)(m->max.x-m->min.x),(double)(m->max.y-m->min.y),(double)(m->max.z-m->min.z),
        (unsigned long)m->omitted_count,omitted,tools[0]?tools:"None",(unsigned long)m->tool_changes,(unsigned long)m->arcs,(unsigned long)m->homings,(unsigned long)m->retracts);
    size_t used_text=strlen(text);
    if(used_text<sizeof(text))snprintf(text+used_text,sizeof(text)-used_text,"\n\nLast solid frame: %.1f ms | Raster work: %.1f ms | Publish: %.1f ms\nLast display cycle: %.1f ms | Flush callbacks: %.1f ms\nCanvas damage: %lu / %lu pixels. Display timings are not panel FPS.\nUp to and Single expose selected-layer material over the exterior cut with a narrow shell contact band. Solid uses gold shading and a 3 x 2 display-pixel minimum cross-section; unresolved gaps may close. Lines retains tool colors and thin paths.",(double)s.frame_us/1000,(double)s.last_raster_us/1000,(double)s.publish_us/1000,(double)s.display_us/1000,(double)s.flush_us/1000,(unsigned long)s.damaged_pixels,(unsigned long)((size_t)s.width*s.height));
    used_text=strlen(text);
    if(used_text<sizeof(text))snprintf(text+used_text,sizeof(text)-used_text,"\n\nSD cache: %s | %lu paths\nSurface panels: %lu | Merged runs: %lu\nSurface coverage: %s\nSelected layer SD paths: %lu | stride 1/%lu",
        m->cache&&m->cache->complete?"Ready":"Unavailable",(unsigned long)(m->cache?m->cache->count:0),
        (unsigned long)(m->surface?m->surface->count:0),(unsigned long)(m->surface?m->surface->merged:0),
        gcode_surface_usable(m->surface)?"Complete planar extrusion overview":"Using path fallback",
        (unsigned long)(s.detail?s.detail->input_segments:0),(unsigned long)(s.detail?s.detail->sample_stride:1));
    used_text=strlen(text);
    if(used_text<sizeof(text))snprintf(text+used_text,sizeof(text)-used_text,"\n\nAutomatic solid: %s\nSurface triangles: %lu | Volume cell: %.4f mm\nWidth/height tags: %lu\nThe automatic solid is a sampled extrusion envelope, not the original CAD model. Small gaps/features can change at this resolution. Single shows the selected layer as shaded extrusion strands over its exterior height interval; Lines retains path inspection. Travel overlays retained/cache paths in gray.",m->overview_mesh?"Ready":s.refining?"Preparing":s.refine_failed?"Unavailable; path fallback":"Pending",(unsigned long)(m->overview_mesh?m->overview_mesh->triangles:0),(double)(m->overview_mesh?m->overview_mesh->cell_mm:0),(unsigned long)m->dimension_tags);
    used_text=strlen(text);if(m->overview_mesh&&used_text<sizeof(text))snprintf(text+used_text,sizeof(text)-used_text,"\nGesture mesh: %lu triangles | Half-resolution shading during movement; full detail on release.",(unsigned long)(m->overview_mesh->interaction?m->overview_mesh->interaction->triangles:m->overview_mesh->triangles));
    used_text=strlen(text);if(s.refine_failed&&used_text<sizeof(text))snprintf(text+used_text,sizeof(text)-used_text,"\nPreparation error: %s",s.refine_error);
    /* Preserve the last movement sample across the final full-detail redraw. */
    char timing[640];
    snprintf(timing,sizeof(timing),"Last frame stages: body %.1f ms | material %.1f ms | travel %.1f ms\nLast movement: %s%s | %.1f ms total\nMovement stages: body %.1f ms | material %.1f ms | travel %.1f ms\nMovement: %lu slices | publish %.1f ms | other elapsed %.1f ms\nMovement samples: %lu. Detailed beads return on release.\n\n",
        (double)s.last_body_us/1000,(double)s.last_material_us/1000,(double)s.last_travel_us/1000,
        s.gesture_frames?"":"No sample ",s.gesture_layer<0?"All":s.gesture_cumulative?"Up to":"Single",
        (double)s.gesture_frame_us/1000,(double)s.gesture_body_us/1000,(double)s.gesture_material_us/1000,(double)s.gesture_travel_us/1000,(unsigned long)s.gesture_slices,(double)s.gesture_publish_us/1000,
        fmax(0,(double)(s.gesture_frame_us-s.gesture_body_us-s.gesture_material_us-s.gesture_travel_us-s.gesture_publish_us))/1000,(unsigned long)s.gesture_frames);
    lv_obj_t *timing_label=lv_label_create(body);ui_apply_custom_label_style(timing_label,UI_FONT_BODY,UI_TEXT);lv_obj_set_width(timing_label,680);lv_label_set_text(timing_label,timing);
    lv_obj_t *label=lv_label_create(body);lv_label_set_text(label,text);lv_obj_update_layout(timing_label);lv_obj_set_y(label,lv_obj_get_height(timing_label)+12);ui_apply_custom_label_style(label,UI_FONT_BODY,UI_TEXT);
    lv_obj_set_width(label,LV_PCT(100));lv_label_set_long_mode(label,LV_LABEL_LONG_WRAP);
    lv_obj_t *close=ui_button_create(s.details,UI_BUTTON_CLOSE,"Close");lv_obj_set_size(close,128,48);lv_obj_align(close,LV_ALIGN_BOTTOM_RIGHT,-20,-16);
    lv_obj_add_event_cb(close,close_details_event,LV_EVENT_CLICKED,NULL);
}
static void caption(void)
{
    char text[240],why[160];if(!s.model)return;
    const gcode_toolpath_t *m=s.model;reason(m,why,sizeof(why));
    unsigned long generated=(unsigned long)(m->input_segments?m->input_segments:m->count);
    bool automatic=s.solid&&m->overview_mesh;
    bool surface=s.solid&&gcode_surface_usable(m->surface)&&!s.view.travel&&(s.view.layer<0||s.view.cumulative);
    bool fine=s.detail&&s.detail->cached_layer==s.view.layer&&(s.solid||!s.view.cumulative);
    uint32_t stride=automatic?(s.view.layer>=0?(fine?s.detail->sample_stride:m->sample_stride):1):fine?s.detail->sample_stride:surface?1:m->sample_stride;
    char detail[48]="";if(stride>1)snprintf(detail,sizeof(detail)," | Detail 1/%lu",(unsigned long)stride);else if(m->coarse_arcs)snprintf(detail,sizeof(detail)," | Coarser arcs");
    if(!m->count)snprintf(text,sizeof(text),"NO PATHS | %s | Details",why);
    else if(m->bounded||m->unsupported||m->malformed)snprintf(text,sizeof(text),"PARTIAL | %.120s | %lu paths, %u layers%s",why,generated,m->layer_count,detail);
    else if(automatic&&!detail[0])snprintf(text,sizeof(text),"AUTO SOLID | %lu triangles | %.3f mm resolution",(unsigned long)m->overview_mesh->triangles,(double)m->overview_mesh->cell_mm);
    else if(surface&&!detail[0])snprintf(text,sizeof(text),"SURFACE | %lu panels from %lu extrusions | %u layers",(unsigned long)m->surface->count,(unsigned long)m->surface->inputs,m->layer_count);
    else if(detail[0])snprintf(text,sizeof(text),"REDUCED DETAIL | %lu paths, %lu samples | %u layers%s",generated,(unsigned long)m->count,m->layer_count,detail);
    else snprintf(text,sizeof(text),"%lu paths | %u layers | %.1f x %.1f x %.1f mm",generated,m->layer_count,(double)(m->max.x-m->min.x),(double)(m->max.y-m->min.y),(double)(m->max.z-m->min.z));
    size_t used_status=strlen(text);
    if(used_status<sizeof(text)){
        if(automatic&&s.view.travel)snprintf(text+used_status,sizeof(text)-used_status," | Travel detail 1/%lu",(unsigned long)(fine?s.detail->sample_stride:m->sample_stride));
        else if(automatic&&(m->bounded||m->unsupported||m->malformed||m->coarse_arcs))snprintf(text+used_status,sizeof(text)-used_status," | Auto %.3f mm",(double)m->overview_mesh->cell_mm);
        else if(fine)snprintf(text+used_status,sizeof(text)-used_status," | SD layer: %lu paths, 1/%lu",(unsigned long)s.detail->input_segments,(unsigned long)s.detail->sample_stride);
        else if(s.detail_loading)snprintf(text+used_status,sizeof(text)-used_status," | Reading SD layer");
        else if(s.detail_failed)snprintf(text+used_status,sizeof(text)-used_status," | SD detail unavailable");
        else if(s.solid&&gcode_surface_usable(m->surface)&&(s.view.layer<0||s.view.cumulative)&&!s.view.travel)snprintf(text+used_status,sizeof(text)-used_status," | Merged surfaces");
    }
    char labelled[400],preparation[128]="";
    if(s.refining){gcode_viewer_progress_t progress=gcode_viewer_loader_progress();snprintf(preparation,sizeof(preparation),"Preparing mesh %u%% | ",progress.percent>100?100:progress.percent);}
    else if(s.refine_failed)snprintf(preparation,sizeof(preparation),"Mesh unavailable: %.95s | ",s.refine_error);
    else if(automatic&&s.view.layer>=0&&s.detail_loading)snprintf(preparation,sizeof(preparation),"Reading layer detail | ");
    snprintf(labelled,sizeof(labelled),"%s | %s%s",!s.solid?"LINES":automatic?"SOLID MESH":surface?"SOLID SURFACES":"SOLID PATHS",preparation,text);
    lv_label_set_text(s.status,labelled);
    if(s.view.layer<0)snprintf(text,sizeof(text),"All %u layers",m->layer_count);
    else snprintf(text,sizeof(text),"%s%d/%u  Z %.2f",s.view.cumulative?"Up to ":"",s.view.layer+1,m->layer_count,(double)m->layer_z[s.view.layer]);
    lv_label_set_text(s.layer,text);lv_label_set_text(s.travel,s.view.travel?"Travel on":"Travel off");
    lv_label_set_text(s.mode,s.view.cumulative?"Up to":"Single");
    lv_slider_set_value(s.slider,s.view.layer<0?(m->layer_count?m->layer_count-1:0):s.view.layer,LV_ANIM_OFF);
    if(m->count&&m->layer_count){
        if(s.view.layer==0)lv_obj_add_state(s.controls[4],LV_STATE_DISABLED);else lv_obj_remove_state(s.controls[4],LV_STATE_DISABLED);
        if(s.view.layer==(int)m->layer_count-1)lv_obj_add_state(s.controls[5],LV_STATE_DISABLED);else lv_obj_remove_state(s.controls[5],LV_STATE_DISABLED);
    }
}
static bool solid_buffers(void)
{
    size_t count=(size_t)s.width*s.height;
    if(!s.depth)s.depth=heap_caps_calloc(count,sizeof(float),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s.depth)return false;
    if(!s.solid_pixels)s.solid_pixels=heap_caps_calloc(count,sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s.solid_pixels){heap_caps_free(s.depth);s.depth=NULL;return false;}
    if(!s.section_mask)s.section_mask=heap_caps_calloc((size_t)((s.width+31)/32)*s.height,sizeof(uint32_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    return true;
}
static bool ready(gcode_toolpath_t *model,const char *error)
{
    if(!s.popup)return false;
    bool file_load=s.loading;s.loading=false;
    if(s.file[0])lv_obj_remove_state(s.reload_button,LV_STATE_DISABLED);
    if(error){lv_label_set_text(s.status,error);return false;}
    if(!model)return false;
    close_details();lv_obj_remove_state(s.details_button,LV_STATE_DISABLED);
    if(model->count)lv_obj_remove_state(s.solid_button,LV_STATE_DISABLED);else lv_obj_add_state(s.solid_button,LV_STATE_DISABLED);
    s.solid_rendering=false;gcode_viewer_loader_free(s.detail);s.detail=NULL;s.detail_loading=false;s.detail_wanted=-1;s.detail_failed=false;
    s.refining=false;s.refine_failed=false;s.refine_error[0]=0;s.refine_pending=file_load&&model->extrusion_seen;
    if(file_load){s.solid=solid_buffers();lv_label_set_text(lv_obj_get_child(s.solid_button,0),s.solid?"Solid":"Lines");}
    s.gesture_frames=s.gesture_slices=0;s.gesture_publish_us=0;s.gesture_frame_us=s.gesture_body_us=s.gesture_material_us=s.gesture_travel_us=0;
    s.frame_us=s.last_raster_us=s.publish_us=s.last_body_us=s.last_material_us=s.last_travel_us=0;
    s.solid_rendering=false;gcode_viewer_loader_free(s.model);s.model=model;gcode_view_reset(&s.view);s.pinch_active=false;s.drag=false;s.dirty=true;
    for(int i=0;i<11;i++){if(!model->count||(((i>=4&&i<=6)||i==10)&&!model->layer_count))lv_obj_add_state(s.controls[i],LV_STATE_DISABLED);else lv_obj_remove_state(s.controls[i],LV_STATE_DISABLED);}
    lv_slider_set_range(s.slider,0,model->layer_count?model->layer_count-1:0);
    if(model->count&&model->layer_count>1)lv_obj_remove_state(s.slider,LV_STATE_DISABLED);else lv_obj_add_state(s.slider,LV_STATE_DISABLED);
    caption();return true;
}
void ui_gcode_viewer_close(void)
{
    gcode_viewer_loader_cancel();lv_obj_t *popup=s.popup;s.popup=NULL;close_details();
    if(s.display)lv_display_remove_event_cb_with_user_data(s.display,display_event,NULL);
    s.display=NULL;
    if(s.timer)lv_timer_delete(s.timer);
    s.timer=NULL;
    if(popup)lv_obj_delete(popup);
    gcode_viewer_loader_free(s.model);gcode_viewer_loader_free(s.detail);heap_caps_free(s.pixels);heap_caps_free(s.depth);heap_caps_free(s.solid_pixels);heap_caps_free(s.section_mask);memset(&s,0,sizeof(s));
}
static void deleted(lv_event_t *e){if(lv_event_get_target_obj(e)==s.popup){s.popup=NULL;ui_gcode_viewer_close();}}
static bool detail_ready(gcode_toolpath_t *model,const char *error){
    s.detail_loading=false;
    if(!s.popup||!s.model)return false;
    if(error){s.detail_failed=true;lv_label_set_text(s.status,error);return false;}
    if(!model||(s.view.cumulative&&!s.solid)||s.view.layer!=model->cached_layer)return false;
    s.solid_rendering=false;gcode_viewer_loader_free(s.detail);s.detail=model;s.dirty=true;caption();return true;
}
static bool refined(mesh_preview_t *mesh,const char *error){
    s.refining=false;
    if(!s.popup||!s.model)return false;
    if(error||!mesh){s.refine_failed=true;snprintf(s.refine_error,sizeof(s.refine_error),"%s",error?error:"Automatic solid unavailable");caption();return false;}
    s.solid_rendering=false;mesh_preview_free(s.model->overview_mesh,release_mesh);s.model->overview_mesh=mesh;s.gesture_frames=s.gesture_slices=0;s.gesture_publish_us=0;s.gesture_frame_us=s.gesture_body_us=s.gesture_material_us=s.gesture_travel_us=0;s.refine_pending=false;s.refine_failed=false;s.dirty=true;caption();return true;
}
static void refinement_tick(void){
    if(s.refining&&s.view.layer>=0&&!s.view.cumulative&&!s.solid){gcode_viewer_loader_cancel();s.refining=false;s.refine_pending=true;}
    if(s.refining&&!gcode_viewer_loader_running()){s.refining=false;s.refine_failed=true;snprintf(s.refine_error,sizeof(s.refine_error),"Automatic preparation interrupted; Reload to retry");caption();}
    if(s.refine_pending&&!s.refining&&!s.refine_failed&&s.model&&(s.solid||s.view.layer<0||s.view.cumulative)&&!gcode_viewer_loader_running()){
        s.refining=gcode_viewer_loader_refine(s.file,s.model,refined);s.refine_pending=false;if(!s.refining){s.refine_failed=true;snprintf(s.refine_error,sizeof(s.refine_error),"Automatic preparation could not start");}caption();
    }
}
static void detail_tick(void){
    /* Keep an already loaded matching layer during a Lines comparison. It is
     * not used by cumulative Lines, but switching back must retain full infill. */
    bool retained=s.detail&&s.detail->cached_layer==s.view.layer;
    int wanted=s.model&&s.model->cache&&s.model->cache->complete&&(s.solid||!s.view.cumulative||retained)?s.view.layer:-1;
    if(wanted!=s.detail_wanted){
        if(s.detail_loading)gcode_viewer_loader_cancel();
        s.detail_loading=false;s.detail_failed=false;s.detail_wanted=wanted;s.detail_due=lv_tick_get()+180;
        s.solid_rendering=false;gcode_viewer_loader_free(s.detail);s.detail=NULL;caption();
    }
    if(s.detail_loading&&!gcode_viewer_loader_running()){s.detail_loading=false;s.detail_failed=true;}
    if(wanted>=0&&!s.detail&&!s.detail_loading&&!s.detail_failed&&(int32_t)(lv_tick_get()-s.detail_due)>=0&&!gcode_viewer_loader_running()){
        s.detail_loading=gcode_viewer_loader_detail(s.model,(unsigned)wanted,detail_ready);
        if(!s.detail_loading)s.detail_failed=true;
        caption();
    }
}
static float layer_floor(const gcode_toolpath_t *m,int layer){
    float high=m->layer_z[layer],step=m->max_height>0?m->max_height:.2f;
    if(layer>0){float gap=high-m->layer_z[layer-1];if(isfinite(gap)&&gap>.00001f&&gap<=5)step=gap;}
    float low=high-step;return low<high?low:nextafterf(high,-INFINITY);
}
static void tick(lv_timer_t *timer)
{
    (void)timer;if(!s.popup)return;
    if(s.owner!=moonraker_config_generation()){ui_gcode_viewer_close();return;}
    if(s.loading&&!gcode_viewer_loader_running()){s.loading=false;if(s.file[0])lv_obj_remove_state(s.reload_button,LV_STATE_DISABLED);lv_label_set_text(s.status,"Load ended without a preview. Use Reload to retry.");}
    if(s.loading&&lv_tick_get()-s.progress_tick>=250){
        gcode_viewer_progress_t state=gcode_viewer_loader_progress();s.progress_tick=lv_tick_get();
        if(state.running&&state.bytes!=s.progress_bytes){
            s.progress_bytes=state.bytes;char text[160];
            snprintf(text,sizeof(text),"Reading %lu KiB | %lu paths | %u layers | Close cancels",(unsigned long)(state.bytes/1024),(unsigned long)state.paths,state.layers);
            lv_label_set_text(s.status,text);
        }
    }
    if(!s.loading){refinement_tick();if(!s.refining)detail_tick();}
    if(s.refining&&lv_tick_get()-s.progress_tick>=250){s.progress_tick=lv_tick_get();caption();}
    bool gesture=s.drag||s.block_one_finger;
    if(s.solid_rendering&&!s.render_interactive&&s.model&&s.model->overview_mesh&&gesture&&s.dirty)s.solid_rendering=false;
    if(s.solid&&!gesture&&((s.display_interactive&&!s.solid_rendering)||(s.solid_rendering&&s.render_interactive)))s.dirty=true;
    bool redraw=s.dirty&&!s.solid_rendering;
    if(redraw){
        s.dirty=false;s.solid_cursor=0;s.travel_cursor=0;s.travel_overlay=false;s.layer_cursor=0;s.layer_overlay=false;s.render_started=esp_timer_get_time();s.raster_us=0;s.body_us=s.material_us=s.travel_us=0;s.render_slices=0;
        s.solid_rendering=s.solid&&s.depth&&s.solid_pixels&&s.model&&s.model->count;
        s.render_view=s.view;
        s.render_model=s.detail&&s.detail->cached_layer==s.view.layer&&!s.view.cumulative?s.detail:s.model;
        s.render_layer_model=s.detail&&s.detail->cached_layer==s.view.layer?s.detail:s.model;
        s.render_mesh=s.model?s.model->overview_mesh:NULL;
        s.render_cut=s.render_mesh&&s.view.layer>=0?s.model->layer_z[s.view.layer]:FLT_MAX;s.render_low=-FLT_MAX;
        if(s.render_mesh&&s.view.layer>=0&&!s.view.cumulative)s.render_low=layer_floor(s.model,s.view.layer);
        s.render_interactive=s.solid_rendering&&s.render_mesh&&gesture;
        s.render_width=s.render_interactive?s.width/2:s.width;s.render_height=s.render_interactive?s.height/2:s.height;s.render_view.raster_ratio=(float)s.render_width/s.width;
        s.layer_bounds[0]=s.render_width;s.layer_bounds[1]=s.render_height;s.layer_bounds[2]=s.layer_bounds[3]=-1;
        /* Up to was still rasterizing the full exterior on every movement frame.
         * Use the same optional shaded gesture mesh as All, clipped at the
         * selected height. Single uses its actual selected-layer material. */
        if(s.render_interactive&&(s.view.layer<0||s.view.cumulative)&&s.render_mesh->interaction)s.render_mesh=s.render_mesh->interaction;
        if(s.render_interactive&&s.view.layer>=0&&!s.view.cumulative){
            /* A coarse mesh can erase a thin Single slab. During movement,
             * draw its actual shaded material directly in the root camera. */
            size_t count=(size_t)s.render_width*s.render_height;
            for(size_t i=0;i<count;i++){s.solid_pixels[i]=0x0863;s.depth[i]=-FLT_MAX;}
            s.layer_overlay=true;
        }
        s.rendering_surface=s.solid_rendering&&!s.render_mesh&&gcode_surface_usable(s.model->surface)&&!s.view.travel&&(s.view.layer<0||s.view.cumulative);
        if(s.solid_rendering&&!s.render_mesh&&s.model->count>2048)lv_label_set_text(s.status,"Rendering solid frame... Controls remain available.");
    }
    if(redraw||s.solid_rendering){
        int64_t start=esp_timer_get_time();bool publish=false;
        if(s.solid_rendering){
            /* Finish a camera snapshot into a private buffer. Incoming gesture
             * positions coalesce into the next frame rather than restarting it.
             * Never expose a cleared or partly filled frame on the canvas. */
            /* Amortize handler/tick handoffs for moving frames. Keep input
             * opportunities between bounded slices, never render a whole frame
             * synchronously. Settled/fallback paths retain their 8 ms budget. */
            bool section_gesture=s.render_interactive&&s.render_view.layer>=0;
            int64_t work_budget=section_gesture?20000:8000;
            unsigned max_batches=section_gesture?32:8;s.render_slices++;
            for(unsigned batch=0;batch<max_batches;batch++){
                int64_t stage_start=esp_timer_get_time();
                bool material_stage=s.render_mesh&&s.layer_overlay,travel_stage=s.render_mesh&&s.travel_overlay;
                if(s.render_mesh){
                    if(s.layer_overlay)s.layer_cursor=(s.render_interactive?mesh_preview_render_layer_gesture_step:mesh_preview_render_layer_step)(s.render_mesh,s.render_layer_model,&s.render_view,s.solid_pixels,s.depth,s.render_width,s.render_height,s.layer_cursor,64,s.layer_bounds);
                    else if(s.travel_overlay)s.travel_cursor=mesh_preview_render_travel_step(s.render_mesh,s.render_model,&s.render_view,s.solid_pixels,s.depth,s.render_width,s.render_height,s.travel_cursor,2048);
                    else if(s.render_view.layer>=0)s.solid_cursor=mesh_preview_render_joined_section_step(s.render_mesh,&s.render_view,s.solid_pixels,s.depth,s.render_width,s.render_height,s.solid_cursor,2048,s.render_low,s.render_cut,s.section_mask);
                    else s.solid_cursor=mesh_preview_render_section_step(s.render_mesh,&s.render_view,s.solid_pixels,s.depth,s.render_width,s.render_height,s.solid_cursor,2048,s.render_cut,NULL);
                    int64_t stage_us=esp_timer_get_time()-stage_start;
                    if(material_stage)s.material_us+=stage_us;else if(travel_stage)s.travel_us+=stage_us;else s.body_us+=stage_us;
                    if(!s.travel_overlay&&!s.layer_overlay&&!s.solid_cursor&&s.render_view.layer>=0){s.layer_overlay=true;s.layer_cursor=0;if(esp_timer_get_time()-start>=work_budget)break;continue;}
                    if(!s.travel_overlay&&!s.solid_cursor&&(!s.layer_overlay||!s.layer_cursor)&&s.render_view.travel){s.layer_overlay=false;s.travel_overlay=true;s.travel_cursor=0;if(esp_timer_get_time()-start>=work_budget)break;continue;}
                }
                else if(s.rendering_surface)s.solid_cursor=gcode_surface_render_step(s.model,&s.render_view,s.solid_pixels,s.depth,s.width,s.height,s.solid_cursor,128);
                else s.solid_cursor=gcode_toolpath_render_solid_step(s.render_model,&s.render_view,s.solid_pixels,s.depth,s.width,s.height,s.solid_cursor,2048);
                if(!s.render_mesh)s.body_us+=esp_timer_get_time()-stage_start;
                if(!s.solid_cursor&&(!s.layer_overlay||!s.layer_cursor)&&(!s.travel_overlay||!s.travel_cursor)){
                    int64_t copy_start=esp_timer_get_time();s.raster_us+=copy_start-start;
                    if(s.render_interactive){
                        for(int y=0;y<s.render_height;y++){const uint16_t *row=s.solid_pixels+(size_t)y*s.render_width;uint16_t *out=s.pixels+(size_t)(2*y)*s.width;for(int x=0;x<s.render_width;x++)out[2*x]=out[2*x+1]=row[x];memcpy(out+s.width,out,(size_t)s.width*sizeof(*out));}
                    }else memcpy(s.pixels,s.solid_pixels,(size_t)s.width*s.height*sizeof(*s.pixels));
                    s.display_interactive=s.render_interactive;s.publish_us=esp_timer_get_time()-copy_start;s.frame_us=esp_timer_get_time()-s.render_started;s.last_raster_us=s.raster_us;s.last_body_us=s.body_us;s.last_material_us=s.material_us;s.last_travel_us=s.travel_us;s.last_slices=s.render_slices;
                    if(s.render_interactive){s.gesture_frame_us=s.frame_us;s.gesture_body_us=s.body_us;s.gesture_material_us=s.material_us;s.gesture_travel_us=s.travel_us;s.gesture_layer=s.render_view.layer;s.gesture_cumulative=s.render_view.cumulative;s.gesture_slices=s.render_slices;s.gesture_publish_us=s.publish_us;s.gesture_frames++;}
                    s.solid_rendering=false;publish=true;if(!s.render_mesh)caption();break;
                }
                if(esp_timer_get_time()-start>=work_budget)break;
            }
            if(s.solid_rendering)s.raster_us+=esp_timer_get_time()-start;
        }else{
            gcode_toolpath_render(s.detail&&s.detail->cached_layer==s.view.layer&&!s.view.cumulative?s.detail:s.model,&s.view,s.pixels,s.width,s.height);
            publish=true;s.display_interactive=false;
        }
        if(publish){
            if(s.model&&s.frames++%30==0)ESP_LOGI("gcode_view","paths=%lu frame_us=%lld canvas=%dx%d",(unsigned long)s.model->count,(long long)(esp_timer_get_time()-s.render_started),s.width,s.height);
            publish_canvas(s.solid&&s.render_mesh&&!s.render_view.travel);
        }
    }
    /* Keep bounded render batches moving instead of idling 33 ms between them. */
    lv_timer_set_period(s.timer,s.solid_rendering||s.dirty?5:33);
}
/* Bed Mesh reference: raw two-point input, 60% scale sensitivity, 45%
 * smoothing, and no return to orbit until every finger has lifted. */
void printerhmi_gcode_viewer_multitouch_update(uint8_t count,int32_t x0,int32_t y0,int32_t x1,int32_t y1)
{
    if(!s.popup)return;
    if(count==0){s.pinch_active=false;s.block_one_finger=false;s.drag=false;if(s.solid&&(s.display_interactive||(s.solid_rendering&&s.render_interactive)))wake_render();return;}
    if(count<2){s.pinch_active=false;return;}
    s.block_one_finger=true;s.drag=false;
    if(!s.model||!s.model->count||s.owner!=moonraker_config_generation())return;
    lv_point_t a={x0,y0},b={x1,y1};
    /* Gestures belong to the visible canvas, never a control or newer modal. */
    if(lv_indev_search_obj(lv_layer_sys(),&a)||lv_indev_search_obj(lv_layer_sys(),&b)||
        lv_indev_search_obj(lv_layer_top(),&a)!=s.canvas||lv_indev_search_obj(lv_layer_top(),&b)!=s.canvas){s.pinch_active=false;return;}
    float dx=(float)x1-(float)x0,dy=(float)y1-(float)y0;
    float distance=sqrtf(dx*dx+dy*dy);if(!isfinite(distance)||distance<12)return;
    if(!s.pinch_active){s.pinch_active=true;s.pinch_distance=distance;s.pinch_base_zoom=s.view.zoom;return;}
    float raw=distance/s.pinch_distance;
    if(raw>.98f&&raw<1.02f)return;
    float target=fmaxf(.25f,fminf(GCODE_VIEW_ZOOM_MAX,s.pinch_base_zoom*(1+(raw-1)*.60f)));
    float delta=target-s.view.zoom;
    float zoom=fmaxf(.25f,fminf(GCODE_VIEW_ZOOM_MAX,s.view.zoom+(fabsf(delta)<.002f?delta:delta*.45f)));
    if(fabsf(zoom-s.view.zoom)>.00001f){lv_area_t canvas;lv_obj_get_coords(s.canvas,&canvas);float ax=((x0+x1)*.5f-canvas.x1)/s.width-.5f,ay=((y0+y1)*.5f-canvas.y1)/s.height-.5f;apply_zoom(zoom,ax,ay);wake_render();}
}
static void pointer(lv_event_t *e)
{
    lv_event_code_t code=lv_event_get_code(e);lv_indev_t *device=lv_indev_active();if(!device)return;
    if(!s.model||!s.model->count||s.block_one_finger){s.drag=false;return;}
    lv_point_t p;lv_indev_get_point(device,&p);
    if(code==LV_EVENT_PRESSED){s.drag=true;s.pointer=p;}
    if(code==LV_EVENT_PRESSING&&s.drag&&(p.x!=s.pointer.x||p.y!=s.pointer.y)){s.view.yaw-=(p.x-s.pointer.x)*.012f;s.view.pitch=fminf(1.5f,fmaxf(.08f,s.view.pitch+(p.y-s.pointer.y)*.008f));s.pointer=p;wake_render();}
    if(code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST){s.drag=false;if(s.solid&&(s.display_interactive||(s.solid_rendering&&s.render_interactive)))wake_render();}
}
static void sample_surface(void *context,const gcode_segment_t *segment){gcode_surface_feed(context,segment);}
static void sample(void)
{
    gcode_viewer_loader_cancel();s.loading=false;s.refining=false;s.refine_pending=false;
    if(s.file[0])lv_obj_remove_state(s.reload_button,LV_STATE_DISABLED);
    size_t capacity=2400;
    gcode_toolpath_t *m=heap_caps_calloc(1,sizeof(*m)+capacity*sizeof(gcode_segment_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    gcode_parser_t *p=heap_caps_calloc(1,sizeof(*p),MALLOC_CAP_8BIT);
    if(!m||!p){heap_caps_free(m);heap_caps_free(p);lv_label_set_text(s.status,"Not enough memory for sample.");return;}
    gcode_parser_init(p,m,(gcode_segment_t *)(m+1),capacity);
    m->surface=heap_caps_calloc(1,sizeof(struct gcode_surface)+GCODE_SURFACE_PANELS_MAX*sizeof(gcode_panel_t)+GCODE_SURFACE_SLOTS*sizeof(uint32_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(m->surface){gcode_panel_t *panels=(gcode_panel_t *)(m->surface+1);gcode_surface_init(m->surface,panels,GCODE_SURFACE_PANELS_MAX,(uint32_t *)(panels+GCODE_SURFACE_PANELS_MAX));p->segment_sink=sample_surface;p->sink_context=m->surface;}
    const char *initial="G90\nM83\nG1 X20 Y20 Z0.2\n";gcode_parser_feed(p,initial,strlen(initial));
    for(int layer=0;layer<60;layer++){
        char line[256];float inset=(float)layer*.10f,z=.2f+(float)layer*.2f;
        int n=snprintf(line,sizeof(line),"G0 X%.2f Y%.2f Z%.2f\nG1 X%.2f Y%.2f E1\nG1 X%.2f Y%.2f E1\nG1 X%.2f Y%.2f E1\nG1 X%.2f Y%.2f E1\n",20+inset,20+inset,z,100-inset,20+inset,100-inset,100-inset,20+inset,100-inset,20+inset,20+inset);
        gcode_parser_feed(p,line,(size_t)n);
    }
    const char *curves="T1\nG0 X70 Y60 Z12\nG3 I-10 J0 E4\nG2 X50 Y60 R10 E2\nG2 X70 Y60 R10 E2\nT0\n";
    gcode_parser_feed(p,curves,strlen(curves));
    gcode_parser_finish(p);if(m->surface)gcode_surface_finish(m->surface);heap_caps_free(p);lv_label_set_text(s.filename,"Offline sample | 60-layer tower + two-tool arcs");if(!ready(m,NULL))gcode_viewer_loader_free(m);
}
static void action(lv_event_t *e)
{
    intptr_t id=(intptr_t)lv_event_get_user_data(e);
    if(id==0){ui_gcode_viewer_close();return;}
    if(id==14){open_details();return;}
    if(id==15){
        if(!s.model||!s.model->count)return;
        if(!s.solid&&!solid_buffers()){lv_label_set_text(s.status,"Not enough PSRAM for solid view. Lines remain available.");return;}
        s.solid_rendering=false;s.solid=!s.solid;lv_label_set_text(lv_obj_get_child(s.solid_button,0),s.solid?"Solid":"Lines");caption();s.dirty=true;return;
    }
    if(id==8){sample();return;}
    if(id==9){
        if(!s.file[0]){lv_label_set_text(s.status,"Select a G-code file in Files, then open 3D view. Sample is available offline.");return;}
        if(s.loading)return;
        s.loading=gcode_viewer_loader_start(s.file,ready);
        if(s.loading){
            s.progress_tick=0;s.progress_bytes=SIZE_MAX;
            close_details();s.solid_rendering=false;gcode_viewer_loader_free(s.detail);s.detail=NULL;s.detail_loading=false;s.detail_wanted=-1;s.refining=false;s.refine_pending=false;s.refine_failed=false;gcode_viewer_loader_free(s.model);s.model=NULL;s.dirty=true;s.pinch_active=false;s.drag=false;
            for(unsigned i=0;i<11;i++)lv_obj_add_state(s.controls[i],LV_STATE_DISABLED);
            lv_obj_add_state(s.slider,LV_STATE_DISABLED);lv_obj_add_state(s.details_button,LV_STATE_DISABLED);lv_obj_add_state(s.reload_button,LV_STATE_DISABLED);lv_obj_add_state(s.solid_button,LV_STATE_DISABLED);
            lv_label_set_text(s.filename,s.file);lv_label_set_text(s.layer,"Loading...");
        }
        lv_label_set_text(s.status,s.loading?"Loading read-only preview... Close cancels.":"Loader busy or file unavailable. Use Reload to retry.");return;
    }
    if(id>=10&&id<=12){
        gcode_view_t preset;gcode_view_reset(&preset);s.view.zoom=1;s.view.pan_x=s.view.pan_y=0;
        s.view.yaw=id==12?preset.yaw:0;
        s.view.pitch=id==10?1.5707963f:id==11?0:preset.pitch;
    }
    if(id==1)apply_zoom(fmaxf(.25f,s.view.zoom/1.25f),0,0);
    if(id==2)apply_zoom(fminf(GCODE_VIEW_ZOOM_MAX,s.view.zoom*1.25f),0,0);
    if(id==3)gcode_view_reset(&s.view);
    if(id==4)s.view.travel=!s.view.travel;
    if(s.model&&s.model->layer_count){
        if(id==5)s.view.layer=s.view.layer<0?s.model->layer_count-1:(s.view.layer>0?s.view.layer-1:0);
        if(id==6)s.view.layer=s.view.layer<0?0:(s.view.layer+1<s.model->layer_count?s.view.layer+1:s.view.layer);
        if(id==7)s.view.layer=-1;
        if(id==13){s.view.cumulative=!s.view.cumulative;if(s.view.layer<0)s.view.layer=s.model->layer_count-1;}
    }
    caption();s.dirty=true;
}
static void layer_changed(lv_event_t *event)
{
    if(lv_obj_get_state(s.slider)&LV_STATE_DISABLED||!s.model||!s.model->layer_count)return;
    if(lv_event_get_target_obj(event)!=s.slider)return;
    int value=lv_slider_get_value(s.slider);
    if(value<0||value>=s.model->layer_count)return;
    s.view.layer=value;caption();s.dirty=true;
}
static lv_obj_t *text(const char *value,int x,int y,int w,const lv_font_t *font)
{
    lv_obj_t *label=lv_label_create(s.popup);lv_label_set_text(label,value);ui_apply_custom_label_style(label,font,UI_TEXT);
    lv_obj_set_pos(label,x,y);lv_obj_set_width(label,w);lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);return label;
}
static lv_obj_t *button(const char *value,int x,int y,int w,int id)
{
    lv_obj_t *b=ui_button_create(s.popup,id==0?UI_BUTTON_CLOSE:UI_BUTTON_SECONDARY,value);
    lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,48);lv_obj_add_event_cb(b,action,LV_EVENT_CLICKED,(void *)(intptr_t)id);
    if((id>=1&&id<=7)||(id>=10&&id<=13)){s.controls[id<=7?id-1:id-3]=b;lv_obj_add_state(b,LV_STATE_DISABLED);}
    return b;
}
void ui_gcode_viewer_open(const char *file)
{
    char selected[256]="";if(file&&strlen(file)<sizeof(selected))memcpy(selected,file,strlen(file)+1);
    ui_gcode_viewer_close();memcpy(s.file,selected,sizeof(selected));s.owner=moonraker_config_generation();gcode_view_reset(&s.view);
    s.popup=ui_popup_create(lv_layer_top(),992,568,UI_POPUP_STANDARD);if(!s.popup)return;
    lv_obj_set_style_pad_all(s.popup,0,0);lv_obj_add_event_cb(s.popup,deleted,LV_EVENT_DELETE,NULL);lv_obj_remove_flag(s.popup,LV_OBJ_FLAG_SCROLLABLE);
    text("3D Preview | Experimental",20,10,600,UI_FONT_TITLE);

    s.solid_button=button("Lines",722,10,112,15);lv_obj_add_state(s.solid_button,LV_STATE_DISABLED);
    s.details_button=button("Details",844,10,128,14);lv_obj_add_state(s.details_button,LV_STATE_DISABLED);
    s.filename=text(s.file[0]?s.file:"Select a file in Files, or explore the offline sample.",20,44,700,UI_FONT_CAPTION);
    s.width=952;s.height=332;
    s.pixels=heap_caps_calloc((size_t)s.width*s.height,sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    s.canvas=lv_canvas_create(s.popup);lv_obj_set_pos(s.canvas,20,76);
    if(!s.pixels){ui_gcode_viewer_close();return;}
    lv_canvas_set_buffer(s.canvas,s.pixels,s.width,s.height,LV_COLOR_FORMAT_RGB565);
    lv_obj_add_flag(s.canvas,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(s.canvas,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s.canvas,pointer,LV_EVENT_ALL,NULL);
    s.status=text("Drag to rotate. Pinch to zoom. Read-only toolpaths.",20,418,952,UI_FONT_CAPTION);
    s.layer=text("All layers",620,464,230,UI_FONT_CAPTION);
    s.slider=lv_slider_create(s.popup);lv_obj_set_pos(s.slider,370,467);lv_obj_set_size(s.slider,230,14);
    lv_obj_set_ext_click_area(s.slider,16);lv_obj_add_state(s.slider,LV_STATE_DISABLED);
    lv_obj_add_event_cb(s.slider,layer_changed,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_t *mode=button("Single",872,450,100,13);s.mode=lv_obj_get_child(mode,0);
    button("Top",20,450,100,10);button("Front",130,450,100,11);button("3D",240,450,100,12);
    button("Close",20,506,90,0);button("-",120,506,48,1);button("+",178,506,48,2);button("Reset",236,506,88,3);
    lv_obj_t *travel=button("Travel off",334,506,120,4);s.travel=lv_obj_get_child(travel,0);
    button("Layer -",464,506,100,5);button("Layer +",574,506,100,6);button("All",684,506,70,7);
    button("Sample",764,506,98,8);lv_obj_t *reload=button("Reload",872,506,100,9);s.reload_button=reload;
    s.display=lv_display_get_default();if(s.display)lv_display_add_event_cb(s.display,display_event,LV_EVENT_ALL,NULL);
    s.timer=lv_timer_create(tick,33,NULL);s.dirty=true;
    if(s.file[0])lv_obj_send_event(reload,LV_EVENT_CLICKED,NULL);
    else lv_obj_add_state(reload,LV_STATE_DISABLED);
}
