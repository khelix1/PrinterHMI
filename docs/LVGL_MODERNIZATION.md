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


## Button centering and native Event History

Shared single-label actions explicitly center their text and label bounds. Horizontal icon actions now use intrinsic label widths capped to the space beside the icon, with the complete icon/text group centered. Native content sizing responds to changed labels without reserving an empty flex-grown text area. Minimum-height native toolbar controls reserve symmetric vertical clearance. Rail icon/text groups center within their controls; Studio retains its existing stacked dock. Studio action labels explicitly center, fit on one line, and avoid ellipsis. Studio Dashboard Pause/Resume/Stop/Object use fitted body labels at their existing 112 x 52 geometry. Drybox retains its accepted vertical arrangement.

The source-derived audit now includes Studio actions and all five themes, checks label/group centers and complete text, and exercises intrinsic/bounded sizing plus dynamic icon labels. Actual theme-manager footers remain covered by their native owner audit because their final widths are measured after creation. Console, Macros, calibration, notifications and theme owners retain dedicated lifecycle/layout checks.

Event History now uses native scrolling message rows and a pinned wrapping footer. Messages grow to their complete height; metadata separates timestamp and severity from the message. Hold keeps the visible event snapshot steady; Follow Live reloads the current ring and brings the newest event into view. Clear keeps its existing event-log behavior. External deletion retires the refresh timer and widget references. See [Event History](EVENT_HISTORY.md).

Host checks cover all five themes, three densities, both text sizes, standard/narrow/short viewports, full-ring replacement, unchanged refresh, Hold/Follow, Clear and deletion/reopen. Target firmware, touch and prior physical printing/recovery tests remain pending. Stable remains 6.5.6; no commit, push or merge is part of this patch.


## Native Network and backup forms

WiFi scan/password, Moonraker port and connection-test results use viewport-clamped native forms, scrolling content, a pinned keyboard when needed, and centered pinned actions. Full SSIDs and signal readings wrap into complete growing rows; every supplied scan result is reachable. SSID callback strings are released when their rows retire. External popup deletion clears owned widget references. Ports accept only digits and retain input with an inline error outside 1–65535. WiFi credential-save, configuration and connection-start failures retain the password editor for retry; accepted startup returns status reporting to Network. Acceptance is not a confirmed WiFi connection.

Backup management, passphrase entry, restore review, removal confirmation and encrypted-worker progress share the native form owner. Restore still requires successful existing preflight/verification and explicit confirmation. Passphrase validation and worker-start failures remain in the active editor. Restore failures retain the review and staged secret for retry; cancellation/completion clears it. Parent/external deletion retires nested dialogs and progress timers without spawning a delayed confirmation. Existing storage, crypto format, worker and rollback behavior remains authoritative. See [Network and backup dialogs](NETWORK_BACKUP_DIALOGS.md).

The dedicated real-LVGL suite covers five themes, three densities, both text sizes, standard/narrow/short displays, complete SSIDs and backup action labels, stationary keyboards/footers, invalid-port retry, preflight/restore failures, worker start/failure/success, staged-secret transfer/cleanup, nested deletion and reopen. The actual WiFi dispatch bridge is compiled with typed ESP/storage fixtures. Storage, crypto and network operations are stubbed in UI tests; target build, panel, real WiFi and SD backup/restore remain pending along with earlier printing/recovery tests. Stable remains 6.5.6; this patch does not commit, push or merge.

## Notifications requiring attention in Quiet

Established connection loss is now a persistent warning at every level. Connection recovery replaces the current owner's previous connection notice and remains visible in Quiet. Paused transitions and reported error/shutdown states require attention; routine print events retain level filtering and an Event History record. Warnings precede informational cards and can displace them in a full queue, while faults retain priority. Lowering the preference to Quiet removes pending routine informational cards, preserving attention and recovery. Existing three-second baseline/debounce and chooser fencing remain intact. Monitoring remains scoped to the active configuration. See [Notification policy](NOTIFICATIONS.md).

The real-LVGL notification suite covers all three levels, all five themes, densities/text sizes and standard/narrow/short displays, including repeated loss/recovery, paused/fault states, saturation, duplicate suppression, preference changes, navigation and teardown. Firmware/panel connection-loss and previously documented physical printing/recovery tests remain pending. Stable stays 6.5.6; no commit, push or merge is included.

## Theme-specific dashboard previews

Built-in previews now show each theme's actual dashboard composition: Foundry's print emphasis, Operator's balanced panels, Operator Shell's enlarged job panel, Dark Glass's split instrument tiles, and Studio's open preview/instrument stage and bottom dock. Legacy proportions come from the production dashboard profiles; Studio reuses its dock icon paths. A shared scene renders cached RGB565 pixels and the native allocation fallback, with miniature fonts independent of the active theme. Scrolling retains the cached-pixel path and existing selection/deferred-switch behavior. See [Theme previews](THEME_PREVIEWS.md).

Real-LVGL owner checks cover all five themes, densities, text sizes and standard/narrow/short viewports. Additional checks validate complete miniature text/bounds at three widths, invariant scene pixels across active themes/text sizes, identical cached/fallback output, scroll occlusion and resource retirement. Host renders were inspected; target build, panel selection/scrolling and prior physical printing/recovery tests remain pending. Stable remains 6.5.6; no commit, push or merge is part of this patch.

## Keyboard and input consistency

Native editors pin their keyboard outside scrolling content and reveal the focused field after layout. Shared textarea styling keeps body fonts and minimum field height. Keyboard completion/cancel now matches owner footer actions for Files, Macros, Network, Backup, OTA, motion measurement and Setup. Files search uses a native viewport-clamped form; Setup preserves the space for Connect. Profile ports and motion values validate before copying and retain rejected input. Temperature/fan controls receive checked dispatch results and keep their controls/values on offline, stale or send failure. Existing Console behavior and arbitrary macro parameter types remain intact. See [Input dialogs](INPUT_DIALOGS.md).

Real-owner checks cover all five themes, densities/text sizes, keyboard actions, focus/scroll and footer bounds, validation/correction, rejected dispatch/retry, deletion and Setup deduplication. Console, hotend, notification and theme preview regressions pass. Host renders were inspected; target firmware, touch/network/temperature and previous printing/recovery tests remain pending. Stable stays 6.5.6; no commit, push or merge is included.

## Loading, empty and error-state recovery

Files retires obsolete indexes while a request is pending, preserves error/loading feedback during search/sort, and distinguishes empty folders from empty searches. Camera provides nonblocking inline Retry, wrapped status and first-frame feedback; configuration changes clear previous frames and refresh the camera selector. Moonraker discovery uses native scrolling content with pinned Retry/Close and cancels on external deletion. Camera discovery captures the edited profile, rejects stale configuration results and imports only during current-owner consumption. Setup verifies the requested Wi-Fi SSID with a bounded wait and retains failed input; its own camera import does not falsely invalidate its next step. OTA avoids duplicate catalog refresh, exposes Retry and retires refresh-time confirmations. Telemetry explains missing history according to transport readiness. See [Page states](PAGE_STATES.md) for behavior, ownership and validation limits.

