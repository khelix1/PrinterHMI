#include "ui_dashboard.h"
#include "ui_text.h"
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
#include "ui_camera.h"
#include "camera_stream_controller.h"
#include "camera_catalog_controller.h"
#include "moonraker_config_controller.h"
#include "esp_heap_caps.h"

static lv_obj_t *dash32_root = NULL;

/*
 * TEST12_NEW_DASHBOARD_OWNER
 *
 * The public ui_dashboard_* API remains the application-facing
 * Dashboard interface. Creation and destruction now delegate to the
 * new Theme B Dashboard page.
 */
static ui_dashboard_page_t dash32_page = {0};

/*
 * TEST9_DASHBOARD_STATUS_OWNER
 *
 * Dashboard Status construction is now routed through the Dashboard
 * module. The returned label handles remain available to main.c
 * during the legacy transition.
 */
static ui_dashboard_status_t dash32_status = {0};

lv_obj_t **ui_dashboard_thumb_canvas_ref(void)
{
    return ui_active_print_thumb_canvas_ref();
}

uint16_t **ui_dashboard_thumb_canvas_buf_ref(void)
{
    return ui_active_print_thumb_canvas_buf_ref();
}

char *ui_dashboard_thumb_canvas_file(void)
{
    return ui_active_print_thumb_canvas_file();
}

size_t ui_dashboard_thumb_canvas_file_size(void)
{
    return ui_active_print_thumb_canvas_file_size();
}

static lv_obj_t *dash32_banner = NULL;
static lv_obj_t *dash32_machine = NULL;
static lv_obj_t *dash32_active_print = NULL;
static lv_obj_t *dash32_thumb_box = NULL;
static lv_obj_t *dash32_thumb_label = NULL;
static lv_obj_t *dash32_camera_image = NULL;
static lv_obj_t *dash32_camera_toggle = NULL;
static lv_obj_t *dash32_camera_status = NULL;
static lv_timer_t *dash32_camera_timer = NULL;
static uint8_t *dash32_camera_frame = NULL;
static lv_image_dsc_t dash32_camera_dsc;
static bool dash32_camera_mode = false;

ui_dashboard_status_t ui_dashboard_create_status(
    lv_obj_t *parent)
{
    /*
     * The replacement page normally creates Print Status as part of
     * ui_dashboard_create(). Preserve a guarded fallback for the
     * legacy startup ordering.
     */
    if (dash32_page.print_status.root) {
        dash32_status = dash32_page.print_status;
        return dash32_status;
    }

    ui_dashboard_status_t empty = {0};

    if (!parent) {
        return empty;
    }

    dash32_status =
        ui_dashboard_status_create(parent);

    dash32_page.print_status = dash32_status;
    dash32_page.print_status_host = dash32_status.root;

    return dash32_status;
}


static const char *dashboard_selected_camera_url(void)
{
    static camera_catalog_entry_t camera;
    const int profile_index = moonraker_config_active_profile_index();
    const size_t camera_index = camera_catalog_default(profile_index);

    if (!camera_catalog_get(profile_index, camera_index, &camera) ||
        !camera.configured ||
        !camera.stream_url[0]) {
        return NULL;
    }

    return camera.stream_url;
}

static void dashboard_camera_release_frame(void)
{
    if (dash32_camera_frame) {
        heap_caps_free(dash32_camera_frame);
        dash32_camera_frame = NULL;
    }
    memset(&dash32_camera_dsc, 0, sizeof(dash32_camera_dsc));
}

static void dashboard_camera_set_status(const char *text)
{
    if (dash32_camera_status) {
        lv_label_set_text(dash32_camera_status, text ? text : ui_text(""));
    }
}

static void dashboard_camera_poll_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!dash32_camera_mode) return;

    uint8_t *pixels = NULL;
    size_t pixel_size = 0;
    int width = 0;
    int height = 0;
    bool ok = false;
    if (camera_stream_take_result(&pixels, &pixel_size, &width, &height, &ok)) {
        if (!ok || !pixels || width <= 0 || height <= 0) {
            if (pixels) heap_caps_free(pixels);
            dashboard_camera_set_status("Camera unavailable");
        } else if (dash32_camera_image) {
            dashboard_camera_release_frame();
            dash32_camera_frame = pixels;
            memset(&dash32_camera_dsc, 0, sizeof(dash32_camera_dsc));
#if defined(LV_IMAGE_HEADER_MAGIC)
            dash32_camera_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
            dash32_camera_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
            dash32_camera_dsc.header.w = width;
            dash32_camera_dsc.header.h = height;
            dash32_camera_dsc.header.stride = width * sizeof(uint16_t);
            dash32_camera_dsc.data = dash32_camera_frame;
            dash32_camera_dsc.data_size = pixel_size;
            lv_image_set_src(dash32_camera_image, &dash32_camera_dsc);
            lv_obj_t *box = lv_obj_get_parent(dash32_camera_image);
            int scale_x = ((lv_obj_get_width(box) - 12) * 256) / width;
            int scale_y = ((lv_obj_get_height(box) - 12) * 256) / height;
            int scale = scale_x < scale_y ? scale_x : scale_y;
            if (scale < 1) scale = 1;
            lv_image_set_scale(dash32_camera_image, scale);
            lv_obj_center(dash32_camera_image);
            lv_obj_move_foreground(dash32_camera_image);
            dashboard_camera_set_status("LIVE CAMERA");
        } else if (pixels) {
            heap_caps_free(pixels);
        }
    }

    if (camera_stream_busy()) return;
    const char *url = dashboard_selected_camera_url();
    if (!url || !url[0]) {
        dashboard_camera_set_status("No camera configured");
        return;
    }
    if (!camera_stream_start(url)) {
        dashboard_camera_set_status("Camera connecting...");
    }
}

