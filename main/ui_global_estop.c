#include "ui_global_estop.h"
#include "ui_text.h"
#include "ui_button.h"
#include "ui_popup.h"
#include "ui_theme.h"
#include "ui_toast.h"
#include "moonraker_config_controller.h"
#include "ui_value_update.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char TAG[] = "ui_global_estop";

typedef struct {
    ui_global_estop_send_gcode_cb_t send_gcode;
    lv_obj_t *button;
    lv_obj_t *popup;
    lv_obj_t *message, *title, *restart;
    ui_global_estop_send_gcode_cb_t popup_send;
    uint32_t popup_owner;
    char printer_name[64];
} ui_global_estop_state_t;

static ui_global_estop_state_t *s_estop;
static uint32_t s_estop_epoch,s_restart_owner,s_restart_tick;
static bool s_restart_pending,s_restart_dispatching,s_stop_dispatching;
static bool restart_waiting(uint32_t owner)
{
    return s_restart_pending && s_restart_owner==owner && (uint32_t)(lv_tick_get()-s_restart_tick)<5000U;
}
static void estop_close_popup(void)
{
    if(!s_estop)return;
    ++s_estop_epoch;
    lv_obj_t *popup=s_estop->popup;
    s_estop->popup=s_estop->message=s_estop->title=s_estop->restart=NULL;s_estop->popup_send=NULL;
    if(popup)lv_obj_delete(popup);
}
static void estop_field_deleted(lv_event_t *event)
{
    if(!s_estop)return;
    lv_obj_t *target=lv_event_get_target_obj(event);
    if(target==s_estop->message)s_estop->message=NULL;
    else if(target==s_estop->title)s_estop->title=NULL;
    else if(target==s_estop->restart)s_estop->restart=NULL;
    else return;
    ++s_estop_epoch;
}

static void estop_popup_deleted(lv_event_t *event)
{
    if(s_estop && lv_event_get_target_obj(event)==s_estop->popup){
        ++s_estop_epoch;s_estop->popup=s_estop->message=s_estop->title=s_estop->restart=NULL;s_estop->popup_send=NULL;
    }
}

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

    if(s_estop->send_gcode!=send_gcode)++s_estop_epoch;
    s_estop->send_gcode = send_gcode;
    return true;
}

void ui_global_estop_set_printer_name(const char *printer_name)
{
    if (!s_estop) return;
    char next[sizeof(s_estop->printer_name)];
    snprintf(next,sizeof(next),"%s",printer_name && printer_name[0]?printer_name:"ACTIVE PRINTER");
    if(strcmp(next,s_estop->printer_name)){++s_estop_epoch;estop_close_popup();}
    snprintf(s_estop->printer_name,sizeof(s_estop->printer_name),"%s",next);
}

static void close_popup_cb(lv_event_t *event)
{
    (void)event;
    estop_close_popup();
}

static void firmware_restart_cb(lv_event_t *event)
{
    if(!event || lv_event_get_code(event)!=LV_EVENT_CLICKED || !s_estop || !s_estop->popup ||
        !s_estop->restart || lv_event_get_target_obj(event)!=s_estop->restart || !s_estop->message ||
        s_restart_dispatching || lv_obj_has_state(s_estop->restart,LV_STATE_DISABLED))return;
    uint32_t owner=s_estop->popup_owner,epoch=s_estop_epoch;
    if(owner!=moonraker_config_generation()){
        ui_value_set_text(s_estop->message,"Printer changed. Close and reopen this review before restarting.");return;
    }
    if(restart_waiting(owner)){
        ui_value_set_text(s_estop->message,"Restart already requested. Wait for the printer to reconnect before continuing.");return;
    }
    ui_global_estop_send_gcode_cb_t send=s_estop->popup_send;
    if(!send)return;
    s_restart_dispatching=true;
    bool sent=send("FIRMWARE_RESTART");
    if(sent){s_restart_pending=true;s_restart_owner=owner;s_restart_tick=lv_tick_get();}
    if(epoch==s_estop_epoch && owner==moonraker_config_generation() && s_estop->popup){
        ui_value_set_text(s_estop->message,sent?
            "Restart requested. Wait for the printer to reconnect before continuing.":
            "RESTART NOT SENT. Check the printer connection and try again.");
        if(sent && s_estop->restart)lv_obj_add_state(s_estop->restart,LV_STATE_DISABLED);
    }
    s_restart_dispatching=false;
}