Real-owner and typed-worker checks pass across five themes and relevant density/text/viewport variants. Discovery renders were inspected; target/panel/network and prior physical printing/recovery tests remain pending. Stable stays 6.5.6; the feature branch remains open and this patch does not commit or push.

## Touch and navigation consistency follow-up

Modal discovery now stays above its blocker while page ownership handles teardown. Nested dialogs preserve their parent and drafts; repeated editor opens no longer discard input. Files preview taps open details while a thumbnail is pending and the lightbox once loaded. Devices, Network and PID actions use minimum 44-pixel heights; Drybox layout is preserved.

Real LVGL pointer tests cover modal blocking, adjacent targets, disabled controls, scroll suppression, nested Back/Close and discovery actions across all themes and text sizes. See [Touch and navigation consistency](TOUCH_NAVIGATION.md). Target firmware and panel validation remain pending; stable version is 6.5.6.

## Scrolling and refresh performance follow-up

Telemetry reads history once for all four charts. Settings and Files skip unchanged dynamic label text; equivalent Files searches and unchanged folder actions preserve rows. Existing time progression, explicit refresh, Hold and stale-result protection are retained.

Host checks exercise graph semantics, no-op redraws and repeated object/timer cleanup across all five themes. See [Scrolling and refresh performance](SCROLL_PERFORMANCE.md). Target frame rate, heap soak and panel tests remain pending; stable version remains 6.5.6.

## Live appearance and scroll preservation follow-up

Settings preserves the visible section across theme/appearance rebuilds, including Studio card reordering. Accent, density and accessibility dialogs refresh in place with current typography, colors and selected options; scroll position and pinned Close are preserved. Screen-local opaque scrolling prevents background composition. Deferred changes are deduplicated and guarded against save/queue failures and deleted owners.

See [Live appearance changes](LIVE_APPEARANCE.md) for host coverage and pending panel checks. Existing theme chooser behavior remains intact; stable version remains 6.5.6.

## Connection-aware controls follow-up

Printer actions now combine live readiness with print state and recheck before dispatch. Files detail/confirmation, Macro Run, primary calibration and Bed Mesh commands disable while unavailable and restore for the same printer when ready. Inline reasons accompany unavailable workflows. Files metadata and Bed Mesh confirmations are fenced against configuration changes; added UI timers retire with their owners. Navigation, diagnostics, emergency/recovery endpoints and active-print temperature/fan controls retain their existing paths.

See [Connection-aware controls](CONNECTION_CONTROLS.md) for policy, host checks and pending panel validation. Stable version remains 6.5.6.

### Printer action lifetime after chooser navigation

Printer teardown retires the complete `printer_actions` structure before deleting
its LVGL parent. The readiness refresh only touches Toolhead Control while the
Printer panel exists. This fixes a stale Motion button reference exposed by
connection-state refresh after opening the chooser with an offline printer.

`tools/audit/printer_action_lifetime_test.py` exercises the actual teardown and
motion readiness block for 1,000 create/destroy cycles with real LVGL, including
absent-page refresh and online/offline transitions. Firmware/panel crash
reproduction remains pending.

### Offline printer readiness and telemetry presentation

The chooser uses both Moonraker connectivity and Klipper/live-data readiness for
the active printer. A shutdown or disconnected active printer cannot inherit an
old online/printing label from background profile health. Cached thumbnails remain
historical images rather than proof of a live connection.

WebSocket subscriptions include `webhooks.state`; explicit shutdown, error,
disconnected and startup states remain unavailable across subsequent partial
status updates until readiness is confirmed. HTTP bootstrap checks the same state
before publishing print/temperature objects. Fresh transport traffic alone does
not establish Klipper readiness. Existing configuration-generation response fences
remain in place for HTTP and WebSocket endpoint ownership.

Unavailable Printer and Dashboard readings use placeholders rather than retaining
live-looking temperatures/progress. Host readiness, chooser (all five themes,
densities and text sizes), telemetry and compilation checks cover this pass.
Hardware offline/reconnect and printer-selection validation remain pending.

### Final Dashboard writer availability gate

The later `dashboard_live_controller` banner and machine updates now use the same
connectivity/live-data gate as the runtime cards. The shared status formatter no
longer treats a reachable Moonraker by itself as READY and recognizes shutdown.
Unavailable readings, active filenames and progress become placeholders in this
final writer rather than overwriting the earlier offline placeholders with stale
values. Bottom page navigation and Moonraker file browsing remain available;
printer commands still require readiness.

`dashboard_offline_test.py` executes both actual Dashboard publication functions
in all five themes with stale printing/paused/ready readings under unavailable
conditions, then verifies restoration on fresh live data. Panel validation remains
pending. Endpoint ownership should be checked with selection/WS_START logs if a
wrong-printer response is still suspected.

### Inactive printer notification coverage

The existing UI refresh observes inactive-profile health as well as the active
printer. Confirmed offline/recovery transitions produce correctly named notices
at every level, deduplicate repeated samples and replace only that printer's
connection notice. Open Printers routes to the chooser without changing the active
profile. Initial offline discovery and endpoint edits establish baselines.

Host notification checks cover inactive loss/recovery in Quiet, verification and
stale health suppression, repeated samples, same-name profile separation,
endpoint edits and chooser routing after active-owner changes, in all five themes,
densities, text sizes and viewport configurations. Panel validation remains pending.


### OTA dialog lifetime and build identity

Custom URL and Releases transitions reserve asynchronous work before changing UI
ownership, cancel on owner deletion and preserve camera quiescence throughout the
transition. OTA action fitting rejects bubbled child size events. Pending updates
can be cancelled before dispatch without starting firmware installation.

The release browser displays the installed Stable/Nightly/Custom channel and
source revision. Local builds remain Custom; stable release tooling explicitly
marks Stable and generated nightly identities mark Nightly. Catalog rows mark
Installed, Update Available, Older, channel switches and unorderable same-day
nightly builds. Host channel/comparison and real-LVGL ownership/layout checks pass;
CMake execution, target build, panel crash reproduction and actual OTA/reboot
validation remain pending in this environment.


### Settings firmware identity and update review

Settings now shows the installed build channel and source revision beside the image version and compilation time. OTA confirmation separates installed and selected builds, download information, reboot/power guidance and scrollable release notes; actions remain pinned. The active chooser card shows Checking before its first health result and Connecting when initial health is online but transport is not ready. Confirmed failures and post-connection losses remain offline. Host checks cover these transitions and responsive OTA review; target/panel reboot and real installation remain pending.


### Shared selector control styling

Console, Telemetry, Motion diagnostics and Probe Accuracy now share flat selector fields and explicitly themed native lists, matching fonts for selected/normal options, a minimum 44-pixel option pitch, stable press geometry and keyboard-only focus outlines. Console reserves more width for filter names. Host checks exercise pointer selection, disabled fields and every theme/density/text configuration, plus native page and calibration layout checks. Panel touch/scroll validation remains pending; the current branch and stable version are retained.

