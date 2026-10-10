# Test plan

PrinterHMI changes are not complete when they compile. The target device is the
acceptance environment.

## Build gate

Host checks for Tools actions (requires a C compiler; pass `--cjson-dir` to
`tools_features_test.py` for JSON controller tests when cJSON sources are not
available under ESP-IDF or managed components):

```bash
python3 tools/audit/tools_action_safety_test.py
python3 tools/audit/tools_features_test.py
```

```bash
./tools/build_idf6_hosted3.sh

source "$HOME/esp/esp-idf-v6.0.2/export.sh"
idf.py -B build-idf6-hosted3 size size-components
git diff --check
```

Requirements:

- no build failure;
- no new actionable compiler warning;
- application and bootloader fit their partitions with margin;
- no unexpected internal-RAM regression;
- only intended files are modified.

## Startup and persistence

- Cold power-on passes FreeRTOS timer-task creation and reaches
  `main_task: Calling app_main()` without rollback.
- Cold power-on reaches the printer chooser/dashboard without repeated splash flashing.
- A single accepted initial panel appearance is distinguished from repeated flashes.
- Warm reboot clears the splash.
- First boot after OTA completes and marks the image valid.
- A second reboot and full power cycle also succeed.
- Clock shows the selected timezone after SNTP synchronization.
- Theme, appearance, brightness, sleep, timezone and printer profile persist.
- Settings System Information reports current/minimum internal heap, largest
  contiguous internal block, and current/minimum PSRAM without overlap.

## Display and interaction

- Touch coordinates align across all screen edges.
- Dashboard, Printer, Files, Bed Mesh, Macros, Console, Telemetry, Drybox,
  Network and Settings open.
- All ten sidebar buttons fit without overlap and follow the documented order.
- Persistent status bar and navigation remain aligned.
- Every popup blocks interaction with content behind it.
- Popup footer buttons are visible, aligned and restore interaction on close.
- Theme A, B and C rebuild all visible pages without reboot.
- Accent, density and each accessibility option produce the expected change.

## Printer and Moonraker

- Each configured profile probes and selects correctly.
- Profile Add/Edit discovery fills host and port; `SAVE` remains explicit.
- Secure profiles verify HTTPS API and WSS with their selected CA, do not downgrade after a failed certificate check, and Standard profiles retain their prior behavior.
- Switching profiles cannot publish stale data from the previous profile.
- Live WebSocket status updates; HTTP polling recovers after disconnect.
- Pause, resume, cancel and motion controls send the intended command.
- Nozzle, bed, fan, speed and flow controls update safely.
- Layer values follow Moonraker and fall back to file metadata when absent.
- Exclude-object list and map agree; selection requires confirmation.
- Offline and error paths show a useful failure state without freezing the UI.

Never run destructive printer commands without a safe machine state and an
operator present.

## Calibration, Bed Mesh, Devices, Macros and Console

- PID, Input Shaper, Axis Twist, Z Tilt, Pressure Advance, Probe Z and custom
  calibration paths open the intended workflow and preserve Back behavior.
- Guided manual-probe controls update session state and require confirmation
  before SAVE_CONFIG.
- Devices filters, horizontal filter scrolling, pagination and automatic
  visible-value refresh remain responsive with multiple hotends.
- Bed Mesh renders the active profile as a solid height-colored surface.
- Surface lines toggle independently from the rear X/Y/Z reference planes.
- Minimum, maximum and range values match the mesh data.
- Lower-left mesh origin and rear-plane X/Y zero markers agree.
- One-finger drag rotates, pinch zoom responds promptly and two-finger drag
  pans without reversing direction or fighting another gesture.
- Calibration and profile Save/Remove paths use confirmation and recover after
  Klipper restart.
- Bed Mesh Calibrate/Load/Save/Remove reject offline, printing, paused, error
  and shutdown states, including state changes while confirmation is open.
- Profile Save/Remove sends one script containing the profile operation followed
  by SAVE_CONFIG. An invalid profile operation must not restart Klipper.
- Switch printer profiles after calibration results appear: old results cannot
  enable SAVE_CONFIG on the new printer, even before the next UI refresh.
- Unfinished accelerometer checks time out after 2 minutes, screws adjustment
  after 10 minutes, and other tracked sessions after 30 minutes. The timeout
  reports missing results and does not cancel the printer operation.
- Accelerometer noise results complete the check without enabling SAVE_CONFIG.
- Completed calibration results remain available on the same printer.
- Devices tile describes inspection and live readings.
- Probe Accuracy appears only when Klipper reports PROBE_ACCURACY support.
  Home XYZ and position the probe over the bed in Printer first. Confirm tests
  with 5/10/20 samples; all six final statistics appear, with no Apply & Restart.
  Unhomed, printing, paused, offline and switched-profile starts are blocked.
- Devices Endstops opens a live view with OPEN/TRIGGERED states. Press a physical
  switch and verify changes arrive on the next 2-second sample. Queries stop on
  close, pause during printing, and clear old readings when offline or switched.
  A failed/timed-out query displays a message and retries; a late reply cannot
  revive a closed viewer. Check a printer with virtual/sensorless endstops too.
- Macro search is case-insensitive and preserves Favorites ordering. Clearing
  restores the full catalog; no-match and discovery-wait states remain usable.
- Macro parameter fields recognize params.NAME, params['NAME'] and
  params.get('NAME', ...). Empty values are omitted; two additional named fields
  cover parameters that cannot be inferred. Dynamic/rawparams macros may need
  Console. Test required fields, macro defaults, parameter-free macros and
  filenames with spaces (quoted automatically). Invalid names, duplicate names, command
  overflow, quotes, backslashes, semicolons and line breaks are rejected.
- Editing parameters never sends a command. Review shows the exact command;
  Run sends it once. Cancel, navigation, disconnect and profile changes leave
  no keyboard or modal behind and cannot send to another printer.
- Public macros appear alphabetically; underscore-prefixed helpers do not.
- Running a safe macro requires confirmation and is recorded in Console
  history.
