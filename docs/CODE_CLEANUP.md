# Independent stale-code cleanup

The first two batches were selectively integrated into main as v7.1.33-dev.
Current branch `chore/stale-code-cleanup-main` continues from that integrated
checkpoint. Experimental viewer ancestry remains on the parked branches.

## Retired code

- 59 completed one-time `tools/migration` source rewrite scripts. No live build,
  release or audit path calls them. Stored-data and backup migrations remain.
- Unbuilt `ui_preview.c/.h`, the `ui_printer_motion` forwarding pair (the application
  now directly opens the identical toolhead popup), and the unbound
  `ui_dashboard_status` pair. Current banner, Active Print, machine and camera
  owners remain, including the active Dashboard status popup.
- Unused Network callback adapters, Dashboard startup fallback and leaf APIs in
  widgets, Printer, Network, Files, machine status and popup factories.
- Unconsumed Phase-1 thumbnail state/file/path accessors and two thumbnail-session
  wrappers. Actual worker result flags, serialized download, PNG ownership,
  selected storage, fullscreen and per-profile previews remain.

## Preserved dependencies

Theme implementations use macro-generated dispatch and must not be classified by
literal call counts. All theme files are unchanged from main. ESP-Hosted and PPA
build helpers, runtime NVS/profile migration, backup compatibility, recovery and
historical records remain. The removed motion wrapper passes exactly the same
arguments to the existing toolhead popup. Files row/cache production files match
the accepted v7.1.31/v7.1.32 implementation byte for byte.

`source_membership_test.py` checks 157 firmware C sources with none outside the
build list. `non_viewer_boundary_test.py` checks absent experimental sources/tools
and routing hooks. Its negative fixtures catch both accidental source inclusion
and a viewer action reintroduced into the coordinator.

## Apply and verify

Start `chore/stale-code-cleanup-main` from main 1b9c3216d8c5.
The updater checks the base revision, changed-file hashes and protected build,
transport, theme and touch inputs before writing; unrelated sdkconfig changes
are preserved. It supports `--check`, backups, guarded recovery and ordinary
write-failure rollback. It performs no commit, push, merge, build or flash.
Use `./tools/build_idf6_hosted3.sh` and verify on the panel before integration.

When the viewer is resumed, its branch will need a separate
reconciliation before it can be merged in future; its old commits include older
copies of these shared modules. Preserve the viewer branch as the work checkpoint.

## Batch 3 — v7.1.34-dev

The following APIs have no source, callback, generated-dispatch, build or host-test
consumer in this branch. Definitions and matching declarations are removed:

| Module | Retired API |
| --- | --- |
| Bed Mesh controller | `bed_mesh_controller_reset` |
| Camera catalog | `camera_catalog_count` |
| Custom theme | `custom_theme_base` (standalone unused getter) |
| Devices catalog | `device_catalog_controller_contains` |
| Moonraker | `moonraker_state_get` (unlocked pointer compatibility accessor) |
| Profile configuration | `moonraker_config_select_host`, `moonraker_config_select_port` |
| Webcam probe | `moonraker_probe_first_webcam_with_api_key` (unused duplicate parser/request) |
| Transport security | `moonraker_transport_security_profile_secure` (unused field getter) |
| Onboarding | `onboarding_controller_reset` |
| Printer policy | `printer_controller_state_text_color` |
| Profile health | `printer_profile_health_reset` |
| Backup | `settings_backup_export_encrypted` (unused no-progress wrapper) |
| Thumbnail session | `thumbnail_session_restore_last_selected_file`, and its result enum |
| Active Print | `ui_active_print_set_filename` (declaration only; never implemented) |
| Bed Mesh UI | `ui_bed_mesh_is_open` |
| Chooser | `ui_printer_chooser_is_visible` |
| Toolhead | `ui_printer_toolhead_init` (empty), `ui_printer_toolhead_format_extrude_command` (unused fixed 10 mm wrapper) |
| Wi-Fi scan | `network_wifi_scan_init` (no-op), outdated future-ownership comment |

