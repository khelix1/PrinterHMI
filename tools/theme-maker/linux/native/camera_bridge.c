/* Compile the actual viewer, exposing only fullscreen Escape for desktop use. */
#include "ui_camera.c"
void host_camera_show(void){ui_camera_show();}
void host_camera_close(void){ui_camera_destroy();}
bool host_camera_escape(void){if(!s_fullscreen)return false;camera_set_viewport(false);return true;}

bool host_camera_inert_image(lv_obj_t *obj){return obj==s_image&&!s_fullscreen;}