- Known commands show normal responses, warnings remain amber and unknown
  commands render red.

## Camera

- Camera opens an active-profile continuous MJPEG view and updates smoothly.
- Fullscreen opens and returns without hiding the top bar or navigation after exit.
- **Configure → FIND CAMERAS** completes over both Standard and Secure
  Moonraker profiles without resetting the HMI.
- A profile without a camera shows no stale image from another profile.
- Camera selection, manual URL configuration and active-camera identity persist
  per printer profile.

## Console filters

- Run `python3 tools/audit/console_filters_test.py` for host matching checks.
- Select All, Errors + warnings, Errors, Warnings, Commands, Responses and System;
  only matching entries appear, in chronological order with no empty row gaps.
- Search matches message text case-insensitively and combines with type/temperature
  filters. Cancel retains the previous search; Apply/keyboard Done applies it.
- TEMPS OFF hides complete numeric temperature responses but retains warnings,
  errors and descriptive heater/probe messages. TEMPS ON restores those responses.
- The visible/total count and empty-state messages track incoming and cleared logs.
- RESET restores All, empty search and TEMPS ON without deleting history.
- Filter settings survive page navigation in this session; search popups/timers
  close with the page. Command history and calibration response handling remain intact.
- FOLLOW ON follows the last matching entry. FOLLOW OFF retains scroll position as
  messages arrive. Normal and larger text controls fit and do not overlap.

## Motion diagnostics

- Calibration → Motion shows LIMITS, DISTANCE and DRIVERS above the existing
  SHAPER, RESONANCE and SENSOR actions. All six buttons fit without overlapping
  touch areas with normal and larger text.
- LIMITS follows current runtime velocity, acceleration, square corner velocity
  and minimum cruise ratio, including changes from M204/SET_VELOCITY_LIMIT.
  Missing fields show unavailable. Opening the view sends no motion commands.
- DRIVERS lists discovered TMC drivers, currents and reported temperature/status.
  Unsupported temperatures and null/unsampled driver status remain unavailable;
  they must not appear healthy. Warning/fault flags update and driver selection
  survives refresh. Standalone drivers show an explanatory empty state.
- Disconnect and printer-profile changes clear live readouts. Closing the popup
  or leaving Calibration stops refresh and closes any numeric editor.
- DISTANCE calculates belt pitch × pulley teeth (2 × 20 = 40 mm) or screw thread
  pitch × starts (2 × 4 = 8 mm), and shows configured X/Y/Z reference values.
  Reject zero, negative, nonfinite, malformed and fractional tooth/start counts.
  Preserve configured gear ratios when applying a result manually. This tool
  sends no G-code, edits no configuration and does not calibrate XYZ from measured
  travel or printed-part dimensions.

## Files and previews

- File search uses the shared keyboard and filters correctly.
- Files with thumbnails show the preview in the row.
- Long press opens the large preview and metadata popup.
- File start requires the intended confirmation path.
- SD cache survives reboot and remains scoped to the correct printer/file.
- Missing SD, missing thumbnail and malformed metadata degrade gracefully.

## OTA

- OTA keyboard and progress popups open and close without top-to-bottom redraw.
- Progress remains responsive during download.
- Cancellation closes promptly, does not reboot and preserves the running image.
- A completed update reboots and validates the new image.

## Drybox and telemetry

- Live drybox temperature, humidity, heater and fan values update.
- PLA, PETG and hold commands reach the intended macros.
- Nozzle/bed and chamber/humidity charts update without scrolling artifacts.
- Chart ranges, reference lines and newest-sample markers remain legible in all
  themes.

## Network interruption

- Boot without an access point remains responsive.
- Moonraker offline does not block navigation.
- Wi-Fi and Moonraker reconnect without reboot when service returns.
- SNTP failure leaves the UI usable and retries on a later network cycle.

## Soak test

For a release candidate, leave the device running with live Moonraker traffic
for at least two hours. Exercise page changes, popup creation/deletion, profile
switching and thumbnail loads while monitoring resets, heap trends and task
watchdogs.

## Test record

Record commit, tag, binary checksum, hardware revision, flash method, tests
performed, pass/fail result and any accepted exception.

## LVGL modernization branch

```bash
python3 tools/audit/lvgl95_modernization_test.py
python3 tools/audit/preview_fill_test.py
```

Both accept `--lvgl-dir /path/to/lvgl`; an optional `--lvgl-lib` reuses a host
archive built with the same font/stdlib flags. The UI tests use real LVGL objects,
themes and layouts with fixture printer/controller data. They check row identity,
bounded object counts, callback rebinding, stale macro generations, Device page
indices, scroll preservation, teardown, ten parameter fields, all three themes,
normal/large text and popup resize/cleanup. Preview tests check native Cover/Contain
sizing and resized frame coverage in addition to the renderer.

Target acceptance after the IDF6 build:

- Add Console messages with Follow On/Off; change filters, clear history and reopen.
- Search/favorite macros, open the correct rebound row, edit all parameter fields
  and confirm that Cancel/Review and the onscreen keyboard remain usable.
- Change Device categories/pages, reconnect and switch printers; confirm each
  live value stays attached to the correct object.
- Open Network and Printer popups; all footer actions must work and close the
  entire modal/backdrop. Check normal/large text and all three themes.
- Compare Dashboard, Printer and Files preview fill and fullscreen proportions;
  check load/retry, tap-to-close, reconnect and profile switch during loading.
- Check target heap/PSRAM and frame pacing before claiming a performance gain.

## Boot splash refresh hold

```bash
python3 tools/audit/boot_splash_freeze_test.py
```

The real LVGL host check renders the splash, then verifies that underlying label
updates produce no further display flushes while application timers continue.
Progress updates must flush only the bar/percentage/status region, reach 100%
and skip duplicate stages. It checks all three themes with normal/large text,
then verifies that close restores invalidation, repaints the whole current screen
and allows normal refreshes, including repeated/no-freeze teardown.
The test accepts the same LVGL source/archive options as the UI checks above.

