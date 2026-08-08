#include "ui_settings_backup_popup.h"

#include "lvgl.h"
#include "settings_backup.h"
#include "settings_backup_crypto.h"
#include "settings_backup_encrypted_worker.h"
#include "ui_popup.h"

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


static void set_status(const char *text)
{
    if (s_status) {
        lv_label_set_text(s_status, text ? text : "");
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

    if (s_secret_popup) {
        lv_obj_delete(s_secret_popup);
    }

    s_secret_popup = NULL;
    s_secret_input = NULL;
    s_secret_confirm = NULL;
    s_secret_keyboard = NULL;
    s_secret_is_restore = false;
}


void ui_settings_backup_popup_close(void)
{
    close_restore_confirmation();
    close_remove_confirmation();
    close_secret_popup(false);

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
    (void)event;
    char status[256];
    bool encrypted = s_pending_encrypted_restore;

    if (encrypted) {
        (void)settings_backup_restore_encrypted(
            s_restore_passphrase,
            status,
            sizeof(status));
    } else {
        (void)settings_backup_restore(status, sizeof(status));
    }

    close_restore_confirmation();
    set_status(status);
}


static void show_restore_confirmation(
    const char *preflight,
    bool encrypted)
{
    if (s_restore_confirmation) {
        lv_obj_move_foreground(s_restore_confirmation);
        return;
    }

    s_pending_encrypted_restore = encrypted;
    s_restore_confirmation = ui_popup_create(
        lv_layer_top(),
        700,
        350,
        UI_POPUP_DANGER);
    if (!s_restore_confirmation) {
        secure_clear(s_restore_passphrase, sizeof(s_restore_passphrase));
        return;
    }

    ui_popup_add_title(
        s_restore_confirmation,
        encrypted ? "RESTORE ENCRYPTED BACKUP?" : "RESTORE CONFIGURATION?",
        true,
        0);
    ui_popup_add_header_divider(s_restore_confirmation, 44);

    char confirmation[448];
    snprintf(
        confirmation,
        sizeof(confirmation),
        "%s\n\nRESTORE replaces current printer profiles and settings. "
        "A reboot is required afterward.",
        preflight ? preflight : "Backup verified.");
    ui_popup_add_body(
        s_restore_confirmation,
        confirmation,
        24,
        64,
        652);
    ui_popup_add_standard_footer_divider(s_restore_confirmation);
    ui_popup_add_footer_action(
        s_restore_confirmation,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        220,
        UI_POPUP_FOOTER_LEFT,
        restore_cancel_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_restore_confirmation,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_REFRESH " RESTORE",
        220,
        UI_POPUP_FOOTER_RIGHT,
        restore_confirm_cb,
        NULL,
        NULL);
    lv_obj_move_foreground(s_restore_confirmation);
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
        lv_label_set_text(s_export_progress_status, status);
    }
    if (s_export_progress_value) {
        lv_label_set_text(s_export_progress_value, value_text);
    }
    if (s_export_progress_bar) {
        lv_bar_set_value(s_export_progress_bar, value, LV_ANIM_OFF);
    }
}


static void show_export_progress_popup(bool restore_verify)
{
    if (s_export_progress_popup) {
        lv_obj_move_foreground(s_export_progress_popup);
        return;
    }
    s_export_progress_popup = ui_popup_create(
        lv_layer_top(), 680, 300, UI_POPUP_STANDARD);
    if (!s_export_progress_popup) {
        return;
    }
    s_export_progress_is_restore_verify = restore_verify;
    ui_popup_add_title(
        s_export_progress_popup,
        restore_verify ? "VERIFYING ENCRYPTED BACKUP" : "SAVING ENCRYPTED BACKUP",
        false,
        0);
    ui_popup_add_header_divider(s_export_progress_popup, 44);
    s_export_progress_status = ui_popup_add_progress_status(
        s_export_progress_popup,
        restore_verify
            ? "Verifying encrypted backup passphrase and contents..."
            : "Preparing configuration backup...",
        24, 78, 632);
    s_export_progress_value = ui_popup_add_progress_value(
        s_export_progress_popup, "WORKING", 24, 116, 632);
    s_export_progress_bar = ui_popup_add_progress_bar(
        s_export_progress_popup, 48, 176, 584, 24, 0, 100, 10);
    ui_popup_add_progress_detail(
        s_export_progress_popup,
        restore_verify
            ? "Do not power off while the encrypted backup is being verified."
            : "Do not power off while the encrypted SD backup is being created.",
        24, 220, 632);
    s_export_progress_pulse = 0;
    s_export_progress_timer = lv_timer_create(export_progress_timer_cb, 150, NULL);
    lv_obj_move_foreground(s_export_progress_popup);
}


