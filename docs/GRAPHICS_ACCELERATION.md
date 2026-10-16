# Graphics acceleration review

PrinterHMI 7.0.0-dev, feature/profile-setup-dialogs. Reviewed against the checked-in BSP/port and pinned LVGL 9.5.0 sources. No accelerator settings are changed by the camera polish patch.

## Verified current path

- BSP `components/espressif__esp32_p4_function_ev_board/esp32_p4_function_ev_board.c` already calls `esp_lcd_dpi_panel_enable_dma2d(disp_panel)` during JD9165 initialization. DMA2D display support is already enabled.
- `sdkconfig` uses RGB565, direct mode, tear avoidance and two DPI framebuffers. The port's DSI tear-avoidance branch in `components/espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port_disp.c` obtains both full-screen driver framebuffers and overrides the nominal 50-line draw-buffer setting. Do not infer a partial/single-buffer runtime from the macros used by main.c.
- LVGL PPA drawing (`CONFIG_LV_USE_PPA`) is disabled; draw-buffer alignment is configured as 4 bytes and there is one software draw unit. Actual LCD framebuffer alignment needs runtime confirmation.
- `CONFIG_LVGL_PORT_ENABLE_PPA` is a separate screen-rotation feature. The app requests no software rotation; enabling this alone is not the proposed UI drawing acceleration trial.
- The project pins LVGL 9.5.0 and uses local esp_lvgl_port 2.8.0~1 and BSP 4.1.1 overrides. Keep those versions fixed during the first comparison.

## Proposed first hardware trial

1. Record effective draw-buffer addresses, sizes, color format, render mode and framebuffer alignment on the device. Capture a software-rendering baseline for chooser/editor scrolling, page transitions and camera streaming, with responsiveness and memory observations.
2. Enable LVGL's PPA draw unit with 64-byte draw-buffer alignment, initially leaving PPA image drawing disabled. Keep the direct/tear-avoidance/LCD timing path fixed. The existing LCD DMA2D enablement remains.
3. Compare the same interactions across the five themes, including with streaming active, and inspect underrun/artifact/heap behavior. Hardware timing cannot be established by the software-only host fixture. Provide a reversible test patch rather than treating the experiment as a release default.
4. Consider image acceleration only after the fill trial is stable and useful; the pinned backend has explicit format, transform, opacity and clipping eligibility filters. Inspect these before promising camera/preview acceleration.

The pinned PPA evaluator supports RGB565 destinations. Its fill path accepts opaque rectangular fills without rounded corners or gradients. It does not offload all typography, layout or drawing. Recent flat opaque viewport work remains useful with hardware acceleration.

## Primary references

- [LVGL 9.5 PPA support](https://lvgl.github.io/open-docs/HTML/9.5/integration/chip_vendors/espressif/hardware_accelerator_ppa.html): distinguishes the LVGL renderer from port rotation, recommends cache-line alignment/double buffering, and identifies the backend as experimental.
- [ESP-IDF ESP32-P4 PPA](https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/api-reference/peripherals/ppa.html): hardware capabilities and memory requirements.
- Pinned LVGL source: `src/draw/espressif/ppa/lv_draw_ppa.c`, `lv_draw_ppa_private.h`, `lv_draw_ppa_buf.c` and its Kconfig options. Documentation benchmark numbers are not PrinterHMI measurements.