On the panel, test cold power-on and warm reset with Wi-Fi connected and unavailable.
The logo/background should stay stable while the bar, percentage and status
advance through startup, then hand off to the chooser once. Confirm normal touch, popup, Console and preview updates afterward. Host
checks cannot validate MIPI timing or backlight behavior.

## Preview cache ownership

```bash
python3 tools/audit/preview_cache_ownership_test.py
python3 tools/audit/preview_fill_test.py
```

Both support `--lvgl-dir` and `--lvgl-lib`. The ownership test uses real LVGL
image descriptors/cache APIs with ESP allocator/lock fixtures. It checks profile
reuse, dimension/revision changes, failed/stale publications, bounded PSRAM-only
allocations, Files display-first publication and single-buffer transfer, and
fullscreen snapshots/repeated teardown. These are host lifetime checks, not a
FreeRTOS race stress test.

On the panel, repeat Files refresh/folder changes, opening detail and fullscreen,
closing while hires is loading, profile switching and reconnects. Confirm every
preview still looks the same and tap-to-close works. Bounded cache pools can
retain their high-water allocation. Cold/warm splash progress must remain stable.

## Telemetry refresh optimization

```bash
python3 tools/audit/telemetry_refresh_test.py
```

Supply `--lvgl-dir`/`--lvgl-lib` for unmanaged host LVGL. The real LVGL test
checks zero invalidation for repeated text/color, local-versus-inherited color
behavior, deferred chart equivalence over wraparound/flat values/gaps, two-second
sampling, capability transitions, expired extrema, bounded full-history reload
and teardown. Changed consumers also compile with warning-as-error flags.

On the panel, compare Dashboard/Printer values during heating, printing, pause
and idle; status, progress, fan and times must advance as before. Open Telemetry,
leave it open through steady temperatures and confirm the graph still advances
once per two seconds. Close/reopen, change printer, test missing sensors and
check all three themes/text sizes. Cold/warm boot and fullscreen previews should
retain their verified behavior. Host checks do not establish hardware performance
gains. The operator confirmed this panel validation; the temporary Performance
monitor is removed. Check Settings System Information ends at Uptime without an
empty extra row, and Storage starts at its normal position in all themes/text sizes.

## Splash-to-chooser handoff

Run `python3 tools/audit/boot_splash_freeze_test.py` with the LVGL arguments above.
It checks partial/direct double-buffered rendering, one completed handoff frame,
held background repaint, bounded progress regions and gradual saved-brightness
restoration without a redundant 100% write. Test cold power-on and warm reset
on the panel at saved 100% and a lower brightness (for example 50%). The chooser
must replace the splash without a flash; lower brightness should settle smoothly.
Confirm the moving progress bar, chooser selection and normal page updates.
These host tests do not establish panel-level flash elimination.

## Chooser periodic refresh

```bash
python3 tools/audit/chooser_refresh_test.py
```

Use `--lvgl-dir`/`--lvgl-lib` as above. The production chooser runs on real LVGL
with double-buffered direct rendering. Repeated 500ms timer callbacks must cause
no invalidations or flushes with unchanged inputs. Tests cover all themes/text
sizes, inactive health text, live status transitions, preview arrival/replacement
revision/removal, active-profile switching and timer cleanup. On hardware, leave
the chooser visible for at least ten seconds after cold and warm startup, then
confirm printer selection, health changes and restored previews still work.
These checks isolate chooser writes; they do not prove panel-level flash removal.

## Runtime startup behind splash

The eight-second isolation test tied the reported white flash to runtime/preview
startup and machine-state arrival. Confirm cold and warm boot now starts those
workers during the splash hold (`STARTUP_TRACE runtime-start under splash hold`).
Readiness/progress must continue advancing. Leave the revealed chooser visible
for at least ten seconds; confirm states/previews update without a white flash,
then select each printer and check normal live data. Network unavailable startup
must still reach the chooser and reconnect normally. There is no diagnostic wait.
The host splash checks cover refresh hold/progress, not MIPI-level flash behavior.

Panel result: the operator confirmed the runtime-startup correction resolved the
reported white flash. Preserve service startup under the splash hold when
changing boot lifecycle order; the eight-second isolation delay is removed.

## Thumbnail storage and download buffers

Run `python3 tools/audit/thumbnail_storage_test.py` with a host C compiler. It
compiles the production SD storage policy and extracts the production HTTP
downloader into an allocator/transport harness. Checks cover 64-file and 16 MiB
eviction, newly written file preservation, separate directory budgets, exclusion
of profile/nested/user files, replacement (including EEXIST/FAT-style rejection and rollback) and
write/close/rename failures,
512 KiB rejection, exact-size download ownership, realloc failure fallback and
failed/short/oversized-path transfer cleanup. This does not emulate FAT or ESP-IDF.

Build with `./tools/build_idf6_hosted3.sh`. On the panel, check file-list previews,
ready-to-print popups and fullscreen previews on first/repeated opening. Switch
printers and cold/warm boot to confirm restored chooser previews and no white
flash. Test without SD and with network unavailable; neither should block the
chooser. Browse more than 64 distinct cached files and inspect cache size on SD;
evicted previews should redownload when revisited. Firmware build and panel
validation are required; host checks cannot establish power-loss durability.

Panel acceptance: the operator reported the SD cache/download cleanup looks
good. This records functional acceptance, not a filesystem power-loss test or
quantified memory/performance measurement. Final integration reruns repository
audits and the canonical IDF6 firmware build through the end-of-night script.

## Status observer pilot

Run `python3 tools/audit/status_observers_test.py`; supply `--lvgl-dir` and
optionally `--lvgl-lib` for unmanaged host LVGL. The production banner runs on
real LVGL across all themes and normal/large text. Checks cover 100 unchanged
updates without subject notifications, independent banner instances, state and
message transitions, literal filename percentages, progress and palette, null/
hidden messages, untruncated long-text fallback, repair after fallback or direct
compatibility writes, unavailable-binding fallback and repeated parent/child
teardown. The canonical IDF6 config already enables `CONFIG_LV_USE_OBSERVER`.

