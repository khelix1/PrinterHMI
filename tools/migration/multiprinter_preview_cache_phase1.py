#!/usr/bin/env python3
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main"


def read(name: str) -> str:
    path = MAIN / name
    if not path.exists():
        raise RuntimeError(f"missing required file: {path}")
    return path.read_text()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


cache_h = r'''#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

/*
 * Profile-indexed rendered preview cache.
 *
 * Each configured printer owns one immutable-sized RGB565 surface. The
 * active thumbnail pipeline publishes a completed render; UI pages consume
 * the same descriptor without decoding the PNG again.
 *
 * publish_* must be called while the LVGL/display lock is held.
 */
bool printer_preview_cache_v32_publish_active(
    const char *file,
    const uint16_t *pixels,
    int width,
    int height);

const lv_image_dsc_t *printer_preview_cache_v32_image(
    int profile_index,
    const char **file_out,
    uint32_t *revision_out);

void printer_preview_cache_v32_invalidate(int profile_index);
void printer_preview_cache_v32_reset(void);
'''


cache_c = r'''#include "printer_preview_cache_v32.h"

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#include "moonraker_config_controller.h"

typedef struct {
    uint16_t *pixels;
    size_t pixel_capacity;
    int width;
    int height;
    char file[160];
    char host[MOONRAKER_CONFIG_HOST_LENGTH];
    int port;
    uint32_t revision;
    bool ready;
    lv_image_dsc_t image;
} preview_slot_t;

static const char *TAG = "printer_preview_cache";

static preview_slot_t
    s_slots[MOONRAKER_CONFIG_MAX_PROFILES];


static bool valid_index(int index)
{
    return index >= 0 && index < MOONRAKER_CONFIG_MAX_PROFILES;
}


static bool slot_matches_profile(
    const preview_slot_t *slot,
    const moonraker_profile_t *profile)
{
    return slot &&
           profile &&
           profile->configured &&
           strcmp(slot->host, profile->host) == 0 &&
           slot->port == profile->port;
}


static bool ensure_pixels(preview_slot_t *slot, size_t count)
{
    if (!slot || count == 0) return false;

    if (slot->pixels && slot->pixel_capacity >= count) {
        return true;
    }

    uint16_t *replacement = heap_caps_malloc(
        count * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (!replacement) {
        replacement = heap_caps_malloc(
            count * sizeof(uint16_t),
            MALLOC_CAP_8BIT);
    }

    if (!replacement) return false;

    if (slot->pixels) {
        heap_caps_free(slot->pixels);
    }

    slot->pixels = replacement;
    slot->pixel_capacity = count;
    return true;
}


bool printer_preview_cache_v32_publish_active(
    const char *file,
    const uint16_t *pixels,
    int width,
    int height)
{
    if (!file || !file[0] || !pixels || width <= 0 || height <= 0) {
        return false;
    }

    int index = moonraker_config_active_profile_index();
    const moonraker_profile_t *profile = moonraker_config_profile(index);

    if (!valid_index(index) || !profile || !profile->configured) {
        return false;
    }

    preview_slot_t *slot = &s_slots[index];
    size_t count = (size_t)width * (size_t)height;

    if (!ensure_pixels(slot, count)) {
        ESP_LOGW(TAG, "Profile %d RGB565 allocation failed", index + 1);
        return false;
    }

    memcpy(slot->pixels, pixels, count * sizeof(uint16_t));

    slot->width = width;
    slot->height = height;
    snprintf(slot->file, sizeof(slot->file), "%s", file);
    snprintf(slot->host, sizeof(slot->host), "%s", profile->host);
    slot->port = profile->port;
    slot->ready = true;
    slot->revision++;

    if (slot->revision == 0) slot->revision = 1;

    memset(&slot->image, 0, sizeof(slot->image));
#if defined(LV_IMAGE_HEADER_MAGIC)
    slot->image.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
    slot->image.header.cf = LV_COLOR_FORMAT_RGB565;
    slot->image.header.w = width;
    slot->image.header.h = height;
    slot->image.header.stride = width * sizeof(uint16_t);
    slot->image.data_size = count * sizeof(uint16_t);
    slot->image.data = (const uint8_t *)slot->pixels;

    ESP_LOGI(
        TAG,
        "Profile %d preview cached: %s %dx%d rev=%u",
        index + 1,
        slot->file,
        width,
        height,
        (unsigned)slot->revision);

    return true;
}


const lv_image_dsc_t *printer_preview_cache_v32_image(
    int profile_index,
    const char **file_out,
    uint32_t *revision_out)
{
    if (file_out) *file_out = NULL;
    if (revision_out) *revision_out = 0;

    if (!valid_index(profile_index)) return NULL;

    preview_slot_t *slot = &s_slots[profile_index];
    const moonraker_profile_t *profile =
        moonraker_config_profile(profile_index);

    if (!slot->ready ||
        !slot->pixels ||
        !slot_matches_profile(slot, profile)) {
        return NULL;
    }

    if (file_out) *file_out = slot->file;
    if (revision_out) *revision_out = slot->revision;
    return &slot->image;
}


void printer_preview_cache_v32_invalidate(int profile_index)
{
    if (!valid_index(profile_index)) return;

    preview_slot_t *slot = &s_slots[profile_index];
    slot->ready = false;
    slot->file[0] = '\0';
    slot->host[0] = '\0';
    slot->port = 0;
    slot->revision++;
}


void printer_preview_cache_v32_reset(void)
{
    for (int index = 0; index < MOONRAKER_CONFIG_MAX_PROFILES; ++index) {
        preview_slot_t *slot = &s_slots[index];

        if (slot->pixels) {
            heap_caps_free(slot->pixels);
        }

        memset(slot, 0, sizeof(*slot));
    }
}
'''


