/* Compile the shared Camera setup dialog; all persistence/transport is mocked. */
#include "ui_printer_profiles.c"
void host_profiles_close(void){ui_printer_profiles_close_all();}
bool host_profiles_escape(void){
 if(s_editor_keyboard_popup){editor_keyboard_discard();return true;}
 if(s_editor_camera_remove_popup){editor_camera_remove_close();return true;}
 if(s_editor_camera_popup){editor_camera_close();return true;}
 return false;
}