Release browser tabs now retain the bounded catalog rows and locally filter their visibility, preserving each channel scroll position. Repeated Stable/Nightly switches perform no catalog entry reads, row recreation or fetches; explicit Refresh still retires confirmation and rebuilds on catalog changes. Host fixtures cover repeated switching, row identity and fetch/read counts. Panel timing remains pending.


### OTA transfer measurement and bounded receive tuning

The common GitHub/custom-URL worker requests a 4 KiB receive buffer while retaining the 2 KiB redirect request buffer, certificate verification, keep-alive, existing cancellation/validation boundaries and sequential flash erasure. Firmware progress uses the ESP HTTPS OTA image counters after each perform call, excluding redirect bodies from transferred-image readings. A short critical section publishes coherent byte, state and clock snapshots; UI-thread formatting replaces per-data-event formatting. The progress dialog reports average effective download speed in KiB/s and elapsed time since the update started, including network preparation; readings freeze when transfer ends, before validation. Unknown image lengths remain supported. Host fault tests cover worker success/restart, cancellation, incomplete images, begin/perform/finish and task failures, lane timeouts, known/unknown lengths, clock snapshots and configuration. Real GitHub/custom download speed, target memory/cancel responsiveness and reboot remain pending; no speedup is claimed without panel measurement.

Telemetry history status now has room for the compiler-reported worst-case text. Its host compile check enables format-truncation warnings as errors.

The stricter Telemetry owner compilation also checks hotend-option assembly: bounded copies retain all four maximum-length object names without a potentially truncating append. Host selector tests cover those names and active-tool selection.


### Diagnostic popup lifetime and retry

Probe Accuracy and Live Endstops retire their refresh timers and widget references on external popup deletion as well as normal closure, before widgets are destroyed. Endstops also resets pending readings. Probe Accuracy ignores callbacks after closure and duplicate/reentrant dispatch, preserves its existing popup/sample selection on repeated opens, retains input with inline retry feedback if a request is rejected, and checks popup/printer ownership after dispatch. The Accuracy launcher clears its pointer when its card is deleted independently. Host checks cover direct deletion, late refresh/run callbacks, reopen cycles, rejection/retry, duplicate callbacks and printer-generation changes across all five themes/densities/text/viewports. Physical probe/endstop/panel checks remain pending.

### Runtime checkpoint: OTA observation (2026-10-11)

OTA did not download during the session and resumed after a controller reboot.
The cause is not established; this remains an unresolved intermittent runtime
issue. A successful download after reboot does not close it. Capture the serial
log if it recurs, including the download start and any network-lane timeout.

The user accepted the current download speed for now; further throughput tuning
is deferred. Firmware/panel and physical print/recovery tests remain pending
where documented. This checkpoint retains the feature branch and stable 6.5.6.


### PID and Pressure Advance dialog lifetime and retry

PID heating confirmation retains the selected heater and target when dispatch is
rejected and shows inline retry feedback. Back still returns to the same target.
Pressure Advance setup retains its drive choices after rejection and opens the
armed instructions only after acceptance. Repeated launcher taps preserve the
current workflow instead of rebuilding it.

Both owners clear their widget references on external deletion, suppress
reentrant dispatch, and discard UI follow-up after owner retirement or a printer
generation change during dispatch. Pressure Advance clears its launcher pointer
when the card is independently deleted. PID catalog population also respects the
provided heater-array capacity.

Real LVGL checks cover accepted/rejected commands, retained input, duplicate opens,
reentrant dispatch, deletion/reopen cycles, owner changes during dispatch and
capacity bounds across all five themes, densities, text sizes and viewports.
Physical heating, PA tower printing and panel checks remain pending. Stable
remains 6.5.6; this follow-up leaves the current feature branch open. The reported
intermittent OTA failure before reboot remains unresolved.


### 7.0.0 development milestone

The feature branch now builds as `7.0.0-dev`; latest published stable remains
`6.5.6`. Earlier version statements in this work log record the identity at the
time of each patch and are historical. Settings, splash and OTA read the embedded
image version. Local development builds remain Custom; `-dev` cannot be labelled
or published Stable. Final `7.0.0` supersedes `7.0.0-dev` in OTA comparison.

Current product headers, draft release notes, changelog and version auditing
follow the new branch identity. Historical release notes stay unchanged. The
reported OTA failure before reboot and physical printing/recovery validation
remain open; this bump is not a stable release.


### Custom calibration workflow retirement and retry

Custom calibration keeps its selected macro and confirmation on rejected dispatch,
with inline retry feedback. Repeated launcher taps preserve the current workflow.
External deletion clears widget references; dispatch guards reject closed/stale
owners and duplicate/reentrant commands, and UI follow-up is discarded after a
printer switch or owner retirement during dispatch. The command is copied before
calling transport so retirement cannot invalidate its source buffer.

Real LVGL checks cover rejected/accepted commands, retained selections, repeated
opens, direct deletion/reopen, reentrant dispatch, printer changes and catalog
capacity in all five themes, densities, text sizes and viewports. Physical macro
execution and target/panel tests remain pending.


### Calibration results and Apply & Restart ownership

Calibration results and the nested save confirmation clear widget references on
external deletion; deleting results also retires its confirmation. Rejected
SAVE_CONFIG requests keep the reviewed results with inline retry feedback.
Accepted requests replace Apply/Back with Close, hide the underlying Apply
action and suppress repeated/reentrant sends without claiming save completion.

The confirmation is bound to the reviewed session generation and active printer.
A changed session requires Back and fresh review. Offline/printing/error states
disable saving and retain results; recovery restores the same confirmation when
its reviewed session is still current. Printer switches stop follow-up updates
and explain that calibration must be reopened. A recreated results view renders
the current session even if its generation was previously displayed.

Real LVGL checks cover rejection/retry, offline/printing recovery, session changes,
accepted/duplicate commands, nested Back/Close, external deletion/reopen cycles,
transport callbacks that delete owners or switch printers, and all five themes,
densities, text sizes and viewports. Target SAVE_CONFIG/reconnect and panel checks
remain pending. Build identity remains 7.0.0-dev; no release, commit or push is
part of this patch.


### Manual Probe/Z and Axis Twist dialog lifetime

The shared manual-probe surface clears its popup, status label and callbacks on
external deletion as well as normal closure. Reopening the same active workflow
retains its instructions and status instead of rebuilding it. Dispatch preserves
the original TESTZ event data, rejects retired/stale owners and suppresses
reentrant step/Accept/Abort callbacks. Callback-driven retirement cannot reopen a
manual adjustment surface before the current dispatch returns. Status updates
ignore a retired dialog or a changed printer. Workflow-specific command routing
and acceptance/abort behavior remain with the existing callers.

Real LVGL checks cover all ten signed TESTZ commands, unchanged repeated opens,
status preservation, printer switches, external-deletion/reopen cycles, late
updates, reentrant callbacks and callback-driven closure across all five themes,
densities, text sizes and viewports. Physical Probe/Z, Axis Twist and panel checks
remain pending. Current development identity remains 7.0.0-dev.

### Panel validation reported 2026-10-11

The operator confirmed that calibration Save/Restart and reconnect work after the
save-review patch. This validates that observed panel path; broader calibration
accuracy, filament recovery, printing tests and the intermittent OTA failure
before reboot remain open where documented.


