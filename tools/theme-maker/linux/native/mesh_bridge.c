/* The shared viewer also owns an in-page calibration confirmation. */
#include "ui_bed_mesh.c"
void host_mesh_refresh(void);
bool host_mesh_send(const char *);
void host_mesh_sender(bool (*)(const char *));
void host_mesh_show(bool (*sender)(const char *)){host_mesh_sender(sender);host_mesh_refresh();ui_bed_mesh_show(host_mesh_send);}
bool host_mesh_interactive(lv_obj_t *obj){return obj&&obj==s.canvas;}
bool host_mesh_escape(void){
 if(!s.calibrate_confirm)return false;
 for(unsigned i=lv_obj_get_child_count(lv_layer_top());i>0;i--){lv_obj_t *obj=lv_obj_get_child(lv_layer_top(),i-1);if(lv_obj_has_flag(obj,LV_OBJ_FLAG_HIDDEN))continue;if(obj!=s.calibrate_confirm)return false;close_calibrate_confirm_cb(NULL);return true;}
 return false;
}
