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
static bool s_pending_encrypted_restore = false;
static lv_obj_t *s_secret_status,*s_restore_status;
static bool s_preserve_secret_on_close;

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
    if(popup==s_popup) {
        s_popup=NULL;s_status=NULL;close_restore_confirmation();close_remove_confirmation();
        close_secret_popup(false);close_export_progress_popup();
    }
    if(popup==s_restore_confirmation) {
        s_restore_confirmation=NULL;s_restore_status=NULL;s_pending_encrypted_restore=false;
        secure_clear(s_restore_passphrase,sizeof(s_restore_passphrase));
    }
    if(popup==s_remove_confirmation)s_remove_confirmation=NULL;
    if(popup==s_secret_popup) {
        s_secret_popup=NULL;s_secret_input=NULL;s_secret_confirm=NULL;s_secret_keyboard=NULL;s_secret_status=NULL;
        s_secret_is_restore=false;
        if(!s_preserve_secret_on_close)secure_clear(s_restore_passphrase,sizeof(s_restore_passphrase));
    }
    if(popup==s_export_progress_popup) {
        if(s_export_progress_timer)lv_timer_delete(s_export_progress_timer);
        s_export_progress_timer=NULL;s_export_progress_popup=NULL;s_export_progress_status=NULL;
        s_export_progress_value=NULL;s_export_progress_bar=NULL;
        if(s_export_progress_is_restore_verify)secure_clear(s_restore_passphrase,sizeof(s_restore_passphrase));
        s_export_progress_is_restore_verify=false;s_export_progress_complete_pending=false;
    }
}
static void backup_dialog_own(lv_obj_t *popup)
{lv_obj_add_event_cb(popup,backup_dialog_deleted,LV_EVENT_DELETE,NULL);}
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
    if (s_restore_confirmation) {
        lv_obj_delete(s_restore_confirmation);
    }
    s_restore_confirmation = NULL;
    s_pending_encrypted_restore = false;
    secure_clear(s_restore_passphrase, sizeof(s_restore_passphrase));
}


static void close_remove_confirmation(void)
{
    if (s_remove_confirmation) {
        lv_obj_delete(s_remove_confirmation);
    }
    s_remove_confirmation = NULL;
}


static void close_secret_popup(bool preserve_restore_passphrase)
{
    if (!preserve_restore_passphrase) {
        secure_clear(s_restore_passphrase, sizeof(s_restore_passphrase));
    }

    s_preserve_secret_on_close=preserve_restore_passphrase;
    if (s_secret_popup) {
        lv_obj_delete(s_secret_popup);
    }

    s_secret_popup = NULL;
    s_secret_input = NULL;
    s_secret_confirm = NULL;
    s_secret_keyboard = NULL;
    s_secret_is_restore = false;
    s_preserve_secret_on_close=false;
}


void ui_settings_backup_popup_close(void)
{
    close_restore_confirmation();
    close_remove_confirmation();
    close_secret_popup(false);
    close_export_progress_popup();

    if (s_popup) {
        lv_obj_delete(s_popup);
    }

    s_popup = NULL;
    s_status = NULL;
}


static void close_cb(lv_event_t *event)
{
    (void)event;
    ui_settings_backup_popup_close();
}


static void backup_cb(lv_event_t *event)
{
    (void)event;
    char status[192];
    (void)settings_backup_export(status, sizeof(status));
    set_status(status);
}


static void restore_cancel_cb(lv_event_t *event)
{
    (void)event;
    close_restore_confirmation();
}


static void restore_confirm_cb(lv_event_t *event)
{
    (void)event;char status[256];
    bool restored=s_pending_encrypted_restore ? settings_backup_restore_encrypted(s_restore_passphrase,status,sizeof(status)) : settings_backup_restore(status,sizeof(status));
    if(restored){close_restore_confirmation();set_status(status);}
    else if(s_restore_status){ui_value_set_text(s_restore_status,status);lv_obj_scroll_to_view(s_restore_status,LV_ANIM_OFF);}
}