### Responsive About PrinterHMI

About now uses a native scrolling column with themed firmware and credits
sections, a viewport-clamped modal and pinned, fitted Close action. It shows the
embedded version, Stable/Nightly/Custom channel, source revision/modified marker,
build timestamp, ESP-IDF version and running LVGL version. Version/channel
identity matches Settings and OTA; the previous fixed-coordinate layout and
unconditional compatibility-patch claim are retired. Existing author and
open-source credits are preserved.

The popup clears ownership on external deletion, preserves the existing view on
repeated opens and safely closes/reopens. Shared form action fitting ignores
bubbled child size events and checks the target label before fitting. Firmware
version copying is bounded to the embedded descriptor's version field.

A dedicated real-LVGL About suite covers all three firmware channels, five themes,
densities, text sizes and viewports; long nightly names, missing descriptors,
pinned Close, scroll reach, pointer closure, repeated opens, external deletion
and child size notifications are checked. Host renders were inspected across all
five themes. Identity and Network/Backup form regressions pass. Panel About
scrolling/closure remains pending; current identity is 7.0.0-dev and the feature
branch stays open.


## Geometry and motion dialog retirement and retry (7.0.0-dev)

Screws Tilt, gantry leveling, Input Shaper, resonance testing and accelerometer
checking now retire their references when their dialogs are deleted. Reopening
an active dialog preserves its choices. Rejected commands stay in the review
dialog with local retry feedback; accepted gantry requests replace the Run/Back
actions with Close and cannot be submitted again from that review. Acceptance
means the transport accepted the request, not that physical calibration finished.

Dispatch rechecks printer ownership, readiness and reported capabilities. Geometry
checks also require a fresh review if homing became necessary after confirmation.
The existing G28, SCREWS_TILT_CALCULATE, QUAD_GANTRY_LEVEL, Z_TILT_ADJUST,
SHAPER_CALIBRATE, TEST_RESONANCES and MEASURE_AXES_NOISE command paths are retained.
Reentrant callbacks, deletion during transport and printer changes discard stale
UI follow-up. Screws results cannot display another selected printer's session.

Host fixtures passed all five themes, densities/text sizes and standard/narrow/short
viewports, including rejected retry, capability/readiness withdrawal, owner changes
and repeated external deletion. The user confirmed the About popup looks good on
the panel on 2026-10-11. Physical alignment and motion measurements remain pending,
as do previously recorded printing/recovery and intermittent pre-reboot OTA checks.


## Native Bed Mesh profile forms (7.0.0-dev)

The user checkpointed and pushed the development milestone as 635fdcf0e8d8 on
feature/profile-setup-dialogs. The branch remains open and the published stable
release remains 6.5.6. No new panel or physical calibration result is inferred
from that checkpoint.

Bed Mesh Profiles now uses native scrolling columns and pinned action rows for
the manager, name editor and Save/Remove review. The name keyboard is pinned
between content and actions. Profile names remain single-line rows with ellipsis
when needed; labels and action buttons fit every tested viewport and text setting.
Empty lists still permit saving an available current mesh. Invalid names have
local feedback instead of silently ignoring Continue.

Load and Save/Remove requests retain their selection and review when transport
rejects them, and close only after acceptance. Duplicate/reentrant callbacks are
blocked. Independent dialog or parent-page deletion retires nested dialogs and
references; transport callbacks that retire the owner or switch printers cannot
follow up on stale widgets. Reported names are copied into bounded owner storage,
and visible rows keep their displayed-name snapshot while live updates arrive.
A reordered live list cannot redirect a tap to another profile. Profiles no longer
reported and absent current meshes are blocked at dispatch. Existing
BED_MESH_PROFILE LOAD/SAVE/REMOVE and SAVE_CONFIG scripts remain unchanged.

The real owner suite passed all five themes, densities/text sizes and
1024/640/480-pixel viewports. Native pointer actions, keyboard/footer bounds,
scroll reach, rejected retry, offline/recovery/printing gates, list reorder/removal,
external deletion and printer changes are covered. Host renders were inspected
in all five themes. Panel touch and actual mesh save/load/remove/restart remain
pending, along with prior physical printing/recovery and intermittent OTA checks.


## Motion Diagnostics retained edits and live ownership (7.0.0-dev)

The user confirmed the Bed Mesh profile form changes look good on the panel on
2026-10-11. This records visual acceptance; physical profile operations and
SAVE_CONFIG restart still need validation.

Reopening the active Motion Limits, Drivers or Distance view for the same printer
now raises the existing view. Distance retains calculator values, results and an
unfinished measurement editor; repeated field taps do not discard typed input.
The numeric editor uses the shared native form with pinned keyboard/actions and
local validation feedback. Calculator and mode callbacks ignore retired dialogs.

Normal close and independent popup deletion retire all widget and timer references
before deletion. A printer switch closes the editor, marks the view stale and
stops its refresh timer; reopening establishes the new owner. Live readings now
require a usable printer connection and a matching diagnostics snapshot owner.
Offline/shutdown views cannot label cached runtime data as live. Configured
rotation-distance references become explicitly unavailable, while same-printer
local geometry calculations remain usable offline. No printer motion or settings
commands are introduced.

Real LVGL checks passed all five themes, three densities, both text sizes and
1024/640/480-pixel viewports, including keyboard/footer bounds, retained edits,
validation, driver selection after reorder, offline recovery, stale snapshots,
printer changes, late callbacks and repeated deletion with balanced timer counts.
The actual calculation helpers and separate owner compile with warnings treated
as errors. Panel diagnostics checks remain pending, as do earlier physical
calibration/printing/recovery and intermittent OTA checks.


## Macros dialog retirement and reviewed dispatch (7.0.0-dev)

The user confirmed Motion Diagnostics looks good on the panel on 2026-10-11.
This records visual acceptance; earlier physical calibration/printing/recovery
and intermittent OTA tests remain open.

Macros now clears its search/parameter editor, parameter references and reviewed
command when the corresponding dialogs are deleted independently. Parent-page
deletion also retires the refresh timer and nested dialogs before their widgets
are destroyed. Late callbacks ignore retired pages. Search and parameter editors
capture their printer generation; Done cannot apply an old printer's draft after
a switch. Parameter editing also requires the current parameter review owner.

Run requires an actual reviewed confirmation and blocks duplicate/reentrant
dispatch. The command is copied before transport; UI follow-up is discarded when
a callback closes the owner or changes printers. A rejected request retains the
exact reviewed command with local retry feedback. Repeating Review on a completed
review does not access retired fields. Opening a macro while its parameter editor
is active raises both existing dialogs. No command-building, parameter validation,
favorite or search semantics are changed.

The responsive real-owner suite passed across all five themes, three densities,
both text sizes and existing width/custom-profile combinations, including
retained retry, parameter/search ownership, external delete/reopen cycles,
callback-driven retirement and timer balance. Existing Tools/Files/Network layout
variants also passed. Panel macro search/edit/review/retry and physical macro
execution remain pending. Branch identity remains 7.0.0-dev; no commit or push is
part of this patch.


