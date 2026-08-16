#include "ui_ota_popup.h"

#include "ota_release_catalog.h"
#include "ui_theme.h"
#include "ui_camera.h"

#include "esp_heap_caps.h"
#include "ui_popup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static lv_obj_t *s_ota_popup = NULL;

static lv_obj_t *s_progress_popup = NULL;
static lv_obj_t *s_progress_bar = NULL;
static lv_obj_t *s_progress_label = NULL;
static lv_obj_t *s_progress_pct_label = NULL;
static lv_obj_t *s_progress_bytes_label = NULL;
static lv_obj_t *s_progress_cancel_btn = NULL;
static ui_ota_cancel_cb_t s_progress_cancel_cb = NULL;
static bool s_progress_visible = false;
static bool s_camera_quiesced = false;

static void ota_quiesce_camera(void)
{
    if (s_camera_quiesced) {
        return;
    }

    /* The persistent MJPEG request shares the network transport with OTA. */
    ui_camera_set_setup_active(true);
    s_camera_quiesced = true;
}

static void ota_resume_camera(void)
{
    if (!s_camera_quiesced) {
        return;
    }

    s_camera_quiesced = false;
    ui_camera_set_setup_active(false);
}
static lv_obj_t *s_ota_url_ta = NULL;
static lv_obj_t *s_ota_kb = NULL;
static ui_ota_start_cb_t s_start_cb = NULL;
static ui_ota_remote_cb_t s_remote_cb = NULL;

/*
 * OTA startup is deferred until LVGL has processed the asynchronous popup
 * deletion and completed a redraw cycle.
 */
static char s_deferred_start_url[256] = "";
static ui_ota_start_cb_t s_deferred_start_cb = NULL;
static lv_obj_t *s_deferred_start_popup = NULL;


static void progress_cancel_cb(lv_event_t *e)
{
    (void)e;

    if (s_progress_cancel_btn) {
        lv_obj_add_state(
            s_progress_cancel_btn,
            LV_STATE_DISABLED);
    }

    if (s_progress_label) {
        lv_label_set_text(
            s_progress_label,
            "Cancelling OTA...");
    }

    if (s_progress_cancel_cb) {
        s_progress_cancel_cb();
    }
}

void ui_ota_progress_show(ui_ota_cancel_cb_t cancel_cb)
{
    s_progress_cancel_cb = cancel_cb;

    if (s_progress_popup) {
        lv_obj_move_foreground(s_progress_popup);
        return;
    }

    s_progress_popup =
        ui_popup_create(
            lv_screen_active(),
            760,
            400,
            UI_POPUP_STANDARD);

    if (!s_progress_popup) {
        return;
    }

    ui_popup_add_title(
        s_progress_popup,
        "OTA UPDATE",
        false,
        0);

    ui_popup_add_header_divider(
        s_progress_popup,
        44);

    s_progress_label =
        ui_popup_add_progress_status(
            s_progress_popup,
            "Checking Server...",
            30,
            58,
            700);

    s_progress_pct_label =
        ui_popup_add_progress_value(
            s_progress_popup,
            "0%",
            30,
            98,
            700);

    s_progress_bar =
        ui_popup_add_progress_bar(
            s_progress_popup,
            40,
            206,
            680,
            40,
            0,
            100,
            0);

    if (s_progress_bar) {
        /* HTTP chunks can advance the integer target several points at once.
         * Let LVGL interpolate each exact target without changing the
         * reported percentage or downloaded byte counts.
         */
        lv_obj_set_style_anim_duration(
            s_progress_bar,
            250,
            0);
    }

    s_progress_bytes_label =
        ui_popup_add_progress_detail(
            s_progress_popup,
            "0.00 MB / 0.00 MB",
            40,
            264,
            680);

    ui_popup_add_standard_footer_divider(
        s_progress_popup);

    s_progress_cancel_btn =
        ui_popup_add_footer_action(
            s_progress_popup,
            UI_POPUP_ACTION_CANCEL,
            "CANCEL",
            220,
            UI_POPUP_FOOTER_CENTER,
            progress_cancel_cb,
            NULL,
            NULL);

    s_progress_visible = true;
}

