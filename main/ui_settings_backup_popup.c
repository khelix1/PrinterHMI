#include "ui_settings_backup_popup.h"

#include "lvgl.h"
#include "settings_backup.h"
#include "ui_popup.h"

#include <stdio.h>

static lv_obj_t *s_popup = NULL;
static lv_obj_t *s_status = NULL;
static lv_obj_t *s_restore_confirmation = NULL;


static void set_status(const char *text)
{
    if (s_status) {
        lv_label_set_text(
            s_status,
            text ? text : "");
    }
}


static void close_restore_confirmation(void)
{
    if (s_restore_confirmation) {
        lv_obj_delete(
            s_restore_confirmation);
    }

    s_restore_confirmation = NULL;
}


void ui_settings_backup_popup_close(void)
{
    close_restore_confirmation();

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

    char status[160];

    (void)settings_backup_export(
        status,
        sizeof(status));

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

    char status[160];

    (void)settings_backup_restore(
        status,
        sizeof(status));

    close_restore_confirmation();
    set_status(status);
}


static void restore_request_cb(lv_event_t *event)
{
    (void)event;

    char preflight[256];
    if (!settings_backup_preflight(preflight, sizeof(preflight))) {
        set_status(preflight);
        return;
    }

    if (s_restore_confirmation) {
        lv_obj_move_foreground(
            s_restore_confirmation);
        return;
    }

    s_restore_confirmation =
        ui_popup_create(
            lv_layer_top(),
            680,
            330,
            UI_POPUP_DANGER);

    if (!s_restore_confirmation) {
        return;
    }

    ui_popup_add_title(
        s_restore_confirmation,
        "RESTORE CONFIGURATION?",
        true,
        0);

    ui_popup_add_header_divider(
        s_restore_confirmation,
        44);

    char confirmation[384];
    snprintf(
        confirmation,
        sizeof(confirmation),
        "%s\n\nRESTORE replaces current printer profiles and settings. "
        "A reboot is required afterward.",
        preflight);

    ui_popup_add_body(
        s_restore_confirmation,
        confirmation,
        24,
        64,
        632);

    ui_popup_add_standard_footer_divider(
        s_restore_confirmation);

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

    lv_obj_move_foreground(
        s_restore_confirmation);
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
        360,
        UI_POPUP_STANDARD);

    if (!s_popup) {
        return;
    }

    ui_popup_add_title(
        s_popup,
        "CONFIGURATION BACKUP",
        false,
        0);

    ui_popup_add_header_divider(
        s_popup,
        44);

    ui_popup_add_caption(
        s_popup,
        "SD CARD FILE",
        24,
        68,
        180);

    ui_popup_add_body(
        s_popup,
        SETTINGS_BACKUP_PATH,
        24,
        94,
        752);

    s_status = ui_popup_add_status_label(
        s_popup,
        "Back up or restore printer profiles and interface settings.",
        24,
        142,
        752);

    ui_popup_add_standard_footer_divider(
        s_popup);

    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        210,
        UI_POPUP_FOOTER_LEFT,
        close_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_CONFIRM,
        LV_SYMBOL_SAVE " BACK UP",
        210,
        UI_POPUP_FOOTER_CENTER,
        backup_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_popup,
        UI_POPUP_ACTION_DANGER,
        LV_SYMBOL_REFRESH " RESTORE",
        210,
        UI_POPUP_FOOTER_RIGHT,
        restore_request_cb,
        NULL,
        NULL);

    lv_obj_move_foreground(s_popup);
}
