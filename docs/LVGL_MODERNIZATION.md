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


### Recovery-dialog safety: Klipper Restart and E-stop Feedback

Restart reviews now capture their sender and printer generation, validate their owned action/message fields and serialize dispatch across popup deletion/recreation. Rejected requests retain inline retry; accepted restart requests suppress repeats for five seconds across reopening on the same generation. Acceptance is not restart completion. Stale same-name printer reviews cannot send, and sender/field/owner changes during callbacks cannot repaint replacement dialogs. Popup cleanup releases references before deletion; individual title/message/Restart deletion retires its reference.

M112 still dispatches before feedback construction, with no accepted-request cooldown, connectivity precondition or dependency on restart dispatch. Only nested callbacks for an already-dispatching stop are suppressed; a stop remains available during restart dispatch. Result feedback fences printer changes and replacement dialogs, and an accepted restart guard does not hide the stop's halt-verification instructions. Existing M112/FIRMWARE_RESTART commands, emergency HTTP/WebSocket routes, native popup layout and transport busy-slot bypass are unchanged. Controller reboot behavior is unchanged. Branch/version remain feature/profile-setup-dialogs / 7.0.0-dev after checkpoint 1716f0b.


### Controller restart/reset safety

The Settings controller reboot action now opens a native Reboot Controller confirmation with pinned Cancel/Reboot actions rather than restarting on the first tap. Its opener is bound to the current Settings action and retires on deletion. Reboot and Factory Reset recheck running OTA and encrypted-backup worker states at confirmation, block preparation/encryption/verification/writing with inline retry feedback, and fence those backend queries against modal replacement or deleted approval fields. Both operations share one-shot accepted restart suppression and the preference dispatch guard. Confirm-field/status deletion retires references; Settings cleanup closes the reboot review. Cancel and blocked actions do not erase or restart anything. Factory Reset retains its existing erase-result handling and erase/delay/restart sequence. Emergency-stop and Klipper recovery paths are unchanged. Identity remains feature/profile-setup-dialogs / 7.0.0-dev.


### Shared popup/footer and keyboard safety

The shared popup footer now retires independently deleted buttons and footer containers, permits a deleted slot/footer to be recreated, and skips missing action labels during resizing. Recreating a footer retains one popup size callback. Popup teardown removes context-backed child callbacks before releasing modal metadata, including blocker-first and owner-first deletion. Both native-form and aligned-popup keyboard constructors now share a binding that detaches a deleted text input and removes the peer deletion hook when either object is destroyed. A caller-installed replacement textarea is preserved; direct LVGL rebinding remains caller-owned. Binding allocation failure removes the unfinished keyboard. Layout, commands and visual styling remain unchanged. Branch/version remain feature/profile-setup-dialogs / 7.0.0-dev after the controller restart/reset batch.


### Bed Mesh profile field and action safety

Saved-profile rows and Save As/Load/Remove/confirmation/Continue actions are bound to their owned controls and reject disabled, deleted or unrelated event targets. A confirmation also retains whether the reviewed operation was Save or Remove. Individually deleted rows, list, name input, feedback and Continue retire their references and advance the modal epoch, fencing transport callbacks that remove fields before returning. Deleting the selected row clears its selection. Editor continuation cannot advance an old value after teardown opens a replacement editor. Closing the manager retires all old popup references before deleting any of them; full owner cleanup clears the old session before callbacks can initialize a replacement owner. Replacement dialogs retain their own fields, sender and reviewed name. Existing copied profile-name snapshots, checked dispatch/retry, printer-generation guards, commands, native layouts and visual styling are retained. Branch/version remain feature/profile-setup-dialogs / 7.0.0-dev after the shared popup/footer and keyboard batch.


### Setup Center operation and field safety

Wi-Fi Connect now validates its owned action/keyboard, captures the SSID, password and sender before dispatch, suppresses nested dispatch/navigation, clears its temporary password copy and fences post-send feedback/verification by the Setup epoch. Repeated Show preserves the sender of the open form; rejected connections retain credentials for retry. Setup fields, actions, rows and navigation references retire on individual deletion; loss of a content/list container retires polling. Poll callbacks reject unrelated timers and retired sessions. Full close retires old ownership before deletion so a callback-created replacement Setup session survives.

Camera selection cannot move to another row while its test is pending. Use camera requires the owned enabled action and a verified result; default-camera and stream-URL persistence failures retain inline retry instead of advancing Setup. The two persistence writes are not transactional: if default selection succeeds but URL saving fails, feedback explicitly reports the partial save. Captured profile/entry, dispatch and generation/epoch checks fence callbacks that close/reopen Setup or change the active profile. Completion validates its owned action, suppresses nested saves, retains failed completion and avoids overwriting a replacement session. Discovery workers remain independently owned; closing Setup stops its UI polling, not an active network worker. Native layouts, commands and visual styling are unchanged; branch/version remain feature/profile-setup-dialogs / 7.0.0-dev after the Bed Mesh profile field/action batch.


### Files browser control and callback safety

Files Search, Sort, Refresh and Up validate the owned enabled control and current printer generation; nested browser/print actions cannot redirect callbacks. Search owns its input, keyboard and footer actions, retires deleted fields, captures the query/sender/page before teardown and suppresses old dispatch if a deletion callback replaces the page or editor. Toolbar labels and list references retire on deletion. File/folder/preview callbacks receive copied paths that remain valid if the callback clears rows or replaces the page. Lazy preview iteration fences each external request by the Files epoch, preventing traversal of a freed row; preview dispatch has its own guard so synchronous browser redraws can still enqueue thumbnails. Individually deleted thumbnail fields retire their references, and row cleanup removes field hooks before freeing row metadata. Folder/row events ignore bubbled child lifecycle events before reading row metadata. Clearing old rows no longer unconditionally discards a callback-created replacement row list. Existing print/detail confirmation behavior, file-list workers, layouts and styling remain unchanged. Identity remains feature/profile-setup-dialogs / 7.0.0-dev after the Setup safety batch.


### Owned loading/empty/offline/error panel safety

The shared page-state helper now provides an owned constructor that clears its caller's persistent handle when the panel root or parent is deleted. Deleting an older panel cannot clear a replacement using the same owner slot. Files, currently the helper's only production consumer, uses that constructor in every theme. Icon, title, detail and spinner deletion retire their references; root teardown removes child metadata hooks before freeing the state. Active Show/Hide calls retain metadata until unwinding, and update epochs prevent an interrupted outer Show from overwriting nested Show/Hide or a replacement panel. Individual UI updates check current ownership; unchanged text/color uses the existing value helpers. The existing unowned constructor remains available for callers whose object tree is known to be alive; owned-slot storage must outlive its panel. Native presentation, status messages and file/print behavior are unchanged. Identity remains feature/profile-setup-dialogs / 7.0.0-dev after the Files browser safety batch.


### Moonraker discovery dialog/session safety

