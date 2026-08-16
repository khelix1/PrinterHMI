#include "ui_camera.h"

#include "camera_stream_controller.h"
#include "camera_catalog_controller.h"
#include "moonraker_config_controller.h"
#include "ui_page_geometry.h"
#include "ui_button.h"
#include "ui_printer_profiles.h"
#include "ui_page_title.h"
#include "ui_shell.h"
#include "ui_theme.h"

#include "esp_heap_caps.h"
#include "draw/lv_image_decoder_private.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_image = NULL;
static lv_obj_t *s_status = NULL;
static lv_obj_t *s_card = NULL;
static lv_obj_t *s_fullscreen_button = NULL;
static lv_obj_t *s_configure_button = NULL;
static lv_obj_t *s_camera_selector = NULL;
static size_t s_camera_index = 0;
static bool s_fullscreen = false;
static int s_view_width = 784;
static int s_view_height = 338;
static lv_timer_t *s_refresh_timer = NULL;
static uint8_t *s_frame = NULL;
static lv_image_dsc_t s_frame_dsc;
static uint32_t s_camera_last_frame_tick = 0;
static uint32_t s_camera_window_tick = 0;
static unsigned s_camera_frame_count = 0;


static void camera_set_status(const char *text)
{
    if (s_status) {
        lv_label_set_text(s_status, text ? text : "");
    }
}


static void camera_release_frame(void)
{
    if (s_frame) {
        heap_caps_free(s_frame);
        s_frame = NULL;
    }
    memset(&s_frame_dsc, 0, sizeof(s_frame_dsc));
}