/* Native modal layout: text scrolls independently from the pinned actions. */
static bool estop_show_dialog(bool restart_only)
{
    uint32_t owner=moonraker_config_generation();
    uint32_t expected=s_estop_epoch+1;
    estop_close_popup();
    if(s_estop_epoch!=expected || s_estop->popup || owner!=moonraker_config_generation())return false;
    int32_t width=lv_display_get_horizontal_resolution(NULL)-32;
    int32_t height=lv_display_get_vertical_resolution(NULL)-32;
    if(width>680)width=680;
    if(height>400)height=400;
    s_estop->popup=ui_popup_create(lv_layer_top(),width,height,UI_POPUP_DANGER);
    if(!s_estop->popup)return false;
    ++s_estop_epoch;
    s_estop->popup_owner=moonraker_config_generation();s_estop->popup_send=s_estop->send_gcode;
    lv_obj_t *popup=s_estop->popup;
    lv_obj_add_event_cb(popup,estop_popup_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_flex_flow(popup,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(popup,16,0);
    lv_obj_set_style_pad_row(popup,12,0);
    lv_obj_t *title=lv_label_create(popup);s_estop->title=title;
    lv_label_set_text(title,ui_text(restart_only?"RESTART KLIPPER?":"STOP REQUEST SENT"));
    ui_apply_custom_label_style(title,UI_FONT_TITLE,UI_DANGER_BRIGHT);
    lv_obj_set_width(title,LV_PCT(100));
    lv_label_set_long_mode(title,LV_LABEL_LONG_WRAP);

    lv_obj_t *body=lv_obj_create(popup);
    lv_obj_remove_style_all(body);
    lv_obj_set_size(body,LV_PCT(100),0);
    lv_obj_set_flex_grow(body,1);
    lv_obj_set_flex_flow(body,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body,12,0);
    lv_obj_add_flag(body,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(body,LV_DIR_VER);
    lv_obj_set_scrollbar_mode(body,LV_SCROLLBAR_MODE_AUTO);
    lv_obj_t *name=lv_label_create(body);
    lv_label_set_text(name,s_estop->printer_name[0]?s_estop->printer_name:"ACTIVE PRINTER");
    ui_apply_custom_label_style(name,UI_FONT_BODY_LARGE,UI_TEXT_BRIGHT);
    lv_obj_set_width(name,LV_PCT(100));lv_label_set_long_mode(name,LV_LABEL_LONG_WRAP);
    lv_obj_t *message=lv_label_create(body);s_estop->message=message;
    lv_label_set_text(message,ui_text(restart_only?
        "Restart Klipper on this printer? This interrupts any operation in progress.":
        "Emergency stop requested. Verify this printer has halted before recovery. Restart Klipper only when you are ready to recover."));
    ui_apply_custom_label_style(message,UI_FONT_BODY_LARGE,UI_TEXT);
    lv_obj_set_width(message,LV_PCT(100));lv_label_set_long_mode(message,LV_LABEL_LONG_WRAP);

    lv_obj_t *footer=lv_obj_create(popup);
    lv_obj_remove_style_all(footer);
    lv_obj_clear_flag(footer,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(footer,LV_PCT(100),LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(footer,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(footer,LV_FLEX_ALIGN_END,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(footer,12,0);lv_obj_set_style_pad_row(footer,12,0);
    ui_popup_add_action_at(footer,UI_POPUP_ACTION_CANCEL,
        restart_only?LV_SYMBOL_CLOSE " CANCEL":LV_SYMBOL_CLOSE " CLOSE",
        0,0,LV_SIZE_CONTENT,52,close_popup_cb,NULL,NULL);
    s_estop->restart=ui_popup_add_action_at(footer,UI_POPUP_ACTION_DANGER,LV_SYMBOL_REFRESH " RESTART KLIPPER",
        0,0,LV_SIZE_CONTENT,52,firmware_restart_cb,NULL,NULL);
    lv_obj_t *fields[]={s_estop->title,s_estop->message,s_estop->restart};
    for(size_t i=0;i<3;i++)if(fields[i])lv_obj_add_event_cb(fields[i],estop_field_deleted,LV_EVENT_DELETE,NULL);
    if(restart_waiting(s_estop->popup_owner)){
        if(restart_only)ui_value_set_text(s_estop->message,"Restart already requested. Wait for the printer to reconnect before continuing.");
        if(s_estop->restart)lv_obj_add_state(s_estop->restart,LV_STATE_DISABLED);
    }
    for(uint32_t i=0;i<lv_obj_get_child_count(footer);i++) {
        lv_obj_t *button=lv_obj_get_child(footer,i);
        lv_obj_t *label=lv_obj_get_child(button,0);
        lv_obj_set_width(label,LV_SIZE_CONTENT);
        lv_label_set_long_mode(label,LV_LABEL_LONG_CLIP);
        int32_t action_width=lv_obj_get_self_width(label)+32;
        if(action_width<128)action_width=128;
        lv_obj_set_width(button,action_width);
        lv_obj_set_style_min_width(button,action_width,0);
        lv_obj_center(label);
    }
    return true;
}

static void estop_event_cb(lv_event_t *event)
{
    if(!event || lv_event_get_code(event)!=LV_EVENT_CLICKED || !s_estop || !s_estop->button ||
        lv_event_get_target_obj(event)!=s_estop->button || !s_estop->send_gcode || s_stop_dispatching)return;
    uint32_t epoch=s_estop_epoch,owner=moonraker_config_generation();
    ui_global_estop_send_gcode_cb_t send=s_estop->send_gcode;
    s_stop_dispatching=true;
    /* Dispatch immediately, before constructing any feedback UI. */
    bool sent=send("M112");
    if(epoch==s_estop_epoch && owner==moonraker_config_generation()){
        bool reviewed=estop_show_dialog(false);
        if(reviewed && !sent && s_estop->popup && s_estop->popup_owner==owner){
            ui_value_set_text(s_estop->title,"STOP NOT CONFIRMED");
            ui_value_set_text(s_estop->message,"Stop request could not be sent. Use the printer power switch if needed. Verify the printer has halted before recovery.");
        }
    }
    s_stop_dispatching=false;
}

void ui_global_estop_show_restart_confirmation(void)
{
    if(!s_estop || !s_estop->send_gcode)return;
    estop_show_dialog(true);
}

static void estop_button_deleted(lv_event_t *event)
{
    if (s_estop && lv_event_get_target(event) == s_estop->button){s_estop->button=NULL;++s_estop_epoch;}
}

void ui_global_estop_create(lv_obj_t *parent)
{
    if (!parent || !s_estop || s_estop->button) return;

    s_estop->button = ui_button_create_empty(parent, UI_BUTTON_DANGER);
    if (!s_estop->button) return;

    lv_obj_add_event_cb(s_estop->button,estop_button_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_set_size(s_estop->button, 140, 52);
    lv_obj_set_pos(s_estop->button, ui_theme_is_studio() ? 852 : 690, 10);
    lv_obj_clear_flag(s_estop->button, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = ui_button_create_label(
        s_estop->button, LV_SYMBOL_WARNING " E-STOP");
    if (label) {
        lv_obj_center(label);
    }

    lv_obj_add_event_cb(
        s_estop->button, estop_event_cb, LV_EVENT_CLICKED, NULL);
}
