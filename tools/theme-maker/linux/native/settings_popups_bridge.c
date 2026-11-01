/* Restart the simulation's preference-controller lifetime after a confirmed
 * controller restart, retaining the process-local NVS backing store. */
#include "ui_settings_popups.c"
void host_preferences_restart(void){ui_settings_popups_close_all();s_controller_restart_accepted=s_reset_accepted=false;}
