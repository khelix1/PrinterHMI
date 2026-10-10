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

## Devices and Drybox refresh follow-up

Devices live labels and Drybox data widgets now compare against current widget
text, local colors, button border/fill properties and instantaneous bar values.
Equal readings avoid repeated writes without a detached state cache. Catalog
rebuilds, page teardown, profile changes and theme/accessibility rebuilding retain
their existing behavior. Drybox online updates restore the humidity value's cyan
color after the offline placeholder. Commands and refresh intervals are unchanged.

The real-LVGL fixture covers silent repeated data refreshes, changed readings,
humidity thresholds/clamping, program selection and disabled states, offline
recovery and rebuilt labels in all three themes/text sizes. Shared status-banner
styling is outside this patch and excluded from its redraw-free data-widget claim.
The operator confirmed these functional changes on the panel.

## Shared banner refresh follow-up

Shared banners skip equal ETA/progress text and local color writes. The theme
dispatcher compares the final local banner properties before invoking the existing
concrete theme recipe; token, custom-opacity and accessibility changes remain
visible, and external style changes are repaired. No kind/theme cache is retained.
Simple banners compare their message width/alignment as well. Progress animation
continues to use LVGL's existing bar setter and motion preference.

Drybox supplies its final shell status once through the simple-kind setter,
retaining its previous state/accent text semantics while avoiding an intermediate
shell style. The earlier data-widget test now includes the real shared banner
and covers all four themes. Banner tests compare all locally written properties
against the unchanged concrete recipes and verify unchanged refresh silence.

## Operator preview cutoff follow-up

Small preview consumers now use aspect-fit cache rendering and native `CONTAIN`
with zero inset. This replaces the earlier two-stage center crop/`COVER` path
after the operator reported cut-off previews. All themes share the same complete
model policy; mismatched proportions leave unused space. Cache sizes and original
fullscreen rendering remain unchanged. See [Preview rendering](PREVIEW_RENDERING.md).

## Tools live refresh follow-up

Live Endstops compares text against its current label before updating. Motion
limits and driver diagnostics compare local status colors and driver dropdown
options; profile-change messages render directly without an intermediate offline
message on every tick. Driver discovery, selected-name restoration and theme-
specific fault colors retain their previous behavior. No timers, query intervals,
G-code commands or readiness guards change.

Real-LVGL checks verify repeated refresh silence, changed limits/endstop readings,
discovery and no-driver transitions, selection after driver reordering, warning/
fault palettes, offline/print/profile guards, two-second query cadence and cleanup
in all four themes and text sizes. The operator confirmed these functional changes on the panel.

## Live-widget refresh closeout

The operator accepted Devices/Drybox data refreshes, shared banner refreshes,
the complete-model preview correction and Tools live diagnostic refreshes. The
preview policy now uses aspect fit at both cache and viewport stages, replacing
the earlier crop-to-fill behavior. Host checks cover redraw silence, transition
correctness, themes/accessibility, preview/cache lifetimes, query timing and
existing command guards. No measured target performance gain or completed soak
test is inferred.

The closeout workflow commits the tested scope and documentation, checkpoints
`feature/live-widget-refresh`, reviews the integration in a disposable worktree,
merges to `main`, and builds/publishes the nightly through
`tools/end_of_night_checkpoint.sh`. The branch is removed after success. Stable
version remains 6.5.6. Reconnect recovery and asynchronous metadata remain deferred.

## Aspect-aware preview canvases

`feature/preview-camera-layout` begins from the accepted live-widget closeout.
First increment: tightly packed, complete-source preview canvases with actual
dimensions carried through profile/Files descriptors and Dashboard restoration.
Maximum allocation capacities remain unchanged. Active Print/Dashboard reused
canvases rebind dimensions and stride. Worker cache publication remains under
the display lock, and stale profile jobs are rejected before modifying pixels.
Native `CONTAIN` fits the packed image without cropping.

Preview hardware validation comes first, followed by the camera pipeline audit
and remaining responsive-layout work. Reconnect recovery and asynchronous
metadata remain deferred. Stable version remains 6.5.6.

## Camera pipeline audit