Build/flash and check Dashboard/Printer state, connection messages, active
filename scrolling and notices during idle, heating, printing and pause. Switch
printers/pages, change theme/text size and reopen repeatedly; the new banner
must show current values without stale text. Check cold/warm splash progress,
chooser handoff and previews. Host tests cannot prove target performance or
display timing. The operator confirmed the banner pilot on the panel; version remains 6.5.6.

## Chooser status observer extension

Run `python3 tools/audit/chooser_refresh_test.py` with the LVGL source/archive
options above. The existing real-LVGL direct-rendering fixture now also verifies
per-card status subjects, zero notifications on unchanged 500ms callbacks,
printing/paused/offline/verifying transitions, stale-state dimming and theme
error colors. It retains preview revision/removal, active-profile and no-redraw
checks, and adds long/unavailable-binding fallback repair, independent card
deletion and chooser subject teardown/reopen across all themes/text sizes.

After the canonical IDF6 build/flash, leave the chooser visible for ten seconds
after cold/warm startup: there must be no delayed white flash. Check current
active/inactive printer states, health changes, dimming while verifying, preview
arrival and switching printers. Reopen the chooser and change theme/text size.
The operator confirmed the preceding banner and chooser status pilots on the
panel. Reconnect recovery remains deferred; version is 6.5.6.

## Chooser profile-text observer extension

The same `chooser_refresh_test.py` checks all three string bindings with real
LVGL across themes/text sizes. It now covers renaming, hostname/port edits,
configured-to-empty and empty-to-configured slots, index-based click routing,
name/endpoint long-text and unavailable-binding fallback, and unchanged edited
profiles remaining free of notifications, invalidations and flushes.

After build/flash, rename a profile and edit its endpoint, then reopen the chooser
and confirm the displayed values and selected printer. Check empty/add-printer
cards, status changes and restored previews; repeat page/profile/theme changes.
Cold/warm startup must retain splash progress and the flash-free chooser handoff.
The operator confirmed the banner, chooser status and name/endpoint observer
changes on the panel. Stable version remains 6.5.6.

Observer branch closeout: all three incremental functional checks were accepted
on the panel. The repository audits and canonical integrated-main IDF6 firmware
build run through the closeout/end-of-night workflow. Host acceptance covers
subject lifetimes and redraw behavior; it does not establish measured target
performance or a completed release soak. Reconnect recovery remains deferred.

## Devices and Drybox refresh checks

Run `python3 tools/audit/live_widget_refresh_test.py` against managed LVGL sources,
or pass `--lvgl-dir /path/to/lvgl` and optionally `--lvgl-lib /path/to/liblvgl.a`.
The real-LVGL test checks data-widget refreshes independently of shared banner
styling; it does not claim the whole page has zero redraws. It uses libc float
formatting in the Devices fixture when the host archive lacks float printf and
separately compiles both unmodified production consumers with strict warnings.

On the panel, verify Devices live values still change; filtering and leaving/
reopening the page populate fresh values. Verify Drybox temperatures, humidity,
heater/fan readings and active-program highlighting. Check offline placeholders
and disabled controls, then restored readings and cyan humidity after reconnect.
Use only an appropriate drying program for the loaded material when testing
commands. Repeat after theme/text-size changes and profile switching.

## Shared banner refresh follow-up checks

Run `python3 tools/audit/status_observers_test.py` and the Devices/Drybox test
above. Both accept `--lvgl-dir` and optional `--lvgl-lib`. The Drybox fixture now
includes the real banner, superseding its earlier data-only redraw scope. The
banner fixture covers four themes, text sizes, status/file/ETA/progress updates,
repeated-value redraw silence, custom opacity, accessibility and external style
repair; all local properties match the concrete theme recipes.

On the panel verify Dashboard and Printer status, filename, ETA and percentage
updates, including pause/resume and error states. Check Drybox READY/HEATING/
DRYING/OFFLINE colors and recovery, then switch themes/text size and revisit
these pages. Normal progress animation and reduced-motion settings should retain
their previous behavior. Panel performance has not been measured by host tests.

## Preview cutoff regression

Run the preview-fill, preview-cache-ownership and LVGL-modernization host checks.
The historical preview-fill filename retains checks for the unused explicit fill
utility and adds aspect-fit pixel/bounds checks. Small-preview consumers must
use `thumbnail_render_to_rgb565_fit` and zero-inset `ui_thumbnail_fit_object`.
On hardware, check a wide and tall model in Operator Dashboard/Printer and Files
ready-to-print popup, then tap fullscreen. Confirm the complete model is visible,
including after page/profile/theme changes. Space around mismatched proportions
is expected. No panel performance result is inferred from host tests.

## Tools live refresh checks

Run `python3 tools/audit/tools_live_refresh_test.py`, with `--lvgl-dir` and optional
`--lvgl-lib` when using external host sources/archive. Production UI functions run
against real LVGL, with transport/controller snapshot fixtures. The check covers
unchanged redraw silence, fresh readings, driver discovery/selection and fault
colors, endstop query cadence and existing print/offline/profile guards.

On the panel check Tools → Calibration → Motion LIMITS and DRIVERS, including
selecting another driver, then reopen them after a profile/theme change. Check
Live Endstops when idle; readings should update as switches change. During a print
it should keep the existing paused-readings notice. No motor motion or settings
change is introduced by this patch. Confirm previews still show the complete model.

Live-widget branch closeout: the operator accepted the incremental Devices,
Drybox, shared-banner, complete-preview and Tools functional checks on the panel.
The host suites validate update/caching lifetimes and rendering/query behavior.
The canonical integrated-main firmware build and repository audits run through
the end-of-night closeout; acceptance does not imply a completed soak or measured
target performance improvement. Stable version remains 6.5.6.

## Aspect-aware preview increment

