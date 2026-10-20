#include "ui_widgets.h"
#include "ui_about_popup.h"
#include "ui_text.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "firmware_identity.h"
#include "lvgl.h"
#include "ui_form_dialog.h"

static lv_obj_t *s_popup;
static uint32_t s_about_epoch;
static bool s_about_building;

static void about_deleted(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) == s_popup){s_popup=NULL;++s_about_epoch;}
}

void ui_about_popup_close(void)
{
    ++s_about_epoch;
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
    if (ui_theme_is_studio()) {
        ui_apply_surface_role(section, UI_SURFACE_TRANSPARENT);
        ui_apply_divider_border(section, LV_BORDER_SIDE_BOTTOM);
    }
    lv_obj_set_size(section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(section, UI_PAD_CARD, 0);
    lv_obj_set_style_pad_row(section, UI_GAP_ROW, 0);
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

static void about_build(void)
{
    if (s_popup) {
        lv_obj_move_foreground(s_popup);
        return;
    }
    uint32_t epoch=s_about_epoch;
    firmware_identity_t identity;
    firmware_identity_get(&identity);
    if(epoch!=s_about_epoch)return;
    const esp_app_desc_t *description=esp_app_get_description();
    if(epoch!=s_about_epoch)return;
    esp_app_desc_t snapshot;
    if(description)memcpy(&snapshot,description,sizeof(snapshot));
    const esp_app_desc_t *app=description?&snapshot:NULL;
    s_popup = ui_form_dialog(ui_text("About PrinterHMI"), 760, 520, UI_POPUP_STANDARD);
    if (!s_popup) return;
    ui_form_divide_title(s_popup);
    lv_obj_set_style_pad_row(s_popup, UI_GAP_ROW, 0);
    lv_obj_add_event_cb(s_popup, about_deleted, LV_EVENT_DELETE, NULL);

    lv_obj_t *body = ui_form_body(s_popup);
    lv_obj_set_style_text_line_space(body, 4, 0);
    ui_form_text(body, "PrinterHMI", UI_FONT_HEADING, UI_TEXT);
    ui_form_text(body, "Open-source print-cell operator interface", UI_FONT_CAPTION, UI_TEXT_MUTED);
    ui_form_text(body,
        "A local-first touchscreen interface for Klipper and Moonraker print cells. "
        "Live printer status, multi-printer management and operator tools in a dedicated ESP32-P4 appliance.",
        UI_FONT_BODY, UI_TEXT_DIM);

    lv_obj_t *software = about_section(body, ui_text("Installed firmware"));
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

    lv_obj_t *credits = about_section(body, ui_text("Created by khelix"));
    ui_form_text(credits,
        "Concept, product direction, visual design, feature decisions and final approval by khelix.",
        UI_FONT_BODY, UI_TEXT);
    ui_form_text(credits, "AI-assisted development", UI_FONT_BODY, UI_ACCENT_BRIGHT);
    ui_form_text(credits,
        "OpenAI ChatGPT/Codex assisted with coding, debugging, code review, test development, "
        "documentation and exploring implementation options. "
        "Design decisions, hardware verification and release approval remain with khelix.",
        UI_FONT_BODY, UI_TEXT_DIM);

    lv_obj_t *foundation = about_section(body, ui_text("Open-source foundations"));
    ui_form_text(foundation, "With thanks to these projects and their authors and contributors.",
        UI_FONT_BODY, UI_TEXT_DIM);
    ui_form_text(foundation,
        "ESP-IDF, ESP-Hosted and ESP Wi-Fi Remote - firmware and hosted networking. "
        "Espressif ESP-BSP, esp_lvgl_port, LCD/touch drivers, esp_websocket_client, "
        "esp_codec_dev and USB - board support, display, input and device services.",
        UI_FONT_BODY, UI_TEXT_DIM);
    ui_form_text(foundation,
        "LVGL - interface, widgets and graphics. "
        "cJSON - JSON parsing. LodePNG by Lode Vandevenne - PNG previews.",
        UI_FONT_BODY, UI_TEXT_DIM);

    lv_obj_t *sdk = about_section(body, ui_text("Supporting libraries"));
    ui_form_text(sdk,
        "FreeRTOS, lwIP, Mbed TLS, FatFs and Newlib/Picolibc, as supplied by the selected "
        "ESP-IDF toolchain - scheduling, networking, TLS, storage and C runtime.",
        UI_FONT_BODY, UI_TEXT_DIM);
    ui_form_text(sdk,
        "Additional declared Espressif components include CMake Utilities, EPPP Link, "
        "ESP Serial Slave Link and Wi-Fi Remote over EPPP. "
        "The source credits list the locked components and upstream notices; "
        "optional components depend on build configuration.",
        UI_FONT_BODY, UI_TEXT_DIM);

    lv_obj_t *fonts = about_section(body, ui_text("Fonts & artwork"));
    ui_form_text(fonts,
        "Inter - The Inter Project Authors. Montserrat - The Montserrat Project Authors. "
        "Font Awesome - Fonticons, Inc.; symbols supplied through LVGL. "
        "Inter, Montserrat and Font Awesome font software use the SIL Open Font License 1.1.",
        UI_FONT_BODY, UI_TEXT_DIM);
    ui_form_text(fonts,
        "DejaVu Sans / Bitstream Vera - lettering used in PrinterHMI logo artwork. "
        "Original font copyright and license notices are retained in the source credits.",
        UI_FONT_BODY, UI_TEXT_DIM);

    lv_obj_t *ecosystem = about_section(body, ui_text("Printer ecosystem"));
    ui_form_text(ecosystem,
        "Klipper and Moonraker - the printer firmware and API that PrinterHMI connects to. "
        "PrusaSlicer - compatible G-code thumbnails and metadata. "
        "These projects run separately from this panel firmware.",
        UI_FONT_BODY, UI_TEXT_DIM);

    lv_obj_t *tools = about_section(body, ui_text("Development tools"));
    ui_form_text(tools,
        "Espressif toolchains, GCC, CMake, Ninja, Python and Git; "
        "lv_font_conv and fontTools for fonts; Inkscape for logo exports; "
        "GitHub for source hosting and release workflows.",
        UI_FONT_BODY, UI_TEXT_DIM);

    lv_obj_t *notices = about_section(body, ui_text("Source & notices"));
    ui_form_text(notices, "github.com/khelix1/PrinterHMI", UI_FONT_BODY, UI_TEXT);
    ui_form_text(notices,
        "PrinterHMI: Apache License 2.0. Third-party projects retain their own copyrights and licenses. "
        "See NOTICE, docs/SOFTWARE_CREDITS.md and docs/licenses in the source repository.",
        UI_FONT_BODY, UI_TEXT_DIM);
    ui_form_action(s_popup, UI_BUTTON_OUTLINED, "Close", close_cb, NULL);
    ui_form_finish(s_popup);
    lv_obj_move_foreground(s_popup);
}

void ui_about_popup_show(void)
{
    if(s_about_building)return;
    s_about_building=true;
    about_build();
    s_about_building=false;
}