The operator confirmed the aspect-aware preview increment. Camera follow-up:
`ui_camera` now detaches the live image source and invalidates its cache before
freeing frame pixels or clearing the descriptor. Hide releases the detached frame
after retiring the stream consumer. Incoming RGB565 dimensions and byte capacity are
validated before mirroring/rendering. Reused status/selector text and unchanged
view transforms skip repeated writes; fullscreen changes transform the current
frame immediately rather than waiting for another frame. The unused private
decoder header is removed.

The stream controller publishes a bounded handoff. The offline-navigation
follow-up below separates UI retirement from worker/transport shutdown. The
JPEG decoder remains unchanged.
Mirror operations still run once per fresh frame. View-setting changes reset
frame-age/FPS counters. Real-LVGL checks exercise source retirement before free,
rotations/mirrors, repeated update silence, malformed frames, viewport changes
and hide/destroy in four themes/text sizes. This is correctness/lifecycle work,
not a measured FPS gain. Responsive-layout work follows panel acceptance.


## Offline navigation follow-up

An offline printer exposed transport waits on the LVGL owner: every sidebar
transition called the waiting camera stop, profile selection stopped/destroyed
the WebSocket inline, and Files performed up to three HTTP retries in an LVGL
timer. Navigation now requests camera retirement without waiting. Retired/late
frames are discarded; a replacement worker starts only after the old worker
finishes. Camera retry backoff checks cancellation in 50 ms slices. The explicit
waiting stop remains for OTA/setup transport quiescence.

Profile selection immediately fences old WebSocket events, freshness, commands
and notifications. The runtime task owns stop/destroy/recreate, checks current
configuration generations before rebind, and abandons snapshots superseded
during teardown. Dashboard also detaches its frame/cache before freeing pixels.
Files loads copy endpoint credentials into one background job, coalesce one
latest pending job, and publish only on LVGL for the current generation/request/
page. A confirmation popup defers publication. HTTP waits/retries never run on
the UI owner; discarded jobs release their buffers. The stable version remains
6.5.6. Hardware offline/recovery testing is still required.


## Responsive Tools, Macros and Files

The next increment on `feature/preview-camera-layout` uses native LVGL flex/grid
layouts on three fixed surfaces. Tools tiles share the current container width,
wrap to one column when necessary, and size their title/body from real text.
Reopening rebuilds the small Tools page so runtime theme/density/font changes
replace previously applied local styles.
Macros use equal-width wrapping rows, content-height buttons, and wrapped names;
search, favorites, parameter dialogs and generation guards retain their owners.
Files actions occupy a separate wrapping toolbar in the existing theme/profile
order. Its breadcrumb and remaining list viewport flow below the header. File
and folder rows follow the viewport width and resolve label space before the
navigation arrow; cached preview buffers and preview-fit policy do not change.
Loading/empty/offline state content uses a centered column so title/detail text
cannot overlap as fonts grow.

Real-LVGL checks exercise all four built-in themes, three density settings,
large text on/off, 854/640/480 px page widths, and representative custom metric/
profile overrides. They cover control/label bounds, non-overlap, wrapped macro
names, file/folder rows, state overlays, favorites/empty states and lifecycle.
Rendered host surfaces for every built-in theme were inspected. This validates
the changed surfaces, not every screen or arbitrary custom theme. The operator accepted this responsive pass. Canonical integration build and
publication run through the closeout below; stable version stays 6.5.6.

## Preview, camera and responsive-layout closeout

The operator accepted aspect-aware print previews and the responsive Tools,
Macros and Files pass. After a printer went offline and navigation slowed, the
offline-navigation follow-up removed camera/WebSocket teardown waits and Files
HTTP retries from the LVGL owner; the operator reported faster navigation. A
power cycle restored the printers earlier, so that recovery alone is not proof
of a completed offline/reconnect soak. Camera lifetime/transform checks and
offline transport ownership were validated with host fixtures; the combined
follow-up was built/flashed during the operator's incremental workflow.

Responsive host checks cover Classic, Operator, Glass and Operator Shell, all
three densities, large text on/off, three widths and representative custom
metric/profile overrides. Rendered host surfaces were inspected; the operator
accepted the resulting responsive pass. No measured throughput/FPS improvement
or target soak is claimed. Asynchronous file-list loading is completed; other
metadata work remains deferred.

