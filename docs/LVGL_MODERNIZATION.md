# LVGL modernization

Integration scope from `feature/lvgl95-modernization`. The project pins LVGL
9.5.0; this work uses its APIs more consistently. Version 6.5.6 remains the stable
release reference. The closeout workflow merges tested work into `main` and
publishes a commit-specific nightly through `tools/end_of_night_checkpoint.sh`.

## Completed scope

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

The operator confirmed stable splash progress on the panel. Later runtime
startup changes resolved the delayed white flash, as recorded below. Host tests
cannot prove the panel-level cause. The three modernization features remain enabled.

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
brightness values 10–100%. The final runtime-startup correction below was
confirmed on the panel; software checks cannot reproduce the MIPI/backlight effect.

## Chooser refresh follow-up

The reported flash occurs after the chooser is already visible. Its 500ms timer
now changes label text/colors only when necessary and no longer repeats preview
z-order writes or the initial refresh. Live status, preview revision and active
profile updates remain enabled. Both health-state fallback buffers remain alive
until their label write; inactive state no longer escapes a block-local buffer.
Host tests verify an unchanged chooser stays redraw-free across timer ticks in
all three themes/text sizes. The final runtime-startup correction below was
confirmed on the panel.

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
2. Broader live-widget update deduplication and rolling-statistics optimization,
   if panel measurements justify them; core telemetry refresh is now optimized.
3. Targeted profiling of cache activity and page-load measurements when needed;
   the temporary runtime monitor is removed after panel validation.
4. ESP32-P4 PPA/DMA2D acceleration, evaluated after profiling and with an explicit
   software fallback; verify driver/config support and image-format limitations.

See [Testing](TESTING.md) for host checks and panel acceptance, and
[Preview rendering](PREVIEW_RENDERING.md) for crop/fullscreen behavior.

## SD retention and download-buffer follow-up

File-preview SD caches now use bounded write-age retention, serialized I/O and
staged replacement. Completed HTTP PNG buffers are trimmed to their received
size when PSRAM realloc succeeds. Profile startup previews and the verified
worker startup under the splash are unchanged. See [Preview rendering](PREVIEW_RENDERING.md)
for limits and error behavior, and [Testing](TESTING.md) for host/panel checks.

## Closeout validation

The operator confirmed the modernization, preview ownership, telemetry refresh,
monitor removal, runtime-startup flash correction and SD/download cleanup on the
panel. Host checks cover layouts/row reuse, preview fill/ownership, telemetry,
splash hold/handoff, chooser refresh and storage/download failure paths. No
quantified performance gain or completed release soak test is inferred from this
acceptance. The canonical IDF6 build runs again on integrated `main` as part of
nightly publication. Stable version and stable release assets remain unchanged.

## Status-label observer pilot

The observer integration scope originated on `feature/lvgl95-status-observers`. The shared
Dashboard/Printer status banner binds its state and operator-message/active-file
labels using LVGL 9.5 string subjects and `lv_label_bind_text`. Subjects are
owned by each banner, publish only on the UI/display-lock path, and include
previous-value buffers so repeated values do not notify observers. Transport,
refresh cadence, colors, progress, layout and previews retain their existing
behavior. This is a binding/lifecycle pilot, not a measured performance claim.

Two current/previous buffer pairs use 640 fixed bytes per banner, plus two
subjects and observer bookkeeping. There are no extra timers or global model
registries. Deleting the banner deinitializes subjects before freeing their
storage; deleting a child label removes its object observer automatically.
Values exceeding the bounded subject storage preserve their complete text via
the existing direct setter. Missing bindings also fall back to direct updates.
A return to short text, or a compatibility writer touching the label, is repaired
even if the subject value is unchanged. Further labels can adopt bindings after
panel validation of this pair. Reconnect recovery remains deferred. Version 6.5.6
is unchanged.

## Chooser status subjects

The operator confirmed the banner observer pilot on the panel. The follow-up
adds one object-bound string subject to each
chooser card's status label. The existing health/freshness, active-profile, color
and preview decisions remain authoritative; only status text publication adopts
the observer pattern. Current/previous buffers use 128 fixed bytes per card,
plus its subject/observer bookkeeping (at most four cards).

Card deletion deinitializes the subject before label destruction or static card
storage reuse. Unavailable bindings and unusually long status text preserve the
direct setter path. Existing 500ms polling remains, but unchanged status values
do not notify observers or redraw labels. The chooser's complete unchanged-tick
redraw/flush checks still pass. Startup worker order and preview behavior are
unchanged. The operator confirmed this chooser status extension on the panel.

## Chooser profile-text subjects

The profile-text extension binds each card's printer name and endpoint in
addition to its status. Configured profile edits, clearing a profile, and adding
a printer to an empty slot publish into the existing card's string subjects.
Click handlers still resolve the current profile index. Names use the profile
name capacity; endpoints use host capacity plus 16 bytes for the port/separator.
Current/previous buffers add 224 fixed bytes per card with current config limits,
plus two subjects and their object observers.

The card DELETE callback deinitializes all three subjects before slot reuse.
Binding failure and long-text fallback retain the existing complete text. Name
and endpoint formatting, dot overflow, status colors, previews and the 500ms
timer remain. The host no-redraw/no-flush and notification checks cover the
complete name/endpoint/status trio, edits, empty-slot transitions and click
routing. The operator confirmed the name/endpoint extension on the panel. Stable
version remains 6.5.6 and reconnect recovery remains deferred.

## Status observer closeout

The operator confirmed all three increments: shared banner state/message,
chooser status, and chooser name/endpoint bindings. Host checks verify unchanged
notifications/refreshes, theme/text-size variants, profile edits, empty slots,
click routing, fallback repair and subject cleanup. The closeout workflow audits
the complete integration, merges into main and builds/publishes a commit-specific
nightly through `tools/end_of_night_checkpoint.sh`. Stable version remains 6.5.6.
No quantified target performance gain or completed soak test is inferred.
Reconnect recovery and asynchronous metadata fetching remain deferred.