Run the preview-fill and cache-ownership checks plus
`python3 tools/audit/aspect_preview_pipeline_test.py`; all accept `--lvgl-dir` and
optional `--lvgl-lib`. The pipeline fixture compiles production Dashboard functions
extracted from main.c and production Active Print code, with transport stubs.
Checks cover actual packed dimensions, every output pixel, untouched tails,
wide/portrait/square canvas reuse, invalid descriptor rejection, display-lock
publication and stale profile jobs across four themes/text sizes.

Flash and check full model visibility, increased usable preview size, file
changes, page/profile/theme switches and fullscreen loading/tap-to-close. Test
portrait as well as the normal 900×520 thumbnail. Camera/layout changes follow
this increment's panel acceptance; no performance measurement is inferred.

## Camera pipeline checks

Run `python3 tools/audit/camera_pipeline_test.py` with managed LVGL sources, or
pass `--lvgl-dir` and optional `--lvgl-lib`. The fixture uses production Camera
functions with a frame/transport/catalog stub and real LVGL. It asserts source
detachment before pixel free, all rotation/mirror combinations, instantaneous
fullscreen fitting, repeated-status/transform redraw silence, truncated-frame
rejection and hide/destroy cleanup across four themes/text sizes.

On the panel check Camera and Dashboard's fullscreen camera entry. Enter/exit
fullscreen, tap to exit, select another camera, rotate/mirror/reset, leave and
reopen the page, and switch profiles/themes. Verify new frames continue, old
profile frames clear, reconnect status works and the aspect-aware print previews
remain correct. These host tests do not measure HTTP/JPEG throughput or prove a
long-running stream soak.


## Offline navigation checks

Run `python3 tools/audit/offline_navigation_test.py` and
`python3 tools/audit/files_load_worker_test.py` (the Files suite accepts
`--lvgl-dir` and `--lvgl-lib`). Re-run the Camera and preview ownership suites.
The offline fixture compiles the production camera worker and extracted
WebSocket lifecycle functions with delayed transport/task mocks. It checks
nonblocking consumer retirement, late-frame disposal, exclusive worker handoff,
interruptible retry backoff, preserved explicit quiescence, generation fencing
and rapid selections during runtime teardown. The Files fixture compiles the
production controller with real LVGL timers: HTTP occurs only in its worker,
latest jobs coalesce, stale page/profile/request results never publish, detail
popups defer publication, and allocation/task failures release ownership.

On the panel, disconnect one printer from the network while PrinterHMI stays
powered. With Camera live or reconnecting, navigate among Dashboard, Printer,
Files and Tools, open the chooser and select the online printer. Navigation
should respond promptly even while new data is pending; the offline connection
must not keep old frames/readings fresh. Repeat rapid selections, close Files
during a pending load, restore the endpoint, and check both printers recover.
Recheck camera fullscreen/rotation/mirror and OTA quiescence. A power cycle
recovering the printers does not establish that the runtime issue is fixed;
these host checks do not prove a target offline/recovery soak.


## Responsive layout acceptance

Run `python3 tools/audit/responsive_layout_test.py` with managed LVGL, or pass
`--lvgl-dir` and optional `--lvgl-lib`. It compiles production Tools, Files and
Macros surfaces against real LVGL, testing all four themes × three densities ×
two text sizes × three widths, both with and without representative custom
metric/profile overrides. The checks cover text/control bounds, non-overlap,
long names, actual Files rows, status overlay title/detail separation, favorites,
empty search and show/hide/reopen. Set `RESPONSIVE_SCREENSHOTS` to an existing
folder to export PPM renders for each built-in theme at comfortable density with
large text. Re-run LVGL modernization, offline-navigation and Files worker suites.

On the 1024×600 panel visit Tools, Macros and Files in Classic, Operator, Glass
and Operator Shell. Check each density and large-text setting: all tile text and
controls should be visible, long macro names should wrap, Files actions should
stay above the list, filenames should leave room for the arrow, and status text
should not overlap. Verify macro search/favorites/parameters, Files selection,
scrolling/search/sort/refresh and fullscreen previews. Recheck switching pages/
printers while one printer is offline. This is the first responsive page pass;
Dashboard/Printer/camera preview geometry and network ownership are preserved.


Preview/camera/layout closeout acceptance: aspect previews and responsive
Tools/Macros/Files were accepted by the operator. Navigation was reported faster
after the offline follow-up. Host coverage includes frame/cache lifetimes, stale
profile/request retirement, worker ownership, native wrapping, all four themes,
densities, text sizes, viewport widths and representative custom overrides.
These checks do not imply an offline/reconnect or camera soak. The real canonical
ESP-IDF build, source push and nightly firmware/checksum publication are performed
by the repository's end-of-night script on integrated main. Stable version 6.5.6
is restored after the nightly build; branch closure occurs only after success.

## Filament recovery

Run `tools/audit/filament_recovery_test.py` against LVGL 9.5 and
`tools/audit/filament_recovery_macros_test.py` with Jinja2. Then verify parking,
load/purge, sensor-triggered pause, cooled pause and cancellation on a small
test print. See [Filament recovery](FILAMENT_RECOVERY.md).

## Responsive temperature/fan dialogs

Run `tools/audit/control_popups_layout_test.py` against LVGL 9.5. On the panel,
check nozzle, bed and fan presets; tap a temperature target to open the numeric
editor; check Back, Set, Off, range validation and keyboard Ready/Cancel. Repeat
across all four themes, densities and large text. Scroll any overflowing body
and confirm the footer remains visible. On multi-hotend printers, verify the
command addresses the selected heater.

## Responsive Motion dialogs

Run `tools/audit/motion_layout_test.py` and `tools/audit/tools_live_refresh_test.py`
against LVGL 9.5. On the panel check Tools → Calibration → Motion: Limits,
Drivers and Distance. Try both Belt/pulley and Leadscrew, edit both fields,
check Done/Cancel and calculate. Repeat across themes, densities and large text;
confirm labels wrap without overlapping and footer actions stay visible while
scrolling. Verify driver discovery, selection and offline/profile transitions
when the printer exposes TMC diagnostics.

## Responsive sensor/status dialogs