Closeout commits the accepted scope and documentation, checkpoints
`feature/preview-camera-layout`, reviews integration in a disposable worktree,
merges main, and runs `tools/end_of_night_checkpoint.sh` with nightly publication
enabled. Remote main, nightly firmware and checksum must verify before the
merged feature branch is removed. Stable version remains 6.5.6.

## Responsive temperature and fan controls

Temperature and part-fan preset dialogs now use a native column with a pinned
header/footer and a separately scrolling body. Current/target cards and preset
buttons use content-sized Grid rows with one/two and one/two/three columns
selected from available width. Captions wrap within their cards; target taps
continue to open the custom-temperature editor. The editor uses a column for
its range hint, numeric input and keyboard; Back/Set stay visible below it.
Modal dimensions are bounded by the current display. Hotend-specific commands,
presets, selected borders, Off, range validation and keyboard Ready/Cancel keep
their previous behavior. Opening another control modal retires the previous
preset/custom pair. No changes are made to filament recovery or printer macros.

`tools/audit/control_popups_layout_test.py` compiles the actual production popup
code against LVGL 9.5 and checks four themes, all densities, large text,
1024/640/480 px display widths, short-height scrolling, pinned footer bounds,
compact card heights, preset/custom/Off commands, invalid input, keyboard events,
hotend prefixes and teardown. Host renders were inspected. Target build and
panel validation remain required; stable version stays 6.5.6.

## Responsive Motion diagnostics

Motion Limits, TMC Driver Diagnostics and Axis Distance Calculator now use
native columns with a separately scrolling body and a visible footer. Limits
use two-column content-height rows. Calculator labels/inputs use Grid cells
with wrapped captions; the measurement editor fits the current display and
keeps Done/Cancel outside its scrollable input/keyboard area. Runtime refresh,
driver selection restoration, palettes, calculation rules and generation
guards retain their previous behavior. Popup/editor delete callbacks retire
LVGL references and the existing refresh timer on external deletion as well as
normal close. The permanent PSRAM-first state allocation remains unchanged.

`tools/audit/motion_layout_test.py` uses production UI code and extracts the
unchanged production calculation functions for host linking. It checks all four
themes/densities/text sizes, 1024/640/480 px widths, short viewports, label/field
and footer bounds, scroll isolation, belt/leadscrew results, invalid input,
editor completion, driver faults/offline/profile changes and deletion/reopen.
Existing Tools refresh-silence checks pass. Host renders were inspected. Target
build and panel validation remain required; stable version stays 6.5.6.

## Responsive filament sensors and printer status

Filament sensor controls now use content-height Grid rows in a scrolling body,
with name/type and live status labels wrapping inside their cells. The Close
action remains outside the scrolling list. Rows bind to full sensor object
names, so reordered discovery cannot redirect a toggle. Missing sensors and
changed endpoint generations disable the corresponding actions; additions are
shown after reopening. Existing sensor commands, five-second pending timeout,
status meanings and palettes are retained. Equal text/color/button states avoid
repeated invalidation. The bounded four-row store and existing 500 ms timer
are cleared on deletion.

The shared Dashboard status modal used by Printer Status and other status
messages now has a wrapping title and scrollable message body with pinned Close.
Its dimensions are bounded by the display; external deletion clears its owner
pointer. Printer Status includes the full supported 255-character file path
instead of clipping it at 120 characters. Status remains a snapshot taken on
open; no polling or transport work is added.

`tools/audit/sensor_status_layout_test.py` builds the production sensor UI and
extracts the production shared-status functions for real-LVGL host checks. It
covers four themes, all densities/text sizes, 1024/640/480 px widths, short
viewports, long sensor names and files, footer bounds under scroll, refresh
silence, commands, pending acknowledgement/timeout, discovery reorder/removal,
profile fencing, unsafe names and external deletion/reopen. Temperature/fan and
Motion layout regressions pass. Host renders were inspected; target build and
panel validation remain required. Stable version stays 6.5.6.

## Responsive hotend selection and activation