static void camera_set_viewport(bool fullscreen)
{
    if (!s_card || !s_image || !s_status || !s_fullscreen_button || !s_configure_button || !s_camera_selector) return;
    s_fullscreen = fullscreen;
    if (fullscreen) {
        lv_obj_set_pos(s_card, 0, 0);
        lv_obj_set_size(s_card, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
        s_view_width = UI_PAGE_ROOT_WIDTH - 16;
        s_view_height = UI_PAGE_ROOT_HEIGHT - 58;
        lv_obj_set_pos(s_image, 0, 0);
        lv_obj_set_size(s_image, s_view_width, s_view_height);
        lv_obj_set_pos(s_configure_button, 8, UI_PAGE_ROOT_HEIGHT - 48);
        lv_obj_set_pos(s_camera_selector, UI_PAGE_ROOT_WIDTH - 392, UI_PAGE_ROOT_HEIGHT - 48);
        lv_obj_set_pos(s_fullscreen_button, UI_PAGE_ROOT_WIDTH - 192, UI_PAGE_ROOT_HEIGHT - 48);
        lv_obj_set_pos(s_status, 204, UI_PAGE_ROOT_HEIGHT - 40);
        lv_obj_set_width(s_status, UI_PAGE_ROOT_WIDTH - 620);
        lv_label_set_text(lv_obj_get_child(s_fullscreen_button, 0), LV_SYMBOL_CLOSE " EXIT FULLSCREEN");
        lv_obj_move_foreground(s_card);
    } else {
        lv_obj_set_pos(s_card, 20, 88);
        lv_obj_set_size(s_card, 800, 392);
        s_view_width = 784;
        s_view_height = 338;
        lv_obj_set_pos(s_image, 0, 0);
        lv_obj_set_size(s_image, s_view_width, s_view_height);
        lv_obj_set_pos(s_configure_button, 12, 344);
        lv_obj_set_pos(s_camera_selector, 374, 344);
        lv_obj_set_pos(s_fullscreen_button, 584, 344);
        lv_obj_set_pos(s_status, 210, 352);
        lv_obj_set_width(s_status, 154);
        lv_label_set_text(lv_obj_get_child(s_fullscreen_button, 0), LV_SYMBOL_IMAGE " FULLSCREEN");
    }
}


static void camera_fullscreen_cb(lv_event_t *event)
{
    (void)event;
    camera_set_viewport(!s_fullscreen);
}


static void camera_configure_cb(lv_event_t *event)
{
    (void)event;
    ui_printer_profiles_open_camera_setup(
        moonraker_config_active_profile_index(), NULL, NULL);
}


static void camera_poll_cb(lv_timer_t *timer)
{
    (void)timer;
    uint32_t now = lv_tick_get();

    uint8_t *pixels = NULL;
    size_t pixel_size = 0;
    int width = 0;
    int height = 0;
    bool ok = false;
    if (camera_stream_take_result(
            &pixels, &pixel_size, &width, &height, &ok)) {
        if (!ok || !pixels || pixel_size == 0 || width <= 0 || height <= 0) {
            if (pixels) heap_caps_free(pixels);
            camera_set_status("Camera JPEG decode failed.");
        } else if (s_image) {
            camera_release_frame();
            s_frame = pixels;
            memset(&s_frame_dsc, 0, sizeof(s_frame_dsc));
#if defined(LV_IMAGE_HEADER_MAGIC)
            s_frame_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
            s_frame_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
            s_frame_dsc.header.w = width;
            s_frame_dsc.header.h = height;
            s_frame_dsc.header.stride = width * sizeof(uint16_t);
            s_frame_dsc.data = s_frame;
            s_frame_dsc.data_size = pixel_size;
            lv_image_set_src(s_image, &s_frame_dsc);
            int scale_x = (s_view_width * 256) / width;
            int scale_y = (s_view_height * 256) / height;
            int scale = scale_x < scale_y ? scale_x : scale_y;
            if (scale < 1) scale = 1;
            lv_image_set_scale(s_image, scale);
            lv_obj_center(s_image);
            s_camera_last_frame_tick = now;
            if (!s_camera_window_tick) s_camera_window_tick = now;
            ++s_camera_frame_count;
            uint32_t elapsed = now - s_camera_window_tick;
            if (elapsed >= 1000) {
                char status[48];
                unsigned fps = (s_camera_frame_count * 1000U + elapsed / 2U) / elapsed;
                snprintf(status, sizeof(status), "LIVE  |  %u FPS", fps);
                camera_set_status(status);
                s_camera_window_tick = now;
                s_camera_frame_count = 0;
            }
        } else if (pixels) {
            heap_caps_free(pixels);
        }
    }

    if (s_camera_last_frame_tick && now - s_camera_last_frame_tick > 3500) camera_set_status("RECONNECTING  |  waiting for camera stream");
    if (camera_stream_busy()) return;
    int profile_index = moonraker_config_active_profile_index();
    camera_catalog_entry_t selected = {0};
    if (!camera_catalog_get(profile_index, s_camera_index, &selected) || !selected.configured) {
        s_camera_index = 0;
        if (!camera_catalog_get(profile_index, s_camera_index, &selected) || !selected.configured) {
            /* Never leave the previous printer's last frame visible. */
            camera_release_frame();
            if (s_image) {
                lv_image_set_src(s_image, NULL);
            }
            camera_set_status("No camera is configured for the active printer.");
            return;
        }
    }
    if (!camera_stream_start(selected.stream_url)) {
        camera_set_status("Unable to start camera stream.");
    }
}



static void camera_start(void)
{
    if (!s_root || !s_refresh_timer) {
        return;
    }
    camera_poll_cb(NULL);
}


static void camera_update_selector(void)
{
    if (!s_camera_selector) return;
    int profile_index = moonraker_config_active_profile_index();
    camera_catalog_entry_t entry = {0};
    bool configured = camera_catalog_get(profile_index, s_camera_index, &entry) && entry.configured;
    if (!configured) {
        s_camera_index = 0;
        configured = camera_catalog_get(profile_index, s_camera_index, &entry) && entry.configured;
    }

    lv_obj_t *selector_label = lv_obj_get_child(s_camera_selector, 0);
    if (selector_label) {
        char text[64];
        if (configured && entry.name[0]) {
            snprintf(text, sizeof(text), LV_SYMBOL_IMAGE " %.16s", entry.name);
        } else {
            snprintf(text, sizeof(text), LV_SYMBOL_IMAGE " CAMERA %u", (unsigned)(s_camera_index + 1));
        }
        lv_label_set_text(selector_label, text);
    }
    lv_obj_clear_flag(s_camera_selector, LV_OBJ_FLAG_HIDDEN);
    if (camera_catalog_count(profile_index) > 1) {
        lv_obj_clear_state(s_camera_selector, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(s_camera_selector, LV_STATE_DISABLED);
    }
}

static void camera_select_next_cb(lv_event_t *event)
{
    (void)event;
    int profile_index = moonraker_config_active_profile_index();
    for (size_t step = 1; step <= CAMERA_CATALOG_MAX_CAMERAS; ++step) {
        size_t candidate = (s_camera_index + step) % CAMERA_CATALOG_MAX_CAMERAS;
        camera_catalog_entry_t entry = {0};
        if (!camera_catalog_get(profile_index, candidate, &entry) || !entry.configured) continue;
        s_camera_index = candidate;
        camera_stream_stop();
        camera_release_frame();
        s_camera_last_frame_tick = 0;
        s_camera_window_tick = 0;
        s_camera_frame_count = 0;
        camera_update_selector();
        camera_set_status("Switching camera...");
        camera_poll_cb(NULL);
        return;
    }
}

void ui_camera_show(void)
{
    if (s_root) {
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_root);
        if (!s_refresh_timer) {
            s_refresh_timer = lv_timer_create(camera_poll_cb, 100, NULL);
        }
        camera_update_selector();
        camera_start();
        return;
    }

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_root, UI_PAGE_ROOT_X, UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);
    ui_page_title_create(s_root, LV_SYMBOL_IMAGE " CAMERA", "Active printer live view");

    s_card = lv_obj_create(s_root);
    lv_obj_t *card = s_card;
    lv_obj_set_size(card, 800, 392);
    lv_obj_set_pos(card, 20, 88);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, UI_CARD_DARK, 0);
    lv_obj_set_style_border_color(card, UI_BORDER_SOFT, 0);
    lv_obj_set_style_border_width(card, UI_BORDER_THIN, 0);
    lv_obj_set_style_radius(card, UI_RADIUS_CARD, 0);
    lv_obj_set_style_pad_all(card, 8, 0);

    s_image = lv_image_create(card);
    lv_obj_set_size(s_image, 784, 338);
    lv_obj_align(s_image, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_image, UI_BG_DEEP, 0);
    lv_obj_set_style_bg_opa(s_image, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_image, UI_RADIUS_BTN, 0);

    s_configure_button = ui_button_create(
        card, UI_BUTTON_SECONDARY, LV_SYMBOL_SETTINGS " CONFIGURE");
    lv_obj_set_size(s_configure_button, 190, 36);
    lv_obj_set_pos(s_configure_button, 12, 344);
    ui_button_expand_touch_target(s_configure_button);
    lv_obj_add_event_cb(s_configure_button, camera_configure_cb, LV_EVENT_CLICKED, NULL);

    s_camera_selector = ui_button_create(
        card, UI_BUTTON_SECONDARY, LV_SYMBOL_IMAGE " CAMERA 1");
    lv_obj_set_size(s_camera_selector, 190, 36);
    lv_obj_set_pos(s_camera_selector, 374, 344);
    ui_button_expand_touch_target(s_camera_selector);
    lv_obj_add_event_cb(s_camera_selector, camera_select_next_cb, LV_EVENT_CLICKED, NULL);
    camera_update_selector();

    s_fullscreen_button = ui_button_create(
        card, UI_BUTTON_OUTLINED, LV_SYMBOL_IMAGE " FULLSCREEN");
    lv_obj_set_size(s_fullscreen_button, 184, 36);
    lv_obj_set_pos(s_fullscreen_button, 584, 344);
    ui_button_expand_touch_target(s_fullscreen_button);
    lv_obj_add_event_cb(s_fullscreen_button, camera_fullscreen_cb, LV_EVENT_CLICKED, NULL);

    s_status = lv_label_create(card);
    ui_apply_text_body(s_status);
    ui_apply_label_dim(s_status);
    lv_obj_set_width(s_status, 154);
    lv_obj_set_pos(s_status, 210, 352);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(s_status, "Connecting to configured camera...");

    s_refresh_timer = lv_timer_create(camera_poll_cb, 100, NULL);
    camera_start();
}


void ui_camera_destroy(void)
{
    /* Theme changes rebuild persistent surfaces so local LVGL styles are
     * resolved from the newly selected runtime palette. */
    if (s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    camera_stream_stop();
    camera_release_frame();
    if (s_root) {
        lv_obj_delete(s_root);
        s_root = NULL;
    }
    s_image = NULL;
    s_status = NULL;
    s_card = NULL;
    s_fullscreen_button = NULL;
    s_configure_button = NULL;
    s_fullscreen = false;
    s_camera_last_frame_tick = 0;
    s_camera_window_tick = 0;
    s_camera_frame_count = 0;
}


void ui_camera_hide(void)
{
    if (s_fullscreen) camera_set_viewport(false);
    if (s_root) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    camera_stream_stop();
    s_camera_last_frame_tick = 0;
    s_camera_window_tick = 0;
    s_camera_frame_count = 0;
    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}