static void secret_submit_cb(lv_event_t *event)
{
    (void)event;
    const char *passphrase = s_secret_input
        ? lv_textarea_get_text(s_secret_input)
        : "";

    if (strnlen(passphrase, BACKUP_PASSPHRASE_MAX + 1) <
        SETTINGS_BACKUP_CRYPTO_MIN_PASSPHRASE_LENGTH) {
        set_status("Use a passphrase of at least 12 characters.");
        return;
    }

    if (!s_secret_is_restore) {
        const char *confirmation = s_secret_confirm
            ? lv_textarea_get_text(s_secret_confirm)
            : "";
        if (strcmp(passphrase, confirmation) != 0) {
            set_status("The two passphrases do not match.");
            return;
        }

        char status[256];
        if (!settings_backup_encrypted_worker_start_export(
                passphrase,
                status,
                sizeof(status))) {
            close_secret_popup(false);
            set_status(status);
            return;
        }
        close_secret_popup(false);
        show_export_progress_popup(false);
        return;
    }

    strlcpy(
        s_restore_passphrase,
        passphrase,
        sizeof(s_restore_passphrase));

    char status[256];
    if (!settings_backup_encrypted_worker_start_verify(
            s_restore_passphrase,
            status,
            sizeof(status))) {
        secure_clear(s_restore_passphrase, sizeof(s_restore_passphrase));
        set_status(status);
        return;
    }

    close_secret_popup(true);
    show_export_progress_popup(true);
}


