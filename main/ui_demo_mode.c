#include "demo_mode.h"
#include "esp_system.h"
#include "ui_calibration_dialog.h"
static lv_obj_t *popup;
static bool confirming;
static void deleted(lv_event_t *event) {
  if (lv_event_get_target_obj(event) == popup) {
    popup = NULL;
    confirming = false;
  }
}
static void close_cb(lv_event_t *event) {
  (void)event;
  if (popup)
    lv_obj_delete(popup);
}
static void scenario_cb(lv_event_t *event) {
  const char *scenario = lv_event_get_user_data(event);
  if (!demo_printer_scenario(scenario))
    ui_toast_show(UI_STATUS_WARNING, "Demo",
                  "Could not change the simulated printer.");
}
static void change_cb(lv_event_t *event) {
  (void)event;
  if (!confirming) {
    confirming = true;
    lv_obj_delete(popup);
    popup = NULL;
    confirming = true;
    ui_demo_mode_show();
    return;
  }
#ifdef PRINTERHMI_DEMO_DESKTOP
  ui_cal_dialog_title(popup, "Desktop already runs offline");
  ui_cal_dialog_caption(
      ui_cal_dialog_body(popup),
      "The desktop already runs offline. Use the scenario controls above the "
      "preview. Boot selection is available on the tablet.");
  return;
#endif
  if (!demo_mode_select(!demo_mode_enabled())) {
    ui_toast_show(UI_STATUS_DANGER, "Demo",
                  "Could not save the boot selection. No restart performed.");
    return;
  }
  esp_restart();
}
void ui_demo_mode_show(void) {
  if (popup) {
    lv_obj_move_foreground(popup);
    return;
  }
  bool active = demo_mode_enabled();
  popup = ui_cal_dialog_create(lv_layer_top(), 720, 440, UI_POPUP_STANDARD);
  if (!popup)
    return;
  lv_obj_add_event_cb(popup, deleted, LV_EVENT_DELETE, NULL);
  ui_cal_dialog_title(
      popup, confirming
                 ? (active ? "Return to your printers?" : "Start offline demo?")
                 : (active ? "Offline demo" : "Try offline demo"));
  lv_obj_t *body = ui_cal_dialog_body(popup);
  ui_cal_dialog_text(
      body, active ? "DEMO: no printer network connection. Settings and sample "
                     "profiles are temporary. Returning to your printers "
                     "restarts the tablet and restores your saved settings."
                   : "The tablet will restart into an offline simulation. Your "
                     "saved printers and settings are kept. Open Settings > "
                     "Demo to return to normal operation.");
  ui_cal_dialog_caption(body,
      "Explore sample prints, Camera, Tools, Calibration, Network and Setup. "
      "Updates are simulated; backup, restore and factory reset require normal mode.");
  if (active && !confirming) {
    const char *names[] = {"idle",     "printing", "paused",
                           "complete", "offline",  "error"};
    const char *labels[] = {"Idle",     "Printing", "Paused",
                            "Complete", "Offline",  "Error"};
    for (unsigned i = 0; i < 6; i++) {
      lv_obj_t *button = ui_button_create(body, UI_BUTTON_OUTLINED, labels[i]);
      lv_obj_set_size(button, LV_PCT(100), 48);
      lv_obj_add_event_cb(button, scenario_cb, LV_EVENT_CLICKED,
                          (void *)names[i]);
    }
  }
  lv_obj_t *footer = lv_obj_get_child(popup, 2);
  ui_cal_dialog_button(footer, UI_POPUP_ACTION_CANCEL, "Close", 120, close_cb,
                       NULL, NULL);
  ui_cal_dialog_button(footer, UI_POPUP_ACTION_PRIMARY,
                       confirming ? "Restart"
                       : active   ? "Exit demo"
                                  : "Start demo",
                       160, change_cb, NULL, NULL);
}
