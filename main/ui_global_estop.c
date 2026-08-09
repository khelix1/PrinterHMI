#include "ui_global_estop.h"
#include "ui_button.h"
#include "ui_popup.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char TAG[] = "ui_global_estop";

typedef struct {
    ui_global_estop_send_gcode_cb_t send_gcode;
    lv_obj_t *button;
    lv_obj_t *popup;
    char printer_name[64];
} ui_global_estop_state_t;

static ui_global_estop_state_t *s_estop;

bool ui_global_estop_init(ui_global_estop_send_gcode_cb_t send_gcode)
{
    if (!s_estop) {
        s_estop = heap_caps_calloc(
            1, sizeof(*s_estop), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_estop) {
            s_estop = heap_caps_calloc(
                1, sizeof(*s_estop), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            ESP_LOGW(TAG, "E-stop state using internal RAM fallback");
        }
        if (!s_estop) {
            ESP_LOGE(TAG, "Unable to allocate E-stop state");
            return false;
        }
        ESP_LOGI(TAG, "E-stop state allocated in %s",
                 heap_caps_check_integrity(MALLOC_CAP_SPIRAM, true)
                    ? "PSRAM" : "internal RAM");
    }

    s_estop->send_gcode = send_gcode;
    return true;
}

void ui_global_estop_set_printer_name(const char *printer_name)
{
    if (!s_estop) return;
    snprintf(s_estop->printer_name, sizeof(s_estop->printer_name), "%s",
             printer_name && printer_name[0] ? printer_name : "ACTIVE PRINTER");
}

static void close_popup_cb(lv_event_t *event)
{
    (void)event;
    if (s_estop && s_estop->popup) { lv_obj_delete(s_estop->popup); s_estop->popup = NULL; }
}

static void firmware_restart_cb(lv_event_t *event)
{
    (void)event;
    if (s_estop && s_estop->send_gcode) (void)s_estop->send_gcode("FIRMWARE_RESTART");
    close_popup_cb(NULL);
}

static void estop_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || !s_estop || !s_estop->send_gcode) return;
    if (!s_estop->send_gcode("M112")) return;
    if (s_estop->popup) lv_obj_delete(s_estop->popup);
    s_estop->popup = ui_popup_create(lv_layer_top(), 650, 360, UI_POPUP_DANGER);
    if (!s_estop->popup) return;
    ui_popup_add_title(s_estop->popup, "EMERGENCY STOP SENT", true, 4);
    ui_popup_add_header_divider(s_estop->popup, 48);
    char message[160];
    char restart_label[96];
    snprintf(message, sizeof(message), "%s is stopped. Inspect the printer before recovery.", s_estop->printer_name[0] ? s_estop->printer_name : "ACTIVE PRINTER");
    snprintf(restart_label, sizeof(restart_label), LV_SYMBOL_REFRESH " RESTART %s", s_estop->printer_name[0] ? s_estop->printer_name : "PRINTER");
    ui_popup_add_body(s_estop->popup, message, 28, 76, 594);
    ui_popup_add_standard_footer_divider(s_estop->popup);
    ui_popup_add_footer_action(s_estop->popup, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " CLOSE", 170, UI_POPUP_FOOTER_LEFT, close_popup_cb, NULL, NULL);
    ui_popup_add_footer_action(s_estop->popup, UI_POPUP_ACTION_DANGER, restart_label, 300, UI_POPUP_FOOTER_RIGHT, firmware_restart_cb, NULL, NULL);
}

void ui_global_estop_show_restart_confirmation(void)
{
    if (!s_estop || !s_estop->send_gcode) return;
    if (s_estop->popup) lv_obj_delete(s_estop->popup);
    s_estop->popup = ui_popup_create(lv_layer_top(), 650, 360, UI_POPUP_DANGER);
    if (!s_estop->popup) return;
    char message[160];
    char restart_label[96];
    const char *name = s_estop->printer_name[0] ? s_estop->printer_name : "ACTIVE PRINTER";
    snprintf(message, sizeof(message), "Restart Klipper on %s? This interrupts that printer.", name);
    snprintf(restart_label, sizeof(restart_label), LV_SYMBOL_REFRESH " RESTART %s", name);
    ui_popup_add_title(s_estop->popup, "RESTART KLIPPER?", true, 4);
    ui_popup_add_header_divider(s_estop->popup, 48);
    ui_popup_add_body(s_estop->popup, message, 28, 76, 594);
    ui_popup_add_standard_footer_divider(s_estop->popup);
    ui_popup_add_footer_action(s_estop->popup, UI_POPUP_ACTION_CANCEL, LV_SYMBOL_CLOSE " CANCEL", 170, UI_POPUP_FOOTER_LEFT, close_popup_cb, NULL, NULL);
    ui_popup_add_footer_action(s_estop->popup, UI_POPUP_ACTION_DANGER, restart_label, 300, UI_POPUP_FOOTER_RIGHT, firmware_restart_cb, NULL, NULL);
}

void ui_global_estop_create(lv_obj_t *parent)
{
    if (!parent || !s_estop || s_estop->button) return;

    s_estop->button = ui_button_create_empty(parent, UI_BUTTON_DANGER);
    if (!s_estop->button) return;

    lv_obj_set_size(s_estop->button, 125, 52);
    lv_obj_set_pos(s_estop->button, 690, 10);
    lv_obj_clear_flag(s_estop->button, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = ui_button_create_label(
        s_estop->button, LV_SYMBOL_WARNING " E-STOP");
    if (label) {
        lv_obj_center(label);
    }

    lv_obj_add_event_cb(
        s_estop->button, estop_event_cb, LV_EVENT_CLICKED, NULL);
}