## Console dispatch and page retirement (7.0.0-dev)

The user confirmed the Macros follow-up looks good on the panel on 2026-10-11.
This records visual acceptance; physical macro/calibration/printing/recovery and
intermittent OTA checks remain open.

Console now retires every command-editor reference before normal deletion, and
independent parent-page deletion closes both editors, clears cached row references
and removes the refresh timer before destroying widgets. Late Send, history,
search/filter and keyboard callbacks ignore absent pages. Search closure clears
its references before deletion; Reset closes an unfinished search editor.

A dispatch guard prevents repeated/reentrant sends and changes to the editor
during transport. The command is copied before dispatch; UI follow-up is discarded
when callbacks retire the editor/page or switch printers. Rejected sends preserve
the exact input for retry. Same-editor reopening continues to preserve typed
input. Console intentionally retains its Moonraker-link readiness rule so raw
RESTART and other recovery commands remain available with a Klipper error or
missing live data. No literal command routing, WS/HTTP fallback, history/filter
preferences or pressed-button geometry is changed.

Real-LVGL checks passed five themes, densities/text sizes, contrast/motion settings
and standard/narrow/short viewports, including timer-balanced delete/reopen cycles,
callback retirement and owner changes. Separate owner compilation treats warnings
as errors. The actual routing fixture also passed literal dispatch, fallback and
inline failures. Panel Console search/history/Send/Close and recovery checks remain
pending. This patch stays on feature/profile-setup-dialogs at 7.0.0-dev.


## Native Camera selection and View forms (7.0.0-dev)

The user confirmed the Console changes look good on the panel on 2026-10-11.
This records visual acceptance; physical recovery and earlier intermittent OTA
checks remain open.

Camera View and Select Camera now use native scrolling bodies with pinned
actions, replacing fixed coordinates. Rotation options use two-column rows;
mirror/rotation settings are marked active, and the picker distinguishes the
current camera and persisted default. Rows and actions fit large text. An empty
picker explains how to add a camera. Accepted selections retain the existing
stream restart and per-profile settings behavior.

Both dialogs capture their printer generation and displayed camera URL. View also
captures the selected camera index. A stale printer, changed camera or replaced
URL cannot apply old controls to the current configuration. Independent deletion
retires popup/feedback references, and late callbacks cannot write settings. Failed
view/default saves keep the dialog with local retry feedback; accepted writes
close it. Reentrant writes and UI follow-up after callback retirement or printer
changes are guarded. No decoding, network worker or fullscreen rendering behavior
is changed.

The actual Camera owner suite passed five themes, three densities, both text
sizes and standard/narrow/short viewports for these dialogs, including pinned
actions, row/label bounds, empty state, saved-option markers, rejected retry,
delete/reopen cycles, changed URLs and printer generations. Existing frame/cache
retirement, all rotations/mirrors, fullscreen fit and malformed-frame checks also
passed. Panel picker/View touch and real persisted settings/reconnect remain
pending. Identity stays 7.0.0-dev on feature/profile-setup-dialogs.


## Camera page and fullscreen retirement (7.0.0-dev)

The user confirmed the native Camera selection/View forms look good on the panel
on 2026-10-11. Persisted settings and real reconnect tests remain pending.

Camera now has a single page-retirement path for normal destruction and external
page deletion. It stops polling/stream requests, detaches the image source/cache
while descriptor/pixels are valid, clears every widget reference, closes camera
dialogs and deletes any fullscreen card reparented above the page. Normal
destruction no longer reparents/repaints fullscreen solely to delete it.

Independent image/card deletion also retires frames and timers; reopening rebuilds
a missing surface. Poll, Retry and Configure/fullscreen callbacks ignore absent,
hidden or setup-suspended pages. Reopening a cached page does not overwrite its
printer generation before the poll can retire the previous printer's frame.
Configure suspension clears the old frame/dialogs and timestamps; resume starts
only a visible, intact camera page. Rebuilding while setup remains active does
not create a background refresh timer.

Real-owner checks passed five themes, densities/text sizes, repeated fullscreen
page deletion, independent image/card deletion, cached-page printer changes, late
callbacks and setup suspension/resume. Timer counts balance and frame allocations
are freed, with the existing pixel/cache/transform/dialog checks still passing.
Panel fullscreen exit/navigation, theme/printer switches and Configure return
remain pending, along with earlier real streaming/persistence, physical
calibration/printing/recovery and intermittent OTA checks. Identity remains
7.0.0-dev on feature/profile-setup-dialogs.


## Files page and print-review retirement (7.0.0-dev)

The user accepted the Camera page/fullscreen follow-up on 2026-10-11. This records
panel acceptance; real camera persistence/reconnect and prior physical/OTA tests
remain open.

Files now retires page references before normal deletion and handles independent
page deletion, including search, detail, metadata and print-confirmation surfaces
and refresh timers. External detail/page deletion notifies the existing cancellation
callback after UI references are retired so the loader/thumbnail owner can clean
up. Metadata's independent deletion clears its label reference before late detail
updates. Normal close clears print/metadata/detail references before deletion.

Start print blocks duplicate/reentrant dispatch and keeps actions disabled while
the existing start callback runs. Rejected requests retain the file/review for
retry through the existing feedback path; accepted-close behavior remains owned
by the existing callback. Printer-generation/readiness gates are retained. Late
refresh/search/toolbar callbacks ignore retired pages, and deferred refresh retires
its own timer/pending flag before invoking the file-list callback. No print command
semantics, file-list worker routing or preview layout is changed.

The real Files responsive suite passed all five themes, densities/text sizes and
existing width/custom-profile combinations, including metadata deletion, callback
retirement, duplicate starts, late updates and object/timer-balanced reopen cycles.
The existing file-list worker suite also passed asynchronous work and stale-result
fencing. Panel preview/metadata/Print/Cancel/refresh/printer-switch checks and
physical print-start validation remain pending. Identity remains 7.0.0-dev.


## Files metadata touch-scroll correction (7.0.0-dev)

On 2026-10-11 the user reported incomplete preview details, no touch scrolling and
a panel that appeared frozen while attempting to scroll. The metadata bodies
created with studio_plane inherited its cleared CLICKABLE flag. SCROLLABLE alone
allowed the prior programmatic scroll test to pass while pointer hit-testing
could bypass those bodies.

Metadata scroll bodies now explicitly receive pointer hits and keep scroll
chaining within the details area. Both the compact Studio preview metadata and
the full Details popup use this behavior. A real LVGL pointer-drag regression
fails against the preceding source and passes with this correction. A forty-line
details fixture verifies the final field is reachable and the footer stays fixed.
The host tests reproduce the missed gesture; no device panic cause is claimed
without a serial trace. Panel touch/freeze verification remains pending.

Files-specific Print readiness text now distinguishes Offline/reconnect before
printing, waiting for Klipper, loading file details and a missing Print action.
The underlying selected-printer readiness gates are retained; an actually ready
printer must not be labeled offline. Current identity remains 7.0.0-dev. Earlier
physical print/calibration/recovery and intermittent OTA checks remain open.