The active multi-camera list probe, profile save/set paths, TLS trust/URL helpers,
locked state snapshots, variable-distance extrusion formatter, onboarding
init/complete, profile reconciliation and encrypted export worker are retained.
The active per-profile preview cache/store continues startup/profile restoration;
the retired global `hmi/last_file` restore routine was not called. Existing
last-selected-file writes and stored data are preserved. Theme macro dispatch
implementations remain unchanged; the removed custom-theme getter is an ordinary
function with no generated consumer.

Files row/cache source, Dashboard, thumbnail manager, display/touch driver and
build helpers match current main byte for byte. This is removal of unused code,
not a performance patch. The host checks and remaining target gate are documented
in [v7.1.34-dev](releases/v7.1.34-dev.md).

## Batch 4 — v7.1.35-dev

After the previous batch, five APIs have no current firmware or host-test
consumer: `printer_controller_has_active_job`,
`thumbnail_session_set_selected_file`, `thumbnail_session_load_selected_cache`,
`ui_popup_add_list` and `ui_printer_info_cards_add_vivid_icon`. The last three
private helper functions behind these paths are also removed: the session's
`copy_text`, and Printer card `card_icon_for_title`/`card_color_for_title`.
The active selected-file buffer interface and clear/save paths remain. The real
thumbnail manager still loads/stores selected-cache PNGs in its worker; only the
uncalled session forwarding function is retired. Current cards and popups retain
their own existing construction and styling.

The completed July `archive_root_clutter_pass2.sh` had no consumers outside its
own usage message and was a one-time source-tree relocation tool. It is removed,
not executed. Seven tracked root files contain zero bytes and have no path-based
build/configuration consumers: `control`, `io`, `mipi_dsi_bus`, `panel`,
`flags.buff_dma,`, `flags.buff_spiram,` and `flags.sw_rotate,`. They are unrelated
to the real panel driver and hardware configuration. Updater backups preserve
all deleted files, including their modes and empty contents.

WebSocket header text no longer says HTTP is authoritative as a Phase-1 policy.
It describes subscribed snapshot publication and leaves freshness/bootstrap to
the application coordinator. Shell comments describe current routing ownership.
Only comments change in those live modules. Data migrations, backup formats,
recovery, transport code, themes, rendering, Files caching and build helpers stay
unchanged. Source membership remains 157 firmware C files with no viewer code.

Use the v7.1.35-dev updater only after applying the verified v7.1.34-dev batch on
`chore/stale-code-cleanup-main`. It checks that incremental baseline before any
write or deletion, supports dry-run/backup/recovery and ordinary failure rollback,
and performs no Git operation, build, flash or printer action.

## Batch 5 — v7.1.39-dev

This batch retires compatibility interfaces that carried no live data:

- `ui_dashboard_set_active_print_file` was a no-op because the shared status
  banner owns the filename. Remove its declaration, implementation and one
  completion-cleanup call. Actual filename and completion cleanup remain.
- Printer Active Print's `active_file_box` and `active_file_label` never pointed
  to a filename widget. Remove their application globals, copied controller fields,
  null-output create argument, clear assignments and ignored refresh arguments.
- The live-status refresh also ignored printer state/filename, metadata dimensions
  and progress. Remove those seven ignored refresh parameters total, along with
  the two controller metadata fields that were only forwarded to those void casts.
  Upstream layer resolution and all banner/preview/info-card inputs remain.
- Direct Camera setup ignored both callback parameters; remove those arguments
  from its header, implementation, current Camera caller and host fixtures.
  The profile editor's real callbacks and standalone setup ownership remain.
- Drybox program buttons ignored `fill` because the shared outlined-button style
  owns their surface. Remove that parameter and five unused color arguments.
  Their commands, accents, geometry and event callbacks remain.
- The unused `MOONRAKER_LIVE_POLL_INTERVAL_US` macro in main is removed. Real poll
  cadence is owned by `moonraker_poll`; freshness expiry is unchanged.

In total, one no-op API, six unused globals/context fields and eleven ignored
parameters are retired. Existing test fixtures use the current interfaces without
removing their action/lifecycle assertions. No new C module or persistent data
change is introduced; source membership remains 157. Parked feature branches may
need call-site reconciliation when resumed, as with the earlier cleanup batches.

