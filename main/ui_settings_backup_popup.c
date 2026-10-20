#include "ui_settings_backup_popup.h"
#include "ui_text.h"

#include "lvgl.h"
#include "settings_backup.h"
#include "settings_backup_crypto.h"
#include "settings_backup_encrypted_worker.h"
#include "ui_popup.h"
#include "ui_form_dialog.h"
#include "ui_value_update.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define BACKUP_PASSPHRASE_MAX 96

static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_status = NULL;
static lv_obj_t *s_restore_confirmation = NULL;
static lv_obj_t *s_remove_confirmation = NULL;
static lv_obj_t *s_secret_popup = NULL;
static lv_obj_t *s_secret_input = NULL;
static lv_obj_t *s_secret_confirm = NULL;
static lv_obj_t *s_secret_keyboard = NULL;
static lv_obj_t *s_export_progress_popup = NULL;
static lv_obj_t *s_export_progress_status = NULL;
static lv_obj_t *s_export_progress_value = NULL;
static lv_obj_t *s_export_progress_bar = NULL;
static lv_timer_t *s_export_progress_timer = NULL;
static bool s_export_progress_complete_pending = false;
static bool s_export_progress_is_restore_verify = false;
static int s_export_progress_pulse = 0;
static bool s_secret_is_restore = false;
static bool s_secret_validation_error;
static bool s_pending_encrypted_restore = false;
static lv_obj_t *s_secret_status,*s_restore_status,*s_remove_status;
static void set_status(const char *text);
static bool s_backup_dispatching;
static uint32_t s_backup_epoch;
static void close_export_progress_popup(void);
static void show_secret_popup(bool restore);
static bool show_sync_progress(const char *title);
static void sync_progress_cb(settings_backup_stage_t stage,void *user_data);
static void finish_sync_progress(bool succeeded,const char *status);
static lv_obj_t *s_progress_close;


static bool backup_operation_begin(void)
{
    if (!s_popup || s_backup_dispatching) return false;
    if(s_export_progress_popup&&!s_export_progress_timer)close_export_progress_popup();
    s_backup_dispatching = true;
    uint32_t epoch = s_backup_epoch;
    settings_backup_encrypted_worker_state_t state = settings_backup_encrypted_worker_state();
    if (epoch != s_backup_epoch || !s_popup) {s_backup_dispatching=false;return false;}
    if (state >= SETTINGS_BACKUP_ENCRYPTED_WORKER_PREPARING && state <= SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING) {
        set_status("A backup operation is still running. Wait for it to finish before trying again.");
        s_backup_dispatching=false;
        return false;
    }
    return true;
}

static void backup_field_deleted(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == s_secret_input) {
        s_secret_input = NULL;
        if (s_secret_keyboard) lv_keyboard_set_textarea(s_secret_keyboard, NULL);
    } else if (target == s_secret_confirm) {
        s_secret_confirm = NULL;
        if (s_secret_keyboard) lv_keyboard_set_textarea(s_secret_keyboard, NULL);
    } else if (target == s_secret_keyboard) s_secret_keyboard = NULL;
    else if (target == s_status) s_status = NULL;
    else if (target == s_secret_status) s_secret_status = NULL;
    else if (target == s_restore_status) s_restore_status = NULL;
    else if (target == s_remove_status) s_remove_status = NULL;
    else if (target == s_export_progress_status) s_export_progress_status = NULL;
    else if (target == s_export_progress_value) s_export_progress_value = NULL;
    else if (target == s_export_progress_bar) s_export_progress_bar = NULL;
    else if (target == s_progress_close) s_progress_close = NULL;
    else return;
    ++s_backup_epoch;
}
static void backup_field_own(lv_obj_t *field)
{
    if (field) lv_obj_add_event_cb(field, backup_field_deleted, LV_EVENT_DELETE, NULL);
}

static void close_export_progress_popup(void);
static char s_restore_passphrase[BACKUP_PASSPHRASE_MAX + 1];


