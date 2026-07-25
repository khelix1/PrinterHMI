#include "ui_preview_lightbox_v32.h"

#include "ui_theme.h"
#include "ui_widgets.h"

static lv_obj_t *s_preview_lightbox = NULL;

static void preview_lightbox_delete_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);

    if (target == s_preview_lightbox) {
        s_preview_lightbox = NULL;
    }
}

static void preview_lightbox_close_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        ui_preview_lightbox_v32_close();
    }
}

bool ui_preview_lightbox_v32_is_open(void)
{
    return s_preview_lightbox != NULL;
}

void ui_preview_lightbox_v32_close(void)
{
    if (!s_preview_lightbox) {
        return;
    }

    lv_obj_t *lightbox = s_preview_lightbox;
    s_preview_lightbox = NULL;
    lv_obj_delete(lightbox);
}

void ui_preview_lightbox_v32_show(const lv_image_dsc_t *image)
{
    if (!image || image->header.w == 0 || image->header.h == 0) {
        return;
    }

    ui_preview_lightbox_v32_close();

    lv_obj_t *screen = lv_screen_active();
    if (!screen) {
        return;
    }

    s_preview_lightbox = lv_obj_create(screen);
    if (!s_preview_lightbox) {
        return;
    }

    lv_obj_set_size(s_preview_lightbox, 1024, 600);
    lv_obj_set_pos(s_preview_lightbox, 0, 0);
    lv_obj_clear_flag(s_preview_lightbox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_preview_lightbox, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_bg_color(
        s_preview_lightbox,
        lv_color_black(),
        0);
    lv_obj_set_style_bg_opa(
        s_preview_lightbox,
        LV_OPA_90,
        0);
    lv_obj_set_style_border_width(s_preview_lightbox, 0, 0);
    lv_obj_set_style_radius(s_preview_lightbox, 0, 0);
    lv_obj_set_style_pad_all(s_preview_lightbox, 0, 0);

    lv_obj_add_event_cb(
        s_preview_lightbox,
        preview_lightbox_close_cb,
        LV_EVENT_CLICKED,
        NULL);
    lv_obj_add_event_cb(
        s_preview_lightbox,
        preview_lightbox_delete_cb,
        LV_EVENT_DELETE,
        NULL);

    lv_obj_t *preview = lv_image_create(s_preview_lightbox);
    if (!preview) {
        ui_preview_lightbox_v32_close();
        return;
    }

    lv_image_set_src(preview, image);

    int scale_x = (900 * 256) / (int)image->header.w;
    int scale_y = (520 * 256) / (int)image->header.h;
    int scale = scale_x < scale_y ? scale_x : scale_y;

    /*
     * Files-list previews can be intentionally small to conserve RAM.
     * Do not apply the 3x in-card thumbnail cap here: this surface exists
     * specifically to enlarge the source to the available screen area.
     */
    if (scale < 1) scale = 1;
    if (scale > 8192) scale = 8192;

    lv_image_set_scale(preview, scale);
    lv_obj_center(preview);
    lv_obj_clear_flag(preview, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *hint =
        ui_create_card_subtitle(
            s_preview_lightbox,
            "TAP ANYWHERE TO CLOSE");

    if (hint) {
        ui_apply_label_bright(hint);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -12);
        lv_obj_clear_flag(hint, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_move_foreground(s_preview_lightbox);
}

void ui_preview_lightbox_v32_show_object(lv_obj_t *image_object)
{
    if (!image_object) {
        return;
    }

    const void *source = lv_image_get_src(image_object);
    if (!source) {
        return;
    }

    ui_preview_lightbox_v32_show(
        (const lv_image_dsc_t *)source);
}
