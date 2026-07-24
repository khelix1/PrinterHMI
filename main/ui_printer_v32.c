#include <string.h>

#include "ui_printer_v32.h"
#include "esp_log.h"
#include "printer_controller.h"
#include "moonraker_config_controller.h"
#include "printer_preview_cache_v32.h"
#include "ui_theme.h"
#include "ui_widgets.h"

/* Temporary bridge while Printer page implementation still lives in main.c. */
void ui_printer_v32_create(void);
void ui_printer_v32_destroy(void);

void ui_printer_v32_show(void)
{
    ui_printer_v32_create();
}

void ui_printer_v32_hide(void)
{
    ui_printer_v32_destroy();
}

void ui_printer_v32_refresh(void)
{
}

/* =================================================================
 * Printer-page thumbnail preview
 * ================================================================= */

static const char *s_preview_tag = "ui_printer_preview";

static lv_obj_t *s_preview_box = NULL;
static lv_obj_t *s_preview_label = NULL;
static lv_obj_t *s_preview_image = NULL;
static lv_obj_t *s_preview_canvas = NULL;

static char s_preview_canvas_file[160] = "";
static uint32_t s_preview_cache_revision = 0;
static int s_preview_cache_profile_index = -1;

static void preview_copy_text(
    char *destination,
    size_t destination_size,
    const char *source)
{
    if (!destination || destination_size == 0) {
        return;
    }

    if (!source) {
        source = "";
    }

    snprintf(destination,
             destination_size,
             "%s",
             source);
}

static void preview_show_placeholder(void)
{
    if (!s_preview_box) {
        return;
    }

    if (!s_preview_label) {
        s_preview_label = lv_label_create(s_preview_box);

        lv_obj_set_width(s_preview_label, 220);

        lv_label_set_long_mode(
            s_preview_label,
            LV_LABEL_LONG_WRAP);

        lv_obj_set_style_text_align(
            s_preview_label,
            LV_TEXT_ALIGN_CENTER,
            0);

        ui_apply_text_body_large(
            s_preview_label);
        ui_apply_label_dim(
            s_preview_label);
    }

    lv_label_set_text(
        s_preview_label,
        "PRINT\nTHUMBNAIL\n\nNo preview loaded");

    lv_obj_center(s_preview_label);
}


void ui_printer_v32_preview_create(lv_obj_t *parent)
{
    if (!parent || s_preview_box) {
        return;
    }

    /*
     * parent is the 800x220 Active Print panel.
     */
    s_preview_box =
        ui_create_operator_card(
            parent,
            16,
            28,
            290,
            184);

    if (!s_preview_box) {
        return;
    }

    preview_show_placeholder();
}


void ui_printer_v32_preview_show(
    const char *printer_state,
    const char *printer_file,
    const char *selected_file)
{
    if (!s_preview_box) {
        return;
    }

    bool printer_is_live =
        printer_controller_is_live_state(printer_state);

    const char *preview_file =
        printer_is_live &&
        printer_file &&
        printer_file[0]
            ? printer_file
            : selected_file;

    const char *cached_file = NULL;
    uint32_t cached_revision = 0;
    int active_profile =
        moonraker_config_active_profile_index();

    const lv_image_dsc_t *cached_image =
        printer_preview_cache_v32_image(
            active_profile,
            &cached_file,
            &cached_revision);

    if (cached_image &&
        cached_file &&
        cached_file[0] &&
        (!preview_file ||
         !preview_file[0] ||
         strcmp(cached_file, preview_file) == 0)) {
        if (s_preview_canvas) {
            lv_obj_delete(s_preview_canvas);
            s_preview_canvas = NULL;
        }

        if (!s_preview_image) {
            s_preview_image = lv_image_create(s_preview_box);
        }

        if (s_preview_image &&
            (s_preview_cache_profile_index != active_profile ||
             s_preview_cache_revision != cached_revision)) {
            lv_image_set_src(s_preview_image, cached_image);

            int scale_x =
                (280 * 256) / (int)cached_image->header.w;

            int scale_y =
                (180 * 256) / (int)cached_image->header.h;

            int scale = scale_x < scale_y ? scale_x : scale_y;
            if (scale > 256) scale = 256;
            if (scale < 1) scale = 1;

            lv_image_set_scale(s_preview_image, scale);
            lv_obj_center(s_preview_image);
            s_preview_cache_revision = cached_revision;
            s_preview_cache_profile_index = active_profile;
        }

        if (s_preview_label) {
            lv_obj_add_flag(s_preview_label, LV_OBJ_FLAG_HIDDEN);
        }

        if (s_preview_image) {
            lv_obj_clear_flag(s_preview_image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(s_preview_image);
        }

        preview_copy_text(
            s_preview_canvas_file,
            sizeof(s_preview_canvas_file),
            preview_file);

        return;
    }

    /* PRINTER_PREVIEW_STRICT_PROFILE_CONSUMER
     * A Printer page may display only the image owned by its active profile.
     * The process-wide thumbnail manager is a transient decode pipeline and
     * must never be used as a UI fallback after a profile cache miss or file
     * identity mismatch.
     */
    preview_show_placeholder();
    return;

}

void ui_printer_v32_preview_reset(void)
{
    s_preview_canvas_file[0] = '\0';
    s_preview_cache_revision = 0;
    s_preview_cache_profile_index = -1;

    if (s_preview_canvas) {
        lv_obj_delete(s_preview_canvas);
        s_preview_canvas = NULL;
    }

    if (s_preview_image) {
        lv_obj_delete(s_preview_image);
        s_preview_image = NULL;
    }

    preview_show_placeholder();
}

void ui_printer_v32_preview_destroy_refs(void)
{
    /*
     * The page parent owns and deletes the LVGL children.
     * Only clear references here.
     */
    s_preview_box = NULL;
    s_preview_label = NULL;
    s_preview_image = NULL;
    s_preview_canvas = NULL;
    s_preview_canvas_file[0] = '\0';
    s_preview_cache_revision = 0;
    s_preview_cache_profile_index = -1;
}

char *ui_printer_v32_preview_canvas_file(void)
{
    return s_preview_canvas_file;
}

size_t ui_printer_v32_preview_canvas_file_size(void)
{
    return sizeof(s_preview_canvas_file);
}

lv_obj_t **ui_printer_v32_preview_canvas_ref(void)
{
    return &s_preview_canvas;
}

lv_obj_t **ui_printer_v32_preview_image_ref(void)
{
    return &s_preview_image;
}