static void secure_clear(char *text, size_t text_size)
{
    if (!text || text_size == 0) {
        return;
    }

    volatile char *volatile_text = text;
    while (text_size-- > 0) {
        *volatile_text++ = '\0';
    }
}

static void close_restore_confirmation(void);
static void close_remove_confirmation(void);
static void close_secret_popup(bool preserve_restore_passphrase);
static void backup_dialog_deleted(lv_event_t *event)
{
    lv_obj_t *popup=lv_event_get_target_obj(event);
    ++s_backup_epoch;
    if(popup==s_popup) {
        s_popup=NULL;s_status=NULL;close_restore_confirmation();close_remove_confirmation();
        close_secret_popup(false);close_export_progress_popup();
    }
    if(popup==s_restore_confirmation) {
        s_restore_confirmation=NULL;s_restore_status=NULL;s_pending_encrypted_restore=false;
        secure_clear(s_restore_passphrase,sizeof(s_restore_passphrase));
    }
    if(popup==s_remove_confirmation)s_remove_confirmation=s_remove_status=NULL;
    if(popup==s_secret_popup) {
        s_secret_popup=NULL;s_secret_input=NULL;s_secret_confirm=NULL;s_secret_keyboard=NULL;s_secret_status=NULL;
        s_secret_is_restore=false;s_secret_validation_error=false;
        secure_clear(s_restore_passphrase,sizeof(s_restore_passphrase));
    }
    if(popup==s_export_progress_popup) {
        if(s_export_progress_timer)lv_timer_delete(s_export_progress_timer);
        s_export_progress_timer=NULL;s_export_progress_popup=NULL;s_export_progress_status=NULL;
        s_export_progress_value=NULL;s_export_progress_bar=NULL;s_progress_close=NULL;
        if(s_export_progress_is_restore_verify)secure_clear(s_restore_passphrase,sizeof(s_restore_passphrase));
        s_export_progress_is_restore_verify=false;s_export_progress_complete_pending=false;
    }
}
static void backup_dialog_own(lv_obj_t *popup)
{
    ui_form_divide_title(popup);
    lv_obj_set_style_pad_row(popup,8,0);++s_backup_epoch;lv_obj_add_event_cb(popup,backup_dialog_deleted,LV_EVENT_DELETE,NULL);}
static void secret_status(const char *text)
{
    if(s_secret_status){ui_value_set_text(s_secret_status,text);lv_obj_scroll_to_view(s_secret_status,LV_ANIM_OFF);}
}


static void set_status(const char *text)
{
    if (s_status) {
        ui_value_set_text(s_status, text ? text : ui_text(""));
        lv_obj_scroll_to_view(s_status,LV_ANIM_OFF);
    }
}


static void close_restore_confirmation(void)
{
    lv_obj_t *popup = s_restore_confirmation;
    s_restore_confirmation = s_restore_status = NULL;
    s_pending_encrypted_restore = false;
    secure_clear(s_restore_passphrase, sizeof(s_restore_passphrase));
    if (popup) lv_obj_delete(popup);
}


static void close_remove_confirmation(void)
{
    lv_obj_t *popup = s_remove_confirmation;
    s_remove_confirmation = s_remove_status = NULL;
    if (popup) lv_obj_delete(popup);
}


static void close_secret_popup(bool preserve_restore_passphrase)
{
    lv_obj_t *popup = s_secret_popup;
    s_secret_popup = s_secret_input = s_secret_confirm = s_secret_keyboard = s_secret_status = NULL;
    s_secret_is_restore = false;
    s_secret_validation_error = false;
    if (!preserve_restore_passphrase) secure_clear(s_restore_passphrase, sizeof(s_restore_passphrase));
    if (popup) lv_obj_delete(popup);
}


