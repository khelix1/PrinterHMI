# PrinterHMI ESP LVGL port customization

Upstream base: `espressif/esp_lvgl_port` 2.8.0~1. PrinterHMI pins LVGL 9.5.0;
this directory is a local override, not an unmodified registry download.

Intentional local behavior:

- The compatibility change removes `LV_COLOR_FORMAT_RGB565_SWAPPED` references;
  RGB565 and the existing `swap_bytes` flag remain the supported path. The old
  9.2.x wording described the original patch checkpoint, not the current pin.
- Display allocations honor `CONFIG_LV_DRAW_BUF_ALIGN`. The supported wrapper
  selects the accepted PPA fill-only configuration and applies the separate
  guarded LVGL alignment/cache corrections.
- The LVGL9 touch adapter forwards bounded touch coordinates through the weak
  `printerhmi_bed_mesh_multitouch_update` hook. Bed Mesh owns gesture interpretation;
  this port does not enable the parked G-code viewer.

See [Building](../../docs/BUILDING.md),
[Graphics acceleration](../../docs/GRAPHICS_ACCELERATION.md) and
[Hardware](../../docs/HARDWARE.md) for the application configuration. Generic
upstream examples and benchmarks in this directory are not PrinterHMI settings
or measured PrinterHMI frame rates. Recheck local changes whenever upgrading
LVGL, the port or BSP; retain target display/touch/OTA regression coverage.