Files Up clarification: Up navigates to a parent folder; the root had silently
ignored it, and library-wide search could leave the same results visible after
folder changes. Up is now disabled at the G-code root when no search is active.
During search, the first Up clears the query and returns to the current folder;
subsequent Up navigates to its parent. UI checks verify root/child/search enabled
states and dispatch, and actual controller tests cover nested-folder navigation
and leaving search without changing its remembered folder. Panel verification
remains pending. This is part of the metadata touch-scroll correction.


## Quiet subscription status sampling (7.0.0-dev)

The user reported that Files still displayed Offline on 2026-10-11 after the
metadata scroll/navigation patch. Source review found a transport path that
cleared connection/readiness after three seconds without changed subscription
status, even while the WebSocket remained connected. Status subscriptions report
changes; a quiet standby printer can reach this path without a disconnect.

The network task now requests a small read-only printer.objects.query sample for
webhooks readiness and print_stats state after 1.5 seconds without status updates.
Requests use the existing selected-profile WebSocket, a separate reserved ID and
a zero-wait send, with a 1.5-second retry limit. It runs only after a valid initial
sample and an established subscription. Frequent telemetry avoids extra queries.
No HTTP fallback or UI-thread network operation is introduced.

Sending is not evidence of readiness. Only the existing generation-fenced response
merge refreshes state/freshness. Failed/unanswered queries leave the three-second
offline gate intact; initial bootstrap, printer changes and Klipper error/shutdown
remain blocked. Host scheduler tests cover quiet/frequent samples, request cadence,
failed sends, absent subscription/discovery, future timestamps and stale profiles,
with existing readiness latch and offline navigation tests passing. Hardware
verification of the reported Offline caption is pending: verify a quiet online
printer, printer switches and real power/network loss before closing this issue.
Prior physical/OTA tests remain open. Identity remains 7.0.0-dev.


## Temporary Files inline-readiness caption removal

On 2026-10-11 the user chose to hide the persistent file-preview readiness caption
for now and did not run the diagnostic logging patch. The preview's readiness
label is empty and hidden in every theme; its refresh timer and Print readiness
gates remain active. Print confirmation retains its blocker/feedback text. This
is a presentation change, not confirmation that the false-offline issue is
resolved. No diagnostic logging is included. The real Files host suite verifies
the hidden caption alongside disabled Print and confirmation offline behavior.
Previous touch scrolling, navigation, physical and OTA checks remain documented.
Current identity stays 7.0.0-dev on feature/profile-setup-dialogs.

## Operator follow-up checkpoint (2026-10-11)

Bed Mesh profile forms, Motion Diagnostics, Macros/Console dialog retirement,
Camera forms/fullscreen cleanup and Files touch/navigation follow-ups are
checkpointed on feature/profile-setup-dialogs at 7.0.0-dev. The branch remains
open; no merge, nightly or stable release is part of this checkpoint.

The false-offline caption remains unresolved after quiet-link sampling. At the
user's request, the inline Files readiness caption is temporarily hidden while
Print readiness gates remain active. The diagnostic logging patch was not run.
The reported scrolling freeze still requires panel confirmation. Earlier physical
printing/recovery/calibration, real camera persistence/reconnect and intermittent
pre-reboot OTA checks remain pending where documented. Published stable remains
6.5.6. Devices and remaining printer-control audits are next.


## Devices lifetime and live-reading follow-up (2026-10-11)

Devices now retires its page references, catalog timer, live-value labels and
Endstops dialog when its LVGL root is deleted externally. Normal close retires
references before deleting widgets. Catalog and live-value owners also handle
independent deletion; old timer and hidden-page navigation callbacks are guarded.
Reopening the same printer preserves the view. Reopening after a printer change
rebuilds it, while the old view refuses navigation and hides its old device rows.

Cached temperatures, fan values and generic readings display UNAVAILABLE when
the current connection policy reports offline, waiting or shutdown, or when the
view belongs to another printer. Readings remain available during a healthy
active print. This change preserves the existing Devices layout and does not
change discovery, send commands, or resolve the documented false-offline cause.

The real LVGL Devices audit passes all five themes, three densities and both
text sizes at the native 1024x600 display size. It covers repeated external page
removal/reopen, timer balance, stale printer navigation, shutdown/offline cached
values and independent live-label deletion. Firmware/panel checks remain pending.
Next: remaining printer-control dialog ownership and failed-command retries.


## Temperature/fan control dispatch lifetime follow-up (2026-10-11)

Preset and custom-temperature actions now copy the command and capture the
owning modal before sending. They guard nested sends and reopen attempts during
dispatch, and discard UI follow-up when sending deletes the popup/editor, deletes
its feedback field, or changes printers. They no longer walk the clicked button's
parents after a send callback that can delete that button. Closing an editor
retires its references first; external deletion clears references and advances
the modal epoch. The fallback sender is captured per control popup.

Both checked and legacy send paths apply connection/shutdown readiness while
allowing hotend, bed and fan controls during a healthy active print. Failed sends
keep custom input or the preset modal for retry. Existing commands, heater names,
zero/off semantics and layout are preserved. This pass covers temperature/fan
controls; remaining motion, hotend activation and other operator dialogs remain
separate follow-ups.

The actual LVGL control audit passes five themes, three densities, normal/large
text and 1024/640/480 widths (480 uses a 400px-high viewport). Additional callback
hooks cover deletion, printer changes, nested actions, repeated external teardown,
failed-send retry and shutdown/printing gating. Firmware and physical print-time
adjustment tests remain pending.


## Applied motion/hotend patch review and native Toolhead form (2026-10-12)

The user-supplied `apply_motion_hotend_dialog_safety(1).py` is the local applied
baseline after checkpoint 62b1962, not yet published. Its exact source payload was
reviewed and its actual LVGL Toolhead safety, hotend layout and temperature/fan
control audits passed. The payload adds checked motion dispatch, printer-owner
and modal epochs, duplicate suppression, external-delete cleanup, hotend rejected
send retry and telemetry-confirmed activation with a bounded confirmation timeout.
Hotend layout tests now include Studio. Physical motion/activation tests remain
pending; passing host checks is not a firmware build or panel confirmation.

Toolhead Control now uses the common native form with a viewport-clamped popup,
a scrolling body and pinned Close action. XY and Z sections use two columns at
the native panel size and stack at narrower widths. Jog controls retain a cross
layout; step, extrusion and runtime-offset controls use native grids. All action
labels fit on one centered line. Position/homed/offset readings use content-height
labels rather than overlapping fixed coordinates. Existing commands, selected
step behavior and the applied patch's dispatch/cooldown/owner checks are retained.

The actual Toolhead audit now verifies button fit/centering, panel bounds, no panel
overlap, pointer scrolling, pinned footer and reachable final controls for all
five themes, three densities and normal/large text at 1024x600, 640x600 and 480x400.
Command assertions verify XY/Z jog, extrusion and runtime Z-offset routing.
The new layout still requires firmware/panel testing. No merge, push or release
is performed by the installer; branch identity remains 7.0.0-dev.

## Expanded Toolhead steps (2026-10-12)

