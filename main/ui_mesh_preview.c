#include "ui_mesh_preview.h"
#include "mesh_preview_loader.h"
#include "ui_popup.h"
#include "ui_button.h"
#include "ui_widgets.h"
#include "moonraker_config_controller.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <dirent.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static struct {
    lv_obj_t *popup,*canvas,*status,*filename,*chooser,*details,*solid_label,*controls[8];
    lv_timer_t *timer;
    mesh_preview_t *mesh;
    uint16_t *pixels;float *depth;
    gcode_view_t view;
    lv_point_t pointer;
    bool dirty,rendering,loading,solid,drag,pinch,block;
    float span,base_zoom;
    uint32_t owner,frames;
    size_t cursor;
    char selected[81],name[81],files[32][81];
    unsigned file_count;
} s;
enum{WIDTH=952,HEIGHT=412};
static void chooser(void);
static void action(lv_event_t *);
static void close_modal(lv_obj_t **slot){lv_obj_t *p=*slot;*slot=NULL;if(p)lv_obj_delete(p);}
static void modal_deleted(lv_event_t *e){lv_obj_t *p=lv_event_get_target_obj(e);if(p==s.chooser)s.chooser=NULL;if(p==s.details)s.details=NULL;}
static void modal_close(lv_event_t *e){lv_obj_t *p=lv_event_get_user_data(e);if(p==s.chooser)close_modal(&s.chooser);else if(p==s.details)close_modal(&s.details);}
static lv_obj_t *label(lv_obj_t *parent,const char *value,int x,int y,int w,const lv_font_t *font){lv_obj_t *o=lv_label_create(parent);lv_label_set_text(o,value);ui_apply_custom_label_style(o,font,UI_TEXT);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);return o;}
static lv_obj_t *button(lv_obj_t *parent,const char *value,int x,int y,int w,lv_event_cb_t cb,void *data){lv_obj_t *b=ui_button_create(parent,UI_BUTTON_SECONDARY,value);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,48);lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,data);return b;}
static bool has_mesh(void){return s.mesh&&s.mesh->triangles;}
static void caption(void){
    if(!has_mesh())return;
    char text[240];snprintf(text,sizeof(text),"MODEL | %lu triangles, %lu vertices | %s | Drag to orbit, pinch to zoom",(unsigned long)s.mesh->triangles,(unsigned long)s.mesh->vertices,s.solid?"Shaded":"Edges");lv_label_set_text(s.status,text);
}
static bool ready(mesh_preview_t *m,const char *error){
    if(!s.popup)return false;
    s.loading=false;
    if(error){lv_label_set_text(s.status,error);return false;}
    if(!m)return false;
    if(!s.pixels)s.pixels=heap_caps_calloc((size_t)WIDTH*HEIGHT,sizeof(uint16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s.depth)s.depth=heap_caps_calloc((size_t)WIDTH*HEIGHT,sizeof(float),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s.pixels||!s.depth){lv_label_set_text(s.status,"Not enough PSRAM for the mesh canvas. Close and retry with a smaller model.");return false;}
    close_modal(&s.details);mesh_preview_loader_free(s.mesh);s.mesh=m;memcpy(s.name,s.selected,sizeof(s.name));
    for(size_t i=0;i<(size_t)WIDTH*HEIGHT;i++)s.pixels[i]=0x0863;
    lv_canvas_set_buffer(s.canvas,s.pixels,WIDTH,HEIGHT,LV_COLOR_FORMAT_RGB565);lv_label_set_text(s.filename,s.name);
    for(unsigned i=0;i<8;i++)lv_obj_remove_state(s.controls[i],LV_STATE_DISABLED);
    gcode_view_reset(&s.view);s.solid=true;lv_label_set_text(s.solid_label,"Solid");s.pinch=s.drag=s.block=false;s.dirty=true;caption();return true;
}
static void choose_file(lv_event_t *e){
    unsigned index=(unsigned)(uintptr_t)lv_event_get_user_data(e);if(index>=s.file_count)return;
    if(mesh_preview_loader_running()){lv_label_set_text(s.status,"Previous mesh read is finishing. Close the chooser and retry shortly.");close_modal(&s.chooser);return;}
    memcpy(s.selected,s.files[index],sizeof(s.selected));
    s.loading=mesh_preview_loader_start(s.selected,ready);
    close_modal(&s.chooser);lv_label_set_text(s.status,s.loading?"Reading SD model... Close cancels. Existing model stays available.":"Mesh loader unavailable. Use Models to retry.");
}
static int compare_names(const void *a,const void *b){return strcmp(a,b);}
static void chooser(void){
    if(s.chooser)return;
    close_modal(&s.details);
    s.chooser=ui_popup_create(lv_layer_top(),760,440,UI_POPUP_STANDARD);if(!s.chooser)return;
    lv_obj_set_style_pad_all(s.chooser,0,0);lv_obj_add_event_cb(s.chooser,modal_deleted,LV_EVENT_DELETE,NULL);lv_obj_remove_flag(s.chooser,LV_OBJ_FLAG_SCROLLABLE);
    label(s.chooser,"SD models",20,12,700,UI_FONT_TITLE);
    lv_obj_t *list=lv_obj_create(s.chooser);ui_apply_surface_role(list,UI_SURFACE_TRANSPARENT);lv_obj_set_pos(list,20,60);lv_obj_set_size(list,720,300);lv_obj_set_style_pad_all(list,0,0);lv_obj_set_scroll_dir(list,LV_DIR_VER);
    s.file_count=0;DIR *dir=opendir(MESH_PREVIEW_DIRECTORY);
    if(dir){struct dirent *entry;unsigned scanned=0;while(scanned++<256&&s.file_count<32&&(entry=readdir(dir))){if(mesh_preview_name_valid(entry->d_name)){snprintf(s.files[s.file_count],81,"%.80s",entry->d_name);s.file_count++;}}closedir(dir);}
    qsort(s.files,s.file_count,sizeof(s.files[0]),compare_names);
    for(unsigned i=0;i<s.file_count;i++)button(list,s.files[i],0,(int)i*56,690,choose_file,(void *)(uintptr_t)i);
    if(!s.file_count)label(list,"No models found. Copy .phm files into /PrinterHMI/meshes/ on the SD card. Use the standalone STL converter or its demo mesh.",8,8,680,UI_FONT_BODY);
    button(s.chooser,"Close",612,376,128,modal_close,s.chooser);
}
static void details(void){
    if(s.details||!s.mesh)return;
    close_modal(&s.chooser);
    s.details=ui_popup_create(lv_layer_top(),760,440,UI_POPUP_STANDARD);if(!s.details)return;
    lv_obj_set_style_pad_all(s.details,0,0);lv_obj_add_event_cb(s.details,modal_deleted,LV_EVENT_DELETE,NULL);lv_obj_remove_flag(s.details,LV_OBJ_FLAG_SCROLLABLE);label(s.details,"Model details",20,12,700,UI_FONT_TITLE);
    lv_obj_t *body=lv_obj_create(s.details);ui_apply_surface_role(body,UI_SURFACE_TRANSPARENT);lv_obj_set_pos(body,20,60);lv_obj_set_size(body,720,300);lv_obj_set_scroll_dir(body,LV_DIR_VER);
    char text[1200];mesh_preview_t *m=s.mesh;snprintf(text,sizeof(text),"%s\n\n%lu display triangles | %lu vertices\nSource: %lu STL facets\nFile size: %lu bytes\nBounds: %.2f x %.2f x %.2f mm\nClustering grid: %.5f mm\n\nThis is standalone model geometry, not a linked G-code preview. Supports, infill and print progress are not automatically included. Coordinates are interpreted as millimetres.\n\nThe converter leaves the source STL and G-code unchanged. Grid simplification can remove small features or change topology; 0 mm means no grid simplification. Quantized coordinates and smooth/crease normals are still used.\n\nDrag/pinch uses a sampled wireframe; release restores the full selected mesh. All model processing and display stay on the tablet after conversion. No printer command is sent.",s.name,(unsigned long)m->triangles,(unsigned long)m->vertices,(unsigned long)m->source_triangles,(unsigned long)(80+m->payload_bytes),(double)(m->max[0]-m->min[0]),(double)(m->max[1]-m->min[1]),(double)(m->max[2]-m->min[2]),(double)m->cell_mm);label(body,text,4,4,680,UI_FONT_BODY);
    button(s.details,"Close",612,376,128,modal_close,s.details);
}
void ui_mesh_preview_close(void){
    mesh_preview_loader_cancel();lv_obj_t *popup=s.popup;s.popup=NULL;close_modal(&s.chooser);close_modal(&s.details);
    if(s.timer)lv_timer_delete(s.timer);
    s.timer=NULL;if(popup)lv_obj_delete(popup);
    mesh_preview_loader_free(s.mesh);heap_caps_free(s.pixels);heap_caps_free(s.depth);memset(&s,0,sizeof(s));
}
bool ui_mesh_preview_active(void){return s.popup!=NULL;}
static void deleted(lv_event_t *e){if(lv_event_get_target_obj(e)==s.popup){s.popup=NULL;ui_mesh_preview_close();}}
static void tick(lv_timer_t *timer){
    (void)timer;if(!s.popup)return;if(s.owner!=moonraker_config_generation()){ui_mesh_preview_close();return;}
    if(s.loading&&!mesh_preview_loader_running()){s.loading=false;lv_label_set_text(s.status,"Mesh read ended. Use Models to retry.");}
    if(!has_mesh())return;
    if(s.dirty){s.dirty=false;s.cursor=0;s.rendering=true;}
    if(!s.rendering)return;
    int64_t start=esp_timer_get_time();
    if(s.drag||s.block){mesh_preview_render_wire(s.mesh,&s.view,s.pixels,WIDTH,HEIGHT,2000);s.rendering=false;lv_label_set_text(s.status,"Orbit/zoom preview: sampled edges. Release for full detail.");}
    else{
        s.cursor=s.solid?mesh_preview_render_step(s.mesh,&s.view,s.pixels,s.depth,WIDTH,HEIGHT,s.cursor,2048):mesh_preview_render_edges_step(s.mesh,&s.view,s.pixels,s.depth,WIDTH,HEIGHT,s.cursor,2048);
        if(!s.cursor){s.rendering=false;caption();}else{char text[120];snprintf(text,sizeof(text),"Drawing model: %lu%% | Controls and Close remain available",(unsigned long)(100*s.cursor/(s.mesh->vertices+s.mesh->triangles)));lv_label_set_text(s.status,text);}
    }
    if(s.frames++%30==0)ESP_LOGI("mesh_view","triangles=%lu chunk_us=%lld",(unsigned long)s.mesh->triangles,(long long)(esp_timer_get_time()-start));
    lv_obj_invalidate(s.canvas);
}
void ui_mesh_preview_multitouch_update(uint8_t count,int32_t x0,int32_t y0,int32_t x1,int32_t y1){
    if(!s.popup)return;
    if(!count){if(s.block||s.drag)s.dirty=true;s.pinch=s.block=s.drag=false;return;}if(count<2){s.pinch=false;return;}
    s.block=true;s.drag=false;if(!has_mesh())return;
    lv_point_t a={x0,y0},b={x1,y1};if(lv_indev_search_obj(lv_layer_sys(),&a)||lv_indev_search_obj(lv_layer_sys(),&b)||lv_indev_search_obj(lv_layer_top(),&a)!=s.canvas||lv_indev_search_obj(lv_layer_top(),&b)!=s.canvas){s.pinch=false;return;}
    float dx=x1-x0,dy=y1-y0,d=hypotf(dx,dy);if(d<12||!isfinite(d))return;
    if(!s.pinch){s.pinch=true;s.span=d;s.base_zoom=s.view.zoom;return;}float ratio=d/s.span;if(ratio>.98f&&ratio<1.02f)return;
    float target=fmaxf(.25f,fminf(4,s.base_zoom*(1+(ratio-1)*.6f)));s.view.zoom=fmaxf(.25f,fminf(4,s.view.zoom+(target-s.view.zoom)*.45f));s.dirty=true;
}
static void pointer(lv_event_t *e){
    lv_indev_t *device=lv_indev_active();if(!device||!has_mesh()||s.block)return;
    lv_event_code_t code=lv_event_get_code(e);lv_point_t p;lv_indev_get_point(device,&p);
    if(code==LV_EVENT_PRESSED){s.drag=true;s.pointer=p;s.dirty=true;}
    if(code==LV_EVENT_PRESSING&&s.drag){s.view.yaw-=(p.x-s.pointer.x)*.008f;s.view.pitch=fminf(1.5f,fmaxf(.08f,s.view.pitch+(p.y-s.pointer.y)*.008f));s.pointer=p;s.dirty=true;}
    if(code==LV_EVENT_RELEASED||code==LV_EVENT_PRESS_LOST){s.drag=false;s.dirty=true;}
}
static void action(lv_event_t *e){
    unsigned id=(unsigned)(uintptr_t)lv_event_get_user_data(e);if(id==0){ui_mesh_preview_close();return;}if(id==8){chooser();return;}if(id==9){details();return;}if(!has_mesh())return;
    if(id==1)s.view.zoom=fmaxf(.25f,s.view.zoom/1.25f);
    if(id==2)s.view.zoom=fminf(4,s.view.zoom*1.25f);
    if(id==3)gcode_view_reset(&s.view);
    if(id>=4&&id<=6){gcode_view_reset(&s.view);if(id!=6){s.view.yaw=0;s.view.pitch=id==4?1.5707963f:0;}}
    if(id==7){s.solid=!s.solid;lv_label_set_text(s.solid_label,s.solid?"Solid":"Edges");}s.dirty=true;
}
void ui_mesh_preview_open(void){
    if(s.popup){chooser();return;}
    s.owner=moonraker_config_generation();s.solid=true;gcode_view_reset(&s.view);
    s.popup=ui_popup_create(lv_layer_top(),992,568,UI_POPUP_STANDARD);if(!s.popup)return;
    lv_obj_set_style_pad_all(s.popup,0,0);lv_obj_remove_flag(s.popup,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(s.popup,deleted,LV_EVENT_DELETE,NULL);
    label(s.popup,"Solid model preview",20,10,940,UI_FONT_TITLE);s.filename=label(s.popup,"Standalone SD model | Choose Models to open a .phm file",20,44,940,UI_FONT_CAPTION);
    s.canvas=lv_canvas_create(s.popup);lv_obj_set_pos(s.canvas,20,76);lv_obj_set_size(s.canvas,WIDTH,HEIGHT);lv_obj_add_flag(s.canvas,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(s.canvas,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(s.canvas,pointer,LV_EVENT_ALL,NULL);
    s.status=label(s.popup,"Select a model. No printer commands are sent.",20,490,952,UI_FONT_CAPTION);
    lv_label_set_long_mode(s.status,LV_LABEL_LONG_DOT);lv_obj_set_height(s.status,22);lv_label_set_long_mode(s.filename,LV_LABEL_LONG_DOT);lv_obj_set_height(s.filename,24);
    static const char *names[]={"Close","-","+","Reset","Top","Front","3D","Solid","Models","Details"};
    static const int x[]={20,120,178,236,334,424,514,594,704,824},width[]={90,48,48,88,80,80,70,100,110,148};
    for(unsigned i=0;i<10;i++){lv_obj_t *b=button(s.popup,names[i],x[i],514,width[i],action,(void *)(uintptr_t)i);if(i>=1&&i<=7){s.controls[i-1]=b;lv_obj_add_state(b,LV_STATE_DISABLED);}if(i==9){s.controls[7]=b;lv_obj_add_state(b,LV_STATE_DISABLED);}if(i==7)s.solid_label=lv_obj_get_child(b,0);}
    s.timer=lv_timer_create(tick,33,NULL);chooser();
}
