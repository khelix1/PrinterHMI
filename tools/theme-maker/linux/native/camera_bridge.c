/* Compile the actual viewer, exposing only fullscreen Escape for desktop use. */
#include "ui_camera.c"
void host_camera_show(void){ui_camera_show();}
void host_camera_close(void){ui_camera_destroy();}
bool host_camera_escape(void){if(!s_fullscreen)return false;ui_camera_exit_fullscreen();return true;}

bool host_camera_inert_image(lv_obj_t *obj){return obj==s_image&&!s_fullscreen;}

bool host_camera_foreground_escape(lv_obj_t *foreground){if(!s_fullscreen||foreground!=s_card)return false;return host_camera_escape();}