Z jog choices: 0.01, 0.05, 0.10, 0.50, 1, 5, 10 and 25 mm. Runtime Z offset has paired adjustments of 0.005, 0.01, 0.025, 0.05 and 0.10 mm plus Reset. Extrude and Retract now use the selected 1, 5, 10, 25 or 50 mm length at the existing F300 speed. Default extrusion remains 10 mm. The legacy command formatter retains its 10 mm default. Responsive scrolling, pinned Close and dispatch/lifetime safeguards are retained.

## Toolhead tabs without panel scrolling (2026-10-12)

Toolhead Control groups Move, Filament and Z Offset in three tabs in a 960x568 viewport-clamped dialog. All options are retained, with 52-pixel touch buttons and a pinned Close. Tab changes only change visibility, preserve selections and do not dispatch commands or rebuild timers. The native 1024x600 panel needs no scrolling in any theme, density or text size. Narrower displays retain scroll fallback when necessary. Live float readings use snprintf before setting label text, independent of LVGL float-format support.

## Filament Recovery dispatch and native form (2026-10-12)

The user confirmed the Toolhead Move/Filament/Z Offset tabs look good on the panel. Filament Recovery now uses a native form with pinned Close and a three-column action grid. All recovery controls fit at 1024x600 in every theme/density/text size, with scroll fallback for smaller screens. Heat/Unload/Load/Purge/Resume routing and telemetry/macro/homing/sensor prerequisites are retained. Dispatch is guarded against reentrant actions; rejected sends retain the temperature for retry. Popup epoch and printer generation fence post-send updates, deletion clears every field and timer, and accepted-action cooldown survives close/reopen on the same printer. Offline and switched-printer views do not present cached sensor readiness as current. No Klipper macros are changed.

## Whole-print Cancel confirmation safety (2026-10-12)

The user confirmed Filament Recovery looks good on the panel; physical runout/load/purge/resume tests remain pending. Whole-print Cancel now owns its sender and printer generation, rechecks readiness and active printing/paused state, and guards reentrant dispatch. Failed checked sends keep the confirmation open with inline retry feedback. Accepted sends close only the original, still-current modal and suppress repeat cancellation on the same printer for five seconds, including reopen. External deletion retires the sender and status references. A printer switch replaces an existing stale review on reopening. The bounded wait prevents immediate repeats and is not cancellation-completion telemetry. Cancel Object remains for the next pass.

## Cancel Object retry and dialog safety (2026-10-12)

The user confirmed the whole-print Cancel confirmation looks good on the panel; actual cancellation remains pending. Cancel Object now has checked-send retry feedback, a dedicated captured sender, a dispatch guard and modal epoch/printer-owner checks before post-send cleanup. Accepted exclusions suppress immediate repeats of the same object on the same printer for five seconds, including reopening; this is dispatch suppression, not telemetry confirmation. Validation uses a separately allocated fresh exclusion snapshot so a failed request or changed live ordering cannot overwrite the displayed list/row data. The reviewed object name remains the command target. Offline, inactive-job, switched-printer and removed/already-excluded objects are blocked with inline feedback. Deletion clears the confirmation status and sender; reopening after a printer change retires the old list. Map/list layout and pinned actions are retained.

### Filament sensor dialog dispatch safety

Filament sensor toggles now use checked dispatch when available and retain accepted requests by printer generation and sensor object name across popup closure. Failed sends leave the action available with inline feedback. Live acknowledgement clears pending state; a five-second elapsed timeout reports an unconfirmed update before allowing retry. Named rows, captured legacy sender, event target and popup epoch guards prevent reordered sensors, duplicate callbacks, printer changes and close/reopen callbacks from redirecting actions or writing to destroyed widgets. Popup deletion clears its timer, sender, row pointers and feedback while retaining accepted-request suppression.

### Direct Printer action dispatch safety

Home and Pause now keep failed sends retryable and show explicit acceptance/failure feedback. Accepted requests have a five-second duplicate guard that survives page recreation; Pause acknowledgement clears its pending guard. Home acceptance is not treated as completed homing. All six Printer action targets are bound to the displayed printer generation and retired on deletion. A callback cannot reuse another button to dispatch a command, nor issue a nested action while a direct send is in progress. Post-send notifications are fenced by page epoch and printer generation, protecting close/reopen and printer-switch callbacks. Resume recovery and Object/Stop confirmation flows retain their existing behavior.

### Speed/Flow tuning dispatch and lifecycle safety

Speed and flow slider requests are bound to the displayed printer generation and owned slider targets. Rejected sends keep the selected input available for retry and display failure feedback. Accepted requests suppress identical release callbacks for five seconds and reconcile with live telemetry; an unconfirmed request times out to the reported value with explicit feedback. Requests survive same-printer page or Studio tuning popup recreation but are discarded on printer changes. Send, deletion and notification callbacks are fenced by epoch; sender/field references retire on owner deletion, and deleting an older Studio card cannot clear a replacement page. The Tune opener is also bound to the current card. Geometry remains unchanged.

### Diagnostics safety batch: Probe Accuracy, Endstops, Motion Diagnostics

Probe Accuracy now captures popup callbacks and uses a modal epoch, owned opener/Run targets and a dispatch flag that survives deletion. It preserves accepted-request suppression for five seconds across reopening, retains rejected sample selections, revalidates readiness after callbacks and avoids overwriting a replacement calibration session after rejection. Popup field deletion retires references and card deletion closes its popup/timer. Live Endstops now guards request reentrancy and checks printer/epoch after request callbacks so a retired query cannot write into a replacement popup or mark its controller state failed; startup/disconnected states are blocked, and deleting its body stops polling. Motion Diagnostics retires editable fields, closes editors whose targets disappear, fences controller refresh callbacks and binds diagnostic openers to their card/printer. Retired cards cannot close replacement views. Commands, diagnostic calculations and layouts are unchanged. This batch follows the verified Speed/Flow tuning patch.

### Command-surface safety batch: Drybox and Macros

Drybox commands now use checked-send feedback and printer/page ownership with a reentrant dispatch guard. Rejections keep the previous program; accepted selections are not presented as an active program or heater state until telemetry confirms them. Identical accepted commands are suppressed for five seconds across page reopening, while different commands (including Stop) remain available. Page and field deletion retire all references before widget release, and older pages cannot clear replacement controls. Drybox startup/error/offline states gate commands consistently. Macro execution captures its checked sender before logging, fences logging callbacks and preserves the reviewed command for failed-send retry. Accepted identical reviewed commands have a five-second reopen guard. Deleted parameter/editor fields cannot remain editor targets or be read during Review; root printer ownership gates stale rows, and editor/review transitions fence replacement callbacks. Existing layouts and macro parameter syntax are unchanged.

### Monitoring-surface safety batch: Console, Event History, Telemetry