The v37 connection-state fix, v38 Quiet connection-loss/recovery policy, cached
preview continuity, thumbnail manager, Files row cache, themes, touch and build
helpers are retained. There is no intended visual/runtime change or claimed
firmware speed/size gain. Apply the guarded v39 updater over the verified v38 tree.

## Batch 6 — v7.1.40-dev

The application still supplied eight legacy Dashboard metric-label pointers that
were permanently NULL: nozzle, bed, chamber, humidity, target, heater, fan and
Moonraker status. Their runtime temperature/environment branches could not update
a widget. Remove those fields/globals and dead branches/private helpers. The
current `dashboard_live_controller` continues publishing banner/machine/hotend/
filament values through the modern Dashboard API; it is unchanged.

Eleven Printer/top-bar globals were either never bound or assigned only as
unused aliases: state/file/top-bar ETA, progress/tuning, nozzle/bed/part fan,
ETA/elapsed/remaining. Remove these, three controller label fields and the dead
file-label/top-bar refresh branches. Retire `printer_ui_controller_update_topbar_eta`
and `printer_controller_format_topbar_eta`, whose sole application path always
received NULL. Active completion-clock and elapsed/remaining formatters remain.
The Dashboard context also loses its unused `current_z` field; real layer
resolution uses the synchronized snapshot's Z and retained metadata dimensions.

A write-only `s_last_moonraker_ok_us` variable is removed. It is separate from the
locked WebSocket publication timestamp that v37 uses; that real freshness state
is untouched. The race fixture keeps conditional compatibility with the older
runtime so its optional v36 reproducer still fails for the original race.

Three ignored arguments are removed: Printer banner's duplicate state-label
handle, Wi-Fi row's absolute Y offset, and webcam normalization's API port. The
Wi-Fi list remains flex-laid-out, and normalization retains the exact existing
rule: absolute URLs unchanged, relative camera paths served through the host
web proxy rather than the Moonraker API port. HTTP endpoint/port selection is
unchanged. The unused Wi-Fi Y accumulator and increments are also retired.

This batch removes 20 unused application globals, 12 context fields, two public
ETA wrappers and two private metric helpers. No module is removed, no persisted
format changes, and 157 firmware C sources remain. The new actual-runtime fixture
covers the active Dashboard flow rather than recreating the old unbound metric
owner. Apply incrementally over v39; no visible or scheduling change is intended.

## Batch 7 — v7.1.41-dev

The timer/callback review found that Calibration only stopped its periodic
refresh through `ui_calibration_hide`. Direct root deletion left the timer and
widget handles alive. Its two locally owned Probe/Z and Axis Twist confirmation
dialogs also lacked DELETE cleanup. Those paths now retire their owners before
LVGL frees widgets, close detached calibration dialogs, and clear callbacks.
A retirement guard prevents child DELETE callbacks from rebuilding the page
halfway through cleanup. Reopening creates one current timer; repeated Show
reuses the current page. The timer rejects callbacks for a different timer.

The two calibration start callbacks now latch dispatch, retain endpoint/epoch
ownership across the transport callback, and stop if that callback closes the
page, deletes the confirmation, or changes printers. They recheck capabilities,
idle status and whether the reviewed workflow permits homing. Existing modal
profile guards, command strings, manual adjustment/results flow and rejected
start feedback remain. Navigation does not send ABORT or stop a running printer.

The real Calibration PAGE fixture adds direct confirmation deletion, root
deletion with two detached dialogs, timer-count stability across repeated
close/reopen, attempted reentrant rebuild, duplicate send reentry, changed
printer/homing/capabilities, close-during-send and rejected-start cases. The new
fixture fails against unchanged v40 on the first stale confirmation handle.
Other reviewed timer owners already have lifecycle cleanup and are retained.
No module is removed; 157 firmware sources remain. Apply incrementally over v40.

## Batch 8 — v7.1.42-dev

Setup's theme/text picker deferred persistence and rebuild with `lv_async_call`
and held `s_setup_dispatching` while queued. A direct DELETE changed the epoch,
so the old callback correctly refused persistence, but never released that busy
latch. Reopened Setup could then ignore navigation. A deleted stage could leave
the still-open Setup similarly stuck. The updated native test reproduces the
busy-latch assertion on unchanged v41.