cmake = read("CMakeLists.txt")
if '"printer_preview_cache_v32.c"' not in cmake:
    cmake = replace_once(
        cmake,
        '        "printer_profile_health.c"\n',
        '        "printer_profile_health.c"\n'
        '        "printer_preview_cache_v32.c"\n',
        "profile-health CMake registration")


chooser = read("ui_printer_chooser_v32.c")

if '#include "printer_preview_cache_v32.h"' not in chooser:
    chooser = replace_once(
        chooser,
        '#include "printer_profile_health.h"\n',
        '#include "printer_profile_health.h"\n'
        '#include "printer_preview_cache_v32.h"\n',
        "chooser health include")

card_old = r'''    lv_obj_t *status;
    lv_obj_t *preview;
    lv_obj_t *active;
'''

card_new = r'''    lv_obj_t *status;
    lv_obj_t *preview_box;
    lv_obj_t *preview;
    lv_obj_t *preview_icon;
    lv_obj_t *preview_image;
    uint32_t preview_revision;
    lv_obj_t *active;
'''

if "preview_revision" not in chooser:
    chooser = replace_once(
        chooser,
        card_old,
        card_new,
        "chooser card preview fields")

chooser = replace_once(
    chooser,
    r'''    lv_obj_t *preview_box = lv_obj_create(card->root);
    lv_obj_set_size(preview_box, 116, 116);
    lv_obj_set_pos(preview_box, 12, 34);
    lv_obj_clear_flag(preview_box, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_preview_style(preview_box);

    lv_obj_t *preview_icon = lv_label_create(preview_box);
    lv_label_set_text(preview_icon, LV_SYMBOL_FILE);
    ui_apply_text_popup_title(preview_icon);
    ui_apply_label_dim(preview_icon);
    lv_obj_align(preview_icon, LV_ALIGN_TOP_MID, 0, 16);

    card->preview = lv_label_create(preview_box);
''',
    r'''    card->preview_box = lv_obj_create(card->root);
    lv_obj_set_size(card->preview_box, 116, 116);
    lv_obj_set_pos(card->preview_box, 12, 34);
    lv_obj_clear_flag(card->preview_box, LV_OBJ_FLAG_SCROLLABLE);
    ui_apply_preview_style(card->preview_box);

    card->preview_icon = lv_label_create(card->preview_box);
    lv_label_set_text(card->preview_icon, LV_SYMBOL_FILE);
    ui_apply_text_popup_title(card->preview_icon);
    ui_apply_label_dim(card->preview_icon);
    lv_obj_align(card->preview_icon, LV_ALIGN_TOP_MID, 0, 16);

    card->preview = lv_label_create(card->preview_box);
''',
    "chooser preview-box construction")

refresh_anchor = r'''        if (!card->root) continue;

        if (!configured) {
'''