void ui_settings_backup_popup_close(void)
{
    lv_obj_t *popup=s_popup;
    s_popup=s_status=NULL;
    close_restore_confirmation();
    close_remove_confirmation();
    close_secret_popup(false);
    close_export_progress_popup();
    if(popup)lv_obj_delete(popup);
}

static void close_cb(lv_event_t *event)
{
    (void)event;
    ui_settings_backup_popup_close();
}


static void backup_cb(lv_event_t *event)
{
    (void)event;
    if (!backup_operation_begin()) return;
    if(!show_sync_progress("Saving plain backup")){s_backup_dispatching=false;set_status("Could not open backup progress. Try again.");return;}
    uint32_t epoch = s_backup_epoch;
    char status[192];
    bool saved=settings_backup_export_with_progress(sync_progress_cb,&epoch,status,sizeof(status));
    if(epoch==s_backup_epoch)finish_sync_progress(saved,status);
    s_backup_dispatching = false;
    if (epoch == s_backup_epoch) set_status(status);
}


static void restore_cancel_cb(lv_event_t *event)
{
    (void)event;
    close_restore_confirmation();
}


static void restore_confirm_cb(lv_event_t *event)
{
    (void)event;
    if (!s_restore_confirmation || !backup_operation_begin()) return;
    if(!show_sync_progress(s_pending_encrypted_restore?"Restoring encrypted backup":"Restoring plain backup")){
        s_backup_dispatching=false;if(s_restore_status)ui_value_set_text(s_restore_status,"Could not open restore progress. Try again.");return;
    }
    uint32_t epoch = s_backup_epoch;
    lv_obj_t *parent = s_popup;
    char status[256], passphrase[sizeof(s_restore_passphrase)];
    snprintf(passphrase, sizeof(passphrase), "%s", s_restore_passphrase);
    bool restored=s_pending_encrypted_restore
        ? settings_backup_restore_encrypted_with_progress(passphrase,sync_progress_cb,&epoch,status,sizeof(status))
        : settings_backup_restore_with_progress(sync_progress_cb,&epoch,status,sizeof(status));
    secure_clear(passphrase, sizeof(passphrase));
    s_backup_dispatching = false;
    if (epoch != s_backup_epoch) return;
    finish_sync_progress(restored,status);
    if(restored){close_restore_confirmation();if(s_backup_epoch==epoch+1 && s_popup==parent)set_status(status);}
    else if(s_restore_status){ui_value_set_text(s_restore_status,status);lv_obj_scroll_to_view(s_restore_status,LV_ANIM_OFF);}
}


static void show_restore_confirmation(const char *preflight,bool encrypted)
{
    if(s_restore_confirmation){lv_obj_move_foreground(s_restore_confirmation);return;}
    s_pending_encrypted_restore=encrypted;
    s_restore_confirmation=ui_form_dialog(encrypted?"Restore encrypted backup?":"Restore configuration?",760,480,UI_POPUP_DANGER);
    if(!s_restore_confirmation){secure_clear(s_restore_passphrase,sizeof(s_restore_passphrase));return;}
    backup_dialog_own(s_restore_confirmation);lv_obj_t *body=ui_form_body(s_restore_confirmation);
    ui_form_text(body,preflight?preflight:"Backup verified.",UI_FONT_BODY,UI_TEXT);
    ui_form_text(body,"Restore replaces current printer profiles, appearance, display and notification settings. A reboot is required afterward.",UI_FONT_BODY,UI_WARN);
    s_restore_status=ui_form_text(body,"",UI_FONT_BODY,UI_DANGER_BRIGHT);
    ui_form_action(s_restore_confirmation,UI_BUTTON_CANCEL,"Cancel",restore_cancel_cb,NULL);
    ui_form_action(s_restore_confirmation,UI_BUTTON_DANGER,"Restore",restore_confirm_cb,NULL);
    backup_field_own(s_restore_status);
    ui_form_finish(s_restore_confirmation);
}


