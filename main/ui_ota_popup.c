#include "ui_ota_popup.h"
#include "ui_text.h"

#include "ota_release_catalog.h"
#include "ui_theme.h"
#include "ui_camera.h"
#include "camera_stream_controller.h"
#include "ui_dashboard.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "ui_popup.h"
#include "ui_ota_layout.h"
#include "ui_value_update.h"
#include "ota_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_ota_popup = NULL;

static lv_obj_t *s_progress_popup = NULL;
static lv_obj_t *s_progress_bar = NULL;
static int32_t s_progress_bar_target;
static lv_obj_t *s_progress_label = NULL;
static lv_obj_t *s_progress_pct_label = NULL;
static lv_obj_t *s_progress_bytes_label = NULL;
static lv_obj_t *s_progress_transfer_label = NULL;
static lv_obj_t *s_progress_cancel_btn = NULL;
static ui_ota_cancel_cb_t s_progress_cancel_cb = NULL;
static bool s_progress_visible = false;
static bool s_progress_cancel_pending;
static bool s_camera_quiesced = false;
static uint32_t s_ota_epoch;

static void ota_quiesce_camera(void)
{
    if (s_camera_quiesced) {
        return;
    }

    /* The persistent MJPEG request shares the network transport with OTA. */
    ui_dashboard_set_camera_quiesced(true);
    ui_camera_set_setup_active(true);
    /* Transport ownership is awaited by catalog/update workers, never LVGL. */
    s_camera_quiesced = true;
}

static void ota_resume_camera(void)
{
    if (!s_camera_quiesced) {
        return;
    }

    s_camera_quiesced = false;
    ui_camera_set_setup_active(false);
    ui_dashboard_set_camera_quiesced(false);
}

void ui_ota_popup_quiesce_camera(void)
{
    ota_quiesce_camera();
}

void ui_ota_popup_resume_camera(void)
{
    ota_resume_camera();
}

static lv_obj_t *s_ota_url_ta = NULL;
static lv_obj_t *s_ota_kb = NULL;
static ui_ota_start_cb_t s_start_cb = NULL;
static ui_ota_remote_cb_t s_remote_cb = NULL;
static bool s_remote_pending, s_editor_transitioning;
static lv_obj_t *s_remote_owner;
static void remote_transition(void *data);

/*
 * OTA startup is deferred until LVGL has processed the asynchronous popup
 * deletion and completed a redraw cycle.
 */
static char s_deferred_start_url[256] = "";
static ui_ota_start_cb_t s_deferred_start_cb = NULL;
static lv_obj_t *s_deferred_start_popup = NULL;
static void deferred_start_cb(void *user_data);


static void progress_cancel_cb(lv_event_t *e)
{
    (void)e;

    if (!s_progress_visible || !s_progress_popup || !s_progress_cancel_btn) return;
    if (e && (lv_event_get_code(e)!=LV_EVENT_CLICKED || lv_event_get_target_obj(e)!=s_progress_cancel_btn)) return;
    if(!ota_manager_is_running()) { ui_ota_progress_close();return; }
    if (s_progress_cancel_pending || lv_obj_has_state(s_progress_cancel_btn, LV_STATE_DISABLED) || !s_progress_cancel_cb) return;
    ui_ota_cancel_cb_t cancel = s_progress_cancel_cb;
    s_progress_cancel_pending=true;
    if (s_progress_cancel_btn) {
        lv_obj_add_state(
            s_progress_cancel_btn,
            LV_STATE_DISABLED);
    }

    if (s_progress_label) {
        lv_label_set_text(
            s_progress_label,
            ui_text("Cancelling OTA..."));
    }

    cancel();
}

/* Retarget from the displayed value: LVGL's built-in bar animation starts
 * a replacement from the previous destination, causing jumps at 50ms pumps. */
static void progress_bar_frame(void *bar, int32_t value)
{
    if (bar == s_progress_bar && s_progress_visible)
        ui_value_set_bar(bar, value);
}