Run `tools/audit/sensor_status_layout_test.py` against LVGL 9.5. Check Printer
filament sensor controls (outside a pause) and Printer Status across every
theme/density/large-text setting. Verify long names/paths wrap, sensor lists and
long details scroll, and Close remains visible. Toggle only when appropriate
for the printer and confirm live acknowledgement or the five-second retry.
Sensor rows remain bound by name; removed sensors/profile changes cannot send
a stale toggle. Reopen to display newly discovered sensors. Printer Status
continues to show an opening-time snapshot.

## Responsive hotend dialogs

Run `tools/audit/hotend_layout_test.py` against LVGL 9.5. On the panel open
hotend controls in Classic, Operator, Glass and Operator Shell; repeat with
large text and each density. Verify long names and temperatures wrap, the list
scrolls and Close remains visible. Select a hotend temperature and check presets,
Off and custom entry address that heater. During printing and pause, verify
hotend temperature, bed temperature and fan speed are still editable. On a
multi-hotend printer, check the active indicator and Back/Activate confirmation
while idle; activation remains disabled during printing/pause. Reopen after
hotend discovery changes. Disconnect or switch printers and confirm stale rows
cannot send commands. Check closing/reopening both dialogs.

## Responsive cancel-object dialogs

Run `tools/audit/object_layout_test.py` against LVGL 9.5. On a multi-object test
print, open Cancel Object in each theme/density/large-text setting. Check map
selection, Current/Excluded markers, long wrapping names, object-list scrolling
and visible Close/Exclude actions. Select an object and verify the confirmation
shows its full name. Back must leave the print unchanged; Exclude must cancel
only that object while the others continue. Check close/reopen, unavailable/all
excluded states and changing printers with an open dialog. Recheck temperature,
bed and fan adjustments during printing and pause.

## Responsive whole-print cancellation

Run `tools/audit/cancel_layout_test.py` against LVGL 9.5. Open the whole-print
Cancel confirmation in each theme/density/large-text setting. Check the warning
and visible Back/Cancel actions. Back must leave the active print unchanged;
Cancel must invoke the existing whole-job cancellation on a disposable test
print. Check close/reopen and preserve single-object exclusion behavior.

## Control-dialog nightly closeout

The operator accepted the incremental layout work. Printing-dependent hardware
checks remain pending: sensor-triggered runout, parking/loading/purging/resume,
cooled pause, active-print/paused temperature and fan edits, object exclusion and
whole-print cancellation. This nightly does not claim a completed printing test
or soak. See [Nightly validation](NIGHTLY_VALIDATION.md).

## Responsive Time Zone and Reset Settings

Run `tools/audit/settings_dialog_layout_test.py` against LVGL 9.5. On the panel,
check Settings Time Zone in every theme/density/large-text setting: long names
wrap, rows scroll, Close remains visible and the selected preset is indicated.
Select the intended local zone and verify the Settings label/clock update.
Reopen and close the dialog. Open Reset Settings, inspect the complete warning
and fixed actions, then use Cancel. The host fixture verifies Erase with mocked
NVS/reboot calls; panel layout validation does not require erasing settings.
Printing-dependent validation from the preceding nightly remains pending.

## Large-text button fit

Run `python3 tools/audit/button_text_fit_test.py` with LVGL sources available
(or `--lvgl-dir` and optional `--lvgl-lib`). After the target build, check Settings
and operator navigation in all themes with large text, including compact density.
Check Camera normal/fullscreen, Console TEMPS ON/OFF, Devices pagination,
Calibration bed/probe actions, Toolhead Z offset controls, Bed Mesh controls,
printer management, setup discovery, configuration backup and theme/OTA footers.
Full names must remain readable, buttons must not overlap, and Close/Back must
remain reachable. Inspect/cancel destructive dialogs during layout validation.
Earlier printing tests remain pending; host checks do not replace panel testing.

## Single-line control labels

Rerun `tools/audit/button_text_fit_test.py`. With large text in every stock theme
and density, verify Dashboard and the other page names remain on one line with
visible icons; Printer Motion/Home/Pause/Resume/Object/Cancel stay on one line;
Calibration Accuracy/Custom/Probe-Z, Bed Mesh Grid ON/OFF and Toolhead Home and
numeric controls are readable without broken words or units. Longer backup and
recovery phrases may wrap between words. Check adjacent buttons stay separate.

### Remaining atomic text and navigation inset

Navigation buttons retain single-line names and full-size text with an 8 px left inset and 156 px width. Long selected printer/camera names scroll horizontally; camera names no longer use a 16-byte prefix. E-STOP is 140 px wide. Console and calibration banner status labels have more horizontal space. Compact telemetry headings and readings select the largest enabled font that fits, down to 14 px, retaining decimal values and units; temperature slash spacing is compact. Calibration card status uses the same fit rule.

Host button/layout checks cover all four stock themes, three densities, both accessibility text settings, inset navigation, live Printer telemetry, compact card widths, E-STOP and connection statuses. Build/flash and panel validation remain pending. Inspect long configured names, first/last navigation items, telemetry during heating, and calibration discovery transitions.

### Motion distance calculator large text

Distance input labels and fields share two grid rows so both numbers stay aligned when a label wraps. One-line inputs have a 56 px minimum height and centered body-large text. Leadscrew labels use “THREAD PITCH (mm)” and “THREAD STARTS.” Config reference uses compact axis names and one missing-data message when no rotation distances are reported. Motion host checks cover field height/alignment in both modes at all stock themes, densities, text sizes and 1024/640/480 widths; hardware validation remains pending.

Devices catalog filters use intrinsic single-line widths in a horizontally scrollable row, including changing counts. Device cards use separate name, object identifier and kind/value rows, with scrolling for long names and complete live-value fitting. The dedicated Devices text/layout host test covers 96-object counts, long names, active-hotend readings, card bounds, empty states and cleanup in all stock themes/densities/text settings. Panel validation remains pending.

### Calibration Motion card actions

