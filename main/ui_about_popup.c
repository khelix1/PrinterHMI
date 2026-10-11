#include "ui_about_popup.h"
#include "ui_text.h"

#include <stdio.h>

#include "esp_app_desc.h"
#include "firmware_identity.h"
#include "lvgl.h"
#include "ui_form_dialog.h"

static lv_obj_t *s_popup;

static void about_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_popup) s_popup = NULL;
}

void ui_about_popup_close(void)
{
    lv_obj_t *popup = s_popup;
    s_popup = NULL;
    if (popup) lv_obj_delete(popup);
}

static void close_cb(lv_event_t *event)
{
    (void)event;
    ui_about_popup_close();
}

static lv_obj_t *about_section(lv_obj_t *parent, const char *heading)
{
    lv_obj_t *section = lv_obj_create(parent);
    lv_obj_remove_style_all(section);
    ui_apply_surface_role(section, UI_SURFACE_SECTION);
    lv_obj_set_size(section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(section, 16, 0);
    lv_obj_set_style_pad_row(section, 10, 0);
    lv_obj_clear_flag(section, LV_OBJ_FLAG_SCROLLABLE);
    ui_form_text(section, heading, UI_FONT_CAPTION, UI_TEXT_MUTED);
    return section;
}

static void about_field(lv_obj_t *parent, const char *name, const char *value)
{
    char text[192];
    snprintf(text, sizeof(text), "%s  %s", name, value && value[0] ? value : "--");
    ui_form_text(parent, text, UI_FONT_BODY, UI_TEXT_DIM);
}

void ui_about_popup_show(void)
{
    if (s_popup) {
        lv_obj_move_foreground(s_popup);
        return;
    }
    s_popup = ui_form_dialog(ui_text("ABOUT PRINTERHMI"), 760, 520, UI_POPUP_STANDARD);
    if (!s_popup) return;
    lv_obj_add_event_cb(s_popup, about_deleted, LV_EVENT_DELETE, NULL);

    lv_obj_t *body = ui_form_body(s_popup);
    ui_form_text(body, "PrinterHMI", UI_FONT_HEADING, UI_TEXT);
    ui_form_text(body, "OPEN-SOURCE PRINT-CELL OPERATOR INTERFACE", UI_FONT_CAPTION, UI_TEXT_MUTED);
    ui_form_text(body,
        "A local-first touchscreen interface for Klipper and Moonraker print cells. "
        "Live printer status, multi-printer management and operator tools in a dedicated ESP32-P4 appliance.",
        UI_FONT_BODY, UI_TEXT_DIM);

    firmware_identity_t identity;
    firmware_identity_get(&identity);
    const esp_app_desc_t *app = esp_app_get_description();
    lv_obj_t *software = about_section(body, ui_text("INSTALLED FIRMWARE"));
    ui_form_text(software, identity.version, UI_FONT_HEADING, UI_ACCENT_BRIGHT);
    about_field(software, "Channel", firmware_identity_channel_name(identity.channel));
    char revision[48];
    snprintf(revision, sizeof(revision), "%s%s", identity.revision,
             identity.modified && identity.channel == FIRMWARE_CHANNEL_CUSTOM ? " (modified)" : "");
    about_field(software, "Source", revision);
    char build[48];
    if (app && app->date[0] && app->time[0])
        snprintf(build, sizeof(build), "%.*s %.*s",
                 (int)sizeof(app->date) - 1, app->date,
                 (int)sizeof(app->time) - 1, app->time);
    else snprintf(build, sizeof(build), "--");
    about_field(software, "Built", build);
    about_field(software, "ESP-IDF", app ? app->idf_ver : NULL);
    char lvgl_version[32];
    snprintf(lvgl_version, sizeof(lvgl_version), "%d.%d.%d",
             lv_version_major(), lv_version_minor(), lv_version_patch());
    about_field(software, "LVGL", lvgl_version);
    about_field(software, "Platform", "ESP32-P4 / ESP32-C6 hosted networking");

    lv_obj_t *credits = about_section(body, ui_text("SOFTWARE & CREDITS"));
    ui_form_text(credits, "Designed and developed by khelix.", UI_FONT_BODY, UI_TEXT);
    ui_form_text(credits,
        "Built with the open-source ESP-IDF, LVGL, ESP-Hosted, Klipper and Moonraker projects.",
        UI_FONT_BODY, UI_TEXT_DIM);
    ui_form_text(credits, "github.com/khelix1/PrinterHMI", UI_FONT_CAPTION, UI_TEXT_MUTED);
    ui_form_action(s_popup, UI_BUTTON_OUTLINED, "CLOSE", close_cb, NULL);
    ui_form_finish(s_popup);
    lv_obj_move_foreground(s_popup);
}
