# LVGL modernization

Branch: `feature/lvgl95-modernization`. The project already pins LVGL 9.5.0;
this work uses its APIs more consistently. Version 6.5.6 remains the stable
release reference until a separate release is requested.

## Current scope

| Area | Implementation |
| --- | --- |
| Console and lists | Reuse Console labels, Macro buttons and Device cards; update bindings/text and hide unused rows. Slots remain bounded and are cleared on page teardown. |
| Popup layouts | Shared modal footers use Grid with up to three non-overlapping actions. Macro parameter fields use a scrollable two-column Grid and Flex cells. Nested buttons resolve their owning popup. |
| Preview sizing | Native `COVER` fills small preview wells, with a quantization guard and rounded clipping. Native `CONTAIN` fits images/fullscreen viewports while keeping proportions. The existing original-image fullscreen renderer remains. |

Lazy row creation avoids an upfront maximum-sized allocation. Pointer stores use
existing PSRAM-first lifetime policies. Objects stay alive while their page is
open, so memory remains at that page's highest displayed row count until close.
Performance and memory improvements must be measured on the ESP32-P4 panel.

## Boot splash regression follow-up

The reported boot flashing is handled with an explicit splash frame hold. After
initial UI construction the complete splash is submitted once. Background
invalidation and automatic refreshes are held through startup; application timers
continue running. Progress stages explicitly repaint only the fixed-size bar,
percentage and status regions. The Wi-Fi starting stage is called before SD/Wi-Fi
startup. Splash teardown restores invalidation and repaints the whole screen.

This suppresses repeated background redraws during boot while retaining progress. Hardware confirmation is still
required to establish that it resolves the reported flashing; the host test cannot
prove the panel-level cause. The three modernization features remain enabled.

## Preview cache cleanup

Profile cache publications invalidate LVGL resources and reuse equal-sized active
buffers. Files previews transfer completed worker buffers under the display lock
instead of duplicating pixels. The pools have explicit small-image limits and
PSRAM-only pixel allocation. Fullscreen owns its fallback and releases it when
hires arrives or the overlay closes. See [Preview rendering](PREVIEW_RENDERING.md).

## Telemetry refresh optimization

Dashboard, Printer and Telemetry live values compare against their widgets before
replacing text or local text colors. Dashboard progress writes only changed bar
values. This avoids repeated invalidation/layout and keeps rebuilt pages and
inherited theme colors correct without another shadow state store.

Shift-mode charts append through LVGL 9.5 public series-array/start-point APIs,
then refresh each shared chart once after the batch. History reloads no longer
invalidate the plot for every replayed point. Two-second sampling, ten-minute
history, duplicate/flat samples, missing-data gaps, axis scaling and rolling
extrema are preserved. See the telemetry refresh checks in [Testing](TESTING.md).

## Temporary monitor removal

The operator confirmed the preview cache cleanup and telemetry refresh changes
on the panel. The temporary Performance Settings row, popup, display event
observer and accumulators have now been removed from the firmware and build.
Their standalone documentation/tests are removed as well. The verified cache,
telemetry, preview and boot splash changes remain. The pre-existing slow-UI log
warning remains available for troubleshooting; it is independent of the monitor.

## Splash-to-chooser handoff follow-up

The chooser layout is now settled while the splash refresh hold remains active.
Teardown queues the final scene once and keeps incidental invalidations held
through that synchronous repaint, then restores normal display servicing. The
100% startup backlight is not rewritten at handoff; lower saved brightness ramps
in up to eight yielding steps afterward instead of changing abruptly.

Host checks cover both partial and double-buffered direct rendering, all three
themes/text sizes, one completed handoff frame, progress redraw bounds and
brightness values 10–100%. Panel confirmation is still required for the reported
flash; software checks cannot reproduce the MIPI/backlight effect.

## Chooser refresh follow-up

The reported flash occurs after the chooser is already visible. Its 500ms timer
now changes label text/colors only when necessary and no longer repeats preview
z-order writes or the initial refresh. Live status, preview revision and active
profile updates remain enabled. Both health-state fallback buffers remain alive
until their label write; inactive state no longer escapes a block-local buffer.
Host tests verify an unchanged chooser stays redraw-free across timer ticks in
all three themes/text sizes. Hardware confirmation remains required for the flash.

## First runtime update burst

The eight-second isolation test moved the full-screen white flash along with the
runtime workers and machine-state updates. Startup now launches those workers
while the opaque splash still holds background redraws, before its Moonraker and
Dashboard readiness stages and teardown. The initial state/preview burst can
settle behind the splash. The diagnostic delay is removed; normal runtime and
inactive-profile polling continue after handoff. The exact display-level cause
is not yet established. The operator confirmed that starting these services
behind the splash resolves the reported white flash on the panel. The temporary
eight-second isolation delay is removed.

## Saved for later

These are follow-up candidates, outside this branch:

1. Observer/data bindings for selected live labels and connection/status fields.
2. Follow-up SD cache retention and download-buffer lifetime review; rendered
   preview ownership, invalidation and pixel-pool budgets are now implemented.
3. Broader live-widget update deduplication and rolling-statistics optimization,
   if panel measurements justify them; core telemetry refresh is now optimized.
4. Targeted profiling of cache activity and page-load measurements when needed;
   the temporary runtime monitor is removed after panel validation.
5. ESP32-P4 PPA/DMA2D acceleration, evaluated after profiling and with an explicit
   software fallback; verify driver/config support and image-format limitations.

See [Testing](TESTING.md) for host checks and panel acceptance, and
[Preview rendering](PREVIEW_RENDERING.md) for crop/fullscreen behavior.