The Motion card is 190 px tall with two 44 px action rows. The diagnostic row uses 100/132/110 px widths; the workflow row uses 100/152/102 px widths. Compact horizontal padding preserves full-size single-line text and gaps. The first row starts at y=86 and the lower row stays at the card bottom. The summary is capped at 24 px. Motion host checks exercise the production Limits/Distance/Drivers construction and measure all six action names across stock themes, densities and text sizes. Build/flash validation remains pending.

## Responsive theme dialogs

Run `tools/audit/theme_dialog_layout_test.py` against LVGL 9.5. Check the built-in
chooser across every theme/density/large-text setting: previews wrap, all four
choices are reachable by scrolling and footer actions stay visible. Tap each
built-in preview, including Operator Shell, and verify one stable transition.
Open Custom Themes, inspect names/authors/descriptions and palette previews;
select a row and use Apply. Check Close and Keep in the removal confirmation.
Remove only a disposable SD-card theme if testing removal; built-in themes
remain protected. Verify active custom-theme removal falls back and rebuilds
once. Recheck Time Zone and inspect/cancel Reset Settings. Earlier printing
validation remains pending.

### Printer chooser large text

Chooser cards keep explicit zero padding for their absolute geometry. ACTIVE is on the bottom text row beside the open hint, separate from the printer name. Names and endpoints use single-line horizontal scrolling; connection states fit complete text. Preview placeholder text uses a 108 px width and two caption lines with clipping, preserving its icon and avoiding repeated writes caused by ellipsis-modified text. The subtitle/card grid have more vertical separation. The chooser refresh audit covers four themes, all densities and text sizes, card/label bounds, long names/addresses/files, active/offline states and unchanged 500 ms refreshes without redraw for fitting labels. Long scrolling names intentionally animate. Target build and panel validation remain pending; previous startup flash fixes and printing tests are retained.

### Printer profile manager and main editor

The profile manager, primary add/edit form and remove confirmation use native column layouts with scrolling content and pinned footer actions. Rows separate the profile name from its endpoint and keep both on a single scrolling line. Name, host and port fields use the large body font with a 56 px minimum touch height. Popups cap their size to the current viewport, and action widths follow their full label width; footers wrap on smaller displays. External manager/editor deletion cleans up owned dialogs and test timers.

Run `python3 tools/audit/profiles_layout_test.py` (optionally `--lvgl-dir /path/to/lvgl`). Host checks cover all four themes, three densities, both text sizes, 1024/640/480 px widths and a 400 px height; pinned action bounds, scroll reach, field sizes, selection, discovered endpoints, validation/failure/cancel/save, last-profile removal guards and teardown. Network and configuration storage use typed test stubs. Target build, touch/keyboard validation and earlier printing tests remain pending. Nested authentication, camera, certificate and keyboard layouts remain a separate follow-up.

## Settings and large-text source closeout

See [Settings validation](SETTINGS_LAYOUT_VALIDATION.md) for the accepted layout scope, host checks and pending hardware tests. No nightly or new stable release is created.

### Nested printer setup dialogs

Authentication, connection security, the SD-card certificate picker, camera setup and camera removal now use scrolling native forms with pinned actions. Studio retains its full-width popup policy; the same helpers serve the other four themes. Camera slots retain their numbered routing and use full stored names on a single scrolling line. Tapping a name, host, port, API key or camera URL opens one shared value editor; Done applies the value and Cancel discards it. Port entry uses a numeric keyboard and digit/length restrictions. API-key entry remains password-masked and each editor inherits its field's maximum length. Closing a field owner discards its open keyboard and clears camera/test timers and setup state. Security has a separate Cancel action that preserves the selected mode. Existing camera catalog, certificate import and profile save behavior remains authoritative.

`profiles_layout_test.py` covers five themes, three densities, both text sizes, three widths and a 400 px short viewport. The test uses typed network/storage stubs and a fake directory for six PEM choices and a missing SD-card directory, without opening network connections or writing certificates. It checks footer bounds and scroll reach, keyboard Done/Cancel/external deletion, numeric/password/length limits, API-key staging, security cancellation/import failure/success, camera slot/default/removal routing, and owner teardown. Build/OTA/touch validation and printing-dependent tests remain pending. No commit, push or nightly is part of this layout patch. Stable version remains 6.5.6.

## Network and ready-to-print follow-up checks

Run `python3 tools/audit/responsive_layout_test.py` with optional `--lvgl-dir` / `--lvgl-lib` host paths. Real LVGL checks cover five themes, three densities, both text sizes, Files modal widths of 760/640/480 px, inspector/details bounds, readiness transitions, action routing and teardown. Network checks use each theme at its native page width; Studio checks cover the full 976 × 424 stage, status/action bounds, six scrollable scan results, SSID selection, empty results and cleanup. Wi-Fi transport is stubbed. Target build, OTA and touch/scroll verification remain pending; earlier printing tests remain pending.

## Setup Center checks

Run `python3 tools/audit/setup_center_layout_test.py`, with optional `--lvgl-dir` and `--lvgl-lib` host paths. Real LVGL checks cover five themes, three densities, both text sizes and 1024/640/480 px displays. Fixtures verify navigation, pinned-action bounds, repeated open/scan, scan selection, password masking/length, Wi-Fi connect/verify, shared profile routing, camera discovery/test/save, missing prerequisites, completion storage failure/success, close/reopen and external deletion. Wi-Fi transport, camera transport and persistence use typed stubs. Target build, OTA and panel checks remain pending; prior printing tests remain pending. Stable version remains 6.5.6.

## Empty-camera regression

`setup_center_layout_test.py` now checks an empty discovery result, Skip camera navigation to Review, and successful completion with Wi-Fi/printer configured and no camera. Existing all-theme/density/text/width checks cover the two-button camera footer. Panel validation remains pending.

## Files Details label regression

The real-LVGL responsive fixture now checks the Files Details Close label text, positive width smaller than its button, and containment after flex layout. It reproduces the old temporary-width failure and passes after the Studio action sizing fix. The fixture also scrolls the full metadata to its final confirmation line, checks that Size/Thumbnail remain in the text, and verifies preview geometry and action positions remain unchanged. Run `tools/audit/responsive_layout_test.py` with the documented host options. Panel validation remains pending.

