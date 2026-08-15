#include "ui_splash.h"

#include "esp_app_desc.h"
#include "lvgl.h"
#include "ui_logo_assets.h"
#include "ui_theme.h"
#include <stdio.h>

static lv_obj_t *splash_root = NULL;
static lv_obj_t *splash_bar = NULL;
static lv_obj_t *splash_status = NULL;
static lv_obj_t *splash_percent = NULL;

static void ui_splash_set_progress(int pct, const char *status)
{
    if (!splash_root) return;

    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    /*
     * The splash is top-layered, but this panel still visibly reacts to
     * frequent full-scene invalidations. Retain useful progress movement
     * while limiting it to four stable stages.
     */
    if (pct != 5 && pct != 45 && pct != 88 && pct != 100) {
        return;
    }

    if (splash_bar) lv_bar_set_value(splash_bar, pct, LV_ANIM_OFF);

    if (splash_percent) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(splash_percent, buf);
    }

    if (splash_status && status) {
        lv_label_set_text(splash_status, status);
    }

}


void ui_splash_display_ready(void)
{
    /* Page construction is complete; raise the overlay exactly once. */
    if (splash_root) lv_obj_move_foreground(splash_root);
    ui_splash_set_progress(25, "Display and touch online...");
}

void ui_splash_wifi_starting(void)
{
    ui_splash_set_progress(45, "Starting WiFi...");
}

void ui_splash_wifi_waiting(bool connected)
{
    ui_splash_set_progress(connected ? 75 : 62,
                               connected ? "WiFi connected. Starting services..." : "WiFi pending. Continuing offline...");
}

void ui_splash_moonraker_ready(void)
{
    ui_splash_set_progress(88, "Moonraker polling active...");
}

void ui_splash_dashboard_ready(void)
{
    ui_splash_set_progress(100, "Dashboard ready.");
}

void ui_splash_create(void)
{
    if (splash_root) return;

    /*
     * Startup pages are created on the active screen. Put the opaque splash
     * on LVGL's top layer so those allocations cannot briefly draw above it.
     */
    lv_obj_t *scr = lv_layer_top();

    splash_root = lv_obj_create(scr);
    /* Prevent intermediate object draws from flashing the display. */
    lv_obj_add_flag(splash_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(splash_root, 1024, 600);
    lv_obj_set_pos(splash_root, 0, 0);
    lv_obj_clear_flag(splash_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(splash_root, UI_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(splash_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(splash_root, 0, 0);
    lv_obj_set_style_radius(splash_root, 0, 0);
    lv_obj_set_style_pad_all(splash_root, 0, 0);

    lv_obj_t *panel = lv_obj_create(splash_root);
    lv_obj_set_size(panel, 700, 360);
    lv_obj_center(panel);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, UI_NAV, 0);
    lv_obj_set_style_border_color(panel, UI_BORDER_SOFT, 0);
    lv_obj_set_style_border_width(panel, UI_BORDER_STRONG, 0);
    lv_obj_set_style_radius(panel, UI_RADIUS_SPLASH, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);

    lv_obj_t *logo = lv_image_create(panel);
    lv_image_set_src(logo, ui_logo_assets_splash());
    lv_obj_align(logo, LV_ALIGN_TOP_MID, 0, 18);

    const esp_app_desc_t *app =
        esp_app_get_description();
    const char *version =
        app && app->version[0]
            ? app->version
            : "--";

    char version_text[64];
    snprintf(
        version_text,
        sizeof(version_text),
        "%s%s  |  ESP32-P4 + ESP32-C6",
        version[0] == 'v' ? "" : "v",
        version);

    lv_obj_t *sub = lv_label_create(panel);
    lv_label_set_text(sub, version_text);
    ui_apply_text_body(sub);
    ui_apply_label_dim(sub);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 130);

    splash_status = lv_label_create(panel);
    lv_label_set_text(splash_status, "Starting...");
    ui_apply_text_title(splash_status);
    lv_obj_set_style_text_color(splash_status, UI_BORDER_BRIGHT, 0);
    lv_obj_align(splash_status, LV_ALIGN_TOP_MID, 0, 164);

    splash_bar = lv_bar_create(panel);
    lv_obj_set_size(splash_bar, 520, 24);
    lv_obj_align(splash_bar, LV_ALIGN_TOP_MID, 0, 216);
    lv_bar_set_range(splash_bar, 0, 100);
    lv_bar_set_value(splash_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(splash_bar, UI_RADIUS_BAR, 0);
    lv_obj_set_style_bg_color(splash_bar, UI_PROGRESS_TRACK, 0);
    lv_obj_set_style_bg_color(splash_bar, UI_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(
        splash_bar,
        UI_RADIUS_BAR,
        LV_PART_INDICATOR);

    splash_percent = lv_label_create(panel);
    lv_label_set_text(splash_percent, "0%");
    ui_apply_text_body_large(splash_percent);
    ui_apply_label_primary(splash_percent);
    lv_obj_align(splash_percent, LV_ALIGN_TOP_MID, 0, 254);

    lv_obj_t *footer = lv_label_create(panel);
    lv_label_set_text(footer, "Industrial control interface");
    ui_apply_text_body(footer);
    ui_apply_label_dim(footer);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -26);

    ui_splash_set_progress(5, "Booting display stack...");
    lv_obj_clear_flag(splash_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(splash_root);
}

void ui_splash_destroy(void)
{
    if (!splash_root) return;

    lv_obj_delete(splash_root);
    splash_root = NULL;
    splash_bar = NULL;
    splash_status = NULL;
    splash_percent = NULL;
}