static void show_restore_confirmation(const char *preflight,bool encrypted)
{
    if(s_restore_confirmation){lv_obj_move_foreground(s_restore_confirmation);return;}
    s_pending_encrypted_restore=encrypted;
    s_restore_confirmation=ui_form_dialog(encrypted?"RESTORE ENCRYPTED BACKUP?":"RESTORE CONFIGURATION?",760,480,UI_POPUP_DANGER);
    if(!s_restore_confirmation){secure_clear(s_restore_passphrase,sizeof(s_restore_passphrase));return;}
    backup_dialog_own(s_restore_confirmation);lv_obj_t *body=ui_form_body(s_restore_confirmation);
    ui_form_text(body,preflight?preflight:"Backup verified.",UI_FONT_BODY,UI_TEXT);
    ui_form_text(body,"Restore replaces current printer profiles, appearance, display and notification settings. A reboot is required afterward.",UI_FONT_BODY,UI_WARN);
    s_restore_status=ui_form_text(body,"",UI_FONT_BODY,UI_DANGER_BRIGHT);
    ui_form_action(s_restore_confirmation,UI_BUTTON_CANCEL,"CANCEL",restore_cancel_cb,NULL);
    ui_form_action(s_restore_confirmation,UI_BUTTON_DANGER,"RESTORE",restore_confirm_cb,NULL);
    ui_form_finish(s_restore_confirmation);
}


static void restore_request_cb(lv_event_t *event)
{
    (void)event;
    char preflight[256];
    if (!settings_backup_preflight(preflight, sizeof(preflight))) {
        set_status(preflight);
        return;
    }
    show_restore_confirmation(preflight, false);
}


static void secret_focus_cb(lv_event_t *event)
{
    if (s_secret_keyboard) {
        lv_keyboard_set_textarea(
            s_secret_keyboard,
            lv_event_get_target(event));
        lv_obj_scroll_to_view(lv_event_get_target_obj(event),LV_ANIM_OFF);
    }
}


static void secret_cancel_cb(lv_event_t *event)
{
    (void)event;
    close_secret_popup(false);
}


static void close_export_progress_popup(void)
{
    if (s_export_progress_timer) {
        lv_timer_delete(s_export_progress_timer);
    }
    s_export_progress_timer = NULL;
    if (s_export_progress_popup) {
        lv_obj_delete(s_export_progress_popup);
    }
    s_export_progress_popup = NULL;
    s_export_progress_status = NULL;
    s_export_progress_value = NULL;
    s_export_progress_bar = NULL;
    s_export_progress_complete_pending = false;
    s_export_progress_is_restore_verify = false;
}