Hotend selection uses content-height Grid rows inside a scrolling body. Names
and temperature readouts wrap; actions move below the readouts when the viewport
is narrow. Close stays outside the list. Activation confirmation uses a wrapped
message and pinned Back/Activate actions. Both dialogs fit the current display
and retain the theme palette, active indication and four-hotend limit.

Rows bind to full hotend object names. Discovery reordering cannot redirect a
temperature or activation command. Offline data, removed objects and endpoint
changes disable actions; new discoveries appear on reopen. Activation rechecks
fresh state at confirmation and remains blocked while printing or paused.
Hotend temperature, bed temperature and fan speed remain adjustable during
printing and pause, including named hotend presets and custom temperatures.
No toolchanger motion commands are introduced. Delete callbacks clear dialog
references and retire the existing 500 ms list timer; equal samples avoid
redrawing unchanged labels.

`tools/audit/hotend_layout_test.py` builds production popup code with real LVGL
9.5. It checks four themes, all densities/text sizes, 1024/640/480 px widths,
short viewports, long names, row/footer bounds, scroll isolation, unchanged
refresh, active indication, named commands, print/pause controls, confirmation
guards, discovery reorder/removal and deletion/reopen. Temperature/fan and
sensor/status regressions pass. Host renders were inspected; target build and
panel validation remain required. Stable version stays 6.5.6.

## Responsive cancel-object dialogs

Cancel Object now has wrapping status text and a native Grid map/list inside a
scrolling body. Wide layouts place map and list side by side; narrow layouts
stack them. The object list keeps its own vertical scroll and uses content-height
rows with full supported 95-character names, including Current/Excluded markers.
Close and Exclude stay outside scrolling content. Confirmation shows the full
captured name in a wrapping body with pinned Back/Exclude actions. Map drawing,
hit testing, theme palettes and the 48-object limit are retained.

Repeated opening retains the existing snapshot and row references. Delete
callbacks clear the map/rows/dialog owners and free the PSRAM-first snapshot on
external deletion as well as normal close. Confirmation binds its own name and
rechecks the current exclusion snapshot and endpoint generation before sending
`EXCLUDE_OBJECT NAME=...`; reordered discovery or later selection cannot redirect
it. Missing, already excluded and unavailable objects do not send a command.
No new polling timer is added. Whole-print cancellation and temperature/fan
controls retain their previous behavior.

`tools/audit/object_layout_test.py` builds production popup code against real
LVGL 9.5. It checks four themes, all densities/text sizes, 1024/640/480 px widths,
short viewports, 48 long-name rows, map hit testing, independent scroll/pinned
actions, exact captured commands, Back, reorder/removal/excluded/profile guards,
repeat-open and external teardown. Hotend, temperature/fan and sensor/status
regressions pass. Host renders were inspected; target build and panel validation
remain required. Stable version stays 6.5.6.

## Responsive whole-print cancellation

The whole-print cancellation confirmation now fits the current display with a
native column layout: wrapping title, separately scrolling warning body and
pinned Back/Cancel actions. It retains the danger palette, warning text and
existing `CANCEL_PRINT` command. No transport, timer or activation guard is
added. A delete callback clears the dialog owner on external deletion as well
as normal close; repeated opening raises the existing dialog.

`tools/audit/cancel_layout_test.py` builds the production popup module with real
LVGL 9.5 and checks four themes, every density/text size, 1024/640/480 px widths,
a short viewport, wrapping/overflow scroll, footer bounds, Back without a
command, exactly one `CANCEL_PRINT`, repeated opening and deletion/reopen.
Hotend, temperature/fan, object and sensor/status regressions pass. Host renders
were inspected; target build and panel validation remain required. Stable
version stays 6.5.6.

## Control-dialog closeout

Filament recovery and responsive temperature/fan, Motion, sensor/status, hotend
and cancellation dialogs are included in the development nightly. Host checks
and incremental layout review are complete; physical printing/recovery checks
remain pending. Stable version stays 6.5.6. Settings Time Zone/Reset layout work
is deferred until after this closeout. See [Nightly validation](NIGHTLY_VALIDATION.md).

## Responsive Time Zone and Reset Settings dialogs

