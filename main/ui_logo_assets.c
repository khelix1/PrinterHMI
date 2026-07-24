#include "ui_logo_assets.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "assets/logo/printerhmi-splash.inc"

const lv_image_dsc_t *ui_logo_assets_splash(void)
{
    static lv_image_dsc_t descriptor;
    static bool initialized = false;

    if (!initialized) {
        memset(&descriptor, 0, sizeof(descriptor));
#if defined(LV_IMAGE_HEADER_MAGIC)
        descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
        descriptor.header.cf = LV_COLOR_FORMAT_RAW;
        descriptor.data = printerhmi_splash_png;
        descriptor.data_size = printerhmi_splash_png_len;
        initialized = true;
    }

    return &descriptor;
}
