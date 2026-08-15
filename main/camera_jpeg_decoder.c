#include "camera_jpeg_decoder.h"

#include "esp_heap_caps.h"
#include "libs/tjpgd/tjpgd.h"

#include <string.h>

#define CAMERA_JPEG_WORK_SIZE 4096
#define CAMERA_JPEG_MAX_PIXELS (1280 * 960)

typedef struct {
    const uint8_t *jpeg;
    size_t size;
    size_t position;
    uint16_t *pixels;
    int width;
} camera_jpeg_input_t;


static size_t camera_jpeg_input(
    JDEC *decoder,
    uint8_t *buffer,
    size_t requested)
{
    camera_jpeg_input_t *input = (camera_jpeg_input_t *)decoder->device;
    if (!input || input->position > input->size) return 0;
    size_t available = input->size - input->position;
    size_t count = requested < available ? requested : available;
    if (buffer && count) memcpy(buffer, input->jpeg + input->position, count);
    input->position += count;
    return count;
}


static int camera_jpeg_output(JDEC *decoder, void *bitmap, JRECT *rect)
{
    camera_jpeg_input_t *input = (camera_jpeg_input_t *)decoder->device;
    const uint8_t *source = (const uint8_t *)bitmap;
    if (!input || !input->pixels || !source || !rect) return 0;
    int rect_width = (int)rect->right - (int)rect->left + 1;
    int rect_height = (int)rect->bottom - (int)rect->top + 1;
    for (int y = 0; y < rect_height; ++y) {
        for (int x = 0; x < rect_width; ++x) {
            const uint8_t *pixel = source + ((y * rect_width + x) * 3);
            input->pixels[((int)rect->top + y) * input->width + (int)rect->left + x] =
                (uint16_t)(((uint16_t)(pixel[0] >> 3) << 11) |
                           ((uint16_t)(pixel[1] >> 2) << 5) |
                           ((uint16_t)(pixel[2] >> 3)));
        }
    }
    return 1;
}


bool camera_jpeg_decode_rgb565(
    const uint8_t *jpeg,
    size_t jpeg_size,
    uint16_t **pixels,
    int *width,
    int *height)
{
    if (!jpeg || jpeg_size < 4 || !pixels || !width || !height) return false;
    *pixels = NULL;
    *width = 0;
    *height = 0;

    uint8_t *work = heap_caps_malloc(
        CAMERA_JPEG_WORK_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!work) return false;

    camera_jpeg_input_t input = {.jpeg = jpeg, .size = jpeg_size};
    JDEC decoder;
    if (jd_prepare(
            &decoder, camera_jpeg_input, work, CAMERA_JPEG_WORK_SIZE, &input) != JDR_OK ||
        decoder.width == 0 || decoder.height == 0 ||
        (size_t)decoder.width * (size_t)decoder.height > CAMERA_JPEG_MAX_PIXELS) {
        heap_caps_free(work);
        return false;
    }

    input.width = decoder.width;
    input.pixels = heap_caps_malloc(
        (size_t)decoder.width * (size_t)decoder.height * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!input.pixels) {
        input.pixels = heap_caps_malloc(
            (size_t)decoder.width * (size_t)decoder.height * sizeof(uint16_t),
            MALLOC_CAP_8BIT);
    }
    if (!input.pixels || jd_decomp(&decoder, camera_jpeg_output, 0) != JDR_OK) {
        if (input.pixels) heap_caps_free(input.pixels);
        heap_caps_free(work);
        return false;
    }

    *pixels = input.pixels;
    *width = decoder.width;
    *height = decoder.height;
    heap_caps_free(work);
    return true;
}