The existing Discover action in the printer-profile editor (also opened through Setup Center's Printer step) now retains its original callbacks/status on repeated Show. Retry/Close validate owned enabled actions; endpoint selection validates its owned result row, captures endpoint strings and sender before dispatch, blocks nested selection/retry and rejects changed printer generations. Independently deleted status/list/action fields retire references; result-list deletion cancels the UI scan. Explicit Close clears the old session before DELETE/close callbacks can create a replacement, clears callbacks after use and suppresses nested Close notification. External owner deletion retires the dialog without replaying old callbacks.

Scan tasks capture the dialog epoch; closing/reopening or changing the printer cancels old publication. Candidate publication rechecks cancellation under the display lock, and worker status publishing retries a transient display-lock miss while the same scan remains current. Public status updates capture the session before waiting for the lock, recheck after acquiring it and tolerate a deleted status label. Status-buffer writes and LVGL updates share that lock. Scan ports, probing sequence/TLS behavior and task lifetime remain unchanged; no Moonraker discovery control is added to the Network page. Identity remains feature/profile-setup-dialogs / 7.0.0-dev after the owned page-state batch.


### Printer-profile draft/test/authentication safety

Connection tests now capture their exact editor, profile index, generation, host, raw port text, API key and security mode. One owned Test action and poll timer suppress duplicate/nested requests; draft or owner changes retire polling instead of applying an old result or identity suggestion. Copied request data survives callbacks that close/reopen the editor, and temporary/snapshot credential copies are cleared on retirement. Result consumption checks the current timer and session before and after the backend call. The endpoint-test worker/probing behavior remains unchanged.

Profile fields and the Test/Use Key/Done controls retire independently deleted references. Generic keyboard completion validates its actual action/keyboard and live target, copies the edited value before writing and fences replacement callbacks before cleanup. Authentication closure retires its old fields before deletion; applying a key cannot repaint a replacement dialog. Editor/full-manager cleanup captures all old dialogs/timers, retires their shared ownership and credentials first, then deletes only those captured resources. A replacement opened during DELETE retains its fields, callbacks and camera setup ownership. Save's post-close notification path also checks its expected epoch before rebuilding/status/callback updates; the existing persistence sequence and validation remain unchanged. Native layout and presentation remain unchanged. Identity remains feature/profile-setup-dialogs / 7.0.0-dev after the discovery-session batch.


### Printer-profile Save/Use/Remove persistence safety

Profile Save now captures its slot, name, endpoint, API key, camera data and security setting before writing. Owned enabled controls and one dispatch guard reject nested Save/Use/Remove and discovery-field redirection. Each persistence stage checks its original editor epoch and active-printer context before continuing. Failed stages retain the draft with explicit feedback about what was already saved; the existing multi-write backend is not transactional and no rollback is claimed. Captured credentials are cleared before completion. Actual active-profile writes notify the captured connection callback even when a later stage fails or the old editor is replaced; camera-model refreshes reflect accepted camera writes without painting retired profile dialogs.

Use Printer captures the selected slot and callback, retains failed activation for retry and fences its status/row rebuild against a replaced manager. Removal captures the reviewed slot and configuration generation, rechecks that another configured profile remains and retains a failed review with inline retry feedback. Accepted removal invalidates the captured slot's health/preview resources and notifies once, without reading a callback-mutated manager selection as the deletion target. Deleted Save/Use/Remove controls and profile rows retire their references; row events validate their owned targets. Row rebuilds retire old row pointers before deletion and fence replacement lists. Repeated manager Show preserves the callbacks belonging to its open review. Native layout/presentation and backend persistence APIs remain unchanged. Identity remains feature/profile-setup-dialogs / 7.0.0-dev after the profile-draft batch.


### Printer-profile camera removal and cleanup safety (7.0.0-dev)

Camera removal now owns its opener, approval, cancel and feedback controls and captures the reviewed printer, numbered camera slot and configuration generation. Slot changes and camera writes/tests/search starts are suppressed while its review is open or removal is dispatching. Changed configurations require a fresh review; rejected removal retains the same confirmation with retry feedback. Accepted removal clears the captured draft slot, closes only its own review and refreshes camera consumers without painting a replacement editor. A removed feedback field does not leave an accepted removal available for duplicate confirmation.

Opening review retires pending camera polling so an earlier Find result cannot subsequently re-import the camera. New Find/Test requests can be started after leaving the review; workers themselves are not cancelled. Poll handlers reject unrelated timer identities. Deleted slot buttons and removal controls retire their references. Camera and review cleanup clear their owned references before deleting captured resources, preserving dialogs opened from DELETE callbacks and the replacement camera setup-active flag. Slot-label catalog reads also fence replacement sessions. Catalog removal still uses the existing multi-write persistence API; a failure can follow a partial write and no rollback is claimed. The prompt accurately describes stream/name clearing rather than promising view-setting deletion. This batch does not add controls or include visual polish. Branch/version remain feature/profile-setup-dialogs / 7.0.0-dev after the profile-persistence safety batch.


### Printer-profile camera draft, apply/default and result ownership (7.0.0-dev)

Use Camera, Default, Find, Test and Cancel now validate owned enabled controls. Apply/default and slot changes capture the printer, active printer, camera slot, name and URL before calling persistence. A dispatch guard suppresses nested writes/navigation. Failed saves retain their fields and selected slot for retry; a failed default selection after an accepted camera save explicitly reports that partial success. Slot 1 remains staged until Apply/Default/main profile Save, while additional slots retain their existing save-on-switch behavior. Invalid URL schemes are rejected before switching slots. Accepted writes refresh camera consumers even if the initiating editor closes, and old completions cannot close or paint its replacement. Accepted removal also clears the visible stream when an independently deleted status field changes the UI epoch.

Find/Test capture their starting dialog, printer/active printer, slot, generation and URL. Find also retains the endpoint, API key, security mode and camera name; changed details retire polling before result consumption/import. Starts and polls fence callback-driven closure and draft edits, pending duplicate requests retain their original poll, unrelated timers are ignored, and temporary/saved discovery credentials are cleared at retirement. Find and Test may still overlap. Slot changes, Apply/Default, removal review and deleted tracked fields retire old camera jobs; workers themselves continue to completion. Current Find imports may advance their own configuration generation. The imported primary URL is synchronized with the profile draft even when another camera slot is selected, and pre-import Test polling is retired. Slot loading/label updates also fence replaced dialogs and active-printer changes. The existing catalog/discovery multi-write persistence APIs remain unchanged; no atomicity or rollback is claimed. No visual polish, commit or push is included; branch/version remain feature/profile-setup-dialogs / 7.0.0-dev after the camera-removal safety patch.


### Printer-profile security and certificate picker ownership (7.0.0-dev)

Security, Standard HTTP, Select PEM, Cancel, picker Close and certificate rows now validate their live owned controls. The security review captures its editor, printer, active printer and configuration generation. A changed printer/configuration requires a fresh review. PEM imports copy the reviewed path and target printer before calling the backend; nested imports, profile saves and security-mode changes are suppressed during dispatch. Failed imports retain the picker and inline retry feedback. Successful imports select the secure draft only for the originating editor, preserve custom ports, and change the default 7125 port to 443 only when it is still unchanged. Feedback explicitly requires saving the profile to apply its transport draft; certificate import retains its existing immediate persistence behavior, with no backend transaction or rollback changes.

Security and picker teardown retire references, rows and path buffers before deleting captured popups, preserving replacement dialogs opened from DELETE callbacks. Independently deleted controls/rows/body/status retire their handles. SD enumeration fences closure, replacement, body deletion and configuration changes after directory operations; an opened directory is closed on an interrupted scan, and Close remains available. Overlong paths and directory components are excluded rather than truncated. Old test/poll jobs are retired when a security change/import starts. Layouts and transport/trust-store APIs remain unchanged. Baseline e38a6bd was fetched and all 32 previously delivered changed/new files were compared byte-for-byte before starting this batch. Identity remains feature/profile-setup-dialogs / 7.0.0-dev; nothing is committed or pushed.


### Printer-profile remaining controls and endpoint application safety (7.0.0-dev)

Manager Close/Edit, editor Cancel/Discover/Authentication/Camera, authentication Cancel and keyboard Cancel now validate their owned enabled controls. Independently deleted controls retire their references. Edit/Add's button wrapper rejects dispatch and deletion review, while the internal editor-opening routine retains the controlled replacement/public-routing paths. A nested Discover launcher is suppressed without blocking its synchronous endpoint-selection callback; other owned profile controls cannot dispatch during that callback. Keyboard Cancel accepts only its own footer or its own keyboard's Cancel event. Opening another field stops if retiring the previous keyboard opens a replacement editor/keyboard, avoiding reuse of the old field pointer.

The endpoint handoff copies host/identity strings before touching LVGL, retires old connection/camera tests, suppresses nested writes and fences the editor, epoch, target printer, active printer and configuration generation after each field update. It preserves custom names and does not persist the endpoint. Existing discovery bridges and scan-session lifecycle remain unchanged; this guards replacement during endpoint application. Manager selection/status and editor initialization also stop after callback-driven owner replacement. Editor initialization snapshots the selected profile, reloads its camera names instead of reusing another profile's cache, and clears the temporary credential snapshot once fields are built. This preserves primary-camera names when saving unrelated profile fields without opening Camera. No visual-polish or backend persistence API change is included. Baseline is the delivered security-dialog patch after e38a6bd; identity remains feature/profile-setup-dialogs / 7.0.0-dev, uncommitted/unpushed.


### Request-bound profile discovery handoff (7.0.0-dev)

Discover now issues a nonzero request identity bound to its originating profile editor, UI epoch, printer/active printer, configuration generation and host/port/name draft. Selection must present that identity and an unchanged owner/draft. Accepted application consumes the request before editing fields; cancellation, external scan deletion and editor closure retire the matching request. Old results and old Close callbacks cannot edit or cancel a newer request, including a reopened editor for the same printer. The synchronous endpoint helper remains available for callers already owning the current editor; both production discovery bridges now use the scoped API.

Moonraker discovery carries request identities in selection and Close callbacks. Repeated Show for the same request preserves its controls/callbacks; a different request retires the previous scan and opens the new review. Close-driven replacement dialogs are preserved and reported to launchers so they do not start work in the replacement. Selection closes only its own request's scan, including the Setup Center route. A cancelled old worker cannot publish into the new scan; Retry remains available after that worker finishes. Main and Setup capture the request before launching and recheck it before starting the worker after Show. Existing legacy discovery APIs remain compatible. Endpoint selection still changes only the draft; operator Save remains required. No scan algorithm, printer persistence, visual-polish, branch or version change is included. Baseline is the delivered editor-control patch after e38a6bd; new work remains uncommitted/unpushed at 7.0.0-dev.


### Printer chooser action, field and refresh lifetime safety (7.0.0-dev)

Decorative preview containers pass native taps to their card. Chooser cards and Manage now validate their owned enabled controls and reject bubbled/unrelated events and nested dispatch. Card activation captures callbacks and printer generation before its profile query, then rejects a replaced owner. Repeated Show preserves the callbacks belonging to the visible chooser. Root deletion now retires its timer, callbacks, card references and subjects just like Hide; cleanup clears ownership before deleting captured objects, so DELETE callbacks can open a replacement without the old cleanup clearing it. Independent deletion retires labels, preview containers/images, badges, hints and Manage handles. Card subject cleanup validates exact root identity, ignoring descendant deletion.

Refresh uses an owner/epoch/printer context, copies only display fields from profile data, rejects unrelated timer callbacks and stops at backend/observer boundaries when its owner changes. Missing widgets are tolerated, invalid preview dimensions cannot divide by zero, and a recreated image receives its source even when the revision is zero. Static subject storage is not cleared/reused until an active notification and retirement finish. A requested replacement waits for that boundary; chained replacements use a cancellable deferred Show instead of recursive reconstruction, and explicit Hide cancels it. Subject readiness is tracked separately from label binding flags so observer cleanup remains complete after a leaf is removed. Status/connection rules, card geometry and the redraw-free unchanged 500 ms refresh behavior are retained. No visual polish or printer persistence change is included. Baseline is the delivered discovery-request safety patch after e38a6bd; branch/version remain feature/profile-setup-dialogs / 7.0.0-dev, uncommitted/unpushed.


### Fullscreen preview owner, launch and result safety (7.0.0-dev)

Fullscreen preview Close accepts only its enabled owned background/image/hint and ignores unrelated or bubbled actions. Image and hint deletion retire their handles; deleting the image also invalidates its high-resolution request. Close/external root deletion retire references, request identity and owned buffers before root DELETE callbacks can open another overlay. Cleanup cannot reset that replacement. Opening snapshots the fallback before closing the previous overlay, including when the input is its own image/descriptor; if rendering or deletion opens a replacement, the interrupted Show frees its candidate and preserves the replacement. This temporarily retains one extra bounded fallback during replacement. Image-derived widgets such as Printer's canvas remain supported; non-image objects, symbols and file paths are rejected by the object API instead of being cast to image descriptors.

High-resolution launch copies the filename before closing an old owner, requires a successful new overlay, and fences its request/configuration after endpoint reads, queue/task creation and loading feedback. An invalid opening attempt cannot enqueue into an existing overlay. Current API-key snapshots are cleared after enqueue/discard and after worker consumption; the queue/HTTP backend still owns its accepted request. Worker publication rechecks ownership after decoding and each LVGL transfer/layout boundary. Stale candidates remain caller-owned or are retired with their replaced overlay; they cannot overwrite a newer preview. Existing 900x520 fit, three-attempt fetch/retry policy, fallback behavior, PSRAM-only buffers and native tap-anywhere interaction remain. No visual polish, fetch algorithm, public API or printer configuration change is included. Baseline is the delivered chooser safety patch after e38a6bd; branch/version remain feature/profile-setup-dialogs / 7.0.0-dev, uncommitted/unpushed.


### Reusable Files thumbnail component and fitting lifetime safety (7.0.0-dev)

Thumbnail images/placeholders retire independently deleted handles. Root teardown detaches the image source, removes child metadata hooks and clears an optional persistent owner slot only when it still points to that component. An in-flight update/click retains component metadata and its bounded PSRAM buffer until returning; nested component updates/clicks are suppressed. Rendering, child deletion and image updates stop after owner/field retirement. Label/canvas replacement retires the old reference before deleting the captured child, so cleanup cannot clear a newly installed handle. Main binds its asynchronous Files thumbnail handle to this owner slot.

The selected-file render path now explicitly records the displayed file alongside its image. Fullscreen taps use that copied filename rather than rereading the mutable global thumbnail selection. Empty, overlong or unscoped filenames can open the owned fallback without fetching another file's high-resolution image. Existing unscoped Show remains available. Clicks require the owned enabled box and visible image, reject bubbled/unrelated events and keep the component alive across lightbox dispatch. Shared scale/fit/fill helpers temporarily observe their borrowed image/container during layout and remove those hooks before returning; interrupted layout cannot leave a deleted object in use. Aspect, containment/cover geometry and the existing per-component 286x215 PSRAM allocation ceiling remain unchanged. No visual polish, fetch/persistence backend or printer command change is included. Baseline is the delivered fullscreen preview safety patch after e38a6bd; branch/version remain feature/profile-setup-dialogs / 7.0.0-dev, uncommitted/unpushed.


### Camera page and review control ownership (7.0.0-dev)

Configure, Retry, Fullscreen, fullscreen image exit, View/picker launchers, rotation/mirror/Reset options, camera rows and review Close now validate their owned enabled controls. Unrelated/bubbled events and nested page/review actions cannot dispatch persistence or redirect an in-flight operation. Page status/buttons, review feedback/Close controls and each option/row retire independently deleted handles. Review teardown clears these handles before deleting the captured popup, preserving a review opened by its DELETE callback. Optional page-control deletion does not prevent leaving fullscreen through the image.

View and picker actions capture their page epoch, profile/generation, selected camera, popup epoch and reviewed URL before catalog reads/writes. They recheck that context after backend boundaries and stream retirement instead of applying old work to a replacement page/review. Picker construction installs Close before catalog reads and stops an interrupted row build; View opening and selector-label reads also fence replacement. Configure and Retry suppress nested action dispatch; Retry remains nonblocking and stops if stream retirement replaces its page. Existing immediate camera-view/default persistence, failure retry, geometry, rotation/mirror behavior and transport APIs remain. This batch covers controls/catalog handoffs; frame-poll callback boundary review remains separate. No visual polish, commit or push is included. Baseline is the delivered thumbnail-component patch after e38a6bd; branch/version remain feature/profile-setup-dialogs / 7.0.0-dev.


### Camera poll, frame-transfer and transform boundary safety (7.0.0-dev)

Camera polling now rejects nested polls and unrelated timers, captures its page epoch/profile/generation/camera and rechecks after result consumption, catalog/default queries, stream retirement/start and UI publication boundaries. An interrupted poll frees only its untransferred candidate; transferred frames remain owned by their current page. Retry, view/default changes, Configure and fullscreen actions invalidate an in-flight poll before changing its stream/view context, so a synchronous action cannot let the old candidate repaint afterward. Internal immediate polling remains supported; a replacement page resumes on its own next poll when an old poll is still unwinding.

Frame cleanup captures the old buffer, retires its global ownership/cache/descriptor and detaches the image before freeing that captured buffer. A callback-created frame cannot be cleared or freed by the old cleanup. View transforms snapshot their owner/image/frame before catalog reads and stop between scale/rotation/centering operations if that context changes, avoiding stale dimensions and object access. Unavailable/timeout feedback also stops after replacement during retirement. Existing frame validation, pixel mirror/rotation, 100 ms polling, two-second unavailable threshold, status semantics, redraw-silence checks and nonblocking stream APIs remain. This batch does not alter the decoder, transport worker, visuals or persistence. Baseline is the delivered Camera control patch after e38a6bd; branch/version remain feature/profile-setup-dialogs / 7.0.0-dev, uncommitted/unpushed.


### Camera Show/Hide, catalog and setup handoff safety (7.0.0-dev)

Camera construction now creates all page fields before catalog/default callbacks. Both new and cached Show paths capture their owner across default selection, selector refresh and stream startup. Show installs its owned timer before backend callbacks but defers polling until persisted-default selection and nested Show calls unwind, so interrupted configuration changes still have a future poll without starting the provisional camera. Fullscreen opening proceeds only when that Show completed for its own page. Selector refresh also supports a legitimately owned hidden/setup-suspended page, while active frame/control operations continue requiring a visible unsuspended owner.

Hide retires its poll timer and hides the captured page before closing dialogs, then fences dialog deletion, stream retirement and frame release. A callback-driven Show cannot have its fresh timer/frame/visibility cleared by the old Hide. Setup suspension checks its operation epoch and latest active flag after dialog closure and explicit stream quiescence; a callback that resumes viewing wins over the interrupted suspension. Catalog refresh fences its queried page/selector before resetting a camera index or repainting it. Existing public APIs, cached page behavior, persisted default/fallback, geometry and explicit setup quiescence remain unchanged. No visual polish, transport/persistence change, commit or push is included. Baseline is the delivered Camera polling patch after e38a6bd; branch/version remain feature/profile-setup-dialogs / 7.0.0-dev.


### Dashboard camera consumer closeout (7.0.0-dev)

Dashboard camera polling now owns its root/epoch/profile/generation, rejects unrelated timers and nested polls, validates RGB565 dimensions/byte counts and discards interrupted candidates. Catalog/default reads use a copied URL and fence replacement before stream startup. Profile-generation changes retire the previous frame/result before rebinding. Buffer retirement clears ownership before detaching the source; old cleanup cannot free a callback-created replacement frame. Repeated status text uses the existing no-op value helper. Root deletion retires all Dashboard references, timer and frame before backend callbacks; independently deleted image/toggle/preview/status fields retire their handles. Toggle/fullscreen actions validate their exact enabled targets and suppress nested action dispatch. Catalog refresh/quiescence retain the intended current-owner behavior; setup/OTA and camera transport APIs remain unchanged. Construction performs its camera catalog refresh last, after Dashboard fields are initialized. Existing page layout and image fit are preserved. No visual polish, commit or push is included; baseline is the Camera lifecycle handoff patch, feature/profile-setup-dialogs / 7.0.0-dev.


### Active Print canvases and Dashboard render caller closeout (7.0.0-dev)

Active Print tracks its panel, preview box/label, footer/remaining fields and canvas, retiring deleted child handles and removing metadata callbacks before root release. In-flight updates/clicks retain metadata until unwinding; nested writes are suppressed. A replaced/older panel cannot clear the current panel/canvas. Canvas creation, dimension/stride rebinding and fit now share one guarded owner path. Fullscreen clicks validate the owned enabled preview well/visible canvas and dispatch a copied filename. Explicit canvas deletion retires handles before deletion callbacks. A component generation invalidates interrupted Dashboard callers, including cached-image reads and render completion. The persistent shared RGB565 buffer remains intentionally retained across page rebuilds for the worker; allocation is bounded to 286x215, with the existing shared-buffer internal-RAM fallback retained.

Dashboard synchronous/worker renders use a bounded temporary PSRAM candidate, verify component/profile/generation/file after rendering and copy pixels only for a current request. A stale render no longer writes directly into the visible shared buffer. Failed temporary allocation preserves the existing preview. Main's former direct canvas construction paths now delegate to the component; render UI polling validates its owned timer and current file/profile. Cached restores reject missing data/invalid dimensions/stride/length, copy their filename, and fence invalid-preview feedback against replacement. The preview coordinator captures and clears all four old object slots before deleting captured objects, preserving callback-created replacement slots and avoiding duplicate deletion when slots alias. Existing renderer sampling/aspect policy, cache persistence and public operator flows remain. No visual polish or printer commands are added; baseline is the Dashboard camera closeout patch, feature/profile-setup-dialogs / 7.0.0-dev.

The planned pre-polish code batches are complete after both closeout installers. See MODERNIZATION_CLOSEOUT.md for host coverage and remaining device acceptance/checkpoint steps. This is a source/host closeout, not a claim that firmware/device soak tests have run here.

## Visual polish: printer chooser list (7.0.0-dev)

After the modernization checkpoint `ed86f07`, the chooser's two-by-two card grid is replaced by four full-width, flat rows. Names/endpoints share a left column; live state, active marker and Open/Set up affordances align on the right. The active row has an accent edge and a subtle theme surface; separators replace card outlines. Cached previews fit a compact 64px well; absent previews use a file symbol and empty slots use a plus. Long names/endpoints truncate to a fixed single line instead of continuous marquee motion. All four slots remain on screen; Studio keeps its 976×424 shell frame and other themes keep their existing page frame.

Selection, Manage, empty-slot setup, status policy, cache ownership, subject retirement and the guarded 500ms timer retain the existing contracts. The removed preview caption is superseded by the row name/status. The chooser audit covers flat-row bounds, non-overlapping columns, active-row transitions, overflow, native taps and unchanged-refresh silence across all five themes, three densities and both text sizes. Firmware build/flash and panel acceptance remain local workflow steps.

### Chooser visual revision: familiar two-column layout

User review rejected the full-width list. `apply_printer_chooser_familiar_layout.py` restores the prior two-column/two-row positions and 116px preview wells, with name, endpoint, status and action back in their familiar positions. The sections use the page background, a thin lower separator and an active accent edge; enclosing card borders, rounded card surfaces and selected-section fill remain removed. The list-polish single-line ellipsis/quiet refresh fix and ownership guards remain. All four profiles are visible without scrolling.

The actual chooser audit passes all five themes, three densities and both text sizes with pairwise section/label bounds, native pointer taps, active transitions, long-name/host quiet refresh, preview ownership and existing interrupted-lifetime checks.

### Chooser detail polish: separators, status and placeholders

`apply_printer_chooser_detail_polish.py` builds on the accepted familiar two-column layout. A non-interactive, theme-colored 1px center separator is inset below the header and above the lower edge. Lower rules and Open/Set up/Active baselines remain aligned. Known live/connection status strings are presented in sentence case (Printing, Paused, Ready, Connecting..., Offline / retrying); unknown strings pass through, and raw printer/health state, freshness and status colors retain their existing policy.

Empty-slot titles use the existing dim text token and their Not configured status uses the caption font, keeping configured printers visually prominent without introducing lower-opacity text. Preview placeholders use Montserrat's 32px symbol glyphs in every theme so Studio no longer falls back to tiny icons; cached thumbnails retain the same centered 108px fit inside the 116px well.

Actual chooser host checks pass the full 5-theme × 3-density × 2-text-size matrix, including center/lower separator geometry, action baselines, 216×108 preview aspect fit, placeholder glyph size, empty/configured transitions, raw-to-display status, unchanged refresh silence and existing action/observer lifetime cases.

## Visual polish: Manage printers

`apply_profile_manager_visual_polish.py` follows the accepted chooser detail polish. The manager now uses flat, single-line name/endpoint rows with a local selection accent and a separate Active marker for the currently active printer. Empty slots use quieter Add printer/Available profile slot wording. Names and endpoints use ellipsis instead of animated marquees. The selected slot drives an explicit Edit printer or Add printer footer label.

Close, Remove, Edit/Add printer and Use printer are all in the pinned wrapping footer; Remove no longer sits below the scrollable profile list. Status feedback stays in the scrollable body and avoids repeating the endpoint already shown on each row. Existing selection persistence, callback ownership, discovery, editor and removal-confirmation contracts remain; this patch does not add a connection probe or change activation policy. Editor/dialog visual polish remains the next small patch.

## Profile editor scroll rendering follow-up

Panel review reported slow scrolling in Edit printer. The editor had a transparent scroll body, and both manager/editor used the top layer. LVGL 9.5 refreshes the top layer unconditionally after the active screen, so making the body opaque alone did not eliminate parent-popup redraws.

`apply_profile_editor_scroll_performance.py` moves the manager/editor to the existing screen-local modal constructor and gives only the editor scroll body an opaque surface matching its popup. Setup Center uses the same constructor so the editor opened from Setup remains above its launcher. Nested authentication/security/keyboard/discovery dialogs keep their existing top-layer placement and ownership; footer actions, form geometry, scroll behavior and saved values retain their contracts.

In the real-LVGL host diagnostic at 1024×600, comfortable density and standard text, six 24px editor scroll steps produced 36 parent-popup draw callbacks in each non-Studio theme and 40 in Studio before the change. Afterward all five produced zero parent-popup draw callbacks while the body still rendered 36/40 times. This measures occlusion/culling, not device FPS or a guaranteed speedup factor. The regression audit repeats scrolling across the existing theme/density/text/viewport matrix and keeps the footer coordinates fixed. Native pointer checks verify screen-local modal blocking and top-layer nested authentication ordering; Setup/shared-modal audits cover the launcher and ownership paths. Physical panel responsiveness remains the acceptance check.

## Visual polish: grouped profile editor

`apply_profile_editor_visual_polish.py` builds on the accepted scroll-performance patch. The main editor groups each caption with its field and places Moonraker host/IP and port together in a responsive connection row. Security/access controls share a wrapping action row. Sentence-case field/dialog/action labels and a clear Test connection footer replace the uppercase form labels. The main form fits the native panel in the tested theme/density/text combinations, while small viewports retain scrolling and a pinned wrapping footer.

Connection security is now a stable action label, with the security dialog describing whether the current draft uses HTTP or HTTPS/WSS; selecting/importing certificates retains the existing draft/persistence behavior. API-key/keyboard headings and actions use the same wording style. No connection, validation, persistence, keyboard, certificate or discovery policy changes. The opaque editor viewport and screen-local modal placement remain.

The profile host audit adds main-form visibility, host/port alignment, five-digit port legibility and body-action text-fit checks. Its rendering check scrolls when content overflows; when the condensed form fits, it invalidates/redraws the viewport instead, still requiring zero parent-popup draw callbacks and pinned footer coordinates.

## Visual polish: profile camera setup

`apply_profile_camera_visual_polish.py` follows the accepted grouped editor. Camera slots have static, single-line ellipsized names and a separate Default/Available slot caption; selection uses the current theme accent and remains selected on repeated native taps. The slot grid keeps four columns at panel width and two at narrow viewports. Name/URL captions are grouped with their fields, action labels use sentence case, and the main camera popup uses the same opaque, screen-local scroll path as Edit printer. Keyboard and removal review remain top-layer nested modals.

The host profile audit checks slot geometry, marker separation, Default versus selection, full action-label fit, camera viewport redraw culling and native modal input/order alongside the existing camera save/default/remove/discovery/test lifetime scenarios. Hardware acceleration was reviewed during this batch; findings and the proposed separate PPA experiment are recorded in GRAPHICS_ACCELERATION.md. This camera patch retains the current accelerator configuration.

## Files scroll redraw containment

`apply_files_scroll_redraw_performance.py` follows the PPA fill trial. Files scrolling was reported laggy with PPA enabled; physical Camera performance is untested because no camera is connected. The Files list itself was transparent in all five themes. Actual LVGL host rendering reproduced 21–33 parent-page and viewport draw callbacks across six 18-pixel scroll steps. Making only the scrolling list opaque with UI_BG, no gradient and square corners reduced both parent counts to zero; visible list draws remained 21–33. This is redraw containment evidence, not a device FPS or latency measurement.

Both Studio and the responsive themes use the same list-surface helper. Rows, thumbnail requests/ownership, workers, sort/search, detail/print review and input callbacks are unchanged. The Glass theme's list area becomes a solid UI_BG surface rather than exposing the page gradient; the surrounding page retains its theme. The existing Files fixture now tests actual scroll drawing and native drag cancellation in every theme/density/text setting before its responsive/lifecycle matrix.

## Accepted fill-only PPA default

Following the cache handoff correction, the user confirmed the Dashboard blocks were gone and scrolling was fast and smooth. The supported IDF6/Hosted3 wrapper now defaults to that exact fill-only PPA renderer and its two guarded LVGL backports. Explicit `PRINTERHMI_PPA_TRIAL=0` retains software rendering. Optional wrapper `flash monitor` actions use the same selected SDKCONFIG as the build so normal commands cannot silently flash the software seed instead. PPA images/rotation remain disabled, LCD/buffer architecture and version pins are unchanged, and physical Camera performance is not yet accepted. See GRAPHICS_ACCELERATION.md for current commands and historical results.

## Files list metadata polish

`apply_files_metadata_visual_polish.py` follows the accepted PPA/diagnostic cleanup. File rows use the theme/density/accessibility caption font instead of a fixed 12-pixel metadata font. Size units now scale from bytes through KiB/MiB/GiB so small files no longer display as 0.0 MB. Available dates remain visible; unavailable dates omit the DATE -- placeholder, and unknown size uses G-code file. The repeated HOLD FOR DETAILS instruction is removed because both row tap and hold already open details. The non-Studio page title uses sentence case; Studio's Job library title remains.

The opaque list, row geometry, filename ellipsis, previews, input callbacks and asynchronous workers are retained. The Files fixture covers actual size captions, metadata/filename separation, accessibility caption sizing and responsive bounds alongside its redraw containment and lifetime checks.

## Inline Files inspector details

`apply_inline_file_details.py` follows the Files metadata polish. The Studio inspector preview is 352x176 instead of 352x228. Filename and metadata move up, and the metadata viewport grows from 54 to 128 pixels. Standard ETA/filament/layer-height/object-height/size fields fit directly, including large text, with two wider Print/Cancel actions. Details is hidden unless formatted metadata actually exceeds the inline viewport; extra fields remain scrollable inline and accessible in the existing expanded view. Updates back to short metadata hide the action again.

Both theme paths omit the duplicate filename, Thumbnail status/path diagnostics and redundant Confirm to start instruction from the metadata display while retaining actual named fields, including unfamiliar extra fields. The separate print review remains. Exact source/ownership/generation checks, Print readiness and commands are unchanged. Metadata viewport opacity retains redraw containment. The host Files matrix and full-LVGL AddressSanitizer run cover short/long/update transitions, action widths, complete extra data, metadata pointer scrolling and popup/print lifetimes. Native Studio large-text rendering was also inspected.

## Content-sized Files preview

`apply_adaptive_file_preview.py` follows inline file details. Studio measures the actual filename and wrapped metadata before allocating the remaining vertical space to the preview. Common metadata keeps every line visible without an expanded Details action, with a larger preview than the previous fixed 176-pixel box. Sparse metadata allows more image space; long extra metadata keeps a usable 128-pixel preview and the existing scroll/Details path. Print/Cancel remain pinned. Measured caption height respects density and large text.

A local preview-box size observer refits an already loaded image proportionally and recenters an empty placeholder, so late metadata updates do not leave the image clipped or scaled for an obsolete box. New box/filename pointers are retired on deletion with the inspector owner. Rendering settings and printer actions are unchanged. The Files fixture checks common-data fit, spacing, preview growth/shrink/restoration and loaded-image fit, plus all existing ownership/print/redraw cases.

Release follow-up: tools/audit/firmware_size_audit.sh enforces a 3 MiB CI/release budget; both app slots are 7 MiB in partitions.csv. The user reports an image near 2.98 MB. Check exact bytes and growth before release; this visual patch does not change that budget or the partition table.

## Files toolbar polish

After checkpoint 6dc2e79, Files uses sentence-case Up/Search/Refresh, sort captions and search dialog actions in all themes. An applied query gives Search a theme-accent checked border; opening or cancelling its editor does not toggle that state. The breadcrumb shows Search / query during library-wide search and restores the last supplied folder when the query is cleared, including when folder information changes during search. Root reads G-code files. Studio breadcrumbs ellipsize instead of scrolling. Sorting, search scope, callbacks and worker behavior retain their existing contracts.

The real-LVGL Files matrix checks query/folder ordering, root and Up transitions, sort caption mapping, geometry, existing input/lifetime cases and scrolling containment. Panel acceptance remains a local build and OTA step.

## Shared status symbol font correction

Panel acceptance found a missing-glyph rectangle above Files empty-search feedback in Studio. The shared page-state icon used UI_FONT_HEADING, which selects Studio Inter without the folder/Wi-Fi/warning symbol glyphs. Only the icon now uses the already-enabled Montserrat 32 symbol font, both at creation and during updates. Titles, descriptions, status policy and ownership remain unchanged; no new font asset is introduced.

The real-LVGL page-state fixture reproduces the missing glyph on the previous source and checks actual non-placeholder folder/Wi-Fi/warning glyph descriptors across all five themes, densities and text sizes, alongside its existing lifetime/reentrancy checks. Files layout regression coverage and device OTA acceptance remain part of the patch workflow.

## Camera control visual polish

Camera page actions, view/picker headings, rotation/mirror choices and default markers now use sentence case. Connecting/Live/Camera unavailable status strings use the same wording style, with Retry detection and fullscreen hide/restore using the corresponding unavailable prefix. Camera selection retains its saved default and callbacks; rotation/mirror persistence, frame transforms, streaming and viewport geometry retain their contracts. The selected-camera toolbar name uses a single-line static ellipsis rather than continuous marquee motion; its line height follows the actual button font, preventing large-text wrapping outside the button. A bounded logical/rendered caption pair avoids redundant refreshes after LVGL inserts dots. The label is watched and retired with the page, and callback fences protect layout updates; the picker retains the camera names.

The actual Camera fixture checks repeated long-name selector refreshes cause no invalidations and preserve ellipsis, plus unavailable Retry visibility through fullscreen and return. Existing dialog layout and native action/lifetime/transform checks remain across themes, density, text and supported viewport combinations. A physical camera is not connected on the user's current setup: host frame tests do not establish real stream performance.

## Direct Camera Configure handoff

Panel review identified that Camera Configure constructed Manage printers, Edit printer and Camera setup as a stack. The direct Camera setup entry now closes prior profile dialogs and builds only Camera setup, using a copied saved profile connection for Find. Camera Configure also retires its View/selection dialogs before launching and aborts if their deletion replaces or hides the page. Cancel/Use camera returns directly to the calling page; Use camera retains the existing direct catalog/profile persistence and failure feedback, without an outer editor Save. Camera setup launched inside Edit printer retains its intentional return to that editor.

Standalone camera controls and the keyboard share the existing action/owner checks. Closing/deleting the standalone panel retires its jobs, keyboard and copied connection/key data. Generation/profile/epoch checks fence catalog reads during construction and reject reentrant direct opens. The public helper's callback parameters remain for source compatibility; this camera-only entry does not activate a printer or edit its connection. Review found this helper is the single direct Camera setup launcher; chooser/Setup Center profile editing and nested confirmation/keyboard flows retain their return paths. No global layer deletion is added.

Host checks cover replacing a prior manager/editor/camera/keyboard stack with one modal, zero leftover modal/blocker children on cancel/save/delete, saved connection capture for Find, direct keyboard completion/cancel, save failure/retry targeting the selected profile and no outer profile save, stale request cleanup and interrupted/reentrant construction. The existing full profile and Camera regressions also pass; targeted direct-entry AddressSanitizer coverage uses full pinned LVGL instrumentation. Firmware build, OTA and panel acceptance remain local.

## Tools entry visual polish

Tools keeps its four entries, ordering, geometry and callbacks. Outlined rounded cards become flat sections with lower separators on UI_BG; titles use Calibration, Bed mesh, Devices and Macros. The scrolling surface is opaque to match the entries, including in Glass. Existing theme press/focus feedback and connection/idle explanations remain. The four standalone icons use already-enabled Montserrat 32 instead of inheriting a text font; Studio's text-font symbol fallback previously rendered smaller icons. No font asset or printer command is added.

The real-LVGL Tools matrix checks actual non-placeholder symbol glyphs at a readable resolved font size, alongside existing page/entry bounds, wrapping, narrow viewport, theme/density/text and connection feedback regressions. Native Studio and other-theme renders are reviewed; panel touch and appearance remain local OTA acceptance.

## Tools divider and Calibration label polish

Tools now has a non-interactive one-pixel center separator between its two columns, inset within the entries viewport and colored with UI_BORDER_SOFT. It follows layout/size changes and hides when responsive entries stack into a single column. The rule lives outside the entry flow and retires with its owner, preserving all four entry hit areas and callbacks.

Calibration landing headings, tool summaries, action captions and availability counts use sentence case while retaining technical acronyms. Existing card geometry, capability visibility, command strings, command policy, dialogs and persistence remain unchanged. Host checks cover divider placement/one-column hiding across the Tools matrix, existing calibration button-fit and dialog flows, and standalone calibration compilation. Device OTA/visual acceptance remains local.

## Shared divider consistency pass

The consistency review found chooser center separators used UI_BORDER at 60% opacity, Tools separators used UI_BORDER_SOFT, and Studio rules used opaque UI_BORDER. Shared ui_create_divider and ui_apply_divider_border helpers now own the decorative surface/border contract: UI_BORDER at full opacity, one-pixel thickness for line/border callers, and non-interactive decorative objects. Pages retain their geometry and responsive visibility. The existing Operator card-divider and Studio rule APIs delegate to the same constructor. Chooser, Tools and Files row separators use these shared items; no asset is added.

Theme/accessibility colors still come from UI_BORDER. Existing press/focus/selection styling remains intentional control feedback. Host checks compare actual separator colors/opacities in the chooser/Tools matrices and exercise Files, Studio page layouts and shared card/dialog geometry. This pass is scoped to these divider paths, not a claim that every application detail has been visually reviewed.

For subsequent visual work, prefer existing shared widgets, layout helpers and theme roles before adding local styling. Review color/opacity, thickness, spacing, typography, icon size and control feedback together every few visual batches, including all themes, density and large text. Extend a shared item when multiple pages need the same treatment.

## Macros labels and shared search feedback

Files and Macros now share ui_button_set_filter_active for query-owned checked-border feedback. Macros uses the same stable Search caption as Files instead of SEARCH*, with Cancel preserving the applied query and Clear search removing the active state. Shared styling uses guarded value setters and does not make the action a toggle. The Macros search label is watched so independent deletion retires its handle.

Macros page/status captions and editor/review actions use sentence case. Macro names, favorites, parameter names/values, command construction, review requirements and command policy remain unchanged. Host Files/Macros layout and action/lifetime matrices cover shared-state transitions, cancel/clear behavior and existing layout/command guards. Screenshots used for visual review are real LVGL host renders with synthetic fixture data; they do not establish device responsiveness or live transport behavior.

## Macros scroll surface cleanup

Macros hides its list scrollbar while retaining vertical touch scrolling. The enclosing card frame is replaced by the shared opaque flat scroll surface, also used by Files and Tools through ui_apply_scroll_surface. Padding, row geometry, macro names, favorites and review/command policy remain unchanged; the helper owns surface appearance while callers retain scroll directions and bar visibility. Clear search is disabled without an applied query and re-enabled when a filter is applied. Its handle is watched/retired with the page.

The real-LVGL Macros matrix exercises a native pointer drag with the bar hidden, requiring positive list movement, no opened parameter dialog and no sent command. Existing search-state, cancel/clear and parameter/review regressions remain. Files and Tools retain their established layout and redraw checks under the shared surface helper. Panel smoothness and touch acceptance remain local OTA checks.

## Theme identity and temporary Studio default

Visual consistency is assessed within each theme across pages: colors/opacity, typography, spacing, dividers, icons and control feedback should agree with that theme's own roles/tokens and layout language. Foundry, Operator, Glass, Operator Shell and Studio retain distinct identities. Shared widgets must resolve the active theme; consistency does not mean making every theme look like Studio. Review page/dialog/empty/offline/large-text/density states periodically as visual batches progress.

UI_THEME_DEFAULT now centralizes the temporary Studio Dark default for startup, missing/unreadable/invalid saved settings, invalid dispatcher input and fallback after removing an active custom theme. Numeric theme IDs and valid persisted choices are unchanged. Existing explicit theme/accent/density/accessibility choices remain honored. Changing the default does not force an OTA-updated unit away from its saved selection.

The theme-default host test links the real settings manager and dispatcher against a transactional NVS fixture. It verifies fresh/missing/error/invalid settings, all valid saved themes, secondary preference retention, explicit save/reload and save-failure/custom-removal fallback. Physical persistence/reboot remains a device acceptance step.

## Temporary two-theme chooser scope

Appearance now offers only Operator (Theme B) and Studio Dark previews. Foundry, Glass and Operator Shell preview objects are not constructed; the Custom themes entry is hidden for this temporary scope. Their implementations, saved IDs and custom files remain intact. This is a presentation-only change: valid saved selections, including a previously selected hidden theme, continue to load. No settings migration or forced switch is added. Studio remains the default established by the preceding patch. Visual development/review currently focuses on Studio Dark and Operator, checking each against its own theme identity.

The theme-dialog fixture checks exactly two preview objects, correct ID routing, absent hidden-theme handles and hidden Custom entry, while retaining deferred/no-op/failed switching, layout and legacy custom-dialog regression coverage. The existing theme-default fixture continues to validate saved choices without migration.

## Calibration landing-page visual pass

Studio Dark uses four flat sections with the shared opaque theme dividers; no
card borders, radii, shadows or separate card fill remain on this landing page.
Operator retains its themed cards, with a shorter banner and taller sections.
Both themes reserve a 24px summary line and at least 12px of separation before
visible actions. Motion diagnostics and measurement actions share a three-column,
two-row layout and the theme's button font preference. Pressure-advance actions
now have the same 44px height as the other landing controls. Workflow eligibility,
confirmation, command dispatch and saved calibration behavior are unchanged.

The PAGE calibration audit instantiates the actual page and all child owners at
1024x600 in Studio Dark and Operator, across densities, text sizes and discovery/
capability states. It checks flat Studio styling, complete child bounds, label/
button separation and action size. Host renders use fixture printer data; OTA
and panel acceptance remain required.

## Devices and Macros visual consistency follow-up

Studio Devices uses compact flat entries with the shared bottom divider, plus
viewport dividers between categories and readings and between reading columns.
The native viewport fits six full entries; twelve entries remain allocated at
most per page and touch scrolling reaches the rest. Operator keeps its cards.
The list uses the shared opaque scroll surface without a vertical scrollbar;
the horizontal Operator category strip remains swipeable. Devices controls and
pagination use sentence case, and Operator header status clears its action row.
Studio Devices/Macros titles now use the shared title font token.

Studio Macro entries use the same shared flat-entry treatment as Devices and
Tools. Operator keeps its button treatment. Favorites, search, parameter review,
command confirmation and touch-scroll cancellation retain their existing behavior.
The shared flat-entry helper consolidates the already accepted Tools styling.

Host checks cover actual Devices header pointer routing, six visible Studio
entries, complete live-value labels, pagination/scrolling, offline/profile guards
and both supported themes plus legacy-theme regressions. Macro/Tools checks cover
flat-entry styling, responsive bounds and existing command/lifetime behavior.
OTA/panel review remains the acceptance gate.

## Devices category-button follow-up

Studio category controls now use flat rail entries and the shared active-filter
underline. Adjacent 44px rows have no expanded click area, so their targets do
not overlap. Operator retains its separated category buttons. The shared active-
filter helper explicitly owns its checked background using UI_CONTROL at full
opacity, preventing LVGL's built-in checked color from leaking into Devices or
Files/Macro Search. The underline retains UI_ACCENT_CYAN. Native pointer tests
cover all categories, empty categories, near-row-edge taps, disabled Previous,
and Next/Previous routing across themes/densities/text sizes.

## Console visual consistency pass

Studio Console history now uses the shared opaque scroll surface and theme
dividers above/below the history instead of a separate rounded card. Operator
retains its history-card treatment. Both viewports retain touch scrolling and
Follow behavior without a side scrollbar. Page, connection, filter and editor
labels use sentence case; command strings and Console messages are untouched.

Search keeps a stable label and uses the shared checked filter state. The hidden-
temperatures filter uses the same checked styling and explicit Temps shown/hidden
captions. Reset clears those checked states. Existing command ownership, offline
handling, rejected-input retention, history anchoring, row reuse and send paths
are unchanged. The frame audit now preserves pre-existing button state while
checking presses/focus; explicitly styled checked filter borders may be 2px while
outer button geometry remains fixed.

## Drybox visual consistency and compact-banner bounds

Drybox keeps its accepted layouts and program controls in both visible themes.
Studio now uses the shared page-title token, right-aligned status, and matching
reading-divider heights. Operator headings/actions use sentence case; humidity unit/condition captions
share a row without overlapping the value or bar. Short Air
temp/Center temp captions preserve clearance from right-aligned readings. Heater
value captions use Heating/Off/Offline without changing transport state tokens.

The actual Operator page audit exposed compact-banner children extending beyond
the shell after theme status padding was applied. The shared status banner now
sizes/aligns its accent and bottom strip within the content area; simple-banner
state/message labels are centered vertically. Dashboard's separate Studio layout
remains handled by its existing path. Shared simple banners, including Network,
receive the same bounds correction. Status classification and program commands
remain unchanged.

The native page fixture now exercises Drybox in Studio Dark and Operator across
all densities/text sizes, with complete reading-label and offline-control checks.
The live-refresh fixture now supplies current owner/readiness state required by
the lifecycle code, retaining its repeated-refresh/no-invalidation checks.

## Drybox status prominence

Studio Drybox status uses the same shared title font and top baseline as the
Drybox heading, retaining right alignment and its 260px field. Operator already
uses its title font for status. Status colors and machine-state parsing remain
unchanged. Heating/Monitoring and longer status captions must remain complete
at both text sizes.

## Settings visual consistency follow-up

Studio Settings uses the shared page-title font token, retaining its existing
flat row layout. Settings headings and launcher actions use sentence case;
Operator keeps its themed sections. Operator section dividers now delegate to
the shared divider builder at the same dimensions/positions. Settings ordering,
scrolling, dialogs and persistence behavior are unchanged. Host source-derived
row/label checks cover both visible themes plus legacy regressions; final page
appearance remains an OTA acceptance check.

## Settings scrolling and Network visual consistency

Settings hides the side scrollbar while retaining its existing vertical touch
scrolling and content extent. Network uses the shared Studio title font and a
consistent body-large font for its three column headings. Studio scan results
reuse flat entries/shared dividers; Operator keeps themed cards and buttons.
Scan rows are 48px high in both themes. Network page headings/actions and scan
feedback use sentence case; SSIDs, host values and connection-state tokens are
unchanged. Discovery, credentials and connection routing are unchanged.

The existing responsive Network audit now checks scan-row touch size and child
bounds in legacy themes as well as Studio. Host fixtures still cover scan/profile
routing, selected SSID ownership, empty scan results and cleanup. Panel review
is required for final appearance and touch feel.

Compact banner states also fit onto one line at large text sizes. Message width
is resolved after the initial shell layout, avoiding an initially one-pixel
message field. The shared fit/update helpers keep unchanged refreshes quiet.

Network connected indicators use the theme's bright success foreground, matching
other status text. Scan SSIDs stay on one line with their existing overflow
policy, keeping the signal reading aligned and the row visually uncluttered.

## Printer page visual consistency

Printer retains its preview and temperature layout. Operator's preview/detail
separator now uses the shared full-opacity divider instead of a separate soft
color. Preview placeholders and reading-card headings use consistent wording and
sentence case. The Studio launcher reads Toolhead control; Operator's launcher
reads Toolhead, and its Stop label matches the Studio action. Command keys are
unchanged.

Operator action icons reuse the enabled 18px symbol font, with clearer icon/text
spacing and explicit single-line fitting after resolved shared icon sizing. The
Toolhead entry receives 12px from Stop's surplus width; the row's outer footprint
and 12px button gaps remain the same. Labels stay at or above body-text size in
the maintained font-fit checks. Running fan values use the bright accent
foreground for readability; off/unknown/full-speed semantics are retained.

The maintained native fixture now renders the Operator Printer body in addition
to Studio, using real layout/readings/actions/preview constructors and mock
printer values. Its application shell/header are outside the fixture. Native
button-fit and dispatch/lifetime audits retain legacy-theme coverage.

### Dashboard visual consistency

Operator keeps its card layout, with sentence-case page, card, reading and action captions. Command keys remain PAUSE, RESUME, CANCEL_OBJECT and CANCEL_PRINT. Studio keeps its flat composition and existing shared rules. Camera/Thumbnail labels and the empty model-preview message now follow Printer's wording.

The full Dashboard banner explicitly clears theme padding after status styling, preserving its outer-shell coordinates; simple message banners keep their separate content-area layout. The Operator preview reserves space for two readable timing lines, anchored inside the card, instead of clipping the long footer. No font or image assets added. Preview ownership, cache, fullscreen and camera transport contracts are retained.

Native LVGL Dashboard fixtures render both visible themes, all densities and normal/large text. Operator checks include preview/footer separation, complete timing-line width, child bounds and all four print action routes. Screenshots use actual shell/page constructors with fixture data and mocked service callbacks; they do not replace OTA panel acceptance.

### Toolhead dialog visual consistency

Toolhead uses sentence-case headings, tabs and action captions. Studio sections use the shared transparent surface and bottom-divider helper instead of boxed sections; Operator keeps its themed section surfaces. Tabs and selected XY/Z/extrusion distances use the shared checked selection helper, matching the established filter treatment. Query/callback state still owns selection; changing tabs or distances sends no command. The dialog body hides its scrollbar while preserving touch scrolling on small displays.

The existing 52px controls, all motion/filament/Z-offset step values, command keys, sender guards and pinned Close footer are retained. No assets added. Native dialog fixtures cover every tab, normal and large text, all densities, both visible themes and compatibility themes. Selected tab state is checked explicitly; screenshots allow theme transitions to settle before capture. OTA panel acceptance remains separate.

### Thermal control dialog visual consistency

Temperature and fan preset dialogs use sentence-case headings, reading captions and actions. The target label reads “Target - tap to set”; custom-temperature behavior is retained. Studio current/target sections and hotend chooser rows use the shared transparent surface and divider border; Operator keeps its themed sections and reading accent. Preset selection uses the shared checked selection helper instead of a private border treatment. Off remains a danger action.

Temperature limits, named heater commands, target values, fan presets, hotend activation guards and pinned footer behavior are retained. No assets added. The real hotend layout fixture now also checks the selected bed preset and preset button bounds, rendering actual preset controls in both visible themes. Existing all-theme/density/text/viewport checks, live no-op refresh, hotend reordering/removal, print/pause adjustments and teardown still apply. Host renders use fixture data and mocked transport; OTA acceptance remains separate.

### Filament sensor and cancellation dialog visual consistency

Studio filament-sensor rows now use the shared transparent surface and bottom-divider helper. Operator retains its themed sections. The shorter “Filament sensors” title and sentence-case state/action labels follow the other polished controls. Motion/switch type descriptors use normal case; sensor object names and ENABLE command arguments remain untouched.

Whole-print confirmation now says “Stop print?” with a “Stop print” action, matching the Printer/Dashboard controls. Object cancellation retains its separate confirmation, explicit object name and explanation that the rest of the print continues; its headings and Back/Exclude/Close actions use sentence case. Shared Back captions, including custom temperature entry, follow that convention. Warning copy, danger roles, command ownership, retries, duplicate suppression and readiness checks are retained. No assets added.

Host fixtures check both visible themes and compatibility themes, all densities/text sizes and smaller viewport fallback. Existing sensor acknowledgement/timeout, name binding, quiet refresh, reordering/removal and cleanup tests pass alongside whole-print and 48-object cancellation fixtures. Rendered samples use fixture state and mocked transport; OTA panel acceptance remains separate.

### Filament recovery visual consistency

Recovery uses a shorter sentence-case heading, caption and action labels. The selected nozzle target uses the shared large body font. Action labels are fitted from that same preferred font after the grid has its final width, avoiding premature tiny text. A shared bottom-divider border separates the temperature controls from recovery actions, and Resume spans the two remaining columns on the final row. The existing 54px action height and fixed Close footer are retained. The body hides its scrollbar while preserving smaller-display touch scrolling.

No recovery macro, temperature limit, selected-target default, cooldown, sender/owner guard, homing/heat/sensor readiness gate or resume route changed. No assets added. Native and smaller-viewport fixtures verify complete button text, touch targets, wider Resume, fixed footer and native no-scroll fit across all themes/densities/text sizes, including rejected-send feedback. Host rendering uses fixture telemetry and mocked transport; hardware acceptance remains separate.

### Calibration and diagnostics visual batch

Motion limits, TMC driver diagnostics, axis-distance calculator/editor, probe accuracy, live endstops and bed-mesh profile list/save/confirmation dialogs now use sentence-case headings and actions. Technical status tokens, reported values, object/profile names, command identifiers and prominent safety instructions remain intact.

The shared calibration dialog footer uses the shared top-divider border with 8px separation before its pinned actions. Motion diagnostics uses the same divider and spacing, with a content-sized footer so the rule/padding do not consume button height. Calibration and motion bodies, and the bed-mesh profile list body, hide side scrollbars while preserving touch scrolling and reachability. Existing flat Studio compositions and Operator surfaces are retained; no assets added.

Validation includes all twelve maintained calibration variants and standalone translation-unit compilation, real Calibration page fixtures in both visible themes, motion and bed-mesh profile layout/lifecycle suites, diagnostics batch safety and complete-button-label checks. Compatibility themes, densities/text sizes and smaller viewports remain covered. Native fixture renders for both visible themes were inspected for the six reviewed surfaces, with shared-footer samples also checked in PID/manual-probe dialogs. Service callbacks and telemetry in these renders are mocked; OTA panel acceptance remains separate.

### Monitoring visual batch

Telemetry Heat/Motion/Environment sections and event-history rows use flat transparent sections with shared bottom-divider borders in Studio. Operator retains its metric/history surfaces. Telemetry tabs and held/target states use the shared checked selection helper instead of private tab colors. Telemetry buttons and selectors have 48px touch height. Graph and event-list side scrollbars are hidden while touch scrolling remains available.

Event history uses sentence-case heading/actions, shared footer divider/spacing and checked Hold state. Hold/Follow label fitting is refreshed after text changes. Severity markers and colors retain their meaning. Chart samples/ranges, target inclusion, gap/offline representation, named hotend binding, follow/hold retention and history clearing/lifecycle behavior are retained. No assets added.

Graph inventory: the shared Telemetry engine owns all time-series plots; Devices > Graphs routes to it. Drybox air was repeated in Heat and Environment. Heat now contains named nozzle, bed and drybox-center temperatures; Environment owns drybox air, humidity, part fan and drybox fan; Motion owns velocity, volumetric flow, speed override and flow override. All eleven distinct channels appear exactly once across the views. Named hotend selection remains available. Actual speed/flow and their percentage overrides are distinct; the two fans are different devices. The existing four reusable chart slots are retained, with the unused Heat slot hidden and buffer sizes synchronized when ranges change. No additional histories or graph instances are added.

Real telemetry and event-history fixtures cover both visible and compatibility themes, all densities/text sizes, native and smaller viewports, actual touch routes, graph ranges/held traces, quiet refresh, owner teardown and ring replacement. Selection state is checked explicitly; telemetry snapshots allow state transitions to settle before capture. Native renders of all three graph views and history in both visible themes were inspected. Host transport/data are mocked; OTA acceptance remains separate.

### Graph tabs grouped by system

The graph grouping is now Thermal, Motion and Drybox. Thermal contains the selected nozzle temperature, bed temperature and printer part-cooling fan. Motion contains actual velocity, actual volumetric flow, speed override and flow override. Drybox contains its center temperature/heater target, air temperature, humidity and fan. This supersedes the initial Heat/Environment grouping: the drybox center moves out of printer thermal controls, and the printer part fan moves out of drybox monitoring. All eleven distinct series remain present exactly once.

The target-scale control is available in both Thermal and Drybox, so the drybox heater target remains usable after the move. The hotend selector is limited to Thermal; Motion has no irrelevant target control. Existing internal view IDs and sample/history formats remain compatible. No new chart slots/assets or history buffers are introduced.

### Calibration workflow label consistency

PID heater selection/target/heat confirmation, pressure-advance tower setup/armed state, manual probing, screws tilt/gantry leveling and calibration-result/save confirmations now use sentence-case workflow headings and action labels. PID and Klipper command acronyms remain intact. Probe/Z and axis-twist caller-provided titles and accept labels follow the same convention, so actual entry points do not reintroduce uppercase defaults.

The existing shared calibration modal structure, flat Studio layout, Operator surfaces, footer divider and pinned actions are retained. Explicit attended-heating, probe/motion and SAVE_CONFIG/restart warnings are unchanged. No command, macro, target/step value, readiness gate, confirmation step, sender/callback ownership, saved state or asset changes are included.

Validation uses the full twelve-variant calibration owner/layout suite and separate translation-unit compilation, plus complete button-label checks across compatibility themes, densities/text sizes and smaller viewports. Native fixture renders of PID, pressure advance, manual probe, geometry and result/save dialogs were reviewed in both visible themes. Fixtures may supply their own titles/accept labels to verify that caller-owned wording is preserved. OTA panel acceptance remains separate.

### Settings arrangement and dialog visual batch

Both themes now present Settings in the same functional order: Display & appearance, Notifications & history, Connections, Time & region, Storage & backup, Firmware & updates, Setup center, Maintenance, System information, About. Sleep timeout moves beside brightness; notifications move alongside event history. Reboot, Klipper restart and factory reset share the lower Maintenance section. Each theme retains its own section appearance; Studio stays flat. Callback bindings, saved settings and confirmation behavior are retained.

Backup choices now occupy a fixed two-column grid: plain/encrypted creation first, plain/encrypted restore second, and backup removal on its own full-width final row. Labels explicitly say “Back up encrypted.” Current file paths, credential-inclusion guidance, nested review dialogs, encryption/restore worker guards and retry behavior are retained. Labels fit after the grid resolves its width.

Appearance, theme/time-zone/reset/reboot, Wi-Fi/password/port, backup/passphrase/progress and About dialogs use sentence-case headings/actions. Studio About sections and Wi-Fi rows use the shared transparent surface/divider helper; Operator keeps its surfaces. Settings/appearance footers use shared divider borders and spacing. Their scroll bodies and the Wi-Fi list hide side scrollbars while retaining touch scrolling. Appearance choice rows have a 48px minimum. The built-in chooser still exposes only Studio Dark and Operator; legacy/custom theme support and saved IDs are retained. Build date/time formatting is bounded to the SDK fields, matching About's handling. No assets added.

Validation includes Settings, theme/appearance, network/backup, About, display fault/lifetime and complete-button-label suites across themes/densities/text sizes and smaller viewports. Full Settings page QA invokes the real ui_settings_show_page constructor with mocked SDK/services, checking section order/non-overlap and the sleep/notifications parent groups, and inspecting native large-text renders in both visible themes. Dialog fixtures cover filenames/SSIDs, keyboards, pinned actions, failures/retries, worker overlap, deferred theme changes and cleanup. Host fixtures do not replace OTA panel acceptance.


### OTA responsiveness

Opening OTA now signals camera shutdown without waiting on LVGL: the Camera setup path uses camera_stream_request_stop, and the OTA popup no longer polls for two seconds. The camera worker retains its shared transport until teardown completes; the existing catalog shared-lane and update exclusive-lane waits stay on their workers. Reopening uses a ready in-memory release catalog instead of downloading it again. Stable/Nightly continue filtering existing rows locally; Refresh explicitly fetches newer releases. A failed/idle catalog still retries on reopening. While a Custom URL transition is queued, catalog rendering is skipped and the status immediately acknowledges opening the URL editor; owner/epoch and async-failure guards remain in place.

The OTA worker captures its HTTP client through the SDK initialization callback, then changes body reads to a one-second timeout after the first image-header perform. ESP-IDF 6 returns IN_PROGRESS for body-read EAGAIN, allowing the worker to check Cancel between reads. Fifteen seconds without image-byte progress fails the transfer instead of retrying indefinitely. Connection/TLS/redirect setup and the first image-header perform retain SDK blocking behavior and the original 15-second socket timeout; cancellation there can still take longer (the SDK header reader can retry internally). Only the worker touches/cleans up the SDK client. Finalization still disables Cancel before image validation/boot selection. No new assets or fonts.

Host checks exercise busy-camera UI entry without delays, cached reopening, repeated local channel switches, Custom URL transition priority and repeated ownership/deletion paths, plus short-read configuration, stalled-read cancellation, transient stalls, the no-progress deadline and all prior transfer failure/finalization cases. Camera pipeline/lifetime and separate translation-unit compilation are checked. Build, real network latency and panel acceptance remain separate.


### Smooth OTA progress

The OTA bar uses a 0..10000 range and byte-derived hundredths of a percent during known-length downloads; the text retains integer percentages, and validation still reserves the final percent. Unknown-length downloads retain their existing bounded stage value. A short linear animation retargets from the currently displayed value instead of LVGL's previous animation destination, preventing jumps when the 50ms UI pump supplies newer progress. Identical snapshots do not restart the animation or invalidate an unchanged bar. Failure/reset values apply immediately; deletion removes animations with the owning bar. No additional task, timer, font or image asset is added. Actual transfer speed is unchanged.

The real LVGL OTA fixture verifies fractional byte progress under an unchanged integer label, no jump on mid-animation retarget, completion despite repeated identical snapshots, the validation limit and deletion with an active animation across themes/text sizes/viewports. Device smoothness remains pending panel verification.


### About: project authorship, AI assistance and software credits

About preserves installed firmware identity and adds explicit khelix authorship/direction and OpenAI ChatGPT/Codex development-tool acknowledgment. Concept, product direction, visual design, feature decisions, hardware verification and release approval remain attributed to khelix; implementation, debugging, review, test development, documentation and implementation-option assistance are identified as AI-assisted work. Third-party authorship remains separate.

Credits now cover Espressif/LVGL foundations, supporting SDK libraries and cJSON/LodePNG, fonts and logo lettering, separately running Klipper/Moonraker/PrusaSlicer, and development/hosting tools. docs/SOFTWARE_CREDITS.md inventories all 19 locked components with roles and versions, retained upstream notices, font notices and local modifications. NOTICE and README link the reference and acknowledge AI assistance. Selected upstream library/font notice copies and ESP-IDF 6.0.2's copyright inventory are retained under docs/licenses, outside the firmware image. This is a credits reference, not a complete linked-binary license audit; exact build/source notices remain authoritative.

The existing shared About sections retain flat Studio dividers, Operator surfaces, touch scrolling and pinned Close. Host tests exercise all channels/themes/densities/text sizes/viewports, expanded credit visibility, last-section scroll reach, actual pointer closure, deleted/reentrant owners and standalone compilation. Native large-text renders of authorship/foundation/font/source sections were reviewed in Studio Dark and Operator. A dependency inventory check fails if a locked component lacks credit or a referenced local notice is missing. Panel/build/size acceptance remains pending; no fonts or images are added to firmware.


### OTA visual consistency

The release browser, release/install confirmation, Custom URL editor, update progress and start-failure message use sentence-case headings, relations and actions. Stable/Nightly use the shared checked-filter helper; Studio release rows are transparent entries with the shared bottom divider, while Operator retains its outlined rows and installed marker. Release versions, identity, notes and install/reinstall/downgrade/nightly warnings remain visible in the existing scroll flow.

The shared OTA layout now supplies a top divider and 8px spacing above its pinned footer; body/list side scrollbars are hidden with touch scrolling retained. A shared final fit resolves action text after flex/keyboard geometry, and the tabs fit the body font at their resolved widths. The previously separate start-failure popup now uses the same header/body/footer helper with danger styling, its existing Close callback and camera resumption. No new fonts or assets; catalog caching, network ownership, safe finalization, cancel guards and smooth byte progress remain in place.

The actual OTA owner fixture passes across all five compatibility themes, densities/text sizes and three viewports, including local channel filtering/cached reopening, complete action bounds, native readable tab text, keyboard/footer checks, animation/failure/cancel paths, async rejection and owner deletion/reentrancy. Standalone owner compilation passes. Native fixtures were reviewed in Studio Dark and Operator with transition settling before captures; fixture releases/identity are simulated. Build/OTA panel acceptance remains pending.


### System information readability

Settings keeps the same four-row System information section but orders it by operational use: Uptime, Internal RAM, PSRAM, ESP-IDF version. The internal memory row exposes Free now, Lowest and Largest block as separate labelled columns; PSRAM's Free now and Lowest align with the same columns. Lowest describes free memory since startup; the largest block remains explicitly separate from total free memory. Values use correct binary KiB/MiB units rather than KB/MB for 1024-based formatting.

A shared read-only Settings metric-row helper reuses the existing themed row surfaces and dividers, with three aligned metric slots, per-cell captions and body-sized values. Studio remains flat, Operator keeps its surfaces, and the section height does not grow. The system-info owner adds independently bound metric labels while preserving the existing aggregate label API, SDK sampling, epoch checks, no-op refresh behavior, deletion/rebind handling and full unbinding. No additional timer, assets or fonts.

Validation includes actual system-info SDK/label refresh through the telemetry audit, independently populated current/lowest/largest values, KiB/MiB/zero formatting, matching native column coordinates, label/ancestor bounds and text width, unchanged-refresh invalidations, partial label deletion and unbinding during reads. The complete telemetry suite passes across themes/densities/text sizes/viewports, with AddressSanitizer/UndefinedBehaviorSanitizer host validation. Display-settings safety and maintenance/reset/time-zone dialog audits pass; those maintenance layouts already use the shared styling and need no source change. Native full Settings constructor renders with mocked services were reviewed for System information in Studio Dark and Operator; section order/non-overlap is retained. Build/size/OTA panel acceptance remains pending.


### Installer-style Setup center and full-height discovery

Setup now has seven selectable steps: Welcome, Network, Time zone, Display & appearance, Printer, Camera and Review. A smaller shared heading reserves space for the tasks, the navigator uses the shared checked state and divider, and Back returns to the previous step. Wi-Fi and a configured printer remain the two essentials; camera setup stays optional. No language selector is shown. Welcome and Review show readiness rather than adding another summary below the seven-step navigator.

Time zone uses the existing Settings catalog and persistence service, with shared selectable rows and a scrollable list. Catalog indexes are retained even if an entry is unavailable. A successful selection updates the checked row and current-zone feedback; a storage failure retains the previous selection. Settings refresh now also refreshes its time-zone value after external changes.

Display & appearance provides the two exposed themes (Studio Dark and Operator), standard/larger text, 10–100% brightness and Never/5/15/30-minute sleep. All four groups fit the native viewport, with touch scrolling on smaller layouts. Brightness saves on release using the existing paired brightness/sleep restore service; sleep uses the same service. Failed saves retain the saved values. Theme/text writes and reconstruction run together after the button event unwinds; a closed/replaced owner cancels a queued choice before it writes. The main application supplies its existing appearance-rebuild callback so persistent surfaces behind Setup also use the saved appearance; Setup then returns to Display. Other accessibility flags are preserved when changing text size. Review shows the saved region and display summary. Reopening uses the current saved values.

Studio choices and review rows stay flat with shared rules; Operator keeps its themed surfaces. The brightness slider uses the shared theme style, and button text is fitted after final flex geometry. Footer dividers and touch scrolling without sidebars match the surrounding dialogs. Wi-Fi and camera result lists fill the available stage. The shared Moonraker discovery popup grows to 860×540 where space permits and its result list fills the remaining body instead of a fixed 160px strip. A font-derived minimum keeps wrapped results touchable on short screens; Retry/Close remain pinned. Request identities, transport and scan ownership are unchanged.

Host validation covers all five compatibility themes, three densities, both text sizes and three viewport widths, preference save failures/retries, sparse catalog indexes, captured/replaced owners, deferred appearance reconstruction and cancellation before persistence. Discovery validates actual pointer selection, eight-result scrolling, full native list width/height and a stationary footer, including short-screen fallback. Existing display-persistence, appearance and profile-discovery bridge checks pass. Native large-text LVGL fixtures are reviewed in Studio Dark and Operator; services and transport are mocked. No new image/font assets. Firmware build, the 3 MiB release budget and OTA/panel acceptance remain separate.


### Setup connection summaries and active-printer editing

The Network step now shows the connected SSID and display IPv4 address, or explicit disconnected/waiting-for-address feedback. Change network/Find a network still opens the existing scan/password flow. Refresh status refreshes the read-only facts without starting a scan. Reads are guarded against replacement of the Setup owner. The shared fact rows use flat Studio dividers and Operator surfaces, with wrapping for descriptive values and hidden side scrollbars.

The Printer step shows the saved active profile name and HTTP/HTTPS Moonraker endpoint. Its access caption reports only whether an API key is saved, never the key itself. Saved configuration is explicitly distinguished from live Printer-page connection status. The unconfigured state points to the shared editor's discovery, connection test and security options. Edit printer now opens the active profile slot rather than always slot zero, retaining the existing request-bound discovery bridge and shared editor lifecycle. Review labels Wi-Fi as Connected and printer/camera configuration as Saved; completion requirements are unchanged.

The real Setup host fixture checks disconnected/connected/address-pending states, exact address formatting, active slot 2 editing, secure/plain endpoints, API-key redaction, native fact/action bounds, both exposed themes plus compatibility themes/densities/text sizes/viewport widths, and all prior preference/Wi-Fi/camera/completion lifetime cases. The profile-discovery bridge audit remains in place. No new assets/fonts or network requests are introduced by these summaries. Firmware build, OTA/panel verification and the 3 MiB release budget remain separate checks.


### Setup Camera summary, empty states and busy actions

The Camera step now matches the Network/Printer summaries. A saved camera shows its catalog name when its saved default matches the configured stream, otherwise Custom camera, plus its printer. Stream URLs are never rendered, including URLs containing credentials. The saved state offers Discover and test plus Continue; the unconfigured state explains that a camera is optional and how to discover/test it. Without a printer profile, Set up printer navigates directly to the Printer step. The shared fact rows retain flat Studio dividers and Operator surfaces.

Discovery and stream testing show a disabled Scanning.../Testing... action while their owned poll is active. Repeated discovery requests leave that poll intact rather than discarding it and retrying the backend. Completion/failure restores Scan again or the verified Use camera action. No-camera results now include a visible empty-state explanation and retry guidance instead of an unexplained blank list. Test failure explicitly permits retrying the selected camera. The scanning footer offers Continue when a camera URL is currently configured and Skip camera otherwise; using either does not itself request another camera save. A printer-generation change retires verification and restores discovery actions.

The existing controller behavior remains: consuming discovery imports returned camera settings into the catalog, including its slot-zero URL bridge. Continue therefore describes navigation after those imports, not restoration of the pre-scan camera. Use camera retains the existing verified default/stream persistence and failure feedback; this change does not alter controller storage semantics, transport, worker ownership or completion requirements. Saved-summary catalog reads are guarded against Setup/printer replacement. The additional scan action participates in the existing field-retirement handling, and final footer text uses the shared fit after flex layout.

The real Setup host fixture covers saved/mismatched-custom summaries, URL credential redaction, optional/no-printer/empty states, discovery/test start failure, failed stream retry, repeated requests while busy, guarded catalog reads, navigation without an additional save, native summary/control bounds and all prior save/reentrant/owner/persistence cases across five compatibility themes, densities, text sizes and viewport widths. Native large-text fixtures cover both exposed themes. Worker discovery regression checks remain separate; firmware build, OTA/panel acceptance and the 3 MiB release budget are still required. No assets/fonts or new workers/timers are added.


### Setup Welcome, Review and completion closeout

Welcome and Review share the existing 48px readiness rows with a native-width value column for the connected SSID and active printer name. Network status remains Connected, profile/camera status remains Saved, and the camera stays optional. Compact viewports retain the two-column title/status layout. Summary cells are explicitly one line with dots for overlong names; the full values remain on their dedicated Network/Printer steps. Studio keeps shared flat dividers and Operator keeps its themed row surfaces.

Welcome explains direct step navigation and that saved choices remain in place on exit. Review explicitly separates immediate preference saves from the Finish action, which marks onboarding complete. Time zone remains visible, and a compact caption includes display theme, standard/larger text, brightness and sleep timeout on one native line. The two exposed theme names use consistent casing. Connected-network sampling for the new value column is guarded against Setup/printer replacement before constructing any summary rows or Finish action.

The completion message is now Setup complete and directs the user to Settings for saved choices and the Printer page for live connection status. It does not infer a currently working printer from a stored profile. Done retains the shared pinned footer/divider and is fitted after final layout. Completion storage, essential checks, save/retry behavior and optional camera handling are unchanged. No extra timer, assets/fonts or storage writes.

Host validation checks actual review SSID/profile values, all saved display fields, native Welcome/Review child bounds, single-line row cells, a one-line native display summary, completion/footer/action bounds and owner replacement during review sampling, alongside the full existing setup/persistence/camera suite across five compatibility themes, densities, text sizes and viewport widths. Native large-text source fixtures are reviewed in Studio Dark and Operator. Firmware build/size, OTA and panel acceptance remain separate.


### Files, Camera and Network state/dialog consistency

After checkpoint 1eb5193, Files loading/search-empty/folder-empty/offline/error headings use sentence case through the existing shared page-state component. Its loading spinner now uses the theme accent and divider colors instead of LVGL default blue/white, benefiting every caller of that component. The Operator file inspector uses File preview/Print details and sentence-case actions, avoiding a readiness claim while metadata is unavailable. Browser, metadata and confirmation bodies keep touch scrolling without side scrollbars. Print confirmation, Operator inspector and expanded Studio metadata use the shared top footer divider with 8px spacing; expanded metadata retains its pinned Close action and scroll reach. Studio's inline preview sizing remains unchanged.

Network connection values and fallback banner text now use Connected/Disconnected/Offline/Wi-Fi, with IP address and protocol acronyms preserved. The Studio banner uses the shared flat bottom divider rather than a rounded outline; Operator retains its surfaces. Available-network lists hide sidebars while remaining scrollable, and an empty result gives actionable Scan networks/range guidance. Returned SSIDs, scan selection callbacks, discovery navigation and external controller text are unchanged.

Camera's unavailable/connecting/no-configuration feedback reads as normal sentences while retaining the existing Camera unavailable retry predicate. Retry has a 48px target. View and Select camera dialogs hide body scrollbars and use the same shared footer divider/spacing. Camera choices are flat in Studio, retain Operator surfaces, and share the standard checked-state helper with rotation/mirror choices. Action fitting runs after final dialog geometry so native picker rows keep the preferred body font. Stream/frame/fullscreen ownership, catalog persistence, retries, test/setup suspension and profile fencing are unchanged.

The existing Files/Network responsive suites pass across compatibility themes, density/text sizes, supported widths and custom profiles; they retain print safety, metadata touch scrolling, preview fit, fixed footers, row refresh/damage and page lifetime checks. Additional fixtures cover all five Files page states, Network empty feedback and hidden sidebars. The Camera suite checks preferred native picker text and pinned body/footer bounds alongside frame retirement, retry/save/rotation/mirror/fullscreen, stale callbacks, deleted fields and timer-balanced cleanup; standalone Camera compilation passes. Native large-text project-source renders are reviewed in Studio Dark and Operator, with simulated services. No assets/fonts or transport/storage changes. The current CI firmware limit is 7 MiB, matching the existing OTA slots; build/OTA/panel acceptance remains pending for this follow-up.


### Devices, Macros and Calibration visual consistency

- Devices keeps theme-specific category layouts and touch scrolling, with the remaining category-strip scrollbar hidden. Live availability and filament status labels use sentence case.
- Macro search, parameter and command-review dialogs use the shared footer divider and spacing, with body scrollbars hidden and actions pinned. Operator feedback uses sentence case.
- Calibration motion/custom workflow titles, actions and failure notices now match the sentence-case dialog convention; PID and axis names remain correctly capitalized. Printer commands and macro matching tokens are unchanged.
- Studio Dark remains flat, while Operator retains its existing surfaces. Native LVGL fixture renders reviewed in both themes. Host checks cover Devices pointer routes/live guards, Macro review/scroll/keyboard lifetimes, and all calibration dialog variants.
- ESP-IDF build and physical-panel OTA verification remain the next local checks. No commit or push performed by this patch.


### Theme consistency back pass across previously polished pages

The acceptance rule applies to every previously worked page and popup: Studio Dark keeps flat page sections and shared dividers; Operator keeps its own surfaces and controls. Use shared theme fonts, colors, checked-state styling and footer structure. Preserve meaningful danger emphasis and protocol/G-code capitalization.

Corrections in this back patch:

- Shared native forms and profile dialogs hide incidental scrollbars while retaining touch scrolling. Native form footers and profile-dialog footers gain the shared top divider and 8px spacing. Interior profile action rows are not mistaken for dialog footers.
- Printer control, thermal, sensor, object and stop dialogs use the same scrolling-body/pinned-footer divider convention.
- Shared selectable lists render as flat, divided rows in Studio, with the shared checked-state treatment; Operator retains its list surfaces. This reaches time zones, appearance preferences, saved bed meshes and other existing list callers. Profile camera-slot selection also uses the shared checked helper.
- Studio Tools, Job library and printer chooser headings use UI_FONT_TITLE instead of fixed 48/32px faces. Tools gains more content height; Files gains 22px of browser-list height without changing the preview dimensions.
- Studio graphs and the empty camera viewport use the shared page background.
- Console Search and Send command actions move below their keyboards with the shared footer divider.
- Notification preferences gain a larger adaptive dialog, complete native-panel explanation, themed checked states instead of inherited default checked colors, a shared footer, balanced actions and sentence-case labels. Alert priority and persistence policy do not change.
- Drybox, graph, profile and motion-diagnostics status labels complete the sentence-case pass. Commands, banner classification tokens, units and axis names remain unchanged.

Retrospective coverage:

| Previously worked area | Back-pass coverage |
| --- | --- |
| Dashboard, Printer and navigation | Source review; native Studio/Operator page renders; large text, action routes, teardown |
| Printer chooser and profile editors | Native chooser; manager, editor, security/authentication/certificates, camera slots, keyboard and removal dialogs |
| Files and previews | Browser/search/empty/offline states, native inline preview, Operator inspector, metadata and print confirmation |
| Camera | Page/no-camera states, View and Picker; setup/catalog and owner lifetimes |
| Tools, Devices and Macros | Native pages, category/selection controls, pointer routes, macro review, keyboard and scrolling |
| Calibration and diagnostics | Page, PID, pressure advance, manual probe, resonance, probe accuracy, endstops, results, geometry, custom macros and motion diagnostics |
| Bed mesh | Page components, profile list/editor/save/remove dialogs and command guards |
| Console, Drybox and graphs | Both exposed themes; editors, readings/program controls, all three graph views, range/hold and lifecycle |
| Settings and Setup | Source layout rules and section components; all Setup steps; theme/time-zone/appearance/network/backup dialogs; display persistence |
| Notifications and information | Preferences, fault notice, event history, About/software-credit sections and system-information metric layouts |
| Stop, recovery and active-print controls | Thermal/hotend/sensor/object/stop dialogs, filament recovery, recovery failures and E-stop recreation |

Native LVGL suites also retain compatibility-theme, density, large-text and smaller-viewport checks where their fixtures support them. Source review covers the previously polished modules, including Settings/OTA surfaces whose full hardware workflows cannot be exercised here. Representative renders in both exposed themes were inspected; fixtures replace transport/data. This is host verification, not a claim of physical-panel acceptance for every runtime state. Build, OTA and the final navigation sweep remain local checks. Nothing is committed or pushed by this patch.


### Operator Tools, chooser and Macros surfaces

- Operator Tools uses its own banner and four themed cards. Flat row dividers and the center separator remain Studio-only. The category surface hides its scrollbar while retaining small-viewport touch scrolling.
- Operator's printer chooser uses a banner and four aligned themed cards, with no Studio center/bottom section dividers. Active changes the card-border color without changing border thickness or child geometry. Preview, status, subject binding and click routing are retained.
- Operator Macros groups its title, status, search controls and guidance in a themed header. A single-line ellipsized status keeps the macro viewport usable; names still wrap inside themed macro cards. Search, favorite, review, dispatch and teardown behavior are retained.
- Studio retains its existing flat layout for all three pages. Hidden compatibility themes continue through their own surface recipes.
- Host checks use responsive_layout_test.py variants TOOLS/MACROS and chooser_refresh_test.py, including theme/density/text/viewports, label bounds, macro keyboard/review lifetimes, active transitions and native chooser touches. Native renders reviewed in Operator and Studio. Build and OTA/panel confirmation remain local.

### Operator Tools-family shared headers

Tools, Calibration, Devices, Macros, Bed Mesh and Telemetry now use the shared Operator page header: an 800px content rail, 86px themed banner, consistent title/subtitle fonts and baselines, and theme-owned colors. Studio keeps its flat page presentation. Calibration cards start below the banner; Macros search actions sit below it; Bed Mesh gesture help occupies the side column; Telemetry keeps its Devices action in the header. Dynamic telemetry text uses clipping to preserve quiet identical refreshes.

Delivered by `apply_operator_tools_headers.py`. Host fixture checks passed; firmware build and OTA/panel verification remain local steps. No commit or push is included.

### Full Studio and Operator theme audit closeout

Audited the cumulative 7.0.0-dev source after the Operator Tools-family header patch, including theme dispatch/tokens, page and popup helpers, the two-theme picker, persistent page recreation, shared dividers, and font/symbol fallback. No page implementation directly invokes another theme's A/B/C style recipes. Studio retains its flat page treatment and Operator its panels and cards; popup surfaces and semantic status colors remain theme-owned. Caption and symbol fallback fonts are intentionally shared.

Found one persistent-surface issue: notifications live on LVGL's top layer and could retain the old palette/font after a theme or appearance change. `ui_toast_refresh_appearance()` now rebuilds a visible fault from its captured queue after the application rebuild, preserving priority, printer owner, destination, count and event-history records. Optional brief confirmations retire with the old appearance without restarting their timer.

Delivered by `apply_full_theme_audit_closeout.py`. The audit also updates stale legacy fixture expectations for sentence-case status labels and adaptive popup footer heights, preserving bounds/non-overlap/full-text checks. No new assets or runtime source files were added. Local firmware build and OTA/panel confirmation remain required; nothing is committed or pushed.

### Tools content polish after the full theme audit

Continues Calibration, Devices and Macros from the accepted Tools-family header checkpoint. Camera, Network and Console are unchanged.

Operator Devices now presents all eight category/count controls as a fixed four-column, two-row grid with the shared checked-state fill and outline. Category targets remain at least 44px and disjoint. The device viewport moves below the grid and fits two complete rows, while retaining touch scrolling, 12 entries per page, hidden side scrollbars and Previous/Next behavior. Studio keeps its existing flat category rail and device rows; compatibility theme layouts are retained.

Macro names now use a shared leading icon/text button layout. Wrapped lines align to the left of the name column instead of centering independently, and the icon retains a separate column. Full command names, favorite ordering/long-press, row reuse, search, review and send guards remain unchanged. Existing centered icon buttons keep their previous layout; the new enum value is appended to the shared contract.

Calibration sections show short, complete purpose descriptions when applicable workflows are available rather than repeating long slash-separated button lists. Discovery and unavailable explanations stay dynamic, and capability-driven controls/status/counts remain in place. Studio remains flat and Operator retains its cards. No assets, fonts, transport work or persistence paths are added. Delivered by `apply_tools_content_visual_polish.py`; local firmware build and OTA/panel confirmation remain required.

### Devices uses fixed pages instead of list swiping

Following the Tools content polish, Devices now pages through only the entries visible in its native viewport: four in Operator and six in Studio. Previous/Next and the page/range summary use the same theme-dependent page size. The device list has no swipe scrolling in either exposed theme; all categories remain directly available. Counts, category filtering, live-reading bindings, printer ownership and row reuse retain their existing behavior. Compatibility themes keep their prior 12-entry layout. Delivered by `apply_devices_button_paging.py`.

### Readable twelve-device pages with vertical scrolling

Clarifies the previous button-only pagination adjustment: vertical scrolling within a page is allowed. Devices retains two readable columns and twelve entries per page, with names, object identifiers, category and complete live readings. Its list scrolls vertically without a side scrollbar; Previous/Next moves between sets of twelve. The recent Operator filter grid/selected feedback and shared headers remain intact. Studio keeps its flat list treatment. Macros retains its current vertical scrolling and leading name alignment. No compact three-column or single-line layout is delivered.

`apply_devices_readable_twelve.py` accepts the exact source baselines immediately before or after `apply_devices_button_paging.py`, so that patch need not be applied just to restore the preferred arrangement. Other source states still fail closed. This supersedes the preceding four/six-entry no-scroll behavior.

### Bed Mesh and Telemetry visual polish

Continues the Tools pages after the readable twelve-device follow-up. Bed Mesh uses sentence-case actions/confirmation/help, a left-aligned Studio title and a single-line profile/grid/Z summary. Minimum, maximum and range now have a separate side readout, preventing the former two-line summary from crowding the Studio canvas or being clipped by Operator's header. The readout clears with unavailable/invalid mesh data and its field ownership retires on deletion. Canvas sizes, gestures, action routes and profile/calibration behavior remain in place.

A populated Studio fixture exposed a renderer defect: raster projection/clipping and pixel indexing still used fixed 620x340 dimensions even though Studio's canvas is 776x250. The renderer now reads dimensions and row stride from the RGB565 canvas descriptor; buffer allocation uses LVGL's stride calculation. This fixes the reproduced striped Studio rendering and keeps direct raster writes within the configured canvas. It does not change the PPA configuration.

Telemetry metric names and readings align vertically in their shared header. Scale and history captions are shorter while preserving their values, target-line legend, sample count and scroll guidance. Graph channels/tabs, ranges, hold behavior and vertical scrolling are unchanged. Delivered by `apply_mesh_graphs_visual_polish.py`; no font/image assets or transport/persistence changes are added.

### Bed mesh viewport and fitted view

Full-width mesh canvases (Studio 976 x 278, Operator 800 x 304), compact footer controls, and one-line height metrics reclaim the former side rail. Studio profile status shares the title row. The renderer fits the default orbit and height walls to the actual viewport with padding; opening and Reset view use this fitted baseline, while manual zoom/pan/orbit remain available. Native fixtures cover both themes, density/text settings, reset image equality, and odd-stride raster bounds. Build and OTA panel confirmation remain required. Firmware partition budget: 7 MiB.

### Taller mesh and Motion diagnostics visual pass

Bed Mesh gains twenty pixels of canvas height in each exposed theme: Studio 976 x 298, Operator 800 x 324. Its Studio stage uses eight pixels of the existing gap above the navigation shelf; the compact title row and Operator banner reclaim the remaining height. Profile and height summaries, footer actions and navigation remain separate. Default fitting and Reset view use the actual taller canvas.

Motion limits now align readings to the right with shared theme-owned row dividers and a subdued configuration note. Limits, Drivers and Distance use a matching title divider. The Distance introduction is a caption, leadscrew labels use sentence case, and the numeric editor names the measurement being edited. Printer-switch status uses sentence case. Calculation, transport, selection, scrolling and owner/lifetime behavior are unchanged. Native layout/interaction checks cover themes, densities, large text, narrow views, driver transitions and keyboard/edit retention; the Bed Mesh fixture checks navigation clearance and fitted-reset equality. Local build and OTA verification remain required; firmware partition budget is 7 MiB.