## Files confirmation and preview-refresh regression

`responsive_layout_test.py` checks all themes/densities/text sizes, confirmation widths, no start before acceptance, cancel, single acceptance, printer-generation changes and detail teardown. Studio refresh keeps the page and selected inspector identities. `files_load_worker_test.py` checks publication while an embedded inspector is open, deferral for a modal, and existing stale-result/worker/cleanup guards. Printer transport is stubbed in host checks. Target build, OTA, confirmation touch checks, visible HTTP failure feedback and physical printing remain pending.

## Emergency-stop regression

Run `python3 tools/audit/estop_transport_test.py` for the actual extracted WebSocket/HTTP adapters: dedicated stop/recovery methods, console token boundaries, ordinary scripts, TLS/API-key/port preservation, busy-slot emergency delivery, and transport/error cleanup. `studio_layout_test.py` exercises pointer hit-testing for E-stop in all five themes and recovery failure/success. Services are stubbed; these tests do not verify physical shutdown. Build/OTA and cold-idle panel checks remain pending: stop the selected printer, verify its Klipper shutdown, and explicitly restart it. Previous physical printing tests remain pending.

## Recovery dialog layout follow-up

`studio_layout_test.py` checks actual E-stop touches in all five themes, stop/restart failures, recovery actions, all densities/text sizes and 1024/640/480 widths with 400 px short viewports. It checks pinned footer labels, message scroll reach, long target names, target-change dismissal and external-deletion/reopen cleanup. `responsive_layout_test.py` measures the File details Close button width, right alignment and label fit. Services remain stubbed. Target build/OTA, panel dialog/touch checks and physical printing tests remain pending. Apply this patch after the immediate E-stop routing patch. Stable remains 6.5.6.

## Devices header reachability regression

`devices_text_layout_test.py` now links the real Devices page as well as the catalog. The fixture reproduces the old Studio off-screen action coordinates, then checks both buttons inside the header/page and real LVGL pointer taps routing to telemetry/endstop callbacks across all five themes, three densities and both text sizes. Header label fit is checked in Studio; page reuse/cleanup and the existing 96-device catalog checks remain. Network and endstop dialog services are stubbed. Build/OTA and panel taps through Tools > Devices > Graphs/Endstops remain pending. Stable version is 6.5.6.

## Setup, Files and Devices source checkpoint

Host LVGL checks cover profile/setup dialogs, responsive pages, compact Details Close, Files confirmation/refresh, recovery actions, and real Devices header touch routing across themes/text sizes. Actual command-adapter fixtures verify dedicated emergency/recovery endpoints and transport error handling. Version/architecture/documentation checks pass. Firmware build/OTA, physical emergency shutdown and printing-dependent recovery/control tests remain pending. A GitHub source checkpoint does not mark these physical checks complete. Telemetry redesign is deferred until after this source checkpoint.

## Modern telemetry regression

`telemetry_modern_layout_test.py` covers the actual page/charts/history across all five themes, densities, text settings and three viewport sizes, with real pointer input, owner bounds/scroll/teardown, named hotend identity, all time windows, targets/detail, graph hold with live readouts, missing-capability N/A, invalid-value gaps, offline time bins, clock/printer resets, visible extrema expiry and no-op refresh. It separately compiles the four telemetry C units with warnings as errors. `telemetry_refresh_test.py` and chooser refresh checks pass. Rendered Studio and Operator views were inspected. The general button-fit audit retains a pre-existing Macros large-text action overflow at ui_macros.c:541, reproduced with the old text helper; that issue remains pending. Target build/OTA and panel checks are pending, as are prior physical printing/recovery tests. See [Telemetry](TELEMETRY.md).

## Native Macros dialogs and action validation

Macros search, parameter editing and command review now use native popup columns with pinned Cancel/Done, Cancel/Review and Cancel/Run actions. The body scrolls independently; parameter fields use a single full-width column, 56 px touch targets and body fonts. Long macro headings remain one line with an ellipsis. Search and Clear Search remain single-line actions. No command is sent before the existing review and Run steps, and printer-generation/readiness guards remain in place.

`responsive_layout_test.py` checks real Macros actions and dialogs in all five themes, normal/large text, three densities and narrowed widths. It exercises search, parameter edits, scrolling with stationary footers, edited command review, rejected stale-printer Run and one successful Run. The source-derived button audit now delegates the two adaptive Macros search actions to that real-owner test instead of reconstructing Studio geometry without its final font fitting. Both host suites pass. Target build and panel verification remain pending; physical printing/recovery tests remain pending. Stable remains 6.5.6 and the current feature branch stays open.

## Motion and calibration native dialogs

The calibration workflows now use shared native columns with scrolling content and pinned actions, plus compact standalone Close controls. Motion measurement input remains touch-sized and runtime limit readings stay on one line. Probe Accuracy displays live homing/readiness and gates Run; PID warning Back preserves the target. Custom macro discovery fixes the existing count-pointer increment. Dialog command choices reject stale printer generations. See [Motion and calibration](MOTION_CALIBRATION.md) for scope, command behavior and host/panel validation. All five themes and normal/large text are covered by the dedicated host suites; target build, panel calibration and existing printing tests remain pending. Stable remains 6.5.6; the feature branch remains open.

## Console and pressed button frames

Console now uses native wrapped history, responsive filters, pinned page actions and command/search columns with the input and actions above the keyboard. Printer-generation/connection guards and persistent inline send feedback preserve rejected input. The appliance-wide Notifications setting defaults optional confirmations OFF and keeps important notices persistent; it is saved in NVS and backups. Shared button presses retain their geometry and semantic stroke; keyboard-only focus remains visible. See [Console](CONSOLE.md) and [Notification policy](NOTIFICATIONS.md) for behavior, audit findings and remaining notification migration. Dedicated host, source-derived frame/text and current owner checks pass; target/panel and physical printing tests remain pending. Stable remains 6.5.6 and the feature branch stays open. Drybox retains its prior layout.
