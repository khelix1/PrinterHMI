#include "ui_dashboard.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_status_banner.h"
#include "ui_active_print.h"
#include "ui_machine_status.h"
#include "ui_command_bar.h"
#include "ui_dashboard_status.h"
#include "ui_dashboard_page.h"
#include "ui_button.h"
#include "ui_popup.h"

static lv_obj_t *dash32_root = NULL;

/*
 * TEST12_NEW_DASHBOARD_OWNER
 *
 * The public ui_dashboard_v32_* API remains the application-facing
 * Dashboard interface. Creation and destruction now delegate to the
 * new Theme B Dashboard page.
 */
static ui_dashboard_page_v32_t dash32_page = {0};

/*
 * TEST9_DASHBOARD_STATUS_OWNER
 *
 * Dashboard Status construction is now routed through the Dashboard
 * module. The returned label handles remain available to main.c
 * during the legacy transition.
 */
static ui_dashboard_status_v32_t dash32_status = {0};

lv_obj_t **ui_dashboard_v32_thumb_canvas_ref(void)
{
    return ui_active_print_v32_thumb_canvas_ref();
}

uint16_t **ui_dashboard_v32_thumb_canvas_buf_ref(void)
{
    return ui_active_print_v32_thumb_canvas_buf_ref();
}

char *ui_dashboard_v32_thumb_canvas_file(void)
{
    return ui_active_print_v32_thumb_canvas_file();
}

size_t ui_dashboard_v32_thumb_canvas_file_size(void)
{
    return ui_active_print_v32_thumb_canvas_file_size();
}

static lv_obj_t *dash32_banner = NULL;
static lv_obj_t *dash32_machine = NULL;
static lv_obj_t *dash32_active_print = NULL;
static lv_obj_t *dash32_thumb_box = NULL;
static lv_obj_t *dash32_thumb_label = NULL;

ui_dashboard_status_v32_t ui_dashboard_v32_create_status(
    lv_obj_t *parent)
{
    /*
     * The replacement page normally creates Print Status as part of
     * ui_dashboard_v32_create(). Preserve a guarded fallback for the
     * legacy startup ordering.
     */
    if (dash32_page.print_status.root) {
        dash32_status = dash32_page.print_status;
        return dash32_status;
    }

    ui_dashboard_status_v32_t empty = {0};

    if (!parent) {
        return empty;
    }

    dash32_status =
        ui_dashboard_status_v32_create(parent);

    dash32_page.print_status = dash32_status;
    dash32_page.print_status_host = dash32_status.root;

    return dash32_status;
}


void ui_dashboard_v32_create(void)
{
    if (dash32_page.root) {
        lv_obj_move_foreground(dash32_page.root);
        return;
    }

    dash32_page =
        ui_dashboard_page_v32_create(
            lv_screen_active());

    dash32_root = dash32_page.root;
    dash32_banner = dash32_page.banner_host;
    dash32_active_print = dash32_page.active_print_host;
    dash32_machine = dash32_page.machine_status_host;
    dash32_status = dash32_page.print_status;

    if (!dash32_root ||
        !dash32_banner ||
        !dash32_active_print ||
        !dash32_machine) {
        ui_dashboard_page_v32_destroy(&dash32_page);

        dash32_root = NULL;
        dash32_banner = NULL;
        dash32_active_print = NULL;
        dash32_machine = NULL;
        dash32_thumb_box = NULL;

        return;
    }

    ui_dashboard_v32_set_active_print(
        "--/--",
        "--:--",
        "REM --:--");

    dash32_thumb_box =
        ui_active_print_v32_thumb_box(
            dash32_active_print);

    ui_dashboard_v32_thumb_set_placeholder(
        "PRINT\nTHUMBNAIL");

    ui_machine_status_v32_set(
        dash32_machine,
        "-- / -- C",
        "-- / -- C",
        "-- C",
        "-- %RH",
        "100%",
        "100%",
        "--%");

    ui_dashboard_v32_update();
}

void ui_dashboard_v32_set_active_print_file(const char *filename)
{
    /*
     * The active filename now belongs exclusively to the Dashboard
     * status banner. Preserve this compatibility API as a no-op so
     * older call sites do not require immediate removal.
     */
    (void)filename;
}

void ui_dashboard_v32_set_active_print(const char *layer,
                                       const char *elapsed,
                                       const char *remaining)
{
    if (!dash32_active_print) return;
    ui_active_print_v32_set(dash32_active_print, layer, elapsed, remaining);
}

void ui_dashboard_v32_update(void)
{
    if (!dash32_banner) return;

    const esp_app_desc_t *app =
        esp_app_get_description();
    const char *version =
        app && app->version[0]
            ? app->version
            : "--";

    char console_name[64];
    snprintf(
        console_name,
        sizeof(console_name),
        "PrinterHMI %s%s Operator Console",
        version[0] == 'v' ? "" : "v",
        version);

    ui_status_banner_v32_set(
        dash32_banner,
        LV_SYMBOL_OK " READY",
        console_name,
        "ETA --:--",
        "--%"
    );
}