void ui_ota_progress_pump(const char *status_text,
                          int percent,
                          int bytes_read,
                          int content_length,
                          bool cancel_enabled)
{
    if (!s_progress_visible) return;

    if (s_progress_cancel_btn) {
        if (cancel_enabled) {
            lv_obj_remove_state(
                s_progress_cancel_btn,
                LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(
                s_progress_cancel_btn,
                LV_STATE_DISABLED);
        }
    }

    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    if (s_progress_label) {
        char status[96];
        snprintf(status, sizeof(status), "%s",
                 (status_text && status_text[0])
                     ? status_text
                     : "Checking Server...");

        char *nl = strchr(status, '\n');
        if (nl) *nl = 0;

        lv_label_set_text(
            s_progress_label,
            strstr(status, "Downloading firmware")
                ? "Downloading Firmware"
                : status);
    }

    if (s_progress_bar) {
        lv_bar_set_value(
            s_progress_bar,
            percent,
            LV_ANIM_ON);
    }

    if (s_progress_pct_label) {
        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", percent);
        lv_label_set_text(s_progress_pct_label, pct);
    }

    if (s_progress_bytes_label) {
        char mbuf[64];

        if (content_length > 0) {
            snprintf(mbuf, sizeof(mbuf),
                     "%.2f MB / %.2f MB",
                     bytes_read / 1048576.0,
                     content_length / 1048576.0);
        } else {
            snprintf(mbuf, sizeof(mbuf),
                     "%.2f MB",
                     bytes_read / 1048576.0);
        }

        lv_label_set_text(s_progress_bytes_label, mbuf);
    }
}


void ui_ota_progress_close(void)
{
    if (s_progress_popup) {
        lv_obj_delete(s_progress_popup);
    }

    s_progress_popup = NULL;
    s_progress_bar = NULL;
    s_progress_label = NULL;
    s_progress_pct_label = NULL;
    s_progress_bytes_label = NULL;
    s_progress_cancel_btn = NULL;
    s_progress_cancel_cb = NULL;
    s_progress_visible = false;
    ota_resume_camera();
}

void ui_ota_popup_close(void)
{
    if (s_ota_popup) {
        lv_obj_delete(s_ota_popup);
        s_ota_popup = NULL;
        s_ota_url_ta = NULL;
        s_ota_kb = NULL;
        s_start_cb = NULL;
        s_remote_cb = NULL;
    }
    ota_resume_camera();
}

static void close_cb(lv_event_t *e)
{
    (void)e;
    ui_ota_popup_close();
}

static void deferred_start_cb(void *user_data)
{
    (void)user_data;

    /*
     * Clear the deferred ownership before invoking the bridge so a future OTA
     * popup can safely schedule another request.
     */
    ui_ota_start_cb_t start_fn = s_deferred_start_cb;
    char url[sizeof(s_deferred_start_url)];

    snprintf(url, sizeof(url), "%s", s_deferred_start_url);

    s_deferred_start_cb = NULL;
    s_deferred_start_url[0] = '\0';

    /*
     * Delete the editor and create the progress popup inside this same LVGL
     * callback. LVGL cannot refresh between those operations, so the page
     * beneath the popups is never exposed as an intermediate frame.
     */
    lv_obj_t *popup = s_deferred_start_popup;
    s_deferred_start_popup = NULL;

    if (popup) {
        lv_obj_delete(popup);
    }

    if (start_fn && url[0]) {
        start_fn(url);
    }
}

static void start_cb(lv_event_t *e)
{
    (void)e;

    /*
     * Preserve everything needed by the deferred callback before releasing
     * popup ownership.
     */
    s_deferred_start_url[0] = '\0';
    s_deferred_start_cb = s_start_cb;

    if (s_ota_url_ta) {
        const char *txt = lv_textarea_get_text(s_ota_url_ta);
        if (txt) {
            snprintf(s_deferred_start_url,
                     sizeof(s_deferred_start_url),
                     "%s",
                     txt);
        }
    }

    /*
     * Transfer popup ownership to the deferred transition. The editor is
     * deleted immediately before the progress popup is created, with no
     * refresh opportunity between them.
     */
    s_deferred_start_popup = s_ota_popup;

    s_ota_popup = NULL;
    s_ota_url_ta = NULL;
    s_ota_kb = NULL;
    s_start_cb = NULL;
    s_remote_cb = NULL;

    if (s_deferred_start_cb && s_deferred_start_url[0]) {
        lv_async_call(deferred_start_cb, NULL);
    } else {
        lv_obj_t *popup = s_deferred_start_popup;

        s_deferred_start_popup = NULL;
        s_deferred_start_cb = NULL;
        s_deferred_start_url[0] = '\0';

        if (popup) {
            lv_obj_delete_async(popup);
        }
    }
}

static void remote_cb(lv_event_t *e)
{
    (void)e;
    if (s_remote_cb) {
        s_remote_cb();
    }
}

void ui_ota_popup_show(const char *current_url,
                       const char *firmware_info,
                       size_t max_url_len,
                       ui_ota_start_cb_t start_cb_fn,
                       ui_ota_remote_cb_t remote_cb_fn)
{
    if (s_ota_popup) {
        lv_obj_move_foreground(s_ota_popup);
        return;
    }

    s_start_cb = start_cb_fn;
    s_remote_cb = remote_cb_fn;

    s_ota_popup =
        ui_popup_create(
            lv_screen_active(),
            960,
            540,
            UI_POPUP_STANDARD);

    if (!s_ota_popup) {
        s_start_cb = NULL;
        s_remote_cb = NULL;
        return;
    }

    ota_quiesce_camera();

    ui_popup_add_title(
        s_ota_popup,
        "OTA UPDATE SERVER",
        false,
        0);

    ui_popup_add_header_divider(
        s_ota_popup,
        42);

    lv_obj_t *info =
        ui_popup_add_progress_detail(
            s_ota_popup,
            firmware_info ? firmware_info : "",
            30,
            48,
            900);

    if (info) {
        lv_label_set_long_mode(
            info,
            LV_LABEL_LONG_DOT);
    }

    s_ota_url_ta =
        ui_popup_add_textarea(
            s_ota_popup,
            900,
            64,
            LV_ALIGN_TOP_MID,
            0,
            62,
            true,
            false,
            max_url_len,
            "https://server/firmware.bin",
            current_url ? current_url : "",
            NULL);

    if (!s_ota_url_ta) {
        ui_ota_popup_close();
        return;
    }

    s_ota_kb =
        ui_popup_add_keyboard(
            s_ota_popup,
            s_ota_url_ta,
            900,
            320,
            LV_ALIGN_TOP_MID,
            0,
            120,
            LV_KEYBOARD_MODE_TEXT_LOWER);

    if (!s_ota_kb) {
        ui_ota_popup_close();
        return;
    }

    ui_popup_add_standard_footer_divider(s_ota_popup);

    ui_popup_add_footer_action(
        s_ota_popup,
        UI_POPUP_ACTION_CANCEL,
        "CANCEL",
        220,
        UI_POPUP_FOOTER_LEFT,
        close_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_ota_popup,
        UI_POPUP_ACTION_SECONDARY,
        "REMOTE BUILDS",
        220,
        UI_POPUP_FOOTER_CENTER,
        remote_cb,
        NULL,
        NULL);

    ui_popup_add_footer_action(
        s_ota_popup,
        UI_POPUP_ACTION_CONFIRM,
        "START OTA",
        220,
        UI_POPUP_FOOTER_RIGHT,
        start_cb,
        NULL,
        NULL);
}
