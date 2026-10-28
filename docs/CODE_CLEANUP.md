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