static void restore_request_cb(lv_event_t *event)
{
    (void)event;
    if (!backup_operation_begin()) return;
    uint32_t epoch = s_backup_epoch;
    char preflight[256];
    bool verified = settings_backup_preflight(preflight, sizeof(preflight));
    s_backup_dispatching = false;
    if (epoch != s_backup_epoch) return;
    if (!verified) {set_status(preflight);return;}
    show_restore_confirmation(preflight, false);
}


static bool secret_input_event_current(lv_event_t *event)
{
    if(!event||s_backup_dispatching||!s_secret_popup)return false;
    lv_obj_t *target=lv_event_get_target_obj(event);
    return target && target==lv_event_get_current_target_obj(event) &&
        (target==s_secret_input||target==s_secret_confirm) &&
        !lv_obj_has_state(target,LV_STATE_DISABLED);
}

static void secret_value_changed(lv_event_t *event)
{
    if(!secret_input_event_current(event)||lv_event_get_code(event)!=LV_EVENT_VALUE_CHANGED||
       !s_secret_validation_error||!s_secret_status)return;
    s_secret_validation_error=false;
    ui_value_set_text(s_secret_status,"");
}

static void secret_focus_cb(lv_event_t *event)
{
    if(!secret_input_event_current(event)||!s_secret_keyboard||
       (lv_event_get_code(event)!=LV_EVENT_FOCUSED&&lv_event_get_code(event)!=LV_EVENT_CLICKED))return;
    lv_obj_t *target=lv_event_get_target_obj(event);
    lv_keyboard_set_textarea(s_secret_keyboard,target);
    lv_obj_scroll_to_view(target,LV_ANIM_OFF);
}


static void secret_cancel_cb(lv_event_t *event)
{
    (void)event;
    close_secret_popup(false);
}


static void close_export_progress_popup(void)
{
    lv_timer_t *timer = s_export_progress_timer;
    lv_obj_t *popup = s_export_progress_popup;
    bool clear_secret = s_export_progress_is_restore_verify;
    s_export_progress_timer = NULL;
    s_export_progress_popup = s_export_progress_status = s_export_progress_value = s_export_progress_bar = s_progress_close = NULL;
    s_export_progress_complete_pending = s_export_progress_is_restore_verify = false;
    if (clear_secret) secure_clear(s_restore_passphrase, sizeof(s_restore_passphrase));
    if (timer) lv_timer_delete(timer);
    if (popup) lv_obj_delete(popup);
}


static void finish_worker_success(const char *status,uint32_t epoch)
{
    if(!s_export_progress_is_restore_verify){
        lv_timer_t *timer=s_export_progress_timer;s_export_progress_timer=NULL;
        if(timer)lv_timer_delete(timer);
        finish_sync_progress(true,status);
        if(s_export_progress_value)ui_value_set_text(s_export_progress_value,"Backup saved successfully");
        set_status(status);
        return;
    }
    s_export_progress_is_restore_verify=false;
    close_export_progress_popup();
    if(s_backup_epoch==epoch+1&&s_popup)show_restore_confirmation(status,true);
}