refresh_insert = r'''        if (!card->root) continue;

        const char *cached_file = NULL;
        uint32_t cached_revision = 0;

        const lv_image_dsc_t *cached_image =
            configured
                ? printer_preview_cache_v32_image(
                    index,
                    &cached_file,
                    &cached_revision)
                : NULL;

        if (cached_image) {
            if (!card->preview_image) {
                card->preview_image =
                    lv_image_create(card->preview_box);
            }

            if (card->preview_image &&
                card->preview_revision != cached_revision) {
                lv_image_set_src(card->preview_image, cached_image);

                int scale_x =
                    (108 * 256) / (int)cached_image->header.w;

                int scale_y =
                    (108 * 256) / (int)cached_image->header.h;

                int scale = scale_x < scale_y ? scale_x : scale_y;
                if (scale > 256) scale = 256;
                if (scale < 1) scale = 1;

                lv_image_set_scale(card->preview_image, scale);
                lv_obj_center(card->preview_image);
                card->preview_revision = cached_revision;
            }

            if (card->preview_image) {
                lv_obj_clear_flag(
                    card->preview_image,
                    LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(card->preview_image);
            }

            lv_obj_add_flag(card->preview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(card->preview_icon, LV_OBJ_FLAG_HIDDEN);
        } else {
            if (card->preview_image) {
                lv_obj_add_flag(
                    card->preview_image,
                    LV_OBJ_FLAG_HIDDEN);
            }

            lv_obj_clear_flag(card->preview, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(card->preview_icon, LV_OBJ_FLAG_HIDDEN);
            card->preview_revision = 0;
        }

        if (!configured) {
'''

if "printer_preview_cache_v32_image(" not in chooser:
    chooser = replace_once(
        chooser,
        refresh_anchor,
        refresh_insert,
        "chooser card refresh anchor")

# Do not overwrite the placeholder label while a real cached image is shown.
chooser = chooser.replace(
    '            lv_label_set_text(card->preview, "ADD A\\nPRINTER");\n',
    '            if (!cached_image)\n'
    '                lv_label_set_text(card->preview, "ADD A\\nPRINTER");\n')

chooser = chooser.replace(
    r'''            if (state && state->live_data_ok && state->printer_file[0]) {
                lv_label_set_text(card->preview, state->printer_file);
                ui_apply_label_bright(card->preview);
            } else {
                lv_label_set_text(card->preview, online ? "READY FOR\nLIVE DATA" : "NO LIVE\nPREVIEW");
                ui_apply_label_dim(card->preview);
            }
''',
    r'''            if (!cached_image) {
                if (state && state->live_data_ok && state->printer_file[0]) {
                    lv_label_set_text(card->preview, state->printer_file);
                    ui_apply_label_bright(card->preview);
                } else {
                    lv_label_set_text(card->preview, online ? "READY FOR\nLIVE DATA" : "NO LIVE\nPREVIEW");
                    ui_apply_label_dim(card->preview);
                }
            }
''')

chooser = chooser.replace(
    r'''            lv_label_set_text(card->preview, online ? "AVAILABLE\nTO OPEN" : "NO LIVE\nPREVIEW");
            ui_apply_label_dim(card->preview);
''',
    r'''            if (!cached_image) {
                lv_label_set_text(card->preview, online ? "AVAILABLE\nTO OPEN" : "NO LIVE\nPREVIEW");
                ui_apply_label_dim(card->preview);
            }
''')


printer = read("ui_printer_v32.c")

if '#include "printer_preview_cache_v32.h"' not in printer:
    printer = replace_once(
        printer,
        '#include "printer_controller.h"\n',
        '#include "printer_controller.h"\n'
        '#include "moonraker_config_controller.h"\n'
        '#include "printer_preview_cache_v32.h"\n',
        "Printer controller include")

if "s_preview_cache_revision" not in printer:
    printer = replace_once(
        printer,
        'static char s_preview_canvas_file[160] = "";\n',
        'static char s_preview_canvas_file[160] = "";\n'
        'static uint32_t s_preview_cache_revision = 0;\n',
        "Printer preview canvas file")

preview_file_anchor = r'''    if (!preview_file ||
        !preview_file[0] ||
        !thumbnail_manager_v32_has_png()) {
        preview_show_placeholder();
        return;
    }
'''