Time Zone now uses content-height wrapping selectable rows in a native column
body with fixed Close. Names/abbreviations are formatted directly into LVGL
labels without a fixed 96-byte intermediate buffer. Selected styling, original
preset indices, failed-save behavior and change callbacks are preserved. Reset
Settings uses a separately scrolling wrapped warning with pinned Cancel/Erase;
the warning and existing NVS erase, 300 ms delay and reboot sequence are
unchanged. Both dialogs fit the viewport and clear their owners on external
as well as normal deletion. No timers or transport work are added. Interface
Theme/custom-theme dialogs retain their current layout.

`tools/audit/settings_dialog_layout_test.py` compiles production Settings popup
code against real LVGL 9.5 with platform persistence/reboot stubs. It checks all
four themes, densities/text sizes, 1024/640/480 px widths and a short viewport,
long labels, footer bounds under scroll, timezone command identity and callbacks,
failed saves, missing-entry indices, reset Cancel without erase, the stubbed
reset sequence, repeated opening and external deletion/reopen. Host renders
were inspected; target build and panel validation remain required. Earlier
nightly printing/recovery checks remain pending. Stable version stays 6.5.6.

## Full-size button text and Settings rows

Shared action labels wrap within their visible buttons without reducing the
accessibility font. Content-sized buttons retain natural label widths; horizontal
icon buttons reserve the icon/gap and let text use the remaining width. The
legacy widget constructor uses the same label policy. Shared actions explicitly
reserve modest padding instead of inheriting LVGL's oversized default padding.

Settings action rows measure button width and use a wrapping text column. Value
and slider rows reserve separate text/value space. Their row-height tokens also
own section and divider spacing. Navigation allows two-line names with matching
row height and stride. Camera and Console actions, Devices telemetry/pagination,
printer management, setup discovery, backup actions, bed-mesh controls and
Calibration actions have updated bounds. Bed actions use at most two columns;
probe actions and encrypted backup restore have room for two lines. Toolhead Z
offset actions use wider buttons in two rows while retaining their exact values.
Native popup footers measure full text and grow row height when wrapping is
needed rather than abbreviating action names with dots.

`tools/audit/button_text_fit_test.py` measures source-derived fixed actions with
real LVGL 9.5 in all stock themes/densities/text sizes. It checks full-label
containment, icon/text separation, actual Settings/Printer/Dashboard groups,
value-row separation, navigation, every bed capability combination, dynamic
Console/fullscreen labels, native footer wrapping and content/bounded sizing.
Existing dialog and responsive checks remain required. Panel verification and
printing-dependent recovery/control checks remain pending. Version stays 6.5.6.

## Single-line names, actions and values

Page names, one-word actions, Toolhead Home and numeric step/offset values stay
on one line at the selected text size. Navigation keeps horizontal icons and
full-size page names by using more of the existing rail and compact icon spacing.
Printer action widths reflect their different label lengths; colored symbols
use a compact font while action text retains its accessibility font. Probe/Z,
Accuracy and Custom have wider actions in two rows. Bed-mesh grid toggles use
GRID ON/OFF, keeping the meaning clear within their existing position. Toolhead
Home has more width and its X+ neighbor moves to preserve separation.

The button host fixture now rejects both wrapping and clipping of atomic labels,
page names and number/unit pairs, while retaining full-label bounds and overlap
checks. Multiword descriptions and longer compound actions may wrap at words;
readable full names remain preferable to shrinking text or dropping actions.
Panel checks remain required. Stable version and pending printing tests unchanged.

### Remaining atomic text and navigation inset

Navigation buttons retain single-line names and full-size text with an 8 px left inset and 156 px width. Long selected printer/camera names scroll horizontally; camera names no longer use a 16-byte prefix. E-STOP is 140 px wide. Console and calibration banner status labels have more horizontal space. Compact telemetry headings and readings select the largest enabled font that fits, down to 14 px, retaining decimal values and units; temperature slash spacing is compact. Calibration card status uses the same fit rule.

Host button/layout checks cover all four stock themes, three densities, both accessibility text settings, inset navigation, live Printer telemetry, compact card widths, E-STOP and connection statuses. Build/flash and panel validation remain pending. Inspect long configured names, first/last navigation items, telemetry during heating, and calibration discovery transitions.

### Motion distance calculator large text

