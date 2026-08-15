#include "camera_jpeg_decoder.h"

#include "driver/jpeg_decode.h"
#include "esp_err.h"
#include "esp_heap_caps.h"

#include <string.h>

#define CAMERA_JPEG_MAX_PIXELS (1280 * 960)

static jpeg_decoder_handle_t s_decoder = NULL;

static bool camera_jpeg_engine(void)
{
    if (s_decoder) return true;
    jpeg_decode_engine_cfg_t cfg = {.intr_priority = 0, .timeout_ms = 100};
    return jpeg_new_decoder_engine(&cfg, &s_decoder) == ESP_OK;
}

bool camera_jpeg_decode_rgb565(const uint8_t *jpeg, size_t jpeg_size,
                                uint16_t **pixels, int *width, int *height)
{
    if (!jpeg || jpeg_size < 4 || !pixels || !width || !height ||
        jpeg_size > UINT32_MAX || !camera_jpeg_engine()) return false;
    *pixels = NULL; *width = 0; *height = 0;
    jpeg_decode_picture_info_t info = {0};
    if (jpeg_decoder_get_info(jpeg, (uint32_t)jpeg_size, &info) != ESP_OK ||
        !info.width || !info.height || (size_t)info.width * info.height > CAMERA_JPEG_MAX_PIXELS) return false;
    size_t in_size = 0, out_size = 0;
    jpeg_decode_memory_alloc_cfg_t in_cfg = {.buffer_direction = JPEG_DEC_ALLOC_INPUT_BUFFER};
    jpeg_decode_memory_alloc_cfg_t out_cfg = {.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER};
    uint8_t *in = jpeg_alloc_decoder_mem(jpeg_size, &in_cfg, &in_size);
    uint8_t *out = jpeg_alloc_decoder_mem((size_t)info.width * info.height * 2, &out_cfg, &out_size);
    if (!in || !out) { if (in) heap_caps_free(in); if (out) heap_caps_free(out); return false; }
    memcpy(in, jpeg, jpeg_size);
    jpeg_decode_cfg_t cfg = {.output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
                             .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
                             .conv_std = JPEG_YUV_RGB_CONV_STD_BT601};
    uint32_t written = 0;
    bool ok = jpeg_decoder_process(s_decoder, &cfg, in, (uint32_t)jpeg_size,
                                   out, (uint32_t)out_size, &written) == ESP_OK;
    heap_caps_free(in);
    if (!ok) { heap_caps_free(out); return false; }
    *pixels = (uint16_t *)out; *width = (int)info.width; *height = (int)info.height;
    return true;
}
