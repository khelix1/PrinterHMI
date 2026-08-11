#include "ui_shell.h"
#include "ui_button.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_global_estop.h"
#include "ui_i18n.h"
#include "ui_i18n_shell.h"

#include "esp_log.h"
#include "esp_heap_caps.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static const char TAG[] = "ui_shell";

typedef struct {
    lv_obj_t *top_bar;
    lv_obj_t *printer_button;
    lv_obj_t *title_label;
    ui_shell_printer_switch_cb_t printer_switch_callback;
    lv_obj_t *clock_label;
    lv_obj_t *wifi_bars[4];
    bool wifi_signal_connected;
    int wifi_signal_rssi;
    int wifi_signal_bars;
    lv_obj_t *eta_label;
    lv_obj_t *nav_rail;
    lv_obj_t *nav_buttons[UI_SHELL_PAGE_COUNT];
    lv_timer_t *clock_timer;
} ui_shell_state_t;

/*
 * This context is allocated once after the scheduler starts and remains
 * allocated for the application lifetime. Only s_shell occupies startup
 * internal RAM.
 */
static ui_shell_state_t *s_shell = NULL;

#define shell_top_bar             (s_shell->top_bar)
#define s_shell_printer_button    (s_shell->printer_button)
#define s_shell_title_label       (s_shell->title_label)
#define s_printer_switch_callback (s_shell->printer_switch_callback)
#define shell_clock_label         (s_shell->clock_label)
#define shell_topbar_wifi_bars    (s_shell->wifi_bars)
#define shell_topbar_eta_label    (s_shell->eta_label)
#define shell_nav_rail            (s_shell->nav_rail)
#define shell_nav_buttons         (s_shell->nav_buttons)
#define s_clock_timer             (s_shell->clock_timer)


static bool shell_state_init(void)
{
    if (s_shell) {
        return true;
    }

    s_shell = heap_caps_calloc(
        1,
        sizeof(*s_shell),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (s_shell) {
        ESP_LOGI(
            TAG,
            "Shell state allocated permanently in PSRAM: %u bytes",
            (unsigned)sizeof(*s_shell));
        return true;
    }

    s_shell = heap_caps_calloc(
        1,
        sizeof(*s_shell),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_shell) {
        ESP_LOGE(TAG, "Unable to allocate shell state");
        return false;
    }

    ESP_LOGW(TAG, "Shell state using internal RAM fallback");
    return true;
}

static void shell_printer_switch_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    if (s_printer_switch_callback) {
        s_printer_switch_callback();
    }
}


static void shell_clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!shell_clock_label) return;

    time_t now = 0;
    struct tm timeinfo = {0};

    time(&now);
    localtime_r(&now, &timeinfo);

    char buf[24];

    if (timeinfo.tm_year < (2024 - 1900)) {
        snprintf(buf, sizeof(buf), "--:--");
    } else {
        strftime(buf, sizeof(buf), "%I:%M %p", &timeinfo);
        if (buf[0] == '0') {
            memmove(buf, buf + 1, strlen(buf));
        }
    }

    lv_label_set_text(shell_clock_label, buf);
}

void ui_shell_refresh_clock(void)
{
    shell_clock_timer_cb(NULL);
}