Distance input labels and fields share two grid rows so both numbers stay aligned when a label wraps. One-line inputs have a 56 px minimum height and centered body-large text. Leadscrew labels use “THREAD PITCH (mm)” and “THREAD STARTS.” Config reference uses compact axis names and one missing-data message when no rotation distances are reported. Motion host checks cover field height/alignment in both modes at all stock themes, densities, text sizes and 1024/640/480 widths; hardware validation remains pending.

Devices catalog filters use intrinsic single-line widths in a horizontally scrollable row, including changing counts. Device cards use separate name, object identifier and kind/value rows, with scrolling for long names and complete live-value fitting. The dedicated Devices text/layout host test covers 96-object counts, long names, active-hotend readings, card bounds, empty states and cleanup in all stock themes/densities/text settings. Panel validation remains pending.

### Calibration Motion card actions

The Motion card is 190 px tall with two 44 px action rows. The diagnostic row uses 100/132/110 px widths; the workflow row uses 100/152/102 px widths. Compact horizontal padding preserves full-size single-line text and gaps. The first row starts at y=86 and the lower row stays at the card bottom. The summary is capped at 24 px. Motion host checks exercise the production Limits/Distance/Drivers construction and measure all six action names across stock themes, densities and text sizes. Build/flash validation remains pending.

## Responsive built-in and custom theme dialogs

The built-in theme chooser now wraps its four previews inside a scrolling
native layout with fixed Custom Themes/Close actions. Preview captions use
native wrapping labels above the retained miniature interface, with explicit
preview palettes that do not change the current theme. Footer actions measure
their labels and wrap to another row when the viewport/text size requires it.

Custom themes show full names, base/author, descriptions and palette previews
in content-height rows. Select a row, then use the pinned Apply/Remove/Close
footer. This replaces immediate application on row tap. The selected row is
highlighted and an active theme is labelled; Apply/Remove remain disabled until
selection. Removal confirmation wraps the full name and pins Keep/Remove.
Selection and removal bind to the captured theme ID to prevent an index change
from targeting a different theme. Failed apply/remove operations leave the
existing dialogs open. Built-in preview taps and Operator Shell pressed/clicked
handling retain the existing deferred, deduplicated theme switch. Active custom
theme removal also retains its deferred rebuild. External deletion retires
nested dialog owners, selected rows and action references. No timers or scans
beyond the existing on-open custom-theme scan are added.

`tools/audit/theme_dialog_layout_test.py` builds production popup and preview
code against real LVGL 9.5. It checks four themes, all densities/text sizes,
1024/640/480 px widths, long names/authors/descriptions, wrapping/scroll/preview
and footer bounds, deferred callbacks, duplicate event fencing, select/Apply,
Keep/Remove, active removal, failed/stale operations, empty lists and teardown.
Time Zone/Reset Settings regressions pass. Host renders were inspected; target
build and panel validation remain required. Stable version stays 6.5.6.

### Printer chooser large text

Chooser cards keep explicit zero padding for their absolute geometry. ACTIVE is on the bottom text row beside the open hint, separate from the printer name. Names and endpoints use single-line horizontal scrolling; connection states fit complete text. Preview placeholder text uses a 108 px width and two caption lines with clipping, preserving its icon and avoiding repeated writes caused by ellipsis-modified text. The subtitle/card grid have more vertical separation. The chooser refresh audit covers four themes, all densities and text sizes, card/label bounds, long names/addresses/files, active/offline states and unchanged 500 ms refreshes without redraw for fitting labels. Long scrolling names intentionally animate. Target build and panel validation remain pending; previous startup flash fixes and printing tests are retained.

### Printer profile manager and main editor

The profile manager, primary add/edit form and remove confirmation use native column layouts with scrolling content and pinned footer actions. Rows separate the profile name from its endpoint and keep both on a single scrolling line. Name, host and port fields use the large body font with a 56 px minimum touch height. Popups cap their size to the current viewport, and action widths follow their full label width; footers wrap on smaller displays. External manager/editor deletion cleans up owned dialogs and test timers.

