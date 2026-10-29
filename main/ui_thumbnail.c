#include "ui_thumbnail.h"
#include "esp_heap_caps.h"
#include "thumbnail_render.h"
#include "misc/cache/instance/lv_image_cache.h"
#include <string.h>
#include "ui_text.h"
#include "ui_preview_lightbox.h"
#include "ui_theme.h"
#include "ui_widgets.h"

struct ui_thumbnail {
    lv_obj_t *box;
    lv_obj_t *label;
    lv_obj_t *canvas;
    void *canvas_buf;
    lv_image_dsc_t canvas_image;
    char loaded_file[160];
    ui_thumbnail_t **owner_slot;
    unsigned active_calls;
    uint32_t epoch;
};

static bool thumbnail_begin(ui_thumbnail_t *thumb)
{
    if (!thumb || !thumb->box || thumb->active_calls) return false;
    ++thumb->active_calls;
    return true;
}

static void thumbnail_end(ui_thumbnail_t *thumb)
{
    if (--thumb->active_calls == 0 && !thumb->box) {
        lv_image_cache_drop(&thumb->canvas_image);
        heap_caps_free(thumb->canvas_buf);
        lv_free(thumb);
    }
}

static bool thumbnail_current(ui_thumbnail_t *thumb, uint32_t epoch)
{
    return thumb->box && thumb->epoch == epoch;
}

static void thumbnail_field_deleted(lv_event_t *event)
{
    ui_thumbnail_t *thumb = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == thumb->label) {thumb->label = NULL; ++thumb->epoch;}
    if (target == thumb->canvas) {thumb->canvas = NULL; ++thumb->epoch;}
}

static void thumbnail_clicked_cb(lv_event_t *event)
{
    ui_thumbnail_t *thumb = lv_event_get_user_data(event);
    if (!thumb || lv_event_get_code(event) != LV_EVENT_CLICKED ||
        lv_event_get_target_obj(event) != thumb->box ||
        lv_event_get_current_target_obj(event) != thumb->box ||
        !thumb->canvas || lv_obj_has_state(thumb->box, LV_STATE_DISABLED) ||
        lv_obj_has_flag(thumb->canvas, LV_OBJ_FLAG_HIDDEN) || !thumbnail_begin(thumb)) return;
    lv_obj_t *canvas = thumb->canvas;
    char file[sizeof(thumb->loaded_file)];
    memcpy(file, thumb->loaded_file, sizeof(file));
    lv_event_stop_bubbling(event);
    ui_preview_lightbox_show_file_object(canvas, file);
    thumbnail_end(thumb);
}

static void thumbnail_deleted_cb(lv_event_t *event)
{
    if (lv_event_get_target_obj(event) != lv_event_get_current_target_obj(event)) return;
    ui_thumbnail_t *thumb = lv_event_get_user_data(event);
    if (!thumb || lv_event_get_target_obj(event) != thumb->box) return;
    if (thumb->owner_slot && *thumb->owner_slot == thumb) *thumb->owner_slot = NULL;
    lv_obj_t *label = thumb->label, *canvas = thumb->canvas;
    thumb->box = thumb->label = thumb->canvas = NULL;
    ++thumb->epoch;
    if (label) lv_obj_remove_event_cb_with_user_data(label, thumbnail_field_deleted, thumb);
    if (canvas) {
        lv_obj_remove_event_cb_with_user_data(canvas, thumbnail_field_deleted, thumb);
        lv_image_set_src(canvas, NULL);
    }
    /* An interrupted render still owns the buffer until it unwinds. */
    if (!thumb->active_calls) {
        lv_image_cache_drop(&thumb->canvas_image);
        heap_caps_free(thumb->canvas_buf);
        lv_free(thumb);
    }
}

void ui_thumbnail_bind_owner(ui_thumbnail_t *thumb, ui_thumbnail_t **owner_slot)
{
    if (!thumb || !thumb->box) return;
    if (thumb->owner_slot && *thumb->owner_slot == thumb) *thumb->owner_slot = NULL;
    thumb->owner_slot = owner_slot;
    if (owner_slot) *owner_slot = thumb;
}

