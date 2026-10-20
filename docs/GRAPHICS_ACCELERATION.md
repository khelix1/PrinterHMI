# Graphics acceleration review

## Current accepted build default

After the cache-handoff fix, the user confirmed Dashboard black blocks were gone and Files scrolling was fast and smooth. The supported `tools/build_idf6_hosted3.sh` wrapper now defaults to the verified PPA fill-only configuration, including both guarded LVGL corrections. PPA image drawing and port rotation remain disabled; version 7.0.0-dev, direct mode, double buffering and LCD timing are unchanged. Physical Camera performance remains untested. Earlier trial/default statements below are historical.

Run `./tools/build_idf6_hosted3.sh` to build, or `./tools/build_idf6_hosted3.sh flash monitor` to build, flash and monitor using the same selected SDKCONFIG. Use `PRINTERHMI_PPA_TRIAL=0 ./tools/build_idf6_hosted3.sh flash monitor` for the explicit software fallback. An unset variable defaults to PPA; an existing exported value of 0 still deliberately selects software. The wrapper prints the renderer chosen.

For compatibility the generated PPA config keeps its existing name `build-idf6-hosted3/sdkconfig.ppa-trial` and `PRINTERHMI_PPA_TRIAL=1` remains valid. The tracked sdkconfig.idf6 remains the software seed/fallback and is not rewritten by promotion. Direct IDF commands that explicitly select sdkconfig.idf6 therefore select software. Prefer the wrapper for building/flashing; it applies both dependency fixes and prevents configuration mismatch. Generated config is verified after reconfigure and recreated from the software seed on each PPA build.

### Diagnostic cleanup

Normal builds retain bounded startup `graphics_diag: renderer mode=... ppa=1 reports=0` and buffer alignment details. Periodic render timing/heap reports are off by default, with no reporting timer or event clock reads. This does not change PPA settings, cache fixes or Files rendering.

For troubleshooting, use `idf.py -B build-idf6-hosted3 -D SDKCONFIG=sdkconfig.idf6 menuconfig`, then enable PrinterHMI diagnostics > Periodic graphics timing and heap reports. The normal wrapper copies this option from the tracked software seed into the generated PPA config. Rebuild/flash with `./tools/build_idf6_hosted3.sh flash monitor`; startup shows reports=1 and active windows log every ten seconds. Disable the option in that same seed menu and rebuild to return to quiet operation. Existing timing logs below are historical measurements taken with reporting enabled.

## Historical review and trial sequence

PrinterHMI 7.0.0-dev, feature/profile-setup-dialogs. Reviewed against the checked-in BSP/port and pinned LVGL 9.5.0 sources. No accelerator settings are changed by the camera polish patch.

## Verified current path

- BSP `components/espressif__esp32_p4_function_ev_board/esp32_p4_function_ev_board.c` already calls `esp_lcd_dpi_panel_enable_dma2d(disp_panel)` during JD9165 initialization. DMA2D display support is already enabled.
- The supported build script explicitly uses `sdkconfig.idf6`; changing only `sdkconfig` does not change that firmware. Both currently use RGB565, direct mode, tear avoidance and two DPI framebuffers. The port's DSI tear-avoidance branch in `components/espressif__esp_lvgl_port/src/lvgl9/esp_lvgl_port_disp.c` obtains both full-screen driver framebuffers and overrides the nominal 50-line draw-buffer setting. Do not infer a partial/single-buffer runtime from the macros used by main.c.
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

## Baseline patch before the PPA trial

`apply_graphics_baseline_diagnostics.py` follows checkpoint b4d4c40. It changes no SDK configuration, BSP, buffer ownership, LCD timing or renderer. Under the existing display lock, it attaches one bounded event observer and a ten-second LVGL reporting timer. No overlay or extra invalidation is introduced. Duplicate attachment is harmless; display deletion retires its timer.

Serial `graphics_diag` lines report effective display mode, resolution, format, configured alignment/cache line size and PPA configuration. Up to three distinct observed active buffer addresses are logged with dimensions, stride, data size and address/size remainders modulo 64. With double buffering, both addresses should appear after rendering resumes. A zero remainder checks that buffer only, not every temporary LVGL layer.

Active windows report completed damaged render cycles, mean/max cycle duration, cumulative flush-callback time, free/largest internal heap and free SPIRAM. The RENDER_START-to-RENDER_READY interval includes synchronous flush calls and any waits inside them. FLUSH_FINISH only means the callback returned, not that asynchronous transfer or physical scanout finished. These values are not CPU-only rendering time, panel FPS, touch latency or proof that PPA executed. Windows with no damaged cycles produce no stats line. Serial logging itself adds a small, equal measurement cost to later comparisons.

Exercise the chooser, manager/Edit printer scrolling, camera setup and page transitions separately for about 15 seconds each, first without and then with streaming. Record the same theme, density, large-text and stream conditions for the eventual comparison. Confirm boot splash/backlight and navigation are unchanged. Return the startup/buffer lines and representative active-window lines.

### PPA prerequisite found in the pinned source

LVGL 9.5.0's `src/draw/lv_draw_buf.c` does not round `_calculate_draw_buf_size()` up to `LV_DRAW_BUF_ALIGN`. Internal layers with arbitrary dimensions can therefore have aligned starts but unaligned lengths. The PPA fill/cache paths use that length. Upstream [issue 9868](https://github.com/lvgl/lvgl/issues/9868) reports this exact failure and links its fix. Do not enable PPA merely by changing alignment: the trial must also verify or apply the narrowly scoped upstream buffer-size correction and test temporary layers. Keep the pinned version and existing LCD path; do not hide cache errors by passing an unaligned-memory flag.