void ui_dashboard_v32_destroy(void)
{
    ui_dashboard_page_v32_destroy(
        &dash32_page);

    dash32_root = NULL;
    dash32_banner = NULL;
    dash32_machine = NULL;
    dash32_active_print = NULL;
    dash32_thumb_box = NULL;
    dash32_thumb_label = NULL;
    dash32_status = (ui_dashboard_status_v32_t){0};
}

void ui_dashboard_v32_set_filament(
    bool moonraker_online,
    const moonraker_filament_state_t *state)
{
    if (!dash32_machine) return;

    ui_machine_status_v32_set_filament(
        dash32_machine,
        moonraker_online,
        state);
}


void ui_dashboard_v32_set_machine_connection(
    bool online)
{
    if (!dash32_machine) return;

    ui_machine_status_v32_set_connection(
        dash32_machine,
        online);
}


void ui_dashboard_v32_set_active_hotend(
    const char *name,
    const char *value)
{
    if (!dash32_machine) return;

    ui_machine_status_v32_set_active_hotend(
        dash32_machine,
        name,
        value);
}


void ui_dashboard_v32_set_machine(
    const char *nozzle,
    const char *bed,
    const char *chamber,
    const char *humidity,
    const char *speed,
    const char *flow,
    const char *fan
)
{
    if (!dash32_machine) return;

    ui_machine_status_v32_set(
        dash32_machine,
        nozzle,
        bed,
        chamber,
        humidity,
        speed,
        flow,
        fan
    );
}


void ui_dashboard_v32_set_banner(
    const char *state,
    const char *file,
    const char *eta,
    const char *progress
)
{
    if (!dash32_banner) return;
    ui_status_banner_v32_set(dash32_banner, state, file, eta, progress);
}





static lv_obj_t *s_dashboard_status_popup = NULL;

static void dashboard_status_close_cb(lv_event_t *e)
{
    (void)e;

    if (s_dashboard_status_popup) {
        lv_obj_delete(s_dashboard_status_popup);
        s_dashboard_status_popup = NULL;
    }
}

void ui_dashboard_v32_status_popup_close(void)
{
    dashboard_status_close_cb(NULL);
}

void ui_dashboard_v32_status_popup_show(const char *title_text, const char *body)
{
    if (s_dashboard_status_popup) {
        lv_obj_delete(s_dashboard_status_popup);
        s_dashboard_status_popup = NULL;
    }

    s_dashboard_status_popup =
        ui_popup_create(
            lv_screen_active(),
            620,
            390,
            UI_POPUP_STANDARD);

    if (!s_dashboard_status_popup) {
        return;
    }

    ui_popup_add_title(
        s_dashboard_status_popup,
        title_text ? title_text : "STATUS",
        false,
        0);

    ui_popup_add_header_divider(
        s_dashboard_status_popup,
        44);

    ui_popup_add_body(
        s_dashboard_status_popup,
        body ? body : "--",
        25,
        60,
        560);

    ui_popup_add_standard_footer_divider(
        s_dashboard_status_popup);

    ui_popup_add_footer_action(
        s_dashboard_status_popup,
        UI_POPUP_ACTION_CLOSE,
        LV_SYMBOL_CLOSE " CLOSE",
        160,
        UI_POPUP_FOOTER_CENTER,
        dashboard_status_close_cb,
        NULL,
        NULL);

    lv_obj_move_foreground(s_dashboard_status_popup);
}


lv_obj_t *ui_dashboard_v32_thumb_box(void)
{
    if (dash32_active_print) {
        dash32_thumb_box = ui_active_print_v32_thumb_box(dash32_active_print);
    }
    return dash32_thumb_box;
}

bool ui_dashboard_v32_thumb_ready(void)
{
    return ui_dashboard_v32_thumb_box() != NULL;
}

void ui_dashboard_v32_thumb_set_placeholder(const char *text)
{
    ui_active_print_v32_thumb_set_placeholder(dash32_active_print, text);
}


bool ui_dashboard_v32_thumb_canvas_matches(const char *file)
{
    return ui_active_print_v32_thumb_canvas_matches(file);
}

bool ui_dashboard_v32_thumb_ensure_canvas_buffer(size_t pixels)
{
    return ui_active_print_v32_thumb_ensure_canvas_buffer(pixels);
}

void ui_dashboard_v32_thumb_delete_canvas(void)
{
    ui_active_print_v32_thumb_delete_canvas();
}

void ui_dashboard_v32_thumb_forget_canvas_file(void)
{
    ui_active_print_v32_thumb_forget_canvas_file();
}


void ui_dashboard_v32_thumb_show_canvas_from_buffer(int w, int h, const char *file)
{
    ui_active_print_v32_thumb_show_canvas_from_buffer(dash32_active_print, w, h, file);
}

void ui_dashboard_v32_thumb_apply_canvas_from_buffer(int w, int h, const char *file)
{
    ui_active_print_v32_thumb_apply_canvas_from_buffer(dash32_active_print, w, h, file);
}


void ui_dashboard_v32_thumb_clear_placeholder(void)
{
    ui_active_print_v32_thumb_clear_placeholder(dash32_active_print);
}