Track the queued appearance operation and its exact epoch. Explicit close,
center deletion and owned-field deletion now cancel that async call and release
only its pending dispatch latch. Callbacks match the tracked epoch before
consuming the pending operation. If persistence replaces Setup and queues a
new choice, the returning old callback leaves that newer latch alone. All Setup dispatch completions preserve a newer pending appearance latch,
including ordinary brightness/network/camera work. Existing synchronous
transport/persistence protection and save failure handling remain.
Closed or replaced unsaved choices do not write preferences or rebuild UI.

Remove the permanently-null `s_step` legacy window handle and unreachable
`destroy_step` deletion branch; current `s_stage` ownership is unchanged. Close
still stops the poll directly. Nine obsolete STEP patch-marker comments are
also removed. The appearance callback reference is cleared on owner retirement,
while an already running save keeps its captured callback for the normal rebuild.

The Setup native fixture adds explicit/root/stage cancellation, immediate busy
release, usable navigation on reopen, duplicate clicks, no canceled preference
writes/rebuilds, successful next saves, timer balance, stale callback injection
and replacement during persistence. Existing Wi-Fi/camera/discovery/keyboard/
layout/save-failure/retry coverage remains. No source module or persisted schema
changes; 157 firmware sources remain. Apply over v41 on the current cleanup branch.

## Setup correction — v7.1.43-dev

Time zone and Display navigation readiness was hard-coded false. Track Welcome,
Time zone and Display completion in the existing onboarding owner using one
`onboard/steps` byte. Mark only after a successful save or explicit Continue;
Continue on a preference step persists its current defaults first. Failed or
canceled preference saves do not create a marker. A marker-write failure keeps
completion retryable; an already saved appearance still rebuilds and reports
that its marker needs retry. Successful markers update navigation immediately.

The existing `onboard/complete` key remains unchanged. Old completed installs
satisfy Welcome/Time zone/Display and Review without migration or new writes.
Network remains live Wi-Fi readiness; Printer and Camera use the active profile's
saved configuration. Skipping an unconfigured optional camera leaves it unchecked.
Review reflects successful Finish, never merely opening Review. New progress bits
are masked and invalid step identifiers are rejected; duplicate marks avoid writes.
The backup schema and saved preferences are unchanged; progress, like the existing
first-run completion flag, is not part of operator settings backup.

The actual onboarding NVS owner now has fault/reboot tests. The native Setup test
checks new marks across all five themes/densities/text sizes/viewports, plus
close/reopen, failed/canceled saves, accepted defaults, marker-write retry,
appearance rebuild despite marker failure, optional camera and Review completion.
Source membership remains 157. Apply incrementally over v42.

## Batch 9 — v7.1.44-dev

Toolhead's 500 ms callback rewrote position, homed/temporary feedback and Z-offset
labels even when their displayed text was identical. Use the existing widget-based
`ui_value_set_text` helper, including command feedback writes. Homed text is still
formatted identically. No separate value cache is introduced: rebuilt widgets,
changed values and expired temporary feedback still receive the current text.
The native fixture reproduces invalidations on unchanged v43 and now checks 100
unchanged polls without invalidation, real changes, invalid positions, missing
homing axes, signed Z offsets and the three-second feedback expiry.

Toolhead, Hotend and Filament Sensor periodic callbacks now reject a non-NULL
timer other than their own current timer before snapshot reads or UI changes.
Explicit NULL immediate refreshes and current timers retain existing behavior.
Tests inject an unrelated live timer and check that no snapshot read occurs;
current-owner refresh still reads/publishes state. Cadence and worker policy stay
unchanged. This does not establish any whole-firmware or hardware FPS improvement.

Remove the write-only `s_active_tab` variable and its three assignments. Actual
panel hidden flags and button checked state own selection; tests now assert those
visible states instead of the unused mirror. Remove the duplicate hotend integer
target array; preset commands use the already required numeric preset values.
All six exact 180/200/215/230/250/Off commands are covered by native tests.

The source count remains 157. No public API, persistent format, layout, command,
connection policy, Setup progress or parked-feature changes. Apply over v43.