static void export_progress_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    settings_backup_encrypted_worker_state_t state =
        settings_backup_encrypted_worker_state();
    char status[256];
    settings_backup_encrypted_worker_status(status, sizeof(status));

    if (s_export_progress_complete_pending) {
        bool restore_verify = s_export_progress_is_restore_verify;
        s_export_progress_is_restore_verify=false;
        close_export_progress_popup();
        if (restore_verify) {
            show_restore_confirmation(status, true);
        } else {
            set_status(status);
        }
        return;
    }

    int value = 10;
    const char *value_text = "WORKING";
    if (state == SETTINGS_BACKUP_ENCRYPTED_WORKER_ENCRYPTING ||
        state == SETTINGS_BACKUP_ENCRYPTED_WORKER_VERIFYING) {
        s_export_progress_pulse = (s_export_progress_pulse + 9) % 55;
        value = 20 + s_export_progress_pulse;
    } else if (state == SETTINGS_BACKUP_ENCRYPTED_WORKER_WRITING) {
        value = 88;
    } else if (state == SETTINGS_BACKUP_ENCRYPTED_WORKER_SUCCEEDED) {
        bool restore_verify = s_export_progress_is_restore_verify;
        s_export_progress_is_restore_verify=false;
        close_export_progress_popup();
        if (restore_verify) {
            show_restore_confirmation(status, true);
        } else {
            set_status(status);
        }
        return;
    } else if (state == SETTINGS_BACKUP_ENCRYPTED_WORKER_FAILED) {
        if (s_export_progress_is_restore_verify) {
            secure_clear(s_restore_passphrase, sizeof(s_restore_passphrase));
        }
        close_export_progress_popup();
        set_status(status);
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


static void show_export_progress_popup(bool restore_verify)
{
    if(s_export_progress_popup){lv_obj_move_foreground(s_export_progress_popup);return;}
    s_export_progress_popup=ui_form_dialog(restore_verify?"VERIFYING BACKUP":"SAVING ENCRYPTED BACKUP",720,400,UI_POPUP_STANDARD);
    if(!s_export_progress_popup)return;
    s_export_progress_is_restore_verify=restore_verify;backup_dialog_own(s_export_progress_popup);
    lv_obj_t *body=ui_form_body(s_export_progress_popup);
    s_export_progress_status=ui_form_text(body,restore_verify?"Verifying encrypted backup passphrase and contents...":"Preparing configuration backup...",UI_FONT_BODY,UI_TEXT);
    s_export_progress_value=ui_form_text(body,"WORKING",UI_FONT_BODY_LARGE,UI_ACCENT_BRIGHT);
    s_export_progress_bar=lv_bar_create(body);lv_obj_set_size(s_export_progress_bar,LV_PCT(100),24);
    lv_bar_set_range(s_export_progress_bar,0,100);lv_bar_set_value(s_export_progress_bar,10,LV_ANIM_OFF);
    ui_form_text(body,"Keep power connected until the backup operation finishes.",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_export_progress_pulse=0;s_export_progress_timer=lv_timer_create(export_progress_timer_cb,150,NULL);
    ui_form_finish(s_export_progress_popup);
}


static void secret_submit_cb(lv_event_t *event)
{
    (void)event;
    const char *passphrase = s_secret_input
        ? lv_textarea_get_text(s_secret_input)
        : "";

    if (strnlen(passphrase, BACKUP_PASSPHRASE_MAX + 1) <
        SETTINGS_BACKUP_CRYPTO_MIN_PASSPHRASE_LENGTH) {
        secret_status("Use a passphrase of at least 12 characters.");
        return;
    }

    if (!s_secret_is_restore) {
        const char *confirmation = s_secret_confirm
            ? lv_textarea_get_text(s_secret_confirm)
            : "";
        if (strcmp(passphrase, confirmation) != 0) {
            secret_status("The two passphrases do not match.");
            return;
        }

        char status[256];
        if (!settings_backup_encrypted_worker_start_export(
                passphrase,
                status,
                sizeof(status))) {
            secret_status(status);
            return;
        }
        close_secret_popup(false);
        show_export_progress_popup(false);
        return;
    }

    snprintf(s_restore_passphrase,sizeof(s_restore_passphrase),"%s",passphrase);

    char status[256];
    if (!settings_backup_encrypted_worker_start_verify(
            s_restore_passphrase,
            status,
            sizeof(status))) {
        secure_clear(s_restore_passphrase, sizeof(s_restore_passphrase));
        secret_status(status);
        return;
    }

    close_secret_popup(true);
    show_export_progress_popup(true);
}


static void show_secret_popup(bool restore)
{
    if(s_secret_popup){lv_obj_move_foreground(s_secret_popup);return;}
    s_secret_popup=ui_form_dialog(restore?"OPEN ENCRYPTED BACKUP":"CREATE ENCRYPTED BACKUP",880,568,UI_POPUP_STANDARD);
    if(!s_secret_popup)return;
    s_secret_is_restore=restore;backup_dialog_own(s_secret_popup);lv_obj_t *body=ui_form_body(s_secret_popup);
    ui_form_text(body,restore?"BACKUP PASSPHRASE":"NEW PASSPHRASE",UI_FONT_CAPTION,UI_TEXT_DIM);
    s_secret_input=ui_form_input(body,"At least 12 characters",NULL,true,BACKUP_PASSPHRASE_MAX);
    if(!restore) {
        ui_form_text(body,"CONFIRM PASSPHRASE",UI_FONT_CAPTION,UI_TEXT_DIM);
        s_secret_confirm=ui_form_input(body,"Enter the same passphrase again",NULL,true,BACKUP_PASSPHRASE_MAX);
        lv_obj_add_event_cb(s_secret_confirm,secret_focus_cb,LV_EVENT_FOCUSED,NULL);
        lv_obj_add_event_cb(s_secret_confirm,secret_focus_cb,LV_EVENT_CLICKED,NULL);
    }
    lv_obj_add_event_cb(s_secret_input,secret_focus_cb,LV_EVENT_FOCUSED,NULL);
    lv_obj_add_event_cb(s_secret_input,secret_focus_cb,LV_EVENT_CLICKED,NULL);
    s_secret_status=ui_form_text(body,"",UI_FONT_CAPTION,UI_DANGER_BRIGHT);
    s_secret_keyboard=ui_form_keyboard(s_secret_popup,s_secret_input,LV_KEYBOARD_MODE_TEXT_LOWER);
    ui_popup_keyboard_actions(s_secret_keyboard,secret_submit_cb,secret_cancel_cb);
    ui_form_action(s_secret_popup,UI_BUTTON_CANCEL,"CANCEL",secret_cancel_cb,NULL);
    ui_form_action(s_secret_popup,UI_BUTTON_PRIMARY,restore?"VERIFY":"SAVE",secret_submit_cb,NULL);
    ui_form_finish(s_secret_popup);
    lv_obj_scroll_to_view(s_secret_input,LV_ANIM_OFF);
}


static void encrypted_backup_request_cb(lv_event_t *event)
{
    (void)event;
    show_secret_popup(false);
}


static void encrypted_restore_request_cb(lv_event_t *event)
{
    (void)event;
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
    char status[192];
    (void)settings_backup_remove_all(status, sizeof(status));
    close_remove_confirmation();
    set_status(status);
}


static void remove_request_cb(lv_event_t *event)
{
    (void)event;if(s_remove_confirmation){lv_obj_move_foreground(s_remove_confirmation);return;}
    s_remove_confirmation=ui_form_dialog("REMOVE ALL BACKUPS?",720,400,UI_POPUP_DANGER);
    if(!s_remove_confirmation)return;
    backup_dialog_own(s_remove_confirmation);
    ui_form_text(ui_form_body(s_remove_confirmation),"This permanently removes plain and encrypted configuration backups, including temporary recovery copies, from the SD card. Live printer profiles and interface settings are not changed.",UI_FONT_BODY,UI_TEXT);
    ui_form_action(s_remove_confirmation,UI_BUTTON_CANCEL,"CANCEL",remove_cancel_cb,NULL);
    ui_form_action(s_remove_confirmation,UI_BUTTON_DANGER,"REMOVE",remove_confirm_cb,NULL);
    ui_form_finish(s_remove_confirmation);
}


static void backup_action(lv_obj_t *row,ui_button_kind_t kind,const char *text,lv_event_cb_t callback)
{
    lv_obj_t *button=ui_button_create(row,kind,text);
    lv_point_t size;lv_text_get_size(&size,text,UI_FONT_BODY,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    int width=size.x+24>240?size.x+24:240;
    lv_obj_set_size(button,width,48);lv_obj_set_style_min_width(button,width,0);
    lv_obj_set_flex_grow(button,1);
    ui_apply_custom_label_style(lv_obj_get_child(button,0),UI_FONT_BODY,UI_TEXT);
    lv_obj_add_event_cb(button,callback,LV_EVENT_CLICKED,NULL);
}
void ui_settings_backup_popup_show(void)
{
    if(s_popup){lv_obj_move_foreground(s_popup);return;}
    s_popup=ui_form_dialog("CONFIGURATION BACKUP",880,540,UI_POPUP_STANDARD);
    if(!s_popup)return;
    backup_dialog_own(s_popup);lv_obj_t *body=ui_form_body(s_popup);
    ui_form_text(body,"PLAIN SD CARD FILE",UI_FONT_CAPTION,UI_TEXT_DIM);
    ui_form_text(body,SETTINGS_BACKUP_PATH,UI_FONT_BODY,UI_TEXT);
    ui_form_text(body,"ENCRYPTED SD CARD FILE",UI_FONT_CAPTION,UI_TEXT_DIM);
    ui_form_text(body,SETTINGS_BACKUP_ENCRYPTED_PATH,UI_FONT_BODY,UI_TEXT);
    s_status=ui_form_text(body,"Plain backups exclude Moonraker API keys. Encrypted backups include them.",UI_FONT_BODY,UI_TEXT_DIM);
    lv_obj_t *actions=lv_obj_create(body);ui_responsive_column(actions);
    lv_obj_set_flex_flow(actions,LV_FLEX_FLOW_ROW_WRAP);lv_obj_set_style_pad_column(actions,8,0);
    backup_action(actions,UI_BUTTON_PRIMARY,"BACK UP PLAIN",backup_cb);
    backup_action(actions,UI_BUTTON_PRIMARY,"ENCRYPT",encrypted_backup_request_cb);
    backup_action(actions,UI_BUTTON_DANGER,"RESTORE PLAIN",restore_request_cb);
    backup_action(actions,UI_BUTTON_DANGER,"RESTORE ENCRYPTED",encrypted_restore_request_cb);
    backup_action(actions,UI_BUTTON_DANGER,"REMOVE BACKUPS",remove_request_cb);
    ui_form_action(s_popup,UI_BUTTON_CLOSE,"CLOSE",close_cb,NULL);ui_form_finish(s_popup);
}