preview_cache_block = r'''    if (!preview_file || !preview_file[0]) {
        preview_show_placeholder();
        return;
    }

    const char *cached_file = NULL;
    uint32_t cached_revision = 0;

    const lv_image_dsc_t *cached_image =
        printer_preview_cache_v32_image(
            moonraker_config_active_profile_index(),
            &cached_file,
            &cached_revision);

    if (cached_image &&
        cached_file &&
        strcmp(cached_file, preview_file) == 0) {
        if (s_preview_canvas) {
            lv_obj_delete(s_preview_canvas);
            s_preview_canvas = NULL;
        }

        if (!s_preview_image) {
            s_preview_image = lv_image_create(s_preview_box);
        }

        if (s_preview_image &&
            s_preview_cache_revision != cached_revision) {
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

    if (!thumbnail_manager_v32_has_png()) {
        preview_show_placeholder();
        return;
    }
'''

if "printer_preview_cache_v32_image(" not in printer:
    printer = replace_once(
        printer,
        preview_file_anchor,
        preview_cache_block,
        "Printer preview availability block")

printer = replace_once(
    printer,
    r'''    s_preview_canvas_file[0] = '\0';

    if (s_preview_canvas) {
''',
    r'''    s_preview_canvas_file[0] = '\0';
    s_preview_cache_revision = 0;

    if (s_preview_canvas) {
''',
    "Printer preview reset")


main = read("main.c")

if '#include "printer_preview_cache_v32.h"' not in main:
    main = replace_once(
        main,
        '#include "printer_profile_health.h"\n',
        '#include "printer_profile_health.h"\n'
        '#include "printer_preview_cache_v32.h"\n',
        "profile-health include")

sync_render = re.compile(
    r'(    if \(!thumbnail_render_v32_to_rgb565\(\n'
    r'            thumbnail_manager_v32_image_dsc\(\),\n'
    r'            dash_thumb_canvas_buf,\n'
    r'            DASH_THUMB_CANVAS_W,\n'
    r'            DASH_THUMB_CANVAS_H\)\) \{\n'
    r'        ESP_LOGW\(TAG, "DASH_CANVAS shared render failed"\);\n'
    r'        return;\n'
    r'    \}\n)')

sync_matches = list(sync_render.finditer(main))
if len(sync_matches) != 1:
    raise RuntimeError(
        f"expected one synchronous Dashboard render, found {len(sync_matches)}")

if "CACHE_PUBLISH_SYNC" not in main:
    match = sync_matches[0]
    addition = r'''

    /* CACHE_PUBLISH_SYNC */
    if (printer_controller_is_live_state(printer_state) &&
        strcmp(thumbnail_session_v32_selected_file(), printer_file) == 0) {
        printer_preview_cache_v32_publish_active(
            printer_file,
            dash_thumb_canvas_buf,
            DASH_THUMB_CANVAS_W,
            DASH_THUMB_CANVAS_H);
    }
'''
    main = main[:match.end()] + addition + main[match.end():]

worker_anchor = r'''        if (ok) {
            ESP_LOGI(TAG,
                     "DASH_WORKER_RENDER shared renderer %dx%d",
                     DASH_THUMB_CANVAS_W,
                     DASH_THUMB_CANVAS_H);
'''

worker_new = r'''        if (ok) {
            if (printer_controller_is_live_state(printer_state) &&
                strcmp(dash_thumb_render_file, printer_file) == 0) {
                printer_preview_cache_v32_publish_active(
                    dash_thumb_render_file,
                    dash_thumb_canvas_buf,
                    DASH_THUMB_CANVAS_W,
                    DASH_THUMB_CANVAS_H);
            }

            ESP_LOGI(TAG,
                     "DASH_WORKER_RENDER shared renderer %dx%d",
                     DASH_THUMB_CANVAS_W,
                     DASH_THUMB_CANVAS_H);
'''

if "printer_preview_cache_v32_publish_active(\n                    dash_thumb_render_file" not in main:
    main = replace_once(
        main,
        worker_anchor,
        worker_new,
        "Dashboard worker success block")


(MAIN / "printer_preview_cache_v32.h").write_text(cache_h)
(MAIN / "printer_preview_cache_v32.c").write_text(cache_c)
(MAIN / "CMakeLists.txt").write_text(cmake)
(MAIN / "ui_printer_chooser_v32.c").write_text(chooser)
(MAIN / "ui_printer_v32.c").write_text(printer)
(MAIN / "main.c").write_text(main)

print("PASS: profile-indexed preview cache Phase 1 installed")
print("  - active Dashboard render publishes once into its profile slot")
print("  - chooser cards display cached RGB565 previews")
print("  - Printer page reuses the same cached descriptor")
print("  - cached previews remain available when switching profiles")
print("Next: idf.py build")