static void show_secret_popup(bool restore)
{
    if (s_secret_popup) {
        lv_obj_move_foreground(s_secret_popup);
        return;
    }

    s_secret_is_restore = restore;
    s_secret_popup = ui_popup_create(
        lv_layer_top(),
        800,
        580,
        UI_POPUP_STANDARD);
    if (!s_secret_popup) {
        return;
    }

    ui_popup_add_title(
        s_secret_popup,
        restore ? "OPEN ENCRYPTED BACKUP" : "CREATE ENCRYPTED BACKUP",
        false,
        0);
    ui_popup_add_header_divider(s_secret_popup, 44);
    ui_popup_add_caption(
        s_secret_popup,
        restore ? "BACKUP PASSPHRASE" : "NEW PASSPHRASE",
        40,
        66,
        300);
    s_secret_input = ui_popup_add_textarea(
        s_secret_popup,
        720,
        44,
        LV_ALIGN_TOP_MID,
        0,
        90,
        true,
        true,
        BACKUP_PASSPHRASE_MAX,
        "At least 12 characters",
        NULL,
        NULL);
    if (s_secret_input) {
        lv_obj_add_event_cb(
            s_secret_input,
            secret_focus_cb,
            LV_EVENT_FOCUSED,
            NULL);
        lv_obj_add_event_cb(
            s_secret_input,
            secret_focus_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    int keyboard_y = 150;
    if (!restore) {
        ui_popup_add_caption(
            s_secret_popup,
            "CONFIRM PASSPHRASE",
            40,
            142,
            300);
        s_secret_confirm = ui_popup_add_textarea(
            s_secret_popup,
            720,
            44,
            LV_ALIGN_TOP_MID,
            0,
            166,
            true,
            true,
            BACKUP_PASSPHRASE_MAX,
            "Enter the same passphrase again",
            NULL,
            NULL);
        if (s_secret_confirm) {
            lv_obj_add_event_cb(
                s_secret_confirm,
                secret_focus_cb,
                LV_EVENT_FOCUSED,
                NULL);
            lv_obj_add_event_cb(
                s_secret_confirm,
                secret_focus_cb,
                LV_EVENT_CLICKED,
                NULL);
        }
        keyboard_y = 236;
    }

    s_secret_keyboard = ui_popup_add_keyboard(
        s_secret_popup,
        s_secret_input,
        760,
        220,
        LV_ALIGN_TOP_MID,
        0,
        keyboard_y,
        LV_KEYBOARD_MODE_TEXT_LOWER);
    ui_popup_add_footer_divider(s_secret_popup, 470);
    ui_popup_add_action_at(
        s_secret_popup,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        24,
        494,
        260,
        52,
        secret_cancel_cb,
        NULL,
        NULL);
    ui_popup_add_action_at(
        s_secret_popup,
        UI_POPUP_ACTION_CONFIRM,
        restore ? LV_SYMBOL_REFRESH " VERIFY" : LV_SYMBOL_SAVE " SAVE ENCRYPTED",
        496,
        494,
        280,
        52,
        secret_submit_cb,
        NULL,
        NULL);
    lv_obj_move_foreground(s_secret_popup);
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
    (void)event;
    if (s_remove_confirmation) {
        lv_obj_move_foreground(s_remove_confirmation);
        return;
    }

    s_remove_confirmation = ui_popup_create(
        lv_layer_top(),
        680,
        300,
        UI_POPUP_DANGER);
    if (!s_remove_confirmation) {
        return;
    }
    ui_popup_add_title(s_remove_confirmation, "REMOVE ALL BACKUPS?", true, 0);
    ui_popup_add_header_divider(s_remove_confirmation, 44);
    ui_popup_add_body(
        s_remove_confirmation,
        "This permanently removes plain and encrypted configuration backups, "
        "including temporary recovery copies, from the SD card. Live printer "
        "profiles and interface settings are not changed.",
        24,
        66,
        632);
    ui_popup_add_standard_footer_divider(s_remove_confirmation);
    ui_popup_add_footer_action(
        s_remove_confirmation,
        UI_POPUP_ACTION_CANCEL,
        LV_SYMBOL_CLOSE " CANCEL",
        220,
        UI_POPUP_FOOTER_LEFT,
        remove_cancel_cb,
        NULL,
        NULL);
    ui_popup_add_footer_action(
        s_remove_confirmation,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_TRASH " REMOVE",
        220,
        UI_POPUP_FOOTER_RIGHT,
        remove_confirm_cb,
        NULL,
        NULL);
    lv_obj_move_foreground(s_remove_confirmation);
}


void ui_settings_backup_popup_show(void)
{
    if (s_popup) {
        lv_obj_move_foreground(s_popup);
        return;
    }

    s_popup = ui_popup_create(
        lv_layer_top(),
        800,
        450,
        UI_POPUP_STANDARD);
    if (!s_popup) {
        return;
    }

    ui_popup_add_title(s_popup, "CONFIGURATION BACKUP", false, 0);
    ui_popup_add_header_divider(s_popup, 44);
    ui_popup_add_caption(s_popup, "PLAIN SD CARD FILE", 24, 68, 250);
    ui_popup_add_body(s_popup, SETTINGS_BACKUP_PATH, 24, 92, 752);
    ui_popup_add_caption(s_popup, "ENCRYPTED SD CARD FILE", 24, 126, 280);
    ui_popup_add_body(s_popup, SETTINGS_BACKUP_ENCRYPTED_PATH, 24, 150, 752);
    s_status = ui_popup_add_status_label(
        s_popup,
        "Plain backups exclude Moonraker API keys. Encrypted backups include them.",
        24,
        198,
        752);

    ui_popup_add_footer_divider(s_popup, 292);
    ui_popup_add_action_at(
        s_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_SAVE " BACK UP",
        20,
        312,
        240,
        50,
        backup_cb,
        NULL,
        NULL);
    ui_popup_add_action_at(
        s_popup,
        UI_POPUP_ACTION_PRIMARY,
        LV_SYMBOL_SAVE " ENCRYPT",
        280,
        312,
        240,
        50,
        encrypted_backup_request_cb,
        NULL,
        NULL);
    ui_popup_add_action_at(
        s_popup,
        UI_POPUP_ACTION_PRIMARY,
        LV_SYMBOL_REFRESH " RESTORE ENCRYPTED",
        540,
        312,
        240,
        50,
        encrypted_restore_request_cb,
        NULL,
        NULL);
    ui_popup_add_action_at(
        s_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        20,
        372,
        240,
        50,
        close_cb,
        NULL,
        NULL);
    ui_popup_add_action_at(
        s_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_REFRESH " RESTORE PLAIN",
        280,
        372,
        240,
        50,
        restore_request_cb,
        NULL,
        NULL);
    ui_popup_add_action_at(
        s_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_TRASH " CLEAR ALL",
        540,
        372,
        240,
        50,
        remove_request_cb,
        NULL,
        NULL);
    lv_obj_move_foreground(s_popup);
}