Run `python3 tools/audit/profiles_layout_test.py` (optionally `--lvgl-dir /path/to/lvgl`). Host checks cover all four themes, three densities, both text sizes, 1024/640/480 px widths and a 400 px height; pinned action bounds, scroll reach, field sizes, selection, discovered endpoints, validation/failure/cancel/save, last-profile removal guards and teardown. Network and configuration storage use typed test stubs. Target build, touch/keyboard validation and earlier printing tests remain pending. Nested authentication, camera, certificate and keyboard layouts remain a separate follow-up.

## Settings and large-text source closeout

Accepted Settings, theme, chooser, profile and large-text fit changes are merged without a nightly. Stable remains 6.5.6; printing tests remain pending. See [Settings validation](SETTINGS_LAYOUT_VALIDATION.md).

### Nested printer setup dialogs

Authentication, connection security, the SD-card certificate picker, camera setup and camera removal now use scrolling native forms with pinned actions. Studio retains its full-width popup policy; the same helpers serve the other four themes. Camera slots retain their numbered routing and use full stored names on a single scrolling line. Tapping a name, host, port, API key or camera URL opens one shared value editor; Done applies the value and Cancel discards it. Port entry uses a numeric keyboard and digit/length restrictions. API-key entry remains password-masked and each editor inherits its field's maximum length. Closing a field owner discards its open keyboard and clears camera/test timers and setup state. Security has a separate Cancel action that preserves the selected mode. Existing camera catalog, certificate import and profile save behavior remains authoritative.

`profiles_layout_test.py` covers five themes, three densities, both text sizes, three widths and a 400 px short viewport. The test uses typed network/storage stubs and a fake directory for six PEM choices and a missing SD-card directory, without opening network connections or writing certificates. It checks footer bounds and scroll reach, keyboard Done/Cancel/external deletion, numeric/password/length limits, API-key staging, security cancellation/import failure/success, camera slot/default/removal routing, and owner teardown. Build/OTA/touch validation and printing-dependent tests remain pending. No commit, push or nightly is part of this layout patch. Stable version remains 6.5.6.

## Network and ready-to-print layout follow-up

Studio Network fills the shell stage while legacy themes retain their existing page geometry and transport bridges. Scan rows preserve SSID callback ownership and deletion. Files retains the shared thumbnail/fullscreen pipeline. The non-Studio ready-to-print modal uses native flex rows, a scrolling metadata area and a pinned wrapping action footer; Studio uses its embedded inspector with a scrolling details popup. Metadata readiness now both enables and disables Print, and the start callback rejects clicks while disabled.

## Setup Center native-layout follow-up

The wizard replaces fixed coordinates with a native flex shell, step navigation, independently scrolling content and pinned action rows. Review cards report Wi-Fi/printer readiness and optional camera configuration without changing the existing completion criteria. The duplicate legacy printer form is replaced by the shared profile editor; its discovery callback and completion refresh remain wired. Closing or externally deleting the center clears widget references and polling timers. Wi-Fi scans share one outstanding scan task; repeated Scan does not spawn another worker.

## Files action ownership

The print confirmation owns its modal independently from the selected-file inspector. It captures the active profile index/configuration generation, rechecks metadata readiness, and closes without issuing a start if the printer changed. Detail teardown closes confirmation and frees the copied full filename. File refresh keeps the page/inspector owners intact; the asynchronous controller distinguishes an embedded Studio inspector from blocking modals through `ui_files_can_refresh_rows()`. Existing HTTP worker, request/profile/page fencing and pending-job coalescing remain in use.

## Immediate emergency-stop routing

