#include "thumbnail_render.h"

#include <stddef.h>

#include "draw/lv_image_decoder_private.h"

static uint16_t thumbnail_render_rgb565_from_rgb(
    uint8_t red,
    uint8_t green,
    uint8_t blue)
{
    return (uint16_t)(
        ((uint16_t)(red >> 3) << 11) |
        ((uint16_t)(green >> 2) << 5) |
        ((uint16_t)(blue >> 3)));
}

static bool thumbnail_render_read_pixel(
    const uint8_t *base,
    uint32_t stride,
    int x,
    int y,
    lv_color_format_t color_format,
    uint16_t *out)
{
    if (!base || !out || x < 0 || y < 0) {
        return false;
    }

    const uint8_t *pixel =
        base + ((uint32_t)y * stride);

    if (color_format == LV_COLOR_FORMAT_RGB565) {
        const uint16_t *row =
            (const uint16_t *)pixel;

        *out = row[x];
        return true;
    }

    if (color_format == LV_COLOR_FORMAT_RGB888) {
        pixel += x * 3;

        *out = thumbnail_render_rgb565_from_rgb(
            pixel[0],
            pixel[1],
            pixel[2]);

        return true;
    }

    if (color_format == LV_COLOR_FORMAT_XRGB8888 ||
        color_format == LV_COLOR_FORMAT_ARGB8888) {
        pixel += x * 4;

        /*
         * LVGL's decoded 32-bit buffer is stored B, G, R, X/A
         * on this target, matching the previous main.c renderer.
         */
        *out = thumbnail_render_rgb565_from_rgb(
            pixel[2],
            pixel[1],
            pixel[0]);

        return true;
    }

    return false;
}

static bool render_rgb565(
    const lv_image_dsc_t *image,
    uint16_t *destination,
    int destination_width,
    int destination_height,
    bool fill,
    int *packed_width,
    int *packed_height)
{
    if (!image ||
        !destination ||
        destination_width <= 0 ||
        destination_height <= 0) {
        return false;
    }

    /* Callers often supply a temporary PNG descriptor. Decode without
     * retaining that stack address or a second full-size image in LVGL cache.
     */
    lv_image_decoder_args_t args = { .no_cache = true };
    lv_image_decoder_dsc_t decoder;
    lv_result_t result =
        lv_image_decoder_open(&decoder, image, &args);

    if (result != LV_RESULT_OK ||
        !decoder.decoded ||
        !decoder.decoded->data) {
        lv_image_decoder_close(&decoder);
        return false;
    }

    const lv_draw_buf_t *decoded =
        decoder.decoded;

    int source_width =
        (int)decoded->header.w;

    int source_height =
        (int)decoded->header.h;

    if (source_width <= 0 || source_height <= 0) {
        lv_image_decoder_close(&decoder);
        return false;
    }

    uint32_t stride =
        decoded->header.stride;

    lv_color_format_t color_format =
        decoded->header.cf;

    const uint8_t *source =
        (const uint8_t *)decoded->data;

    if (packed_width && packed_height) {
        /* Pack the complete source into the largest aspect-preserving rectangle
         * within the caller's capacity. No synthetic letterbox pixels. */
        int width = destination_width;
        int height = (int)((int64_t)source_height * width / source_width);
        if (height > destination_height) {
            height = destination_height;
            width = (int)((int64_t)source_width * height / source_height);
        }
        if (width < 1) width = 1;
        if (height < 1) height = 1;
        bool valid = true;
        for (int y = 0; y < height && valid; y++) {
            int sy = (int)((int64_t)y * source_height / height);
            for (int x = 0; x < width; x++) {
                int sx = (int)((int64_t)x * source_width / width);
                if (!thumbnail_render_read_pixel(source, stride, sx, sy, color_format,
                        &destination[(size_t)y * width + x])) {
                    valid = false;
                    break;
                }
            }
        }
        lv_image_decoder_close(&decoder);
        if (valid) { *packed_width = width; *packed_height = height; }
        return valid;
    }

    if (fill) {
        /* Center-crop the source, then sample every destination pixel. No
         * letterbox pixels are baked into the shared cached preview. */
        int crop_width = source_width, crop_height = source_height;
        if ((int64_t)source_width * destination_height >
            (int64_t)source_height * destination_width) {
            crop_width = (int)((int64_t)source_height * destination_width / destination_height);
        } else {
            crop_height = (int)((int64_t)source_width * destination_height / destination_width);
        }
        if (crop_width < 1) crop_width = 1;
        if (crop_height < 1) crop_height = 1;
        int left = (source_width - crop_width) / 2;
        int top = (source_height - crop_height) / 2;
        bool valid = true;
        for (int y = 0; y < destination_height && valid; ++y) {
            int source_y = top + (int)((int64_t)y * crop_height / destination_height);
            for (int x = 0; x < destination_width; ++x) {
                int source_x = left + (int)((int64_t)x * crop_width / destination_width);
                if (!thumbnail_render_read_pixel(source, stride, source_x, source_y,
                        color_format, &destination[(size_t)y * destination_width + x])) {
                    valid = false;
                    break;
                }
            }
        }
        lv_image_decoder_close(&decoder);
        return valid;
    }

    size_t destination_pixels =
        (size_t)destination_width *
        (size_t)destination_height;

    for (size_t index = 0;
         index < destination_pixels;
         index++) {
        destination[index] = 0x0000;
    }

    int draw_width =
        destination_width;

    int draw_height =
        (int)((int64_t)source_height * draw_width / source_width);

    if (draw_height > destination_height) {
        draw_height = destination_height;
        draw_width =
            (int)((int64_t)source_width * draw_height / source_height);
    }

    /* A one-pixel-wide/tall thumbnail still has a valid fitted preview. */
    if (draw_width < 1) draw_width = 1;
    if (draw_height < 1) draw_height = 1;

    int offset_x =
        (destination_width - draw_width) / 2;

    int offset_y =
        (destination_height - draw_height) / 2;

    for (int y = 0; y < draw_height; y++) {
        int source_y =
            (y * source_height) / draw_height;

        for (int x = 0; x < draw_width; x++) {
            int source_x =
                (x * source_width) / draw_width;

            uint16_t pixel = 0x0000;

            if (!thumbnail_render_read_pixel(
                    source,
                    stride,
                    source_x,
                    source_y,
                    color_format,
                    &pixel)) {
                pixel = 0x0000;
            }

            destination[
                (y + offset_y) * destination_width +
                (x + offset_x)
            ] = pixel;
        }
    }

    lv_image_decoder_close(&decoder);
    return true;
}

bool thumbnail_render_to_rgb565(const lv_image_dsc_t *image, uint16_t *destination,
    int width, int height)
{
    return render_rgb565(image, destination, width, height, true, NULL, NULL);
}

bool thumbnail_render_to_rgb565_fit(const lv_image_dsc_t *image, uint16_t *destination,
    int width, int height)
{
    return render_rgb565(image, destination, width, height, false, NULL, NULL);
}

bool thumbnail_render_to_rgb565_aspect(const lv_image_dsc_t *image,
    uint16_t *destination, int max_width, int max_height, int *width, int *height)
{
    if (!width || !height) return false;
    *width = *height = 0;
    return render_rgb565(image, destination, max_width, max_height, false, width, height);
}