## Batch 10 — v7.1.45-dev

Network lacked DELETE ownership callbacks for its page and cached fields. Direct
root deletion left stale pointers for late refresh/reopen. Normal teardown also
cleared the context after deletion callbacks, which could erase a newly opened
replacement page. Retire references first, track root/field deletion and guard
refresh with the current page epoch. Native tests reproduce the stale-root
assertion on unchanged v44 and cover external deletion, late calls, deleted Wi-Fi
sections/Studio banner labels, safe rebuild and replacement pages from DELETE.
Keep an explicit page-style flag so partially deleted Studio banner labels never
fall through to the differently structured legacy banner helper.

Network refresh, port and scan-status setters now use the existing widget-based
value helpers. Identical values/colors produce no invalidations over 100 native
polls; actual offline/online transitions still update. No transport scheduling,
connection-state debounce or Quiet notification behavior changes.

Scan rendering rejects nested calls while retiring old rows and checks ownership
after cleanup/status updates. SSID text/callback copies are bounded to the 32-byte
Wi-Fi limit even if the SDK array lacks a terminator. Hidden-network presentation
is retained. Rows whose SSID copy allocation fails are disabled and get no click
callback. Row DELETE cleanup verifies its actual owner before freeing the copy.
Tests cover nested rendering, maximum/missing termination, hidden rows, allocation
failure, balanced row-data allocations/frees and no leftover UI objects.

Remove three application globals that were always NULL (`network_selected_ssid_label`,
`network_password_ta`, `network_keyboard`) and their three `ui_network_destroy_objects`
output arguments, plus the unbound `networks_hint` context field. Actual password/
keyboard ownership remains in Network Tools and Setup; their behavior is retained.
No module or persisted format changes; 157 firmware sources remain. Apply over v44.

## Batch 11 — v7.1.46-dev

This batch starts from the pushed v45 cleanup checkpoint
`b180ec7cf752b396985956d09b698f4ad3c14743`. Its committed tree matches the accepted
v45 files. Future updater preflight uses this checkpoint instead of the original
main revision; no earlier updater needs to be replayed after that commit.

The shared shell lacked direct top-bar/navigation/field DELETE cleanup. Its
clock timer also survived normal shell teardown. Direct deletion left cached
pointers for later clock, Wi-Fi or navigation calls. Register owner/field cleanup,
retire both old roots before normal deletion callbacks, and stop the clock timer
when its top bar or clock label disappears. Preserve cached Wi-Fi state and the
application's printer-switch callback across rebuilds. Calls before initialization
or after retirement are safe; navigation can initialize its own shell context.
A replacement created by a DELETE callback remains owned after old teardown ends.

Clock, printer-name and Wi-Fi indicator setters now use widget-based comparison
helpers. One-second clock polling stays unchanged, but identical displayed minutes
and signal indicators do not redraw. Wall-clock formatting, unknown-time text,
timezone handling, signal thresholds and three-dB downward hysteresis remain.
Navigation and printer-switch callbacks verify the current control target. The
active-printer name still reaches E-stop when the shell's title label is absent.
E-stop dispatch/recovery behavior is otherwise unchanged.

Remove the permanently-null shell ETA field, macro and two clearing assignments.
Simplify a font conditional whose two branches both selected Montserrat 14.
No visible font/geometry, public interface, source count (157), persisted data,
connection policy or parked-feature changes.

The real shell fixture reproduces stale top-bar handles on unchanged v45. It
covers direct/normal/field teardown, callback-created replacements, stable timer/
object counts, 100 unchanged refreshes without invalidation, controlled wall time,
unknown/minute rollover, Wi-Fi hysteresis/disconnect, owned routes, fake targets,
E-stop dispatch, partial fields and early/late calls across themes/density/text.
Apply incrementally over committed v45; build and panel-check before integration.

## Batch 12 — v7.1.47-dev

This removal-focused batch deletes nine unused public functions: the standalone
Moonraker port popup show/close pair, three backup compatibility wrappers, two
Dashboard thumbnail forwarding helpers, the duplicate thumbnail-ready setter and
the unused application Files destroy wrapper. Their declarations and private popup
callbacks/state are removed too. The active printer-profile port editor remains;
its tests reject both zero and 65536 and retain storage-failure/retry coverage.