static void progress_bar_set_target(int32_t value)
{
    if (!s_progress_bar || value == s_progress_bar_target) return;
    s_progress_bar_target = value;
    int32_t displayed = lv_bar_get_value(s_progress_bar);
    lv_anim_delete(s_progress_bar, progress_bar_frame);
    if (value <= displayed) {
        ui_value_set_bar(s_progress_bar, value);
        return;
    }
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_progress_bar);
    lv_anim_set_exec_cb(&animation, progress_bar_frame);
    lv_anim_set_values(&animation, displayed, value);
    lv_anim_set_duration(&animation, 150);
    lv_anim_set_path_cb(&animation, lv_anim_path_linear);
    lv_anim_start(&animation);
    /* LVGL removes animations keyed to the bar when that object is deleted. */
}

static void progress_field_deleted(lv_event_t *event)
{
    lv_obj_t *target=lv_event_get_target_obj(event);
    if(target==s_progress_bar)s_progress_bar=NULL;
    else if(target==s_progress_label)s_progress_label=NULL;
    else if(target==s_progress_pct_label)s_progress_pct_label=NULL;
    else if(target==s_progress_bytes_label)s_progress_bytes_label=NULL;
    else if(target==s_progress_transfer_label)s_progress_transfer_label=NULL;
    else if(target==s_progress_cancel_btn)s_progress_cancel_btn=NULL;
}
static void progress_deleted(lv_event_t *event)
{
    (void)event;s_progress_popup=s_progress_bar=s_progress_label=s_progress_pct_label=s_progress_bytes_label=s_progress_transfer_label=s_progress_cancel_btn=NULL;
    s_progress_visible=false;s_progress_cancel_cb=NULL;
}
void ui_ota_progress_show(ui_ota_cancel_cb_t cancel_cb)
{
    if(s_progress_popup){lv_obj_move_foreground(s_progress_popup);return;}
    s_progress_cancel_cb=cancel_cb;s_progress_cancel_pending=false;
    s_progress_popup=ui_ota_dialog("Firmware update",760,440,UI_POPUP_STANDARD);
    if(!s_progress_popup)return;
    lv_obj_add_event_cb(s_progress_popup,progress_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_t *body=ui_ota_body(s_progress_popup);
    s_progress_label=ui_ota_text(body,"Checking server...",UI_FONT_BODY,UI_TEXT);
    s_progress_pct_label=ui_ota_text(body,"0%",UI_FONT_TITLE,UI_ACCENT_BRIGHT);
    s_progress_bar=lv_bar_create(body);lv_obj_set_size(s_progress_bar,LV_PCT(100),32);
    lv_bar_set_range(s_progress_bar,0,10000);s_progress_bar_target=0;
    s_progress_bytes_label=ui_ota_text(body,"0.00 MB",UI_FONT_BODY,UI_TEXT_DIM);
    ui_ota_text(body,"Keep the controller powered during installation. The controller restarts after a successful update.",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_progress_transfer_label=ui_ota_text(body,"Average: -- | Elapsed: 0 s",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_progress_cancel_btn=ui_ota_action(s_progress_popup,UI_POPUP_ACTION_CANCEL,"Cancel",progress_cancel_cb,NULL);
    lv_obj_t *fields[]={s_progress_bar,s_progress_label,s_progress_pct_label,s_progress_bytes_label,s_progress_transfer_label,s_progress_cancel_btn};
    for(size_t i=0;i<6;i++)lv_obj_add_event_cb(fields[i],progress_field_deleted,LV_EVENT_DELETE,NULL);
    s_progress_visible=true;
    ui_ota_finish(s_progress_popup);
}

void ui_ota_progress_set_transfer(const char *text)
{
    if(s_progress_transfer_label)
        ui_value_set_text(s_progress_transfer_label,text ? text : "");
}

void ui_ota_progress_pump(const char *status_text,
                          int percent,
                          int bytes_read,
                          int content_length,
                          bool cancel_enabled)
{
    if (!s_progress_visible) return;

    if(s_progress_cancel_btn) {
        bool finished=!ota_manager_is_running() && percent<100;
        ui_value_set_text(lv_obj_get_child(s_progress_cancel_btn,0),finished ? "Close" : "Cancel");
        if((cancel_enabled && !s_progress_cancel_pending) || finished)lv_obj_remove_state(s_progress_cancel_btn,LV_STATE_DISABLED);
        else lv_obj_add_state(s_progress_cancel_btn,LV_STATE_DISABLED);
    }

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    if(s_progress_label)ui_value_set_text(s_progress_label,status_text && status_text[0] ? status_text : "Checking server...");

    /* Hundredths of a percent keep byte progress moving between integer
     * label updates. Reserve the final percent for validation as before. */
    int32_t bar_value = percent * 100;
    if (bytes_read > 0 && content_length > 0 && percent > 0 && percent < 99) {
        int64_t precise = (int64_t)bytes_read * 10000 / content_length;
        bar_value = precise > 9900 ? 9900 : (int32_t)precise;
    }
    progress_bar_set_target(bar_value);

    if (s_progress_pct_label) {
        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", percent);
        ui_value_set_text(s_progress_pct_label, pct);
    }

    if (s_progress_bytes_label) {
        char mbuf[64];

        if (content_length > 0) {
            snprintf(mbuf, sizeof(mbuf),
                     "%.2f MB / %.2f MB",
                     bytes_read / 1048576.0,
                     content_length / 1048576.0);
        } else {
            snprintf(mbuf, sizeof(mbuf),
                     "%.2f MB",
                     bytes_read / 1048576.0);
        }

        ui_value_set_text(s_progress_bytes_label,mbuf);
    }
}


void ui_ota_progress_close(void)
{
    lv_obj_t *popup=s_progress_popup;
    s_progress_popup = NULL;
    s_progress_bar = NULL;
    s_progress_label = NULL;
    s_progress_pct_label = NULL;
    s_progress_bytes_label = NULL;
    s_progress_transfer_label = NULL;
    s_progress_cancel_btn = NULL;
    s_progress_cancel_cb = NULL;
    s_progress_visible = false;
    if(popup)lv_obj_delete(popup);
    if(!ota_manager_is_running())ota_resume_camera();
}

void ui_ota_popup_close(void)
{
    if (s_remote_pending) lv_async_call_cancel(remote_transition, NULL);
    s_remote_pending = false;
    s_remote_owner = NULL;
    bool had_editor_popup = s_ota_popup != NULL || s_deferred_start_popup != NULL;
    if (s_deferred_start_popup) {
        lv_async_call_cancel(deferred_start_cb, NULL);
        lv_obj_t *pending = s_deferred_start_popup;
        s_deferred_start_popup = NULL;
        s_deferred_start_cb = NULL;
        s_deferred_start_url[0] = '\0';
        lv_obj_delete(pending);
    }
    lv_obj_t *editor=s_ota_popup;
    s_ota_popup=s_ota_url_ta=s_ota_kb=NULL;
    s_start_cb=NULL;s_remote_cb=NULL;
    if(editor)lv_obj_delete(editor);
    if (had_editor_popup && !s_editor_transitioning) {
        ota_resume_camera();
    }
}

static void close_cb(lv_event_t *e)
{
    (void)e;
    ui_ota_popup_close();
}

static void deferred_start_cb(void *user_data)
{
    (void)user_data;

    /*
     * Clear the deferred ownership before invoking the bridge so a future OTA
     * popup can safely schedule another request.
     */
    ui_ota_start_cb_t start_fn = s_deferred_start_cb;
    char url[sizeof(s_deferred_start_url)];

    snprintf(url, sizeof(url), "%s", s_deferred_start_url);

    s_deferred_start_cb = NULL;
    s_deferred_start_url[0] = '\0';

    /*
     * Delete the editor and create the progress popup inside this same LVGL
     * callback. LVGL cannot refresh between those operations, so the page
     * beneath the popups is never exposed as an intermediate frame.
     */
    lv_obj_t *popup = s_deferred_start_popup;
    s_deferred_start_popup = NULL;
    uint32_t epoch=s_ota_epoch;
    if (popup) lv_obj_delete(popup);

    if (popup && s_ota_epoch==epoch+1 && !s_ota_popup && !ota_manager_is_running() && start_fn && url[0]) {
        start_fn(url);
    }
}

static void start_cb(lv_event_t *e)
{
    (void)e;
    if (ota_manager_is_running() || s_remote_pending || s_deferred_start_popup || !s_ota_popup || !s_start_cb || !s_ota_url_ta) return;
    if(s_ota_url_ta && !lv_textarea_get_text(s_ota_url_ta)[0]) {
        lv_textarea_set_placeholder_text(s_ota_url_ta,"Firmware URL required.");
        lv_obj_set_style_border_color(s_ota_url_ta,UI_DANGER_BRIGHT,0);
        return;
    }

    if (strlen(lv_textarea_get_text(s_ota_url_ta)) >= sizeof(s_deferred_start_url)) {
        lv_textarea_set_placeholder_text(s_ota_url_ta, "Firmware URL is too long.");
        return;
    }
    /* Reserve the asynchronous callback before transferring any ownership. */
    if (lv_async_call(deferred_start_cb, NULL) != LV_RESULT_OK) {
        ui_popup_feedback(ui_ota_body(s_ota_popup), UI_STATUS_DANGER,
                          "Update not started", "Could not queue update. Your URL is retained; try again.");
        return;
    }

    /*
     * Preserve everything needed by the deferred callback before releasing
     * popup ownership.
     */
    s_deferred_start_url[0] = '\0';
    s_deferred_start_cb = s_start_cb;

    if (s_ota_url_ta) {
        const char *txt = lv_textarea_get_text(s_ota_url_ta);
        if (txt) {
            snprintf(s_deferred_start_url,
                     sizeof(s_deferred_start_url),
                     "%s",
                     txt);
        }
    }

    /*
     * Transfer popup ownership to the deferred transition. The editor is
     * deleted immediately before the progress popup is created, with no
     * refresh opportunity between them.
     */
    s_deferred_start_popup = s_ota_popup;

    s_ota_popup = NULL;
    s_ota_url_ta = NULL;
    s_ota_kb = NULL;
    s_start_cb = NULL;
    s_remote_cb = NULL;

    if (s_deferred_start_cb && s_deferred_start_url[0]) {
        /* Already queued before ownership transfer. */
    } else {
        lv_obj_t *popup = s_deferred_start_popup;

        s_deferred_start_popup = NULL;
        s_deferred_start_cb = NULL;
        s_deferred_start_url[0] = '\0';

        if (popup) {
            lv_obj_delete_async(popup);
        }
    }
}

static void remote_transition(void *data)
{
    (void)data;
    if (!s_remote_pending || !s_remote_owner || s_remote_owner != s_ota_popup) return;
    ui_ota_remote_cb_t remote_fn = s_remote_cb;
    s_remote_pending = false;
    s_remote_owner = NULL;
    s_editor_transitioning = true;
    ui_ota_popup_close();
    s_editor_transitioning = false;
    if (remote_fn) remote_fn();
    else ota_resume_camera();
}
static void remote_cb(lv_event_t *e)
{
    (void)e;
    if (s_remote_pending || s_deferred_start_popup || !s_remote_cb || !s_ota_popup) return;
    if (lv_async_call(remote_transition, NULL) != LV_RESULT_OK) {
        ui_popup_feedback(ui_ota_body(s_ota_popup), UI_STATUS_DANGER,
                          "Releases not opened", "Could not queue dialog. Your URL is retained; try again.");
        return;
    }
    s_remote_pending = true;
    s_remote_owner = s_ota_popup;
}

static void editor_field_deleted(lv_event_t *event)
{
    lv_obj_t *target=lv_event_get_target_obj(event);
    if(target==s_ota_url_ta){s_ota_url_ta=NULL;if(s_ota_kb)lv_keyboard_set_textarea(s_ota_kb,NULL);}
    else if(target==s_ota_kb)s_ota_kb=NULL;
}
static void editor_deleted(lv_event_t *event)
{
    ++s_ota_epoch;
    lv_obj_t *deleted=lv_event_get_target_obj(event);
    if(deleted==s_ota_popup) {
        s_ota_popup=s_ota_url_ta=s_ota_kb=NULL;s_start_cb=NULL;s_remote_cb=NULL;
        if (s_remote_pending) lv_async_call_cancel(remote_transition, NULL);
        s_remote_pending = false;
        s_remote_owner = NULL;
        if (!s_editor_transitioning) ota_resume_camera();
    }
    if(deleted==s_deferred_start_popup) {
        lv_async_call_cancel(deferred_start_cb, NULL);
        s_deferred_start_popup=NULL;s_deferred_start_cb=NULL;s_deferred_start_url[0]='\0';
        ota_resume_camera();
    }
}
void ui_ota_popup_show(const char *current_url,const char *firmware_info,size_t max_url_len,
                       ui_ota_start_cb_t start_cb_fn,ui_ota_remote_cb_t remote_cb_fn)
{
    if(ota_manager_is_running())return;
    if(s_deferred_start_popup){lv_obj_move_foreground(s_deferred_start_popup);return;}
    if(s_ota_popup){lv_obj_move_foreground(s_ota_popup);return;}
    s_start_cb=start_cb_fn;s_remote_cb=remote_cb_fn;
    s_ota_popup=ui_ota_dialog("Update from URL",960,540,UI_POPUP_STANDARD);
    if(!s_ota_popup){s_start_cb=NULL;s_remote_cb=NULL;ota_resume_camera();return;}
    ++s_ota_epoch;
    lv_obj_add_event_cb(s_ota_popup,editor_deleted,LV_EVENT_DELETE,NULL);
    ota_quiesce_camera();
    lv_obj_t *body=ui_ota_body(s_ota_popup);
    ui_ota_text(body,firmware_info ? firmware_info : "",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_ota_url_ta=ui_popup_add_textarea(body,LV_PCT(100),56,LV_ALIGN_TOP_LEFT,0,0,true,false,
        max_url_len,"https://server/firmware.bin",current_url ? current_url : "",NULL);
    ui_apply_custom_label_style(s_ota_url_ta,UI_FONT_BODY,UI_TEXT);
    lv_obj_set_height(s_ota_url_ta,56);
    s_ota_kb=ui_popup_add_keyboard(body,s_ota_url_ta,LV_PCT(100),224,LV_ALIGN_TOP_LEFT,0,0,LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_style_min_height(s_ota_kb,180,0);
    ui_ota_action(s_ota_popup,UI_POPUP_ACTION_CANCEL,"Cancel",close_cb,NULL);
    ui_ota_action(s_ota_popup,UI_POPUP_ACTION_SECONDARY,"Releases",remote_cb,NULL);
    ui_ota_action(s_ota_popup,UI_POPUP_ACTION_CONFIRM,"Start update",start_cb,NULL);
    lv_obj_add_event_cb(s_ota_url_ta,editor_field_deleted,LV_EVENT_DELETE,NULL);
    lv_obj_add_event_cb(s_ota_kb,editor_field_deleted,LV_EVENT_DELETE,NULL);
    ui_popup_keyboard_actions(s_ota_kb,start_cb,close_cb);
    ui_popup_pin_keyboard(s_ota_popup,s_ota_kb);
    ui_ota_finish(s_ota_popup);
}

static lv_obj_t *s_start_failure;
static void start_failure_deleted(lv_event_t *event)
{
    (void)event; s_start_failure = NULL; ota_resume_camera();
}
static void start_failure_close(lv_event_t *event)
{
    (void)event; if (s_start_failure) lv_obj_delete(s_start_failure);
}
void ui_ota_start_failure(const char *message)
{
    if (s_start_failure) lv_obj_delete(s_start_failure);
    s_start_failure = ui_ota_dialog("Update not started",650,340,UI_POPUP_DANGER);
    if (!s_start_failure) { ota_resume_camera(); return; }
    lv_obj_add_event_cb(s_start_failure,start_failure_deleted,LV_EVENT_DELETE,NULL);
    ui_ota_text(ui_ota_body(s_start_failure),message,UI_FONT_BODY,UI_TEXT);
    ui_ota_action(s_start_failure,UI_POPUP_ACTION_CLOSE,"Close",start_failure_close,NULL);
    ui_ota_finish(s_start_failure);
}
