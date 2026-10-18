/* Real profile owner with LVGL pointer input and stubbed printer/transport. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "custom_theme.h"
#include "ui_theme.h"
#include "ui_bed_mesh_profiles.c"
static moonraker_state_t live;
static uint32_t generation=1;
static unsigned sends;
static bool accept;
static void (*send_hook)(void);
static char last_command[160];
static uint16_t raster[1024*600];
static lv_point_t touch;
static bool pressed;
bool custom_theme_color_override(uint32_t a,uint32_t b,uint32_t c,uint32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_metric_override(int32_t a,int32_t b,int32_t c,int32_t*d){(void)a;(void)b;(void)c;(void)d;return false;}
bool custom_theme_accent_override(uint8_t a,uint32_t*b){(void)a;(void)b;return false;}
bool custom_theme_surface_opacity(uint8_t*a){(void)a;return false;}
uint32_t moonraker_config_generation(void){return generation;}
void moonraker_state_snapshot(moonraker_state_t*out){*out=live;}
static bool send_command(const char*command){sends++;snprintf(last_command,sizeof(last_command),"%s",command);if(send_hook)send_hook();return accept;}
static void read_touch(lv_indev_t*i,lv_indev_data_t*d){(void)i;d->point=touch;d->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void tap(lv_indev_t*i,lv_obj_t*b){lv_obj_update_layout(lv_layer_top());lv_area_t a;lv_obj_get_coords(b,&a);touch=(lv_point_t){(a.x1+a.x2)/2,(a.y1+a.y2)/2};pressed=true;lv_indev_read(i);pressed=false;lv_indev_read(i);}
static void flush(lv_display_t*d,const lv_area_t*a,uint8_t*p){uint16_t*v=(uint16_t*)p;for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)raster[y*1024+x]=*v++;lv_display_flush_ready(d);}
static void inside(lv_obj_t*c,lv_obj_t*p){lv_area_t a,b;lv_obj_get_coords(c,&a);lv_obj_get_coords(p,&b);if(!(a.x1>=b.x1&&a.x2<=b.x2&&a.y1>=b.y1&&a.y2<=b.y2)){fprintf(stderr,"bounds child %d,%d-%d,%d parent %d,%d-%d,%d text=%s\n",a.x1,a.y1,a.x2,a.y2,b.x1,b.y1,b.x2,b.y2,lv_obj_check_type(c,&lv_label_class)?lv_label_get_text(c):"object");abort();}}
static void layout(lv_obj_t*p){assert(p);lv_obj_update_layout(p);inside(p,lv_screen_active());lv_obj_t*footer=lv_obj_get_child(p,-1);inside(footer,p);if(lv_obj_get_child_count(p)==4)inside(lv_obj_get_child(p,2),p);for(unsigned i=0;i<lv_obj_get_child_count(footer);i++){lv_obj_t*b=lv_obj_get_child(footer,i);inside(b,footer);inside(lv_obj_get_child(b,0),b);assert(lv_obj_get_height(b)>=48);}lv_area_t before,after;lv_obj_get_coords(footer,&before);lv_obj_t*body=ui_form_body(p);lv_obj_scroll_to_y(body,lv_obj_get_scroll_bottom(body),LV_ANIM_OFF);lv_obj_update_layout(p);assert(lv_obj_get_scroll_bottom(body)<=1);lv_obj_get_coords(footer,&after);assert(!memcmp(&before,&after,sizeof(before)));lv_obj_scroll_to_y(body,0,LV_ANIM_OFF);}
static void capture(unsigned theme,const char*name){const char*folder=getenv("MESH_PROFILE_SCREENSHOTS");if(!folder)return;lv_refr_now(NULL);char path[512];snprintf(path,sizeof(path),"%s/mesh-profiles-%s-theme%u.ppm",folder,name,theme);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P6\n1024 600\n255\n");for(unsigned i=0;i<1024*600;i++){uint16_t v=raster[i];unsigned char c[3]={(v>>11)*255/31,((v>>5)&63)*255/63,(v&31)*255/31};fwrite(c,1,3,f);}fclose(f);}
static void reenter_load(void){load_selected_profile_cb(NULL);ui_bed_mesh_profiles_show_cb(NULL);}
static void retire_send(void){lv_obj_delete(s.profile_popup);}
static void switch_send(void){generation++;}
static void reenter_save(void){confirm_save_profile_cb(NULL);}
static void reenter_remove(void){confirm_remove_profile_cb(NULL);}
static void delete_load_feedback(void){lv_obj_delete(s.connection_label);}
static void delete_confirm_feedback(void){lv_obj_delete(s.confirm_connection_label);}
static void replacement_on_delete(lv_event_t *event)
{
 (void)event;ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);
 lv_textarea_set_text(s.profile_name_input,"replacement_kept");save_name_continue_cb(NULL);
}
static void replacement_owner_on_delete(lv_event_t *event)
{
 lv_obj_t *owner=lv_event_get_user_data(event);
 bed_mesh_profile_name_t name="new_owner";
 bed_mesh_snapshot_t mesh={.valid=true,.profile_count=1,.profile_names=&name};
 strcpy(mesh.profile_name,name);
 ui_bed_mesh_profiles_init(owner,send_command);ui_bed_mesh_profiles_update(&mesh);
 ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);
}
static void replacement_editor_on_delete(lv_event_t *event)
{
 (void)event;save_as_profile_cb(NULL);lv_textarea_set_text(s.profile_name_input,"new_editor_kept");
}
static void click(lv_obj_t *object){lv_obj_send_event(object,LV_EVENT_CLICKED,NULL);}
static void field_safety_checks(const bed_mesh_snapshot_t *mesh)
{
 accept=false;send_hook=NULL;unsigned before=sends;
 ui_bed_mesh_profiles_show_cb(NULL);
 lv_obj_t *fake=lv_button_create(s.profile_list);
 lv_obj_add_event_cb(fake,profile_row_cb,LV_EVENT_CLICKED,(void *)(uintptr_t)2);
 int16_t selected=s.selected_profile;click(fake);assert(s.selected_profile==selected);lv_obj_delete(fake);
 lv_obj_t *selected_row=s.rows[selected];lv_obj_delete(selected_row);
 assert(s.selected_profile==-1&&!s.rows[selected]);load_selected_profile_cb(NULL);assert(sends==before);
 close_profile_popup_cb(NULL);ui_bed_mesh_profiles_show_cb(NULL);
 fake=lv_button_create(s.profile_popup);
 lv_obj_add_event_cb(fake,load_selected_profile_cb,LV_EVENT_CLICKED,NULL);
 lv_obj_add_event_cb(fake,save_as_profile_cb,LV_EVENT_CLICKED,NULL);
 lv_obj_add_event_cb(fake,remove_selected_profile_cb,LV_EVENT_CLICKED,NULL);
 click(fake);assert(sends==before&&!s.profile_editor&&!s.profile_confirm);lv_obj_delete(fake);
 for(unsigned i=0;i<3;++i) {
  lv_obj_add_state(s.actions[i],LV_STATE_DISABLED);
  if(i==0)save_as_profile_cb(NULL);else if(i==1)load_selected_profile_cb(NULL);else remove_selected_profile_cb(NULL);
  assert(sends==before&&!s.profile_editor&&!s.profile_confirm);
  lv_obj_remove_state(s.actions[i],LV_STATE_DISABLED);
 }
 send_hook=delete_load_feedback;load_selected_profile_cb(NULL);send_hook=NULL;
 assert(sends==++before&&s.profile_popup&&!s.connection_label);
 ui_bed_mesh_profiles_refresh_availability();load_selected_profile_cb(NULL);assert(sends==++before);
 close_profile_popup_cb(NULL);
 for(unsigned field=0;field<3;++field) {
  ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);
  lv_textarea_set_text(s.profile_name_input,"field_deleted");
  if(field==0) {
   lv_obj_t *keyboard=lv_obj_get_child(s.profile_editor,2);lv_obj_delete(s.profile_name_input);
   assert(!s.profile_name_input&&!lv_keyboard_get_textarea(keyboard));
  } else if(field==1)lv_obj_delete(s.editor_continue);
  else lv_obj_delete(s.editor_feedback);
  save_name_continue_cb(NULL);
  if(field<2)assert(s.profile_editor&&!s.profile_confirm);
  else assert(s.profile_confirm&&!s.profile_editor);
  assert(sends==before);close_profile_popup_cb(NULL);
 }
 for(unsigned action=0;action<4;++action) {
  ui_bed_mesh_profiles_show_cb(NULL);
  if(action==3)remove_selected_profile_cb(NULL);
  lv_obj_delete(s.actions[action]);assert(!s.actions[action]);
  if(action==0)save_as_profile_cb(NULL);else if(action==1)load_selected_profile_cb(NULL);
  else if(action==2)remove_selected_profile_cb(NULL);else confirm_remove_profile_cb(NULL);
  assert(sends==before);close_profile_popup_cb(NULL);
 }
 ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);
 lv_textarea_set_text(s.profile_name_input,"reviewed_save");save_name_continue_cb(NULL);
 confirm_remove_profile_cb(NULL);assert(sends==before); /* Wrong review operation. */
 send_hook=delete_confirm_feedback;confirm_save_profile_cb(NULL);send_hook=NULL;
 assert(sends==++before&&s.profile_confirm&&!s.confirm_connection_label);
 ui_bed_mesh_profiles_refresh_availability();close_profile_popup_cb(NULL);
 ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);
 lv_textarea_set_text(s.profile_name_input,"retired_editor_value");
 lv_obj_add_event_cb(s.profile_editor,replacement_editor_on_delete,LV_EVENT_DELETE,NULL);
 save_name_continue_cb(NULL);
 assert(s.profile_editor&&!s.profile_confirm&&!strcmp(lv_textarea_get_text(s.profile_name_input),"new_editor_kept"));
 close_profile_popup_cb(NULL);
 ui_bed_mesh_profiles_show_cb(NULL);lv_obj_delete(s.profile_list);assert(!s.profile_list);
 ui_bed_mesh_profiles_refresh_availability();close_profile_popup_cb(NULL);
 /* Old editor teardown can open a complete replacement review. */
 ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);
 lv_obj_add_event_cb(s.profile_editor,replacement_on_delete,LV_EVENT_DELETE,NULL);
 close_profile_popup_cb(NULL);
 assert(s.profile_popup&&s.profile_confirm&&!strcmp(s.pending_profile_name,"replacement_kept"));
 assert(s.confirm_connection_label&&s.actions[3]);close_profile_popup_cb(NULL);
 /* Full owner retirement must finish before callbacks initialize a new owner. */
 lv_obj_t *original_owner=s.owner,*replacement_owner=lv_obj_create(lv_screen_active());
 ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);
 lv_obj_add_event_cb(s.profile_editor,replacement_owner_on_delete,LV_EVENT_DELETE,replacement_owner);
 ui_bed_mesh_profiles_close();
 assert(s.owner==replacement_owner&&s.profile_popup&&s.profile_editor&&s.command==send_command);
 lv_obj_delete(replacement_owner);assert(!s.owner&&!s.profile_popup&&!s.profile_editor);
 ui_bed_mesh_profiles_init(original_owner,send_command);ui_bed_mesh_profiles_update(mesh);
 assert(sends==before);
}
int main(void){
 lv_init();lv_display_t*d=lv_display_create(1024,600);static uint8_t pixels[1024*50*2];lv_display_set_buffers(d,pixels,NULL,sizeof(pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);lv_display_set_flush_cb(d,flush);lv_indev_t*pointer=lv_indev_create();lv_indev_set_type(pointer,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(pointer,read_touch);
 for(unsigned theme=0;theme<5;theme++)for(unsigned density=0;density<3;density++)for(unsigned large=0;large<2;large++)for(unsigned narrow=0;narrow<3;narrow++){
  lv_display_set_resolution(d,(int[]){1024,640,480}[narrow],narrow==2?400:600);ui_theme_set_active(theme);ui_theme_set_density(density);ui_theme_set_accessibility((ui_accessibility_t){.large_text=large});
  live=(moonraker_state_t){.moonraker_ok=true,.live_data_ok=true};strcpy(live.printer_state,"ready");accept=false;send_hook=NULL;
  lv_obj_t*owner=lv_obj_create(lv_screen_active());ui_bed_mesh_profiles_init(owner,send_command);
  bed_mesh_profile_name_t names[BED_MESH_MAX_PROFILES];for(unsigned i=0;i<BED_MESH_MAX_PROFILES;i++)snprintf(names[i],sizeof(names[i]),"plate_%02u_with_a_long_descriptive_name",i);
  bed_mesh_snapshot_t mesh={.valid=true,.profile_count=BED_MESH_MAX_PROFILES,.profile_names=names};strcpy(mesh.profile_name,names[0]);
  ui_bed_mesh_profiles_update(&mesh);ui_bed_mesh_profiles_show_cb(NULL);assert(s.profile_popup&&s.selected_profile==0);layout(s.profile_popup);
  for(unsigned i=0;i<lv_obj_get_child_count(s.profile_list);i++){
   lv_obj_t*row=lv_obj_get_child(s.profile_list,i),*name=lv_obj_get_child(row,0);
   inside(name,row);assert(!strcmp(lv_label_get_text(name),names[i]));
   assert(lv_label_get_long_mode(name)==LV_LABEL_LONG_WRAP);
   if(i==0){lv_obj_t *badge=lv_obj_get_child(row,1);inside(badge,row);assert(!strcmp(lv_label_get_text(badge),"Active"));
    lv_area_t a,b;lv_obj_get_coords(name,&a);lv_obj_get_coords(badge,&b);assert(a.x2<b.x1);}
  }
  lv_obj_t*initial=s.profile_popup;ui_bed_mesh_profiles_show_cb(NULL);assert(s.profile_popup==initial);
  if(!narrow&&density==1&&large)capture(theme,"list");
  unsigned before=sends;tap(pointer,s.actions[1]);assert(sends==before+1&&s.profile_popup==initial&&strstr(lv_label_get_text(s.connection_label),"not accepted"));
  /* Reordering a live snapshot must not change the name bound to a visible row. */
  char original[BED_MESH_PROFILE_NAME_MAX];strcpy(original,names[0]);char tmp[BED_MESH_PROFILE_NAME_MAX];strcpy(tmp,names[0]);strcpy(names[0],names[1]);strcpy(names[1],tmp);ui_bed_mesh_profiles_update(&mesh);assert(strstr(lv_label_get_text(s.connection_label),"not accepted"));
  accept=true;send_hook=reenter_load;load_selected_profile_cb(NULL);assert(sends==before+2&&!s.profile_popup);char expected[160];snprintf(expected,sizeof(expected),"BED_MESH_PROFILE LOAD=%s",original);assert(!strcmp(last_command,expected));send_hook=NULL;
  ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);assert(s.profile_editor);layout(s.profile_editor);lv_obj_t*editor=s.profile_editor;save_as_profile_cb(NULL);assert(s.profile_editor==editor);
  save_name_continue_cb(NULL);assert(s.profile_editor&&strstr(lv_label_get_text(s.editor_feedback),"Enter a name"));lv_textarea_set_text(s.profile_name_input,"textured_plate");
  if(!narrow&&density==1&&large)capture(theme,"editor");
  tap(pointer,lv_obj_get_child(lv_obj_get_child(s.profile_editor,-1),1));assert(s.profile_confirm&&!s.profile_editor);layout(s.profile_confirm);assert(!strcmp(s.pending_profile_name,"textured_plate"));
  if(!narrow&&density==1&&large)capture(theme,"confirm");
  accept=false;before=sends;tap(pointer,s.actions[3]);assert(sends==before+1&&s.profile_confirm&&strstr(lv_label_get_text(s.confirm_connection_label),"not accepted"));
  live.moonraker_ok=false;ui_bed_mesh_profiles_refresh_availability();assert(lv_obj_has_state(s.actions[3],LV_STATE_DISABLED));confirm_save_profile_cb(NULL);assert(sends==before+1);live.moonraker_ok=true;ui_bed_mesh_profiles_refresh_availability();assert(!lv_obj_has_state(s.actions[3],LV_STATE_DISABLED)&&!lv_label_get_text(s.confirm_connection_label)[0]);
  accept=true;send_hook=reenter_save;confirm_save_profile_cb(NULL);assert(sends==before+2&&!s.profile_confirm&&!s.profile_popup&&!strcmp(last_command,"BED_MESH_PROFILE SAVE=textured_plate\nSAVE_CONFIG"));send_hook=NULL;
  ui_bed_mesh_profiles_show_cb(NULL);remove_selected_profile_cb(NULL);assert(s.profile_confirm);layout(s.profile_confirm);if(!narrow&&density==1&&large)capture(theme,"remove");char removal[BED_MESH_PROFILE_NAME_MAX];strcpy(removal,s.pending_profile_name);lv_obj_t*confirm=s.profile_confirm;remove_selected_profile_cb(NULL);assert(s.profile_confirm==confirm);
  accept=false;before=sends;confirm_remove_profile_cb(NULL);assert(sends==before+1&&s.profile_confirm);accept=true;send_hook=reenter_remove;confirm_remove_profile_cb(NULL);snprintf(expected,sizeof(expected),"BED_MESH_PROFILE REMOVE=%s\nSAVE_CONFIG",removal);assert(sends==before+2&&!s.profile_popup&&!strcmp(last_command,expected));send_hook=NULL;
  /* Removed selected profile, inactive mesh and stale printer cannot dispatch. */
  ui_bed_mesh_profiles_show_cb(NULL);mesh.profile_count=0;ui_bed_mesh_profiles_update(&mesh);before=sends;load_selected_profile_cb(NULL);remove_selected_profile_cb(NULL);assert(sends==before&&!s.profile_confirm&&lv_obj_has_state(s.actions[1],LV_STATE_DISABLED));mesh.valid=false;ui_bed_mesh_profiles_update(&mesh);save_as_profile_cb(NULL);assert(!s.profile_editor);
  mesh.valid=true;mesh.profile_count=BED_MESH_MAX_PROFILES;ui_bed_mesh_profiles_update(&mesh);close_profile_popup_cb(NULL);
  /* Empty manager still offers Save As for an available current mesh. */
  mesh.profile_count=0;ui_bed_mesh_profiles_update(&mesh);ui_bed_mesh_profiles_show_cb(NULL);assert(!s.displayed_count&&lv_obj_get_child_count(s.profile_list)==1&&!lv_obj_has_state(s.actions[0],LV_STATE_DISABLED));layout(s.profile_popup);close_profile_popup_cb(NULL);
  mesh.profile_count=BED_MESH_MAX_PROFILES;ui_bed_mesh_profiles_update(&mesh);ui_bed_mesh_profiles_show_cb(NULL);
  lv_obj_t*row=lv_obj_get_child(s.profile_list,0);lv_obj_scroll_to_view_recursive(row,LV_ANIM_OFF);tap(pointer,row);assert(s.selected_profile==0);assert(lv_obj_get_style_pad_left(row,0)==16);
  before=sends;strcpy(live.printer_state,"printing");ui_bed_mesh_profiles_refresh_availability();load_selected_profile_cb(NULL);assert(sends==before&&lv_obj_has_state(s.actions[1],LV_STATE_DISABLED));strcpy(live.printer_state,"paused");load_selected_profile_cb(NULL);assert(sends==before);strcpy(live.printer_state,"ready");ui_bed_mesh_profiles_refresh_availability();assert(!lv_obj_has_state(s.actions[1],LV_STATE_DISABLED));
  save_as_profile_cb(NULL);layout(s.profile_editor);tap(pointer,lv_obj_get_child(lv_obj_get_child(s.profile_editor,-1),0));assert(!s.profile_editor&&s.profile_popup);
  remove_selected_profile_cb(NULL);layout(s.profile_confirm);tap(pointer,lv_obj_get_child(lv_obj_get_child(s.profile_confirm,-1),0));assert(!s.profile_confirm&&!s.pending_profile_name[0]);close_profile_popup_cb(NULL);
  for(unsigned i=0;i<10;i++){
   ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);lv_obj_delete(s.profile_editor);assert(!s.profile_editor&&!s.profile_name_input&&!s.editor_feedback);save_name_continue_cb(NULL);
   remove_selected_profile_cb(NULL);assert(s.profile_confirm);lv_obj_delete(s.profile_confirm);assert(!s.profile_confirm&&!s.pending_profile_name[0]&&!s.actions[3]);confirm_remove_profile_cb(NULL);
   save_as_profile_cb(NULL);lv_obj_delete(s.profile_popup);assert(!s.profile_popup&&!s.profile_editor&&!s.profile_list&&!s.connection_label);ui_bed_mesh_profiles_refresh_availability();load_selected_profile_cb(NULL);
  }
  ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);lv_textarea_set_text(s.profile_name_input,"retire_during_send");save_name_continue_cb(NULL);before=sends;send_hook=retire_send;confirm_save_profile_cb(NULL);assert(sends==before+1&&!s.profile_popup&&!s.profile_confirm);send_hook=NULL;
  ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);lv_textarea_set_text(s.profile_name_input,"printer_changed");save_name_continue_cb(NULL);before=sends;send_hook=switch_send;confirm_save_profile_cb(NULL);assert(sends==before+1&&s.profile_confirm);send_hook=NULL;confirm_save_profile_cb(NULL);assert(sends==before+1);ui_bed_mesh_profiles_close();
  ui_bed_mesh_profiles_init(owner,send_command);ui_bed_mesh_profiles_update(&mesh);field_safety_checks(&mesh);ui_bed_mesh_profiles_show_cb(NULL);save_as_profile_cb(NULL);lv_obj_delete(owner);assert(!s.owner&&!s.profile_popup&&!s.profile_editor&&!lv_obj_get_child_count(lv_layer_top()));
 }
 lv_indev_delete(pointer);lv_display_delete(d);lv_deinit();puts("PASS: Bed Mesh profile native layouts, exact command/retry, owned/disabled/deleted actions and rows, retired inputs/feedback, wrong-operation rejection, replacement teardown and printer changes across five themes/densities/text/viewports");
}