static void dashboard_camera_mode_set(bool enabled)
{
    dash32_camera_mode = enabled;
    if (dash32_camera_image) {
        if (enabled) lv_obj_clear_flag(dash32_camera_image, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(dash32_camera_image, LV_OBJ_FLAG_HIDDEN);
    }
    if (dash32_camera_status) {
        if (enabled) lv_obj_clear_flag(dash32_camera_status, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(dash32_camera_status, LV_OBJ_FLAG_HIDDEN);
    }
    if (dash32_camera_toggle) {
        lv_obj_t *label = lv_obj_get_child(dash32_camera_toggle, 0);
        if (label) lv_label_set_text(label,
            enabled ? ui_text(LV_SYMBOL_IMAGE " THUMBNAIL") : ui_text(LV_SYMBOL_IMAGE " CAMERA"));
    }
    if (!enabled) {
        if (dash32_camera_timer) {
            lv_timer_delete(dash32_camera_timer);
            dash32_camera_timer = NULL;
        }
        camera_stream_stop();
        dashboard_camera_release_frame();
        return;
    }
    if (!dash32_camera_timer) {
        dash32_camera_timer = lv_timer_create(dashboard_camera_poll_cb, 100, NULL);
    }
    dashboard_camera_set_status("Connecting camera...");
    dashboard_camera_poll_cb(NULL);
}

void ui_dashboard_refresh_camera(void)
{
    if (!dash32_camera_toggle) {
        return;
    }

    const int profile_index = moonraker_config_active_profile_index();
    const size_t camera_index = camera_catalog_default(profile_index);
    camera_catalog_entry_t camera = {0};
    const bool configured =
        camera_catalog_get(profile_index, camera_index, &camera) &&
        camera.configured &&
        camera.stream_url[0];

    if (configured) {
        lv_obj_clear_flag(dash32_camera_toggle, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (dash32_camera_mode) {
        dashboard_camera_mode_set(false);
    }
    lv_obj_add_flag(dash32_camera_toggle, LV_OBJ_FLAG_HIDDEN);
}

static void dashboard_camera_toggle_cb(lv_event_t *event)
{
    (void)event;
    dashboard_camera_mode_set(!dash32_camera_mode);
}

static void dashboard_camera_open_fullscreen_cb(lv_event_t *event)
{
    (void)event;
    if (!dash32_camera_mode) {
        return;
    }

    /* Release the Dashboard consumer before Camera takes over the stream. */
    dashboard_camera_mode_set(false);
    ui_camera_show_fullscreen();
}


void ui_dashboard_create(void)
{
    if (dash32_page.root) {
        lv_obj_move_foreground(dash32_page.root);
        return;
    }

    dash32_page =
        ui_dashboard_page_create(
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
        ui_dashboard_page_destroy(&dash32_page);

        dash32_root = NULL;
        dash32_banner = NULL;
        dash32_active_print = NULL;
        dash32_machine = NULL;
        dash32_thumb_box = NULL;

        return;
    }

    ui_dashboard_set_active_print(
        "--/--",
        "--:--",
        "REM --:--");

    dash32_thumb_box =
        ui_active_print_thumb_box(
            dash32_active_print);

    ui_dashboard_thumb_set_placeholder(
        "PRINT\nTHUMBNAIL");

    lv_obj_t *preview_box = ui_dashboard_thumb_box();
    if (preview_box) {
        dash32_camera_image = lv_image_create(preview_box);
        lv_obj_add_flag(dash32_camera_image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(dash32_camera_image,
                            dashboard_camera_open_fullscreen_cb,
                            LV_EVENT_CLICKED,
                            NULL);
        lv_obj_set_style_bg_color(dash32_camera_image, UI_CARD_DARK, 0);
        lv_obj_set_style_bg_opa(dash32_camera_image, LV_OPA_COVER, 0);
        lv_obj_add_flag(dash32_camera_image, LV_OBJ_FLAG_HIDDEN);
        dash32_camera_status = lv_label_create(preview_box);
        ui_apply_text_caption(dash32_camera_status);
        ui_apply_label_bright(dash32_camera_status);
        lv_obj_align(dash32_camera_status, LV_ALIGN_BOTTOM_LEFT, 10, -8);
        lv_obj_add_flag(dash32_camera_status, LV_OBJ_FLAG_HIDDEN);
    }
    dash32_camera_toggle = ui_button_create(
        dash32_active_print, UI_BUTTON_OUTLINED, LV_SYMBOL_IMAGE " CAMERA");
    if (dash32_camera_toggle) {
        lv_obj_set_size(dash32_camera_toggle, 172, 32);
        lv_obj_set_pos(dash32_camera_toggle, 308, 8);
        const char *camera_url = dashboard_selected_camera_url();
        if (!camera_url || !camera_url[0]) {
            lv_obj_add_flag(dash32_camera_toggle, LV_OBJ_FLAG_HIDDEN);
        } else {
            ui_button_expand_touch_target(dash32_camera_toggle);
            lv_obj_add_event_cb(dash32_camera_toggle, dashboard_camera_toggle_cb,
                                LV_EVENT_CLICKED, NULL);
        }
    }

    ui_machine_status_set(
        dash32_machine,
        "-- / -- C",
        "-- / -- C",
        "-- C",
        "-- %RH",
        "100%",
        "100%",
        "--%");

    ui_dashboard_update();
}

void ui_dashboard_set_active_print_file(const char *filename)
{
    /*
     * The active filename now belongs exclusively to the Dashboard
     * status banner. Preserve this compatibility API as a no-op so
     * older call sites do not require immediate removal.
     */
    (void)filename;
}

void ui_dashboard_set_active_print(const char *layer,
                                       const char *elapsed,
                                       const char *remaining)
{
    if (!dash32_active_print) return;
    ui_active_print_set(dash32_active_print, layer, elapsed, remaining);
}

void ui_dashboard_update(void)
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

    ui_status_banner_set(
        dash32_banner,
        LV_SYMBOL_OK " READY",
        console_name,
        "ETA --:--",
        "--%"
    );
}

void ui_dashboard_destroy(void)
{
    dashboard_camera_mode_set(false);
    dash32_camera_image = NULL;
    dash32_camera_toggle = NULL;
    dash32_camera_status = NULL;

    ui_dashboard_page_destroy(
        &dash32_page);

    dash32_root = NULL;
    dash32_banner = NULL;
    dash32_machine = NULL;
    dash32_active_print = NULL;
    dash32_thumb_box = NULL;
    dash32_thumb_label = NULL;
    dash32_status = (ui_dashboard_status_t){0};
}

void ui_dashboard_set_filament(
    bool moonraker_online,
    const moonraker_filament_state_t *state)
{
    if (!dash32_machine) return;

    ui_machine_status_set_filament(
        dash32_machine,
        moonraker_online,
        state);
}


void ui_dashboard_set_machine_connection(
    bool online)
{
    if (!dash32_machine) return;

    ui_machine_status_set_connection(
        dash32_machine,
        online);
}


void ui_dashboard_set_active_hotend(
    const char *name,
    const char *value)
{
    if (!dash32_machine) return;

    ui_machine_status_set_active_hotend(
        dash32_machine,
        name,
        value);
}


void ui_dashboard_set_machine(
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

    ui_machine_status_set(
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


void ui_dashboard_set_banner(
    const char *state,
    const char *file,
    const char *eta,
    const char *progress
)
{
    if (!dash32_banner) return;
    ui_status_banner_set(dash32_banner, state, file, eta, progress);
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

void ui_dashboard_status_popup_close(void)
{
    dashboard_status_close_cb(NULL);
}

void ui_dashboard_status_popup_show(const char *title_text, const char *body)
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
        title_text ? title_text : ui_text("STATUS"),
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


lv_obj_t *ui_dashboard_thumb_box(void)
{
    if (dash32_active_print) {
        dash32_thumb_box = ui_active_print_thumb_box(dash32_active_print);
    }
    return dash32_thumb_box;
}

bool ui_dashboard_thumb_ready(void)
{
    return ui_dashboard_thumb_box() != NULL;
}

void ui_dashboard_thumb_set_placeholder(const char *text)
{
    ui_active_print_thumb_set_placeholder(dash32_active_print, text);
}


bool ui_dashboard_thumb_canvas_matches(const char *file)
{
    return ui_active_print_thumb_canvas_matches(file);
}

bool ui_dashboard_thumb_ensure_canvas_buffer(size_t pixels)
{
    return ui_active_print_thumb_ensure_canvas_buffer(pixels);
}

void ui_dashboard_thumb_delete_canvas(void)
{
    ui_active_print_thumb_delete_canvas();
}

void ui_dashboard_thumb_forget_canvas_file(void)
{
    ui_active_print_thumb_forget_canvas_file();
}


void ui_dashboard_thumb_show_canvas_from_buffer(int w, int h, const char *file)
{
    ui_active_print_thumb_show_canvas_from_buffer(dash32_active_print, w, h, file);
}

void ui_dashboard_thumb_apply_canvas_from_buffer(int w, int h, const char *file)
{
    ui_active_print_thumb_apply_canvas_from_buffer(dash32_active_print, w, h, file);
}


void ui_dashboard_thumb_clear_placeholder(void)
{
    ui_active_print_thumb_clear_placeholder(dash32_active_print);
}
