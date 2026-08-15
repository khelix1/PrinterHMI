#include "ui_camera.h"

#include "camera_stream_controller.h"
#include "moonraker_config_controller.h"
#include "ui_page_geometry.h"
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
static lv_timer_t *s_refresh_timer = NULL;
static uint8_t *s_frame = NULL;
static lv_image_dsc_t s_frame_dsc;


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





static void camera_poll_cb(lv_timer_t *timer)
{
    (void)timer;

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
            int scale_x = (784 * 256) / width;
            int scale_y = (338 * 256) / height;
            int scale = scale_x < scale_y ? scale_x : scale_y;
            if (scale < 1) scale = 1;
            lv_image_set_scale(s_image, scale);
            lv_obj_center(s_image);
            camera_set_status("LIVE  |  updating camera frames");
        } else if (pixels) {
            heap_caps_free(pixels);
        }
    }

    if (camera_stream_busy()) return;
    int profile_index = moonraker_config_active_profile_index();
    const char *url = moonraker_config_camera_stream_url(profile_index);
    if (!url || !url[0]) {
        camera_set_status("No camera is configured for the active printer.");
        return;
    }
    if (!camera_stream_start(url)) {
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


void ui_camera_show(void)
{
    if (s_root) {
        lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_root);
        if (!s_refresh_timer) {
            s_refresh_timer = lv_timer_create(camera_poll_cb, 100, NULL);
        }
        camera_start();
        return;
    }

    s_root = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_root, UI_PAGE_ROOT_WIDTH, UI_PAGE_ROOT_HEIGHT);
    lv_obj_set_pos(s_root, UI_PAGE_ROOT_X, UI_PAGE_ROOT_Y);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_root_style(s_root);
    ui_page_title_create(s_root, LV_SYMBOL_IMAGE " CAMERA", "Active printer live view");

    lv_obj_t *card = lv_obj_create(s_root);
    lv_obj_set_size(card, 800, 392);
    lv_obj_set_pos(card, 20, 88);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x101827), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x31425F), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 8, 0);

    s_image = lv_image_create(card);
    lv_obj_set_size(s_image, 784, 338);
    lv_obj_align(s_image, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_image, lv_color_hex(0x060B14), 0);
    lv_obj_set_style_bg_opa(s_image, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_image, 8, 0);

    s_status = lv_label_create(card);
    ui_apply_text_body(s_status);
    /* Camera viewport footer: status remains readable over the dark well. */
    lv_obj_set_style_text_color(s_status, lv_color_hex(0xC7D4E8), 0);
    lv_obj_set_width(s_status, 760);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_label_set_text(s_status, "Connecting to configured camera...");

    s_refresh_timer = lv_timer_create(camera_poll_cb, 100, NULL);
    camera_start();
}


void ui_camera_hide(void)
{
    if (s_root) {
        lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_refresh_timer) {
        lv_timer_delete(s_refresh_timer);
        s_refresh_timer = NULL;
    }
    camera_stream_stop();
    ui_shell_raise_topbar();
    ui_shell_raise_nav();
}