static void thumbnail_enable_lightbox(ui_thumbnail_t *thumb)
{
    if (!thumb || !thumb->box) {
        return;
    }

    lv_obj_add_event_cb(thumb->box, thumbnail_deleted_cb, LV_EVENT_DELETE, thumb);
    lv_obj_add_flag(thumb->box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        thumb->box,
        thumbnail_clicked_cb,
        LV_EVENT_CLICKED,
        thumb);
}

ui_thumbnail_t *ui_thumbnail_create(lv_obj_t *parent, int x, int y, int w, int h)
{
    ui_thumbnail_t *thumb = lv_calloc(1, sizeof(ui_thumbnail_t));
    if (!thumb) return NULL;

    thumb->canvas = NULL;
    thumb->canvas_buf = NULL;
    memset(&thumb->canvas_image, 0, sizeof(thumb->canvas_image));
    thumb->loaded_file[0] = 0;

    thumb->box = ui_create_card(parent);
    if (!thumb->box) {lv_free(thumb); return NULL;}
    lv_obj_set_size(thumb->box, w, h);
    lv_obj_set_pos(thumb->box, x, y);
    ui_apply_preview_style(thumb->box);

    thumb->label = ui_create_card_subtitle(thumb->box, "THUMBNAIL");
    lv_obj_center(thumb->label);
    lv_obj_add_event_cb(thumb->label, thumbnail_field_deleted, LV_EVENT_DELETE, thumb);
    thumbnail_enable_lightbox(thumb);

    return thumb;
}


lv_obj_t *ui_thumbnail_box(ui_thumbnail_t *thumb)
{
    return thumb ? thumb->box : NULL;
}

/* Fit helpers also serve externally owned images. Observe their lifetime only
 * for this call, so a layout callback cannot leave a borrowed pointer in use. */
typedef struct {lv_obj_t *object, *box;} thumbnail_fit_guard_t;
static void thumbnail_fit_deleted(lv_event_t *event)
{
    thumbnail_fit_guard_t *guard = lv_event_get_user_data(event);
    lv_obj_t *target = lv_event_get_target_obj(event);
    if (target == guard->object) guard->object = NULL;
    if (target == guard->box) guard->box = NULL;
}
static void thumbnail_fit_watch(thumbnail_fit_guard_t *guard, lv_obj_t *object, lv_obj_t *box)
{
    guard->object = object; guard->box = box;
    lv_obj_add_event_cb(object, thumbnail_fit_deleted, LV_EVENT_DELETE, guard);
    if (box != object) lv_obj_add_event_cb(box, thumbnail_fit_deleted, LV_EVENT_DELETE, guard);
}
static void thumbnail_fit_unwatch(thumbnail_fit_guard_t *guard)
{
    if (guard->object) lv_obj_remove_event_cb_with_user_data(guard->object, thumbnail_fit_deleted, guard);
    if (guard->box && guard->box != guard->object)
        lv_obj_remove_event_cb_with_user_data(guard->box, thumbnail_fit_deleted, guard);
}

void ui_thumbnail_fit_object(
    lv_obj_t *object,
    lv_obj_t *box,
    int source_width,
    int source_height,
    int inset)
{
    if (!object || !box) return;

    /* Compressed PNG descriptors have zero dimensions. lv_image_set_src()
     * resolves the decoder header; fit the decoded source, not that stub.
     */
    if (source_width <= 0 || source_height <= 0) {
        source_width = lv_image_get_src_width(object);
        source_height = lv_image_get_src_height(object);
    }
    if (source_width <= 0 || source_height <= 0) return;

    thumbnail_fit_guard_t guard;
    thumbnail_fit_watch(&guard, object, box);
    lv_obj_update_layout(box);
    if (!guard.object || !guard.box) goto finished;
    int width = lv_obj_get_content_width(box) - inset * 2;
    int height = lv_obj_get_content_height(box) - inset * 2;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    lv_obj_set_size(object, width, height);
    if (!guard.object || !guard.box) goto finished;
    lv_image_set_inner_align(object, LV_IMAGE_ALIGN_CONTAIN);
    if (!guard.object || !guard.box) goto finished;
    lv_obj_center(object);
finished:
    thumbnail_fit_unwatch(&guard);
}

