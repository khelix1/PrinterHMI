# PrinterHMI ESP LVGL port customization

This local component preserves a compatibility adjustment required by the
PrinterHMI LVGL dependency.

Upstream base: `espressif/esp_lvgl_port` 2.8.0~1.

Intentional change:

- Removed references to `LV_COLOR_FORMAT_RGB565_SWAPPED`, which is unavailable
  in the selected LVGL 9.2.x API. RGB565 and the existing `swap_bytes` flag
  remain the supported path.

Recheck this patch whenever LVGL or esp_lvgl_port is upgraded.