Remove the permanently-null Files popup label and the never-written HTTP-busy
flag. The poll call receives the same false value explicitly. Active transport
ownership, page teardown, thumbnail workers, backup schema/progress/rollback and
Quiet connection-loss warnings keep their existing behavior. Specialized test
hooks and generated theme dispatch remain. No module is deleted; the firmware
source inventory remains 157. No binary-size or performance improvement is claimed.

Backup tests now exercise actual progress-enabled APIs with NULL callbacks as
well as progress callbacks, including plain/encrypted restore validation and
rollback. Thumbnail tests use the existing result setter. Remove obsolete popup
fixtures; retain Wi-Fi/password/backup lifecycle checks and active-profile editor
coverage. The old connection-race negative fixture still reaches its original
runtime assertion rather than failing to compile.

Host validation: Network/backup dialogs (also ASan/UBSan), profiles, notification
backup, thumbnail manager, aspect preview, all responsive variants, Camera,
printer switching, connection refresh races and security boundaries pass. Source
membership, version/public-tree and non-viewer checks accompany guarded updater
rollback/recovery verification. Host fixtures do not validate the ESP-IDF build or
the physical panel.

Apply over effective v7.1.46-dev on chore/stale-code-cleanup-main at checkpoint
b180ec7cf752b396985956d09b698f4ad3c14743. The updater preserves sdkconfig and performs
no Git mutation, build, flash or printer action. Build with
`./tools/build_idf6_hosted3.sh`. Panel-check profile port editing, Wi-Fi/password
popups, backup export/restore, Files thumbnails and Dashboard preview.

## Batch 13 — v7.1.48-dev

Remove the unused combined Internal RAM and PSRAM label bindings, their two
cached label pointers, and the combined memory-health formatter. Production uses
separate Free now / Lowest / Largest block metric cells; their formatting and SDK
readings remain unchanged. IDF version and uptime bindings remain active.

Retarget the host lifecycle/rebinding fixture to the metric API. Keep checks for
nested refresh, rebinding during SDK reads, deletion of old/replacement fields,
no reads after retirement, unchanged refresh silence and individual cell values.
No scheduling, layout, transport, backup, persisted format or Quiet notification
policy changes. Firmware source membership remains 157.

Apply over effective v7.1.47-dev on chore/stale-code-cleanup-main at checkpoint
b180ec7cf752. Build with `./tools/build_idf6_hosted3.sh`; host tests cannot validate
the target firmware or panel. Panel-check Settings System Information values,
page close/reopen and theme changes. No Git mutation, build or flash is performed
by the updater.

## Batch 14 — v7.1.49-dev

Remove four preview helpers with no firmware callers: ui_thumbnail_fit_scale,
ui_thumbnail_fill_object, ui_thumbnail_show_image and ui_thumbnail_clear. Remove
their declarations. Keep the active file-pinned image entry point, placeholder
setter and containment helper, including temporary lifetime guards during layout.
No pixel decoder, cache, worker, lightbox ownership or scheduling change is made.

Retarget wrapper-based safety checks to show_file_image with a NULL filename and
the existing placeholder setter. Keep allocation failure, nested updates, callback
retirement, native routing, temporary-event-hook balance and layout-interruption
coverage for the active fit helper. Drop tests specific to the deleted scale/fill
utilities; test containment after resizing instead. Retain decoder pixel sampling,
aspect fit/packed dimensions, padding, crop, bounds and degenerate-image tests.

Apply over effective v7.1.48-dev on chore/stale-code-cleanup-main, checkpoint
b180ec7cf752. Source membership remains 157. Build with
`./tools/build_idf6_hosted3.sh`; target build and panel validation are not available
here. Panel-check Files row previews, Dashboard/Printer thumbnails, image taps and
fullscreen fit, including page/theme changes. The updater performs no commit,
push, build, flash or printer action.

## Batch 15 — v7.1.50-dev