static void export_progress_timer_cb(lv_timer_t *timer)
{
    if (!s_popup || !s_export_progress_popup || timer != s_export_progress_timer) return;
    uint32_t epoch = s_backup_epoch;
    settings_backup_encrypted_worker_state_t state =
        settings_backup_encrypted_worker_state();
    if (epoch != s_backup_epoch) return;
    char status[256];
    settings_backup_encrypted_worker_status(status, sizeof(status));
    if (epoch != s_backup_epoch) return;

    if (s_export_progress_complete_pending) {
        finish_worker_success(status,epoch);
        return;
    }

    int value = 10;
    const char *value_text = "Working";
    if (state == SETTINGS_BACKUP_ENCRYPTED_WORKER_ENCRYPTING ||
        state == SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING) {
        s_export_progress_pulse = (s_export_progress_pulse + 9) % 55;
        value = 20 + s_export_progress_pulse;
    } else if (state == SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING) {
        value = 88;
    } else if (state == SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED) {
        finish_worker_success(status,epoch);
        return;
    } else if (state == SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED) {
        bool restore_verify=s_export_progress_is_restore_verify;
        if(!restore_verify){
            lv_timer_t *finished=s_export_progress_timer;s_export_progress_timer=NULL;
            if(finished)lv_timer_delete(finished);
            finish_sync_progress(false,status);
            if(s_export_progress_value){
                ui_value_set_text(s_export_progress_value,"Backup failed");
                ui_apply_custom_label_style(s_export_progress_value,UI_FONT_BODY_LARGE,UI_DANGER_BRIGHT);
            }
            set_status(status);
            return;
        }
        close_export_progress_popup();
        if (s_backup_epoch != epoch + 1 || !s_popup) return;
        if(restore_verify){
            show_secret_popup(true);
            secret_status(status);
        }else set_status(status);
        return;
    }

    if (s_export_progress_status) {
        ui_value_set_text(s_export_progress_status,status);
    }
    if (s_export_progress_value) {
        ui_value_set_text(s_export_progress_value,value_text);
    }
    if (s_export_progress_bar) {
        lv_bar_set_value(s_export_progress_bar, value, LV_ANIM_OFF);
    }
}


static void progress_close_cb(lv_event_t *event)
{
    (void)event;
    if(!s_backup_dispatching&&!s_export_progress_timer)close_export_progress_popup();
}