void ui_shell_create(void)
{
    if (!shell_state_init()) {
        return;
    }

    if (shell_top_bar) {
        ui_shell_raise_topbar();
        return;
    }

    lv_obj_t *scr = lv_screen_active();

    shell_top_bar = lv_obj_create(scr);
    lv_obj_set_size(shell_top_bar, 1024, 72);
    lv_obj_set_pos(shell_top_bar, 0, 0);
    ui_apply_surface_role(shell_top_bar, UI_SURFACE_SHELL_TOPBAR);

    /* TOPBAR_FIXED_NON_SCROLLING_CONTENT_AREA
     * Absolute shell geometry must not inherit LVGL container padding or
     * auto-scroll a focused child into view.
     */
    lv_obj_clear_flag(shell_top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(shell_top_bar, 0, 0);

    s_shell_printer_button =
        ui_button_create_empty(
            shell_top_bar,
            UI_BUTTON_OUTLINED);

    if (s_shell_printer_button) {
        lv_obj_set_size(s_shell_printer_button, 460, 52);
        lv_obj_set_pos(s_shell_printer_button, 12, 10);
        lv_obj_clear_flag(
            s_shell_printer_button,
            LV_OBJ_FLAG_SCROLLABLE);

        s_shell_title_label =
            ui_button_create_label(
                s_shell_printer_button,
                "PRINTERHMI  |  SELECT PRINTER  " LV_SYMBOL_DOWN);

        lv_obj_add_event_cb(
            s_shell_printer_button,
            shell_printer_switch_event_cb,
            LV_EVENT_CLICKED,
            NULL);
    }

    ui_global_estop_create(shell_top_bar);

    shell_clock_label = lv_label_create(shell_top_bar);
    lv_label_set_text(shell_clock_label, "--:--");
    ui_apply_text_title(shell_clock_label);
    ui_apply_label_primary(shell_clock_label);
    lv_obj_align(shell_clock_label, LV_ALIGN_RIGHT_MID, -20, 0);

    for (int i = 0; i < 4; i++) {
        shell_topbar_wifi_bars[i] = lv_obj_create(shell_top_bar);
        lv_obj_clear_flag(shell_topbar_wifi_bars[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(shell_topbar_wifi_bars[i], 5, 6 + (i * 4));
        ui_apply_surface_role(shell_topbar_wifi_bars[i], UI_SURFACE_INDICATOR);
        lv_obj_set_style_bg_color(shell_topbar_wifi_bars[i], UI_WIFI_INACTIVE, 0);
        lv_obj_set_style_opa(shell_topbar_wifi_bars[i], LV_OPA_20, 0);

        lv_obj_align_to(shell_topbar_wifi_bars[i], shell_clock_label,
                        LV_ALIGN_OUT_LEFT_MID,
                        -90 + (i * 8),
                        8 - (i * 2));
    }

    shell_topbar_eta_label = NULL;

    if (!s_clock_timer) {
        s_clock_timer = lv_timer_create(
            shell_clock_timer_cb,
            1000,
            NULL);
    }

    shell_clock_timer_cb(NULL);
}

void ui_shell_destroy(void)
{
    if (shell_top_bar) {
        lv_obj_delete(shell_top_bar);
    }

    if (shell_nav_rail) {
        lv_obj_delete(shell_nav_rail);
    }

    shell_top_bar = NULL;
    s_shell_printer_button = NULL;
    s_shell_title_label = NULL;
    shell_clock_label = NULL;
    shell_topbar_eta_label = NULL;
    shell_nav_rail = NULL;

    for (int index = 0;
         index < UI_SHELL_PAGE_COUNT;
         ++index) {
        shell_nav_buttons[index] = NULL;
    }

    for (int index = 0; index < 4; ++index) {
        shell_topbar_wifi_bars[index] = NULL;
    }

}

void ui_shell_raise_topbar(void)
{
    if (shell_top_bar) {
        lv_obj_move_foreground(shell_top_bar);
    }
}

/* ui_shell_raise() is still implemented in main.c during Phase 1. */

void ui_shell_set_active_nav(int idx)
{
    for (int i = 0; i < UI_SHELL_PAGE_COUNT; i++) {
        if (!shell_nav_buttons[i]) {
            continue;
        }

        ui_operator_nav_button_set_selected(
            shell_nav_buttons[i],
            i == idx);
    }
}

void ui_shell_update_status_icons(void)
{
    if (!s_shell) return;

    int bars = 0;

    if (s_shell->wifi_signal_connected) {
        int rssi = s_shell->wifi_signal_rssi;

        if (rssi >= -55) bars = 4;
        else if (rssi >= -67) bars = 3;
        else if (rssi >= -75) bars = 2;
        else if (rssi >= -85) bars = 1;
    }

    for (int i = 0; i < 4; i++) {
        if (!shell_topbar_wifi_bars[i]) continue;

        lv_color_t color = UI_WIFI_INACTIVE;

        if (i < bars) {
            if (bars >= 3) color = UI_OK_BRIGHT;
            else if (bars == 2) color = UI_WARN;
            else color = UI_DANGER_BRIGHT;
        }

        lv_obj_set_style_bg_color(
            shell_topbar_wifi_bars[i],
            color,
            0);

        lv_obj_set_style_opa(
            shell_topbar_wifi_bars[i],
            i < bars ? LV_OPA_COVER : LV_OPA_20,
            0);
    }

    if (s_shell->wifi_signal_bars != bars) {
        ESP_LOGI(
            TAG,
            "WiFi signal rssi=%d bars=%d",
            s_shell->wifi_signal_connected
                ? s_shell->wifi_signal_rssi
                : -127,
            bars);

        s_shell->wifi_signal_bars = bars;
    }
}


void ui_shell_set_wifi_signal(bool connected, int rssi)
{
    if (!s_shell) return;

    s_shell->wifi_signal_connected = connected;
    s_shell->wifi_signal_rssi = rssi;
    ui_shell_update_status_icons();
}

static void shell_nav_btn_event_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);

    if (idx < UI_SHELL_PAGE_DASHBOARD || idx >= UI_SHELL_PAGE_COUNT) {
        return;
    }

    ui_shell_set_active_nav(idx);
    ui_shell_page_action((ui_shell_page_t)idx);
}

void ui_shell_create_nav(void)
{
    if (shell_nav_rail) {
        ui_shell_raise_nav();
        return;
    }

    lv_obj_t *scr = lv_screen_active();

    shell_nav_rail = lv_obj_create(scr);
    lv_obj_set_size(shell_nav_rail, 170, 528);
    lv_obj_set_pos(shell_nav_rail, 0, 72);
    ui_apply_surface_role(shell_nav_rail, UI_SURFACE_SHELL_NAV);
    lv_obj_clear_flag(shell_nav_rail, LV_OBJ_FLAG_SCROLLABLE);

    typedef struct {
        const char *icon;
        const char *text;
    } shell_nav_item_t;

    /*
     * Operator order follows the normal print-cell workflow:
     * primary work, printer setup, diagnostics, then auxiliary/system.
     */
    const shell_nav_item_t nav[] = {
        { LV_SYMBOL_HOME,     ui_i18n_shell_text(UI_I18N_SHELL_NAV_DASHBOARD) },
        { LV_SYMBOL_LIST,     ui_i18n_shell_text(UI_I18N_SHELL_NAV_PRINTER) },
        { LV_SYMBOL_FILE,     ui_i18n_shell_text(UI_I18N_SHELL_NAV_FILES) },
        { LV_SYMBOL_IMAGE,    ui_i18n_shell_text(UI_I18N_SHELL_NAV_BED_MESH) },
        { LV_SYMBOL_REFRESH,  ui_i18n_shell_text(UI_I18N_SHELL_NAV_CALIBRATION) },
        { LV_SYMBOL_CHARGE,   ui_i18n_shell_text(UI_I18N_SHELL_NAV_DEVICES) },
        { LV_SYMBOL_PLAY,     ui_i18n_shell_text(UI_I18N_SHELL_NAV_MACROS) },
        { LV_SYMBOL_EDIT,     ui_i18n_shell_text(UI_I18N_SHELL_NAV_CONSOLE) },
        { LV_SYMBOL_LOOP,     ui_i18n_shell_text(UI_I18N_SHELL_NAV_DRYBOX) },
        { LV_SYMBOL_SETTINGS, ui_i18n_shell_text(UI_I18N_SHELL_NAV_SETTINGS) }
    };

    for (int i = 0; i < UI_SHELL_PAGE_COUNT; i++) {
        lv_obj_t *button =
            ui_create_operator_nav_button(
                shell_nav_rail,
                0,
                0,
                150,
                44,
                nav[i].icon,
                nav[i].text);

        if (!button) {
            ESP_LOGE(
                TAG,
                "Failed to create navigation button %d",
                i);
            continue;
        }

        shell_nav_buttons[i] =
            button;
        ui_operator_nav_button_set_text_font(
            button, ui_i18n_text_font(UI_FONT_BODY));

        /*
         * Center against the rail's actual content area rather than
         * relying on a hard-coded X coordinate.
         */
        lv_obj_align(
            button,
            LV_ALIGN_TOP_MID,
            0,
            8 + i * 50);

        lv_obj_add_event_cb(
            button,
            shell_nav_btn_event_cb,
            LV_EVENT_CLICKED,
            (void *)(intptr_t)i);
    }

    ui_shell_set_active_nav(0);
}

void ui_shell_raise_nav(void)
{
    if (shell_nav_rail) {
        lv_obj_move_foreground(shell_nav_rail);
    }
}



void ui_shell_set_active_printer_name(const char *printer_name)
{
    if (!s_shell_title_label) return;

    char title[96];

    if (printer_name && printer_name[0]) {
        lv_snprintf(
            title,
            sizeof(title),
            "PRINTERHMI  |  %s  %s",
            printer_name,
            LV_SYMBOL_DOWN);
    } else {
        lv_snprintf(
            title,
            sizeof(title),
            "PRINTERHMI  |  SELECT PRINTER  %s",
            LV_SYMBOL_DOWN);
    }

    lv_label_set_text(s_shell_title_label, title);
    ui_global_estop_set_printer_name(printer_name);
}



void ui_shell_set_printer_switch_callback(
    ui_shell_printer_switch_cb_t callback)
{
    s_printer_switch_callback = callback;
}