Remove the unused boolean notification getter/setter. Active callers already use
Quiet / Standard / Detailed through ui_toast_level and ui_toast_set_level. Host
preference tests now use those APIs too. Retain the stored boolean migration in
ui_toast_init and backup parsing: older preferences and backups remain readable.
No notice filtering, warning delivery or Quiet connection-loss policy changes.

Remove the unused Active Print canvas filename-size accessor and placeholder-hide
helper. Keep the active image application, placeholder setter, buffer ownership,
file matching and preview generation paths. These removals have no production
callers. No modules or persisted formats change; source membership remains 157.

Run notifications, notification_backup, active_preview_safety and
aspect_preview_pipeline host suites, plus inventory, version, public-tree,
non-viewer and guarded updater recovery checks. Existing notification fixtures
retain persistence failures/retry, legacy migration, warning/connection policy and
all theme/density/text-size layouts. Host tests cannot validate target firmware.

Apply over effective v7.1.49-dev on chore/stale-code-cleanup-main at checkpoint
b180ec7cf752. Build with `./tools/build_idf6_hosted3.sh`. Panel-check notification
level selection/persistence, genuine Quiet connection-loss/recovery and Dashboard/
Printer previews. The updater performs no Git mutation, build, flash or printer
command.

## Batch 16 — v7.1.51-dev

Remove the unused ui_create_button custom-color constructor and its primary,
secondary, success, warning and danger wrappers from ui_widgets.c/.h. No firmware
caller uses this family. Active controls use ui_button_create and the shared
semantic styling/touch-target paths; these remain unchanged.

Remove the obsolete custom-color sample from the Console pressed-frame catalog.
Retain every semantic button kind, status style, navigation, Studio action, theme
preview and surface-role checks. Drop the removed API names from the button-label
inventory scanner; continue scanning active button constructors and popup actions.
No layout, action routing, persisted format, transport or Quiet notice policy
changes. Firmware source membership remains 157.

Validate Console layout/pressed frames, button text-fit inventory and LVGL95
modernization suites, plus source membership, version/public-tree/non-viewer and
guarded updater rollback/recovery checks. Host tests cannot validate the target
firmware or physical panel.

Apply over effective v7.1.50-dev on chore/stale-code-cleanup-main at checkpoint
b180ec7cf752. Build with `./tools/build_idf6_hosted3.sh`. Panel-check Console actions
and dialogs, button pressed states and theme changes. The updater performs no
Git mutation, build, flash or printer action.

## Batch 17 — v7.1.52-dev

Remove three test-only constructors: ui_active_print_create, ui_page_state_create
and ui_thumbnail_wrap. Production uses ui_active_print_create_profile,
ui_page_state_create_owned and ui_thumbnail_create. Remove the old declarations,
fixed Active Print layout preset and alternate thumbnail-box initialization.
No active page layout or thumbnail ownership behavior changes.

Active Print fixtures now use the current Dashboard layout profile with explicit
test rectangles. Keep canvas replacement, generation, buffer bounds, nested and
interrupted update tests. Page-state tests exercise an optional NULL owner slot
through create_owned, preserving the lifetime check. Remove the obsolete wrapped-
box scenario from thumbnail safety tests; standard creation, owner binding,
replacement slots, layout interruption, native routing and allocations remain.

Run aspect_preview_pipeline, active_preview_safety, page_state_safety and
thumbnail_component_safety host suites with usual LVGL flags. Retain source,
version/public-tree/non-viewer and updater rollback/recovery checks. Source
membership remains 157. No persisted formats, connection policy or Quiet warnings
change. Host checks do not validate firmware or the physical panel.

Apply over effective v7.1.51-dev on chore/stale-code-cleanup-main at checkpoint
b180ec7cf752. Build with `./tools/build_idf6_hosted3.sh`. Panel-check Dashboard and
Printer previews, Files image taps/fullscreen, and loading/offline/empty page
states. The updater performs no Git mutation, build, flash or printer command.

## Batch 18 — v7.1.53-dev

Remove five public entry points with no firmware callers: the lightbox descriptor
show, image-object show and is-open wrappers, and the owned Wi-Fi scan show/render
wrappers. Keep the underlying lightbox snapshot/image-object implementation and
file-pinned asynchronous fullscreen entry point. Keep the Wi-Fi scan rendering,
password workflow, owned close and status APIs. Fix the Network Tools header's
stale port-dialog ownership comment.