M112 is routed through printer.emergency_stop rather than the queued printer.gcode.script API, including Console input. Standalone FIRMWARE_RESTART uses printer.firmware_restart. Both WebSocket and HTTP retain the selected host, port, TLS and API-key handling. Emergency HTTP requests use a private client even when the ordinary background HTTP slot is busy; recovery and ordinary commands retain that slot policy. The popup reports a request sent, not confirmed shutdown, and transport failures show a toast. See the [Moonraker printer API](https://moonraker.readthedocs.io/en/latest/external_api/printer/).

## Compact Details action and responsive recovery dialogs

Studio File details retains its scrolling metadata body and a pinned, right-aligned 128 x 48 Close action. Fullscreen images retain tap-to-close. E-stop request and Restart Klipper dialogs use a native column layout, wrapping printer-name/message in a scrolling body, and intrinsic-width actions in a pinned wrapping footer. RESTART KLIPPER keeps the action concise; the target printer is shown in the body. Popups cap to the viewport. Changing the active printer dismisses any recovery modal naming the former target; external deletion clears the owner. Immediate emergency transport is unchanged.

## Studio Devices header actions

Graphs and Endstops explicitly reset to LV_ALIGN_TOP_LEFT before Studio positions them. Legacy bottom-right alignment had made the Studio coordinates offsets from the right edge, placing both controls outside the visible header. Studio labels the existing telemetry route GRAPHS; other themes retain TELEMETRY. Catalog, history and endstop query behavior are unchanged.

## Source checkpoint before telemetry redesign

The profile/setup, Network/Files, immediate E-stop routing, responsive recovery and Studio Devices action fixes are checkpointed on feature/profile-setup-dialogs. This checkpoint retains the feature branch and stable version 6.5.6 without merging or producing a nightly. The telemetry redesign has not begun: next work audits live data/history, modernizes graph layouts and controls across themes, and verifies missing/offline/printer-switch behavior.

## Native telemetry instruments

Telemetry now provides Heat, Motion and Environment views, named hotend selection, 2/5/10-minute windows, Targets on/Detail only and graph Hold/Resume. Current readings, historical targets, measured extrema, explicit scales and timestamp-based gap handling share reusable native instrument owners. Studio uses its full stage; all five themes use responsive scrolling graph content below pinned controls. The history ring records all supported hotends and capability-normalized channels while hidden. Reset generations retire held data on a printer change. Reapplying an unchanged label long mode is now avoided, preventing scroll-animation restarts during repeated fitted-value updates. See [Telemetry](TELEMETRY.md) for behavior, source limits and validation.

## Native Macros dialogs and action validation

Macros search, parameter editing and command review now use native popup columns with pinned Cancel/Done, Cancel/Review and Cancel/Run actions. The body scrolls independently; parameter fields use a single full-width column, 56 px touch targets and body fonts. Long macro headings remain one line with an ellipsis. Search and Clear Search remain single-line actions. No command is sent before the existing review and Run steps, and printer-generation/readiness guards remain in place.

`responsive_layout_test.py` checks real Macros actions and dialogs in all five themes, normal/large text, three densities and narrowed widths. It exercises search, parameter edits, scrolling with stationary footers, edited command review, rejected stale-printer Run and one successful Run. The source-derived button audit now delegates the two adaptive Macros search actions to that real-owner test instead of reconstructing Studio geometry without its final font fitting. Both host suites pass. Target build and panel verification remain pending; physical printing/recovery tests remain pending. Stable remains 6.5.6 and the current feature branch stays open.

## Motion and calibration native dialogs

The calibration workflows now use shared native columns with scrolling content and pinned actions, plus compact standalone Close controls. Motion measurement input remains touch-sized and runtime limit readings stay on one line. Probe Accuracy displays live homing/readiness and gates Run; PID warning Back preserves the target. Custom macro discovery fixes the existing count-pointer increment. Dialog command choices reject stale printer generations. See [Motion and calibration](MOTION_CALIBRATION.md) for scope, command behavior and host/panel validation. All five themes and normal/large text are covered by the dedicated host suites; target build, panel calibration and existing printing tests remain pending. Stable remains 6.5.6; the feature branch remains open.

## Console and pressed button frames

Console now uses native wrapped history, responsive filters, pinned page actions and command/search columns with the input and actions above the keyboard. Printer-generation/connection guards and persistent inline send feedback preserve rejected input. The appliance-wide Notifications setting defaults optional confirmations OFF and keeps important notices persistent; it is saved in NVS and backups. Shared button presses retain their geometry and semantic stroke; keyboard-only focus remains visible. See [Console](CONSOLE.md) and [Notification policy](NOTIFICATIONS.md) for behavior, audit findings and remaining notification migration. Dedicated host, source-derived frame/text and current owner checks pass; target/panel and physical printing tests remain pending. Stable remains 6.5.6 and the feature branch stays open. Drybox retains its prior layout.