Console now captures its checked sender before command logging and fences both command and failure-log callbacks; history retrieval cannot write to a retired or replaced editor. Deleted command/search inputs detach their keyboards, and deleted editor fields retire their references. Literal command handling, retained rejected input and connected-host recovery commands remain available. Event History fences log reads and Clear callbacks, prevents nested rebuilds, ignores retired timer invocations and stops polling when its list is deleted. Hold/Follow handles a missing action safely. Telemetry binds actions to owned controls and retires deleted selectors/buttons/chart hosts; chart data callbacks are fenced before series-buffer writes, and deletion of chart-owned fields retires the chart model. Refresh cannot paint old data into a replacement panel. Existing layouts and graph/data semantics are unchanged.


### Settings-operation safety batch: Backup, Network, Firmware Update

Backup actions serialize synchronous backend calls and reject overlapping actions while encrypted preparation, encryption, verification or writing is active, including after closing and reopening the parent. Failed removal retains its confirmation for retry. Passphrases are copied into temporary buffers before backend callbacks and cleared afterward; backend results and progress polling cannot repaint replacement dialogs or attach a stale encrypted restore confirmation. Dialog and individual field deletion retire references, detach keyboards and preserve only the verified restore passphrase required by the active confirmation. Closing the UI does not cancel the encrypted worker.

Network dialogs retain shared field-ownership records until all child deletion callbacks finish. Deleted WiFi/port inputs detach keyboards; deleted scan labels, lists, selected rows and password feedback retire their references. Owned WiFi Connect and port Save reject nested dispatch and missing inputs. Port saving captures its callback and rejects a replacement dialog created during teardown.

Firmware progress rejects duplicate or disabled cancellation callbacks, preserves cancellation-pending state when shown again and retires individually deleted progress fields. URL inputs detach their keyboards; deferred starts reject retired/replaced editors and running updates. Release catalog reads use local entry copies and fence backend callbacks before painting rows or selections. Catalog polling and refresh reject nested or retired calls; deleted browser fields retire polling/control references. Teardown releases old ownership before deletion, preserving replacement dialogs. Existing transfer verification, OTA worker cancellation/restart semantics, layouts and firmware version remain unchanged.


### Preference-dialog safety batch: Reset, Time Zone, Theme/Custom Theme

Factory Reset serializes erase requests, checks the NVS erase result before requesting restart, retains a failed confirmation with inline retry feedback and suppresses further erase requests after success. Reset review text remains visible with error feedback; success still uses the existing erase, delay and restart sequence. Time Zone captures its notification callback, fences persistence against dialog replacement, closes the accepted original before notifying and keeps failed selections retryable with inline feedback. Repeated show calls preserve the displayed dialog's callback.

Built-in, Operator Shell and custom theme selections reserve the deferred rebuild before persistence. Queue reservation failure does not change the selected theme; persistence failure cancels its reserved callback and displays retry feedback. Shared dispatch guards reject nested preference operations. Active custom-theme removal reserves the rebuild before deleting; rejected removal retains the reviewed name and an inline error. Custom row/action and theme preview references retire on individual deletion, and action targets must belong to the displayed chooser. Dialog teardown retires parent ownership before closing children, and an old queued theme callback cannot close a replacement chooser. Chooser preview caching, opaque scrolling, native layouts and theme definitions are retained. Branch/version remain feature/profile-setup-dialogs / 7.0.0-dev.


### Live-appearance lifecycle safety: Accent, Density, Accessibility

Live appearance actions are bound to the displayed dialog kind and exact owned row, including enum bounds and disabled-state checks. Independent deletion retires the body, feedback and control references. Shared dispatch guards cover persistence and accepted refresh notifications. Closing releases ownership before widget deletion; accepted changes notify once after an external close but do not revive the closed chooser. Refresh fences deletion and notification callbacks, preserves callback-created replacement UI and does not restore an old scroll anchor into a replacement chooser. A repeated show of the same kind preserves its captured callback; a request for another kind retires the original before opening the new kind. Existing queue-before-save behavior now includes visible failure feedback, and rejected saves remain retryable. Popup allocation failure clears kind/callback metadata. Live refresh/reopening, accessibility semantics, opaque scroll rendering, spacing, fonts and palette definitions remain unchanged.


### Notification-surface lifecycle safety

Notification preference Save captures its proposed level and callback, serializes dispatch, fences storage callbacks against dialog replacement, and retains rejected saves for inline retry. Toggle and Save validate owned controls; deleted level buttons, selected labels, status and Save references retire independently. Successful Save closes only the original preference dialog before notifying. Closing retires ownership before widget deletion.

Transient confirmation timers validate current timer ownership, and old surface deletion cannot clear replacement controls/timers. Notice dismissal removes the reviewed queue item before teardown callbacks so a newly published notice cannot be popped in its place. Surface rebuilds reject callback-created replacements. Notice Open captures the printer-chooser action, blocks nested navigation/dismissal, validates owned body/action controls and rechecks printer generation after dismissal and page-navigation callbacks. Event-log callbacks that retire the queue or switch printers cannot resurrect or misattribute their old notice. Displayed notice text uses a copied record while building widgets. Existing Quiet/Standard/Detailed policy, persistent warnings/faults, debounce, priority, four-notice capacity, profile identity and Event History logging remain intact. Brightness/sleep persistence remains for a subsequent pass.


### Display-setting lifecycle safety: Brightness, Persistence, Sleep/Wake

Brightness preview and release callbacks validate their exact current slider, event and page, serialize display/storage operations, capture the applied value before persistence and retire deleted controls/labels. Failed hardware application rolls the current slider back to the last applied brightness with visible retry feedback; failed storage retains the live brightness and previous saved value for release retry. Settings page deletion retires public/current references and associated dialogs before widget release. Refresh reconciles replacement controls with current brightness/timeout while avoiding writes during an operation or after a replaced system-info callback.

Sleep timeout cycling persists the proposed value before advancing runtime state; failures retain the previous timeout and show retry feedback. Sleep polling validates its owned timer. Wake keeps the display asleep and retains its first-touch overlay when the hardware restore fails, allowing another touch to retry. Overlay deletion restores the backlight, or recreates a retry shield if restoring fails; deletion during backlight-off is reconciled after the hardware call. Failed backlight-off removes the overlay without entering sleep. Successful wake resets activity and asynchronously removes the shield. Restored preferences are serialized, leave a sleeping display dark until wake, and wake when the restored timeout is Off. Timeout choices, brightness range, overlay touch behavior and page layout remain unchanged. Identity remains feature/profile-setup-dialogs / 7.0.0-dev.


### Information-surface lifecycle safety: System Info and About

System Info bindings attach deletion hooks to each owned label; deleting an older label cannot retire a replacement binding. Refresh serializes nested calls, snapshots the binding epoch, reads only metrics needed by currently bound fields and fences SDK queries and field updates against deletion, unbinding or rebinding. A replaced binding receives data only from a subsequent current refresh. No-op text updates, memory-health formatting and uptime formatting remain unchanged.

About captures firmware identity and application metadata before creating its widget tree. Nested show calls are suppressed during construction, and Close during metadata retrieval cancels construction without leaving a partial popup. Application metadata is copied before use. About retains its current layout, credits and build/channel/source fields; closing old objects cannot clear a replacement popup. Settings-page retirement now closes its About dialog alongside its other owned dialogs. Branch/version remain feature/profile-setup-dialogs / 7.0.0-dev.