static void show_backup_progress(const char *title,const char *status,bool monitor,bool restore_verify)
{
    if(s_export_progress_popup){lv_obj_move_foreground(s_export_progress_popup);return;}
    s_export_progress_popup=ui_form_dialog(title,720,400,UI_POPUP_STANDARD);
    if(!s_export_progress_popup)return;
    s_export_progress_is_restore_verify=restore_verify;backup_dialog_own(s_export_progress_popup);
    lv_obj_t *body=ui_form_body(s_export_progress_popup);
    s_export_progress_status=ui_form_text(body,status,UI_FONT_CAPTION,UI_TEXT_DIM);
    s_export_progress_value=ui_form_text(body,"Working",UI_FONT_BODY_LARGE,UI_ACCENT_BRIGHT);
    s_export_progress_bar=lv_bar_create(body);lv_obj_set_size(s_export_progress_bar,LV_PCT(100),24);
    lv_bar_set_range(s_export_progress_bar,0,100);lv_bar_set_value(s_export_progress_bar,10,LV_ANIM_OFF);
    ui_form_text(body,"Keep power connected until the backup operation finishes.",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_export_progress_pulse=0;
    if(monitor)s_export_progress_timer=lv_timer_create(export_progress_timer_cb,150,NULL);
    if(!restore_verify){
        s_progress_close=ui_form_action(s_export_progress_popup,UI_BUTTON_CLOSE,"Close",progress_close_cb,NULL);
        lv_obj_add_state(s_progress_close,LV_STATE_DISABLED);
    }
    backup_field_own(s_export_progress_status);backup_field_own(s_export_progress_value);backup_field_own(s_export_progress_bar);backup_field_own(s_progress_close);
    ui_form_finish(s_export_progress_popup);
}


static void show_export_progress_popup(bool restore_verify)
{
    show_backup_progress(restore_verify?"Verifying backup":"Saving encrypted backup",
        restore_verify?"Verifying encrypted backup passphrase and contents...":"Preparing configuration backup...",
        true,restore_verify);
}

static bool show_sync_progress(const char *title)
{
    show_backup_progress(title,"Preparing operation...",false,false);
    if(!s_export_progress_popup)return false;
    lv_refr_now(NULL);
    return true;
}

static void sync_progress_cb(settings_backup_stage_t stage,void *user_data)
{
    if(!user_data||*(uint32_t *)user_data!=s_backup_epoch||!s_export_progress_popup)return;
    const char *message="Preparing configuration backup...";int value=10;
    switch(stage){
    case SETTINGS_BACKUP_STAGE_PREPARING:break;
    case SETTINGS_BACKUP_STAGE_READING:message="Reading and validating the SD card backup...";value=25;break;
    case SETTINGS_BACKUP_STAGE_AUTHENTICATING:message="Checking passphrase and authenticating encrypted backup...";value=25;break;
    case SETTINGS_BACKUP_STAGE_WRITING:message="Writing backup to the SD card...";value=65;break;
    case SETTINGS_BACKUP_STAGE_APPLYING:message="Applying restored configuration...";value=75;break;
    case SETTINGS_BACKUP_STAGE_ROLLING_BACK:message="Restore failed. Reapplying previous settings...";value=90;break;
    }
    if(s_export_progress_status)ui_value_set_text(s_export_progress_status,message);
    if(s_export_progress_value)ui_value_set_text(s_export_progress_value,"Working");
    if(s_export_progress_bar)lv_bar_set_value(s_export_progress_bar,value,LV_ANIM_OFF);
    lv_refr_now(NULL);
}

static void finish_sync_progress(bool succeeded,const char *status)
{
    if(s_export_progress_status)ui_value_set_text(s_export_progress_status,status);
    if(s_export_progress_value)ui_value_set_text(s_export_progress_value,succeeded?"Complete":"Failed - review the message");
    if(succeeded&&s_export_progress_bar)lv_bar_set_value(s_export_progress_bar,100,LV_ANIM_OFF);
    if(s_progress_close)lv_obj_remove_state(s_progress_close,LV_STATE_DISABLED);
    lv_refr_now(NULL);
}


static void secret_submit_cb(lv_event_t *event)
{
    (void)event;
    if (!s_secret_popup || !s_secret_input ||
        (!s_secret_is_restore && !s_secret_confirm) || !backup_operation_begin()) return;
    uint32_t epoch = s_backup_epoch;
    bool restore = s_secret_is_restore;
    s_secret_validation_error=false;
    char passphrase[BACKUP_PASSPHRASE_MAX + 1], status[256];
    snprintf(passphrase, sizeof(passphrase), "%s", lv_textarea_get_text(s_secret_input));
    if (strlen(passphrase) < SETTINGS_BACKUP_CRYPTO_MIN_PASSPHRASE_LENGTH) {
        s_secret_validation_error=true;
        secret_status("Use a passphrase of at least 12 characters.");
        goto done;
    }
    if (!restore && strcmp(passphrase, lv_textarea_get_text(s_secret_confirm))) {
        s_secret_validation_error=true;
        secret_status("The two passphrases do not match.");
        goto done;
    }
    bool started = restore
        ? settings_backup_encrypted_worker_start_verify(passphrase, status, sizeof(status))
        : settings_backup_encrypted_worker_start_export(passphrase, status, sizeof(status));
    if (epoch != s_backup_epoch) goto done;
    if (!started) {secret_status(status);goto done;}
    if (restore) snprintf(s_restore_passphrase, sizeof(s_restore_passphrase), "%s", passphrase);
    close_secret_popup(restore);
    if (s_backup_epoch == epoch + 1 && s_popup) show_export_progress_popup(restore);
 done:
    secure_clear(passphrase, sizeof(passphrase));
    s_backup_dispatching = false;
}

static void show_secret_popup(bool restore)
{
    if(s_secret_popup){lv_obj_move_foreground(s_secret_popup);return;}
    s_secret_popup=ui_form_dialog(restore?"Open encrypted backup":"Create encrypted backup",880,568,UI_POPUP_STANDARD);
    if(!s_secret_popup)return;
    s_secret_validation_error=false;
    s_secret_is_restore=restore;backup_dialog_own(s_secret_popup);lv_obj_t *body=ui_form_body(s_secret_popup);
    ui_form_text(body,restore?"Backup passphrase":"New passphrase",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_secret_input=ui_form_input(body,"At least 12 characters",NULL,true,BACKUP_PASSPHRASE_MAX);
    if(!restore) {
        ui_form_text(body,"Confirm passphrase",UI_FONT_CAPTION,UI_TEXT_DIM);
        s_secret_confirm=ui_form_input(body,"Enter the same passphrase again",NULL,true,BACKUP_PASSPHRASE_MAX);
        lv_obj_add_event_cb(s_secret_confirm,secret_focus_cb,LV_EVENT_FOCUSED,NULL);
        lv_obj_add_event_cb(s_secret_confirm,secret_focus_cb,LV_EVENT_CLICKED,NULL);
    }
    lv_obj_add_event_cb(s_secret_input,secret_focus_cb,LV_EVENT_FOCUSED,NULL);
    lv_obj_add_event_cb(s_secret_input,secret_focus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_add_event_cb(s_secret_input,secret_value_changed,LV_EVENT_VALUE_CHANGED,NULL);
    if(s_secret_confirm)lv_obj_add_event_cb(s_secret_confirm,secret_value_changed,LV_EVENT_VALUE_CHANGED,NULL);
    s_secret_status=ui_form_text(body,"",UI_FONT_CAPTION,UI_DANGER_BRIGHT);
    s_secret_keyboard=ui_form_keyboard(s_secret_popup,s_secret_input,LV_KEYBOARD_MODE_TEXT_LOWER);
    ui_popup_keyboard_actions(s_secret_keyboard,secret_submit_cb,secret_cancel_cb);
    ui_form_action(s_secret_popup,UI_BUTTON_CANCEL,"Cancel",secret_cancel_cb,NULL);
    ui_form_action(s_secret_popup,UI_BUTTON_PRIMARY,restore?"Verify":"Save",secret_submit_cb,NULL);
    backup_field_own(s_secret_input);backup_field_own(s_secret_confirm);backup_field_own(s_secret_keyboard);backup_field_own(s_secret_status);
    ui_form_finish(s_secret_popup);
    lv_obj_scroll_to_view(s_secret_input,LV_ANIM_OFF);
}


static void encrypted_backup_request_cb(lv_event_t *event)
{
    (void)event;
    if (!backup_operation_begin()) return;
    s_backup_dispatching = false;
    show_secret_popup(false);
}


static void encrypted_restore_request_cb(lv_event_t *event)
{
    (void)event;
    if (!backup_operation_begin()) return;
    s_backup_dispatching = false;
    show_secret_popup(true);
}


static void remove_cancel_cb(lv_event_t *event)
{
    (void)event;
    close_remove_confirmation();
}


static void remove_confirm_cb(lv_event_t *event)
{
    (void)event;
    if (!s_remove_confirmation || !backup_operation_begin()) return;
    uint32_t epoch = s_backup_epoch;
    lv_obj_t *parent = s_popup;
    char status[192];
    bool removed = settings_backup_remove_all(status, sizeof(status));
    s_backup_dispatching = false;
    if (epoch != s_backup_epoch) return;
    if (removed) {
        close_remove_confirmation();
        if (s_backup_epoch == epoch + 1 && parent == s_popup) set_status(status);
    } else if(s_remove_status) {
        ui_value_set_text(s_remove_status,status);
        lv_obj_scroll_to_view(s_remove_status,LV_ANIM_OFF);
    }
}


static void remove_request_cb(lv_event_t *event)
{
    (void)event;if(!backup_operation_begin())return;
    s_backup_dispatching=false;
    if(s_remove_confirmation){lv_obj_move_foreground(s_remove_confirmation);return;}
    s_remove_confirmation=ui_form_dialog("Remove all backups?",720,400,UI_POPUP_DANGER);
    if(!s_remove_confirmation)return;
    backup_dialog_own(s_remove_confirmation);
    ui_form_text(ui_form_body(s_remove_confirmation),"This permanently removes plain and encrypted configuration backups, including temporary recovery copies, from the SD card. Live printer profiles and interface settings are not changed.",UI_FONT_BODY,UI_TEXT);
    s_remove_status=ui_form_text(ui_form_body(s_remove_confirmation),"",UI_FONT_BODY,UI_DANGER_BRIGHT);
    backup_field_own(s_remove_status);
    ui_form_action(s_remove_confirmation,UI_BUTTON_CANCEL,"Cancel",remove_cancel_cb,NULL);
    ui_form_action(s_remove_confirmation,UI_BUTTON_DANGER,"Remove",remove_confirm_cb,NULL);
    ui_form_finish(s_remove_confirmation);
}


static void backup_action(lv_obj_t *row,ui_button_kind_t kind,const char *text,lv_event_cb_t callback)
{
    lv_obj_t *button=ui_button_create(row,kind,text);
    unsigned index=lv_obj_get_child_count(row)-1;
    lv_obj_set_size(button,LV_PCT(100),52);
    lv_obj_set_grid_cell(button,LV_GRID_ALIGN_STRETCH,index%2,index==4?2:1,LV_GRID_ALIGN_START,index/2,1);
    lv_obj_add_event_cb(button,ui_form_action_fit,LV_EVENT_SIZE_CHANGED,NULL);
    lv_obj_add_event_cb(button,callback,LV_EVENT_CLICKED,NULL);
}
void ui_settings_backup_popup_show(void)
{
    if(s_popup){lv_obj_move_foreground(s_popup);return;}
    s_popup=ui_form_dialog("Configuration backup",880,540,UI_POPUP_STANDARD);
    if(!s_popup)return;
    backup_dialog_own(s_popup);lv_obj_t *body=ui_form_body(s_popup);
    ui_form_text(body,"Plain SD card file",UI_FONT_CAPTION,UI_TEXT_DIM);
    ui_form_text(body,SETTINGS_BACKUP_PATH,UI_FONT_BODY,UI_TEXT);
    ui_form_text(body,"Encrypted SD card file",UI_FONT_CAPTION,UI_TEXT_DIM);
    ui_form_text(body,SETTINGS_BACKUP_ENCRYPTED_PATH,UI_FONT_BODY,UI_TEXT);
    s_status=ui_form_text(body,"Plain backups exclude Moonraker API keys. Encrypted backups include them.",UI_FONT_BODY,UI_TEXT_DIM);
    lv_obj_t *actions=lv_obj_create(body);ui_responsive_column(actions);
    static const int32_t columns[]={LV_GRID_FR(1),LV_GRID_FR(1),LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[]={LV_GRID_CONTENT,LV_GRID_CONTENT,LV_GRID_CONTENT,LV_GRID_TEMPLATE_LAST};
    lv_obj_set_grid_dsc_array(actions,columns,rows);
    lv_obj_set_style_pad_column(actions,8,0);lv_obj_set_style_pad_row(actions,12,0);
    backup_action(actions,UI_BUTTON_PRIMARY,"Back up plain",backup_cb);
    backup_action(actions,UI_BUTTON_PRIMARY,"Back up encrypted",encrypted_backup_request_cb);
    backup_action(actions,UI_BUTTON_DANGER,"Restore plain",restore_request_cb);
    backup_action(actions,UI_BUTTON_DANGER,"Restore encrypted",encrypted_restore_request_cb);
    backup_action(actions,UI_BUTTON_DANGER,"Remove backups",remove_request_cb);
    backup_field_own(s_status);
    ui_form_action(s_popup,UI_BUTTON_CLOSE,"Close",close_cb,NULL);ui_form_finish(s_popup);
    for(unsigned i=0;i<lv_obj_get_child_count(actions);i++)
        ui_text_fit_single_line(lv_obj_get_child(lv_obj_get_child(actions,i),0),UI_FONT_BODY);
}