void ui_thumbnail_show_file_image(ui_thumbnail_t *thumb, const lv_image_dsc_t *dsc,
    int scale, const char *file)
{
    if (!dsc || !thumbnail_begin(thumb)) return;
    uint32_t epoch = thumb->epoch;
    char loaded_file[sizeof(thumb->loaded_file)] = "";
    if (file && strlen(file) < sizeof(loaded_file)) memcpy(loaded_file, file, strlen(file) + 1);
    if (!thumb->canvas_buf) {
        thumb->canvas_buf = heap_caps_malloc(
            THUMBNAIL_PREVIEW_WIDTH * THUMBNAIL_PREVIEW_HEIGHT * sizeof(uint16_t),
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    int width = 0, height = 0;
    bool rendered = thumb->canvas_buf && thumbnail_render_to_rgb565_aspect(
        dsc, thumb->canvas_buf, THUMBNAIL_PREVIEW_WIDTH, THUMBNAIL_PREVIEW_HEIGHT, &width, &height);
    if (!thumbnail_current(thumb, epoch)) goto finished;
    if (!rendered) {
        thumbnail_end(thumb);
        ui_thumbnail_set_placeholder(thumb, "PREVIEW UNAVAILABLE");
        return;
    }
    lv_image_cache_drop(&thumb->canvas_image);
    memset(&thumb->canvas_image, 0, sizeof(thumb->canvas_image));
    thumb->canvas_image.header.magic = LV_IMAGE_HEADER_MAGIC;
    thumb->canvas_image.header.cf = LV_COLOR_FORMAT_RGB565;
    thumb->canvas_image.header.w = width;
    thumb->canvas_image.header.h = height;
    thumb->canvas_image.header.stride = width * sizeof(uint16_t);
    thumb->canvas_image.data_size = (size_t)width * height * sizeof(uint16_t);
    thumb->canvas_image.data = thumb->canvas_buf;

    lv_obj_t *label = thumb->label;
    thumb->label = NULL;
    if (label) {
        lv_obj_remove_event_cb_with_user_data(label, thumbnail_field_deleted, thumb);
        lv_obj_delete(label);
    }
    if (!thumbnail_current(thumb, epoch)) goto finished;
    if (!thumb->canvas) {
        thumb->canvas = lv_image_create(thumb->box);
        lv_obj_add_event_cb(thumb->canvas, thumbnail_field_deleted, LV_EVENT_DELETE, thumb);
    }
    lv_image_set_src(thumb->canvas, &thumb->canvas_image);
    if (!thumbnail_current(thumb, epoch)) goto finished;
    memcpy(thumb->loaded_file, loaded_file, sizeof(loaded_file));
    if (scale > 0) {
        lv_image_set_scale(thumb->canvas, scale);
        if (!thumbnail_current(thumb, epoch)) goto finished;
        lv_obj_center(thumb->canvas);
    } else {
        ui_thumbnail_fit_object(thumb->canvas, thumb->box, width, height, 0);
    }
finished:
    thumbnail_end(thumb);
}

void ui_thumbnail_set_placeholder(ui_thumbnail_t *thumb, const char *text)
{
    if (!thumbnail_begin(thumb)) return;
    uint32_t epoch = thumb->epoch;
    lv_obj_t *canvas = thumb->canvas;
    thumb->canvas = NULL;
    thumb->loaded_file[0] = 0;
    if (canvas) {
        lv_obj_remove_event_cb_with_user_data(canvas, thumbnail_field_deleted, thumb);
        lv_obj_delete(canvas);
    }
    if (!thumbnail_current(thumb, epoch)) goto finished;
    if (!thumb->label) {
        thumb->label = ui_create_card_subtitle(thumb->box, text ? text : "THUMBNAIL");
        lv_obj_add_event_cb(thumb->label, thumbnail_field_deleted, LV_EVENT_DELETE, thumb);
    }
    lv_label_set_text(thumb->label, text ? text : ui_text("THUMBNAIL"));
    if (!thumbnail_current(thumb, epoch)) goto finished;
    lv_obj_clear_flag(thumb->label, LV_OBJ_FLAG_HIDDEN);
    if (!thumbnail_current(thumb, epoch)) goto finished;
    lv_obj_center(thumb->label);
finished:
    thumbnail_end(thumb);
}