Retarget tests that include the real implementation to the underlying functions
and current popup state. Keep assertions for immutable snapshots, source aliasing,
worker queue/retry/stale publication, native close taps, allocation failure and
buffer cleanup. Retain Wi-Fi row selection, status-field deletion/reopen, keyboard
and popup ownership, inline validation, backups and replacement callbacks. No
user-facing route, cache algorithm, persisted format, scheduling or Quiet warning
policy changes. Firmware source membership remains 157.

Run the full preview_cache_ownership suite (with cJSON and LVGL host paths) and
network_backup_dialog suite. Retain source, version/public-tree/non-viewer and
updater rollback/recovery checks. Host checks do not validate the ESP-IDF build
or physical panel.

Apply over effective v7.1.52-dev on chore/stale-code-cleanup-main at checkpoint
b180ec7cf752. Build with `./tools/build_idf6_hosted3.sh`. Panel-check Files and
Dashboard/Printer image taps, fullscreen close/reopen and Wi-Fi scan/password
flows. The updater performs no Git mutation, build, flash or printer command.

## Batch 19 — v7.1.54-dev

Remove the unused thumbnail_render_to_rgb565 center-crop entry point and its
sampling branch. Remove the shared renderer's fill flag: all active callers use
either tightly packed aspect-preserving previews or complete-image letterbox fit.
Keep those algorithms, decoded pixel-format conversion, no-cache decoder lifetime,
stride handling and descriptor ownership unchanged.

Remove crop-only pixel assertions. Retain full-image exact sampling/padding,
packed width/height, untouched buffer tails, guard pixels, degenerate sources,
NULL rejection and containment/resizing coverage. The preview_fill test filename
stays stable for existing commands, with its description updated to active modes.
No user-facing crop option is removed: firmware had no caller of this path.

Run preview_fill, aspect_preview_pipeline and full preview_cache_ownership host
suites, plus source/version/public-tree/non-viewer and updater recovery checks.
Firmware source membership remains 157. No cache format, worker scheduling,
persisted setting, transport behavior or Quiet warning policy changes. No target
binary-size or performance claim is made.

Apply over effective v7.1.53-dev on chore/stale-code-cleanup-main at checkpoint
b180ec7cf752. Build with `./tools/build_idf6_hosted3.sh`; host tests cannot validate
target firmware or the panel. Panel-check wide/portrait previews in Files,
Dashboard/Printer and fullscreen, including close/reopen. The updater performs no
Git mutation, build, flash or printer command.

## Batch 20 — v7.1.55-dev

Remove ui_printer_popups_show_printer_status, an unreachable status-summary
formatter with no firmware caller, and its public declaration. Retain the shared
ui_dashboard_status_popup_show/close implementation used by active workflows,
including error/status messages and Drybox live status.

Retarget the sensor/status fixture's long-file presentation case to the shared
popup with fixture-supplied text. Keep full 255-character filename visibility,
wrapping, scroll/footer bounds, native close and external deletion/reopen checks.
Keep filament sensor toggles, acknowledgement/timeout, reorder/removal, stale
profile and unsafe-name tests. Active Printer controls and status routes do not
change. No scheduling, schema, cache or Quiet warning policy changes.

Run sensor_status_layout, hotend_layout and motion_layout host suites, plus source,
version/public-tree/non-viewer and guarded updater recovery checks. Firmware source
membership remains 157. Host tests cannot validate the ESP-IDF build or panel.

Apply over effective v7.1.54-dev on chore/stale-code-cleanup-main at checkpoint
b180ec7cf752. Build with `./tools/build_idf6_hosted3.sh`. Panel-check filament
sensors, active Printer controls and shared status dialogs. No commit, push,
build, flash or printer command is performed by the updater.

Checkpoint recommendation: the straightforward unused-interface removal pass is
ready for integration review after panel validation. Remaining low-reference
interfaces include intentional test/reset hooks, generated theme dispatch and
legacy/request ownership code that needs separate design review. Reference counts
alone are not sufficient evidence to remove them.