## Reversible fill-only PPA trial

The captured device baseline confirms 1024x600 direct RGB565 rendering, two 1,228,800-byte framebuffers, 2,048-byte strides and zero address/size remainders modulo 64. Observed buffers were 0x48303a40 and 0x4842fa80; addresses can change on later boots. The five supplied windows averaged 11.5–15.6 ms per damaged render cycle, with a 221.5 ms maximum; workload windows were not individually labelled.

Apply `apply_ppa_fill_trial.py` after the graphics baseline patch, then run:

```sh
PRINTERHMI_PPA_TRIAL=1 ./tools/build_idf6_hosted3.sh
```

The wrapper creates `build-idf6-hosted3/sdkconfig.ppa-trial` from the untouched `sdkconfig.idf6`, enabling PPA fills, leaving PPA images/rotation disabled, using 64-byte alignment and a 128-byte PPA burst. It verifies the actual reconfigured options, applies the exact-hash guarded upstream LVGL buffer-length correction in managed_components, then builds. BSP, LCD timing, direct mode, two framebuffers, software renderer and version pins remain fixed. No performance gain is assumed.

Use the same firmware output/flash/OTA path as normal. The startup diagnostics must report `ppa=1 draw_align=64` (fields may be separated). Repeat the baseline interactions and themes, including streaming. Capture representative labelled ten-second windows, new buffer reports and any `PPA fill failed`, cache alignment, panic, flashing, missing-draw or touch problems. Keep the diagnostics unchanged for comparison. Stop the trial if those appear; send the logs.

To return to the software renderer, run `PRINTERHMI_PPA_TRIAL=0 ./tools/build_idf6_hosted3.sh` and flash again. The ordinary wrapper also defaults to this configuration. Reconfigure switches the same build directory back to `sdkconfig.idf6`. The small LVGL allocation-length correction remains in the generated dependency, also with the normal renderer; no application settings are restored or discarded. Regenerated managed components receive the correction again on the next trial build. Use the wrapper for trial builds so component restoration cannot bypass its check.

For serial monitoring of a trial build:

```sh
idf.py -B build-idf6-hosted3 -D SDKCONFIG=build-idf6-hosted3/sdkconfig.ppa-trial monitor
```

The initial graphics log label remains `baseline mode=` for both builds; `ppa=0/1` and `draw_align=4/64` identify the renderer. Cycle times include flush callbacks and do not equal CPU-only render times, FPS or confirmed accelerated-task counts. Physical panel acceptance and comparison remain pending.

## PPA Dashboard artifacts: cache handoff backport

The first PPA trial produced black blocks around Dashboard speed/flow/fan/drybox/air/humidity and occasionally temperature values. The user confirmed these disappeared after rebuilding/flashing with `ppa=0`. Keep PPA opt-in; physical Camera performance remains untested. Rediscovering/saving a printer corrected a separate reported temperature mismatch; its underlying cause is not established by this graphics change.

The pinned 9.5.0 PPA cache handler calls C2M writeback even after hardware has written the draw buffer. Its dispatcher also permits PPA to run while another draw unit has work in progress in the same layer. Upstream [PR 10657](https://github.com/lvgl/lvgl/pull/10657), also recorded in the LVGL 9.6 changelog as afba8ce, addresses this cache handoff. `apply_ppa_cache_handoff_fix.py` backports the relevant changes to pinned 9.5.0 without a dependency upgrade: flush cache to memory before PPA, invalidate memory to cache after PPA, use buffer-relative whole-row cache-line-aligned ranges and defer PPA while another unit is writing the layer. The pinned fill/image code has no software fallback; images remain disabled by the trial configuration. Buffer-length rounding from the earlier patch remains required.

ESP-IDF 6.0's memory synchronization documentation distinguishes CPU-to-memory writeback before DMA from memory-to-CPU invalidation after DMA. The old handler used the former for both. This is a source-confirmed defect and a plausible explanation for the observed blocks, not confirmation that every panel artifact is resolved. The new row ranges also avoid syncing the entire framebuffer for every small fill. No unaligned-cache flag is introduced and no cache range is expanded outside the aligned allocated buffer.

The wrapper applies both exact-source guarded LVGL corrections only on explicit PPA builds. Software rendering remains the default and rollback. Capture the build PASS line for the cache handoff/serialization correction, then confirm `ppa=1 draw_align=64` after flashing. Watch Dashboard values update for at least 30–60 seconds, navigate Dashboard/Printer/Files, switch themes and check the same blocks. Do not alter heater/fan targets merely to test rendering. If blocks recur, capture the log and return to `PRINTERHMI_PPA_TRIAL=0` plus flashing with SDKCONFIG=sdkconfig.idf6.

Host `ppa_cache_coherency_test.py` compiles the actual backported dispatcher/cache source with real LVGL types and a deterministic ESP cache/DMA model. It checks ordered C2M/PPA/M2C handoff, clipped/nonzero layer origins, row/cache-line bounds, adjacent CPU glyph/pixel preservation and exclusion of in-progress software drawing. The original pinned code fails the same regression assertion. These tests do not emulate the physical ESP cache, DMA timing or DSI panel; hardware acceptance remains pending.
