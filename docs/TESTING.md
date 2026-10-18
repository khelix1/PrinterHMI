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


## 7.0.0-dev calibration follow-up validation (2026-10-11)

The operator reports successful calibration Save/Restart and reconnect after the
save-review patch. No additional calibration-accuracy or physical printing tests
are inferred from that report.

The calibration owner host suite now checks manual Probe/Z and Axis Twist dialog
retirement, callback/data preservation, repeated opens, late status updates,
reentrant dispatch and printer changes in all five themes, densities, text sizes
and standard/narrow/short viewports. Run
`python3 tools/audit/calibration_dialog_layout_test.py`, with optional
`--lvgl-dir` and `--lvgl-lib` host paths. Services remain stubbed; actual TESTZ
motion, ACCEPT/ABORT and Axis Twist points need physical validation. Earlier
version-specific sections remain historical records. Current branch identity is
7.0.0-dev; intermittent OTA and pending printing/recovery checks remain open.


## About layout and identity checks

Run `python3 tools/audit/about_dialog_layout_test.py`, optionally with
`--lvgl-dir` and `--lvgl-lib` host paths. The real owner is tested under compiled
Custom/Stable/Nightly channel definitions, with actual firmware identity code and
native pointer input. All five themes, densities/text sizes and 1024/640/480 px
viewports are covered, including long nightly versions, descriptor fallback,
scroll reach, pinned Close, repeated open/delete cycles and child-bubbled size
events. Host rendering was inspected across the five themes. Panel scrolling and
About launch/close remain pending. The current branch is 7.0.0-dev; existing
physical printing/recovery and intermittent OTA checks remain open.


## Geometry and motion retry/lifetime checks

`python3 tools/audit/calibration_dialog_layout_test.py` now exercises the real
geometry and motion owners with stubbed services across all five themes, densities,
text sizes and standard/narrow/short viewports. It checks retained retry choices,
exact command strings, fresh capability/readiness/homing checks, duplicate and
reentrant submission, external deletion and transport callbacks that retire the
dialog or switch printers. Accepted gantry requests expose Close and cannot resend.
The suite compiles separate owners with warnings treated as errors.

On 2026-10-11 the user confirmed the About popup looks good on the panel, superseding
the pending visual review in the preceding About section. This does not validate
physical calibration: Screws Tilt, QGL/Z Tilt, shaper calibration, resonance and
accelerometer measurement still require printer checks. Confirm rejected requests
retain their choices and accepted requests show actual results without duplicate
commands. Branch identity remains 7.0.0-dev. Previously documented physical
printing/recovery and intermittent pre-reboot OTA tests remain pending.


## Bed Mesh profile manager, editor and review

Run `python3 tools/audit/bed_mesh_profiles_layout_test.py`, optionally with
`--lvgl-dir` and `--lvgl-lib`. The actual owner and shared LVGL form/button/theme
code are compiled with warnings treated as errors. Five themes, three densities,
both text sizes and standard/narrow/short viewports cover pinned actions/keyboard,
single-line row bounds, long names, empty lists, native pointer action selection,
invalid-name feedback, unchanged repeated opens, exact scripts, rejected retry,
offline recovery and printing/paused blocks. Owned copies preserve displayed
selection through reordered snapshots. Removed profiles, absent meshes, external
delete/reopen cycles, parent deletion and printer changes cannot issue stale
commands. Services are stubbed. Host renders of the three dialogs were inspected
across all five themes.

`python3 tools/audit/tools_action_safety_test.py` retains calibration-session and
actual Bed Mesh command-bridge coverage; profile review/retry coverage now uses
the real-owner suite instead of an extracted function with synthetic UI state.
Panel checks should cover list scrolling, Save As input, Back/Cancel/Close and
confirmation visibility with large text. Physical mesh save/load/remove and
SAVE_CONFIG restart are pending. Checkpoint 635fdcf0e8d8 was confirmed pushed by
the user; this follow-up stays on feature/profile-setup-dialogs at 7.0.0-dev.
Previously documented printing/recovery and intermittent OTA checks remain open.


## Motion Diagnostics repeat-open and retirement checks

Run `python3 tools/audit/motion_layout_test.py`, optionally with `--lvgl-dir` and
`--lvgl-lib`. The real owner, theme/layout helpers and calculation code are covered
across five themes, three densities, both text sizes and standard/narrow/short
viewports. Checks include retained calculator/editor state on repeated opens,
invalid-number retry, native keyboard Ready, stationary actions/keyboard, driver
selection through reordered readings, offline/shutdown recovery, mismatched
snapshot owners and no reads after a printer switch. Repeated external deletion
of all three views balances timer counts; late refresh/mode/calculation callbacks
are harmless. A standalone owner compilation treats warnings as errors.

The user accepted Bed Mesh profile visuals on 2026-10-11; physical profile
operations remain pending. After this Motion patch, check repeated Distance and
field opening, keyboard Done/Cancel, Limits/Drivers reconnect and printer switching
on the panel. Geometry calculations must still send no motion/configuration
commands. Current identity remains 7.0.0-dev on feature/profile-setup-dialogs.
Earlier physical calibration/printing/recovery and intermittent OTA tests remain
open.


## Macros dialog lifetimes and retry

`python3 tools/audit/responsive_layout_test.py` now extends the real Macros owner
with rejected-request retry, repeated reviewed callbacks, duplicate/reentrant
Run, independent search/parameter/confirmation deletion, parent-page deletion
during transport, printer changes during dispatch and stale editor Done. Reopen
cycles balance refresh timer counts. Existing parameter command validation,
keyboard Ready/Cancel, footer scroll stability and theme/width/custom-token checks
remain covered. Services are stubbed; physical macro execution is not tested.

The user reported Motion Diagnostics looks good on 2026-10-11. For this Macros
follow-up, verify search typing/cancel, parameter edits, Review/Run, reconnect
retry and page/printer switching on the panel. Existing physical calibration,
printing/recovery and intermittent OTA tests remain pending. Current development
identity stays 7.0.0-dev on feature/profile-setup-dialogs.


## Console callback and external page retirement

`python3 tools/audit/console_modern_layout_test.py` covers reentrant Send, editor or
page deletion during transport, printer changes after dispatch, late callbacks
after deletion, retained repeated-open drafts, external search/command deletion
and timer-balanced page reopen cycles. Existing filters, history, scroll anchoring,
no-op refresh, rejection retention and button-state frame checks remain covered
across five themes, three densities, text/contrast/motion settings and
standard/narrow/short viewports. Actual UI owners compile with warnings as errors.

`python3 tools/audit/console_transport_test.py` passed unchanged literal command
routing, WS/HTTP fallback and inline-owned failures. Recovery commands remain
sendable through a connected Moonraker even when Klipper is in error or live data
is unavailable. These are host tests with stubbed transport. Check Console
search/history/Send/Close, reconnect retry and page/printer switching on the panel.
The user accepted Macros visuals on 2026-10-11; physical execution/recovery and
intermittent OTA checks remain pending. Current identity remains 7.0.0-dev.


## Camera native selection and view dialog checks

`python3 tools/audit/camera_pipeline_test.py` now includes real Camera View and
picker layouts under five themes, three densities, both text sizes and
1024/640/480-pixel displays, including the short 400-pixel display. It checks
stationary actions, scroll reach, touch-sized controls and label bounds, current
rotation/mirror markers, repeated opens, empty catalog, failed save retry, exact
selected defaults, external deletion, reentrant writes and changed printer/URL
ownership. Existing frame/free/cache, transform, fullscreen, invalid-frame and
Dashboard retirement checks remain active. Transport and storage are stubbed;
physical persistence and streaming are not established by these tests.

The user accepted Console visuals on 2026-10-11. For this Camera patch, check
Select Camera, current/default markers, View rotation/mirrors, Reset/Close and
printer switching on the panel. Real saved settings after reboot and reconnect
remain pending, along with earlier physical calibration/printing/recovery and
intermittent OTA checks. Development identity remains 7.0.0-dev.


## Camera root, fullscreen and setup lifecycle checks

`python3 tools/audit/camera_pipeline_test.py` adds actual page creation and
timer-balanced repeated fullscreen root deletion in all five themes, three
densities and both text sizes. It checks orphan-card/dialog retirement, frame/cache
detachment before free, late callbacks after hide/delete, independent image/card
deletion and rebuild, setup suspension while reopening, visible-only resume,
hidden-page resume suppression and cached-page printer changes. Existing Camera
dialog viewport tests and rotation/mirror/frame validation remain active.

The user accepted native Camera dialog visuals on 2026-10-11. This lifecycle
follow-up still needs panel fullscreen exit, navigation/theme/printer switching
and Configure pause/return checks. Streaming/storage are host fixtures; real
connection and persisted settings are pending, as are earlier physical
calibration/printing/recovery and intermittent OTA checks. Current development
identity remains 7.0.0-dev.


## Files external deletion and print dispatch checks

`python3 tools/audit/responsive_layout_test.py` extends the real Files owner with
reentrant Start print, detail deletion during dispatch, independent metadata
deletion followed by updates, page deletion with nested dialogs, late callback
guards and object/timer-balanced reopen cycles. Existing file/preview bounds,
metadata scroll reach, pinned actions, printer/offline/loading gates, rejection
retry and search keyboard checks remain covered under all five themes and the
existing density/text/width/custom-profile combinations. Services are fixtures;
these checks do not start a physical print.

`python3 tools/audit/files_load_worker_test.py` passed the actual worker routing
checks for work outside LVGL, coalesced latest requests, profile/page/serial fencing,
deferred detail publication and allocation/task-failure cleanup. After flashing,
verify file selection, preview/details, Print review/Cancel, refresh and printer
switching. Physical print-start validation remains pending. The user accepted the
Camera lifecycle follow-up on 2026-10-11; earlier real streaming/persistence,
physical calibration/printing/recovery and intermittent OTA checks remain open.
Current identity remains 7.0.0-dev.


## Real metadata pointer-drag regression

The Files variant of `tools/audit/responsive_layout_test.py` now injects actual
pointer presses/moves/releases into metadata scroll bodies and asserts a scroll
position change. It exercises the compact preview metadata and full Studio
Details popup, with a forced forty-line payload, final-field reach and stationary
Close footer. Previous source fails the gesture assertion; the correction passes
the existing theme/density/text/width/custom-profile combinations. The older
programmatic checks remain useful for layout but did not establish touch routing.

The user reported non-scrolling details and an apparent freeze on 2026-10-11.
After flashing, drag over the details text in both surfaces, reach the end, scroll
back and use Close/Cancel. Real panel verification of the reported freeze is still
pending; the host regression identifies a missed pointer hit, not a proven device
panic. Check that Print status names the actual selected-printer blocker and that
ready printers do not show Offline. Earlier physical and OTA checks remain open.


Files Up clarification: Up navigates to a parent folder; the root had silently
ignored it, and library-wide search could leave the same results visible after
folder changes. Up is now disabled at the G-code root when no search is active.
During search, the first Up clears the query and returns to the current folder;
subsequent Up navigates to its parent. UI checks verify root/child/search enabled
states and dispatch, and actual controller tests cover nested-folder navigation
and leaving search without changing its remembered folder. Panel verification
remains pending. This is part of the metadata touch-scroll correction.


## Quiet-link status-query cadence and offline guards

`python3 tools/audit/offline_navigation_test.py` now exercises the real extracted
WebSocket quiet-status scheduler: no queries with frequent telemetry, a read-only
webhooks/print_stats query after 1.5 seconds, bounded retries, no freshness advance
on send, failure aging, subscription/discovery/bootstrap guards and selected-owner
fencing. Existing nonblocking camera retirement and rapid WebSocket profile
selection tests pass. `python3 tools/audit/klipper_readiness_test.py` still passes
bootstrap, shutdown/error/disconnected/startup latching and explicit recovery.
The transport fixture does not prove actual server responses or network timing.

The user reports an Offline caption still present in Files on 2026-10-11. After
flashing, leave the selected online printer quiet, open file details and check
that real status replies retain correct readiness. Also remove printer/network
power and switch printers: unanswered queries must not keep an offline printer
ready. Metadata touch/freeze and physical/OTA checks remain pending. Current
identity stays 7.0.0-dev on feature/profile-setup-dialogs.


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


### Devices page retirement and reading validity

Run `python3 tools/audit/devices_text_layout_test.py` with managed LVGL sources
(or supply `--lvgl-dir`). The actual Devices/catalog/live-value code checks
native-size layout in every theme/density/text-size combination, pointer routing,
external page deletion/reopen and balanced timers, independent live-label deletion,
printer generation fencing, offline/shutdown value suppression and printing reads.

After flashing, open Devices and Graphs/Endstops, navigate away and return, switch
printers and themes, and disconnect/reconnect the selected printer. Verify old
readings become UNAVAILABLE and current readings return without a crash. Panel
verification is pending. The false-offline cause, previously reported Files
scrolling freeze and physical/OTA checks remain open where documented; the Files
inline readiness caption remains hidden by request.


### Temperature/fan send-callback lifetime

Run `python3 tools/audit/control_popups_layout_test.py` (with `--lvgl-dir` if
needed). The actual popup code tests preset/custom/zero/range/keyboard commands,
retained retry input, callback deletion of parent/editor/feedback, printer changes,
nested sends and reopen attempts, repeated external teardown and both checked and
legacy sender readiness. Layout checks cover five themes/densities/text sizes and
1024/640/480 display widths, with a 400px-high short viewport at 480px.

After flashing, check nozzle/bed presets and Custom temperature, fan presets,
Back/keyboard cancel, and reopening a control after a printer change. Physical
adjustment during a print remains pending. Existing OTA, false-offline and Files
scrolling-freeze verification remain open where documented.


### Reviewed motion/hotend patch and responsive Toolhead layout

`tools/audit/toolhead_dialog_safety_test.py`, `hotend_layout_test.py` and
`control_popups_layout_test.py` passed against the exact user-supplied motion/
hotend safety patch. The Toolhead audit additionally checks native panel bounds,
centered single-line button labels, pointer dragging of the content, pinned Close,
final-control reachability and exact jog/extrude/runtime-offset commands across
all five themes/densities/text sizes and 1024x600, 640x600 and 480x400 viewports.

Run `python3 tools/audit/toolhead_dialog_safety_test.py` with managed LVGL sources
(or `--lvgl-dir`). After flashing, check Toolhead with normal and large text in all
themes: Close stays visible, scrolling reaches Filament and runtime offset, step
selection is clear and reopening does not duplicate the popup. Verify physical
motion, extrusion, offsets and hotend activation separately when appropriate.
These physical checks, intermittent OTA and the documented false-offline/Files
freeze verification remain pending. Files' inline readiness caption stays hidden.

### Expanded Toolhead Z and extrusion steps

The real LVGL toolhead_dialog_safety audit passes all five themes, three densities, normal/large text and 1024/640/480 widths. It checks exact commands for all eight Z lengths in both directions, all ten signed runtime offsets, Reset, and both extrusion directions at all five lengths, plus button bounds, real pointer scrolling and pinned Close. Firmware build and physical movement/extrusion checks remain pending.

### Toolhead tab fit and touch checks

The actual LVGL audit verifies zero overflow and whole-panel visibility in all three tabs at 1024x600 across all five themes, three densities and both text sizes. Real pointer taps verify tab switching without sending commands; selections and timers survive tab switching/reopening. Smaller views retain working pointer scrolling. Existing exact-command checks for every Z length, signed runtime offset and extrusion length still pass, alongside ownership, duplicate-send, retry and deletion checks. Fifteen native renders were inspected across all themes with large text. ESP-IDF build and physical panel/movement checks remain pending.

### Filament Recovery lifecycle and fit

Toolhead tabs passed the user panel check. The actual LVGL filament_recovery_test passes all five themes, three densities and normal/large text; layout fixtures additionally cover 1024x600, 640x600 and 480x400. Tests cover exact Heat/Unload routing and Resume interception, temperature bounds, missing macros, cold nozzle, homing, unknown/empty/truncated sensors, offline/printer ownership, failure/retry, duplicate/reentrant callbacks, close/reopen during dispatch, independent field deletion and timer retirement. Native-panel checks verify no scroll overflow, button bounds and text fit, fixed Close and actual pointer temperature/Close taps. Five large-text renders were inspected. Macro files are unchanged; physical runout/load/purge/resume validation and target build remain pending. The separate macro simulation could not run here because Jinja2 is unavailable; no macro-simulation pass is claimed. Earlier false-offline/Files freeze and intermittent OTA checks remain open.

### Whole-print Cancel readiness and lifecycle

The actual LVGL Cancel audit now includes Studio and passes five themes, three densities, both text sizes and 1024/640/480 widths including a 240-pixel-high viewport. Checks cover pinned actions, Back without dispatch, exact CANCEL_PRINT, rejected send/retry, offline/waiting/error/inactive states, printer switching, repeat-open sender ownership, duplicate/reentrant callbacks, deletion/reopening mid-send and independent status deletion. The existing temperature/fan audit is run to check the shared popup source. Filament Recovery was reported good on the panel; physical recovery and cancellation checks, target build, false-offline/Files freeze and intermittent OTA verification remain pending.

### Cancel Object lifecycle and retry checks

The actual LVGL object audit now includes Studio and passes all five themes, three densities, both text sizes and 1024/640/480 widths. Fixtures cover 48 long names, map projection, pinned actions, reviewed-name dispatch after list selection/live reordering, failure/retry retaining the displayed snapshot, duplicate/reentrant callbacks, deletion/reopening during dispatch, sender capture, printer/readiness gates, removed/already-excluded names and independent status deletion. The whole-print Cancel audit also passes with the shared source. User panel confirmation for whole-print Cancel is recorded; actual printing/cancellation, target build, false-offline/Files freeze and intermittent OTA verification remain pending.

### Filament sensor safety follow-up (7.0.0-dev)

Run `python3 tools/audit/filament_sensor_safety_test.py` after dependencies are available. The real LVGL host audit covers rejected sends and retry, duplicate/reentrant clicks, name reordering, telemetry acknowledgement, elapsed timeout, accepted-request close/reopen suppression, printer switching during dispatch, missing/offline/unsafe sensors, isolated legacy sender, external deletion, timer cleanup and all five themes/densities/text sizes. `sensor_status_layout_test.py` also covers narrow layouts, quiet refresh and pinned Close.

Panel verification pending: check Enable/Disable for the intended sensor, accepted state updates, Close/reopen, printer switching, and failure feedback when the connection fails. Changes to a filament sensor affect runout detection; verify the final sensor state in the printer host. Physical runout testing remains separate.

### Direct Printer action safety follow-up (7.0.0-dev)

Run `python3 tools/audit/printer_direct_action_safety_test.py` with LVGL dependencies available. It exercises real themed action buttons and the controller across all five themes, densities and text sizes: rejected Home/Pause sends and retries, accepted-command cooldown across page recreation, Pause acknowledgement, duplicate/reentrant events, retired or mismatched targets, printer changes, callback close/reopen/switch, fenced notifications and the existing Resume/Object/Stop/Toolhead paths. The connection and filament recovery audits bind their real action targets; the teardown audit provides the Toolhead close stub used by the page. Older Bed Mesh audit fixtures now supply valid mesh data and use the current feedback/name-buffer API.

Panel verification pending: Home once while idle, confirm normal homing; Pause during a suitable print, then Resume through recovery; rapid repeat taps, leave/reopen Printer, switch printers and retry after a failed command. The five-second guard suppresses immediate duplicates; it is not a homing-completion signal. Follow-ups remain on feature/profile-setup-dialogs, version 7.0.0-dev, with no release merge or visual-polish work in this patch.

### Speed/Flow tuning safety follow-up (7.0.0-dev)

Run `python3 tools/audit/printer_tuning_safety_test.py` with LVGL dependencies available. It covers real sliders across all five themes/densities/text sizes: rejected selected values and retry, accepted identical-request suppression across recreation, independent speed/flow commands, telemetry acknowledgement, elapsed timeout restoring live values, printer ownership, offline and mismatched targets, callback close/reopen/switch, individually deleted fields, teardown during failure/timeout notifications, expired requests while opening controls and overlapping old/new pages. Studio page audit fixtures now locate the current tabbed Toolhead controls by label, acknowledge their tuning telemetry and supply the current Bed Mesh profile availability stub.

Panel verification pending: change Speed and Flow once and confirm values in the host; reopen Tune or leave/reopen Printer, repeat the same release, switch printers and check reported values, then check unavailable/retry behavior. Resume, cancellation and filament controls remain separate. No changes to tuning range (50-150 percent), theme geometry or release version are part of this patch.

### Diagnostics safety batch (7.0.0-dev)

Run `python3 tools/audit/diagnostics_batch_safety_test.py` with LVGL dependencies available, plus `calibration_dialog_layout_test.py` and `tools_action_safety_test.py`. The new real-LVGL variants cover Probe Accuracy rejected sample selection/retry, accepted reopen suppression, reentrant readiness/send callbacks, printer/session replacement and deleted fields; Endstops rejected request/retry pacing, nested refresh, request close/reopen/switch callbacks, controller snapshot teardown, unavailable/startup/printing states and deleted polling body; Motion Diagnostics real local calculations, numeric editor rejection, deleted targets/values/selectors/labels, controller snapshot close/reopen/switch, card teardown and replacement ownership. All variants run across five themes, densities and text sizes. Existing calibration layout checks retain the narrow viewport, long-text, dropdown, footer and keyboard checks.

Combined panel verification pending: Probe Accuracy sample chooser, Back/reopen and ordinary Run Check once while XYZ is homed and the probe is over a clear bed; Live Endstops refresh, Close/reopen and printer switch; Motion Limits, Drivers, Distance calculation, numeric edit Done/Cancel and repeated close/reopen. An accepted Probe Accuracy request is suppressed for five seconds, not declared complete. Physical probing/results should be checked on the printer. Keep one patch/build/flash for this related batch; version remains 7.0.0-dev.

### Drybox/Macro command-surface batch (7.0.0-dev)

Run `python3 tools/audit/command_surfaces_safety_test.py` with LVGL dependencies available. Real-LVGL Drybox/Macros variants cover checked acceptance/rejection, retained old Drybox program, live program confirmation, duplicate guards across reopening, independent Stop, send close/reopen/switch callbacks, captured Macro sender across logging mutations, pre-send logging teardown, error-log teardown, stale printer roots, individually deleted fields, editor targets and page replacement across all five themes/densities/text sizes. The maintained Macro feedback and Studio Drybox layout checks also cover their native UI. The older LVGL macro row audit now checks the current ten-field native column instead of its removed two-column grid.

Combined panel verification pending: Drybox PLA/PETG/Hold/Resume/Stop with the actual program/heater state checked in the host, leave/reopen and printer switching; Macros parameter edit Done/Cancel, Review/Run, failed-send retry and repeated opening. Accepted guards indicate dispatch, not completion. This batch changes neither dryer temperatures nor macro definitions, and stays on feature/profile-setup-dialogs version 7.0.0-dev.

### Monitoring-surface safety batch (7.0.0-dev)

Run `python3 tools/audit/monitoring_safety_batch_test.py` with LVGL dependencies available. The existing Console, Event History and Telemetry real-LVGL audits now include pre-send command-log teardown, captured-sender mutation, error-log teardown, history lookup closing an editor, deleted input/keyboard detachment, log-read close/reopen/reentrancy, deleted History list/Follow and Clear callback teardown, telemetry history reads closing/replacing the chart host, individually deleted telemetry controls/charts/graph host and retired callbacks. Their original checks still cover five themes/densities/text sizes and smaller viewports, native layout, Hold/Follow, history-ring replacement, numeric/time gaps, named hotends, no-op refresh and external page deletion.

Combined panel verification pending: Console editor/history/search, one harmless `STATUS` command and rejected retry; Event History Hold/Follow/Clear/Close and repeated reopening; Telemetry Heat/Motion/Environment, range/hotend/targets, Hold/Resume and leave/reopen. Printer switching should keep editor ownership and named/history traces correct. This batch follows command-surface safety and retains feature/profile-setup-dialogs version 7.0.0-dev; no visual-polish or release merge is included.


### Settings-operation safety batch (7.0.0-dev)

Run `python3 tools/audit/settings_safety_batch_test.py` with LVGL dependencies available. The maintained Network/Backup and OTA dialog audits cover all five themes, densities, text sizes and viewports, including nested dispatch, encrypted worker overlap across parent reopening, backend callbacks replacing backup dialogs, failed removal retry, retired inputs/keyboard detachment, selected-row deletion, duplicate cancellation across progress reopening, deleted progress fields, catalog reads closing/replacing the browser, and deleted tabs/lists. Translation units compile independently with warnings treated as errors. The actual OTA worker fault audit checks cancellation, SDK/task failures, incomplete images, known/unknown lengths, transport configuration, transfer counters and restart behavior. `calibration_dialog_layout_test.py --variant OTAFAILURE` runs just the firmware dialog variant; its default still runs the full suite.

Panel verification pending: plain/encrypted backup success and failed retry with the SD card; restore preflight and Cancel; remove Cancel and a failed retry where practical; WiFi scan/password Back and failed Connect retry; Moonraker port invalid/valid Save; firmware URL Cancel/Release switching, catalog Retry, download Cancel once and failure Close/reopen. Check an actual successful firmware installation only with the intended compatible build. Closing backup progress leaves its worker running; do not treat UI closure as cancellation. Do not perform a destructive restore or remove backups solely for a smoke test. Branch remains feature/profile-setup-dialogs, version 7.0.0-dev, before visual polish.


### Preference-dialog safety batch (7.0.0-dev)

Run `python3 tools/audit/settings_dialog_layout_test.py` and `python3 tools/audit/theme_dialog_layout_test.py` with LVGL dependencies available. Settings tests cover all five themes, densities, normal/large text and 1024/640/480 widths (including a short viewport), failed NVS erase with nested retry blocked and no restart, accepted erase suppression, timezone persistence replacing its popup, notification reopening a new popup, failure retry and normal Close. Erase/restart calls are stubbed, so no device settings are erased. Theme tests cover rejected async reservation without persistence, rejected persistence, nested theme selection, teardown during persistence, missing selected custom rows, removal failure feedback, active removal, stable identity and queued chooser cleanup. Original preview bounds, allocation/fallback cleanup and no-background-redraw scrolling checks remain.

Panel verification pending: Time Zone select/Close and persisted choice; built-in theme switching in both directions, custom Select/Apply/Keep where themes are installed, repeated open/close and chooser scrolling; Factory Reset open/Cancel only for routine testing. Successful reset and custom-theme deletion should be verified only when those operations are intended. This patch follows the backup/network/update safety batch without committing, pushing or performing visual polish.


### Live-appearance lifecycle safety batch (7.0.0-dev)

Run `python3 tools/audit/live_appearance_test.py` with LVGL dependencies available. The real-LVGL audit retains all five themes, densities, normal/large text and 1024/640/480 viewports, live accessibility toggles, pinned Close, opaque scrolling, queue/save failures and section-identity scroll restoration. Added hooks cover same-kind show preserving its callback, accepted notification opening or opening/closing another chooser, a cross-kind action delivered to a live density row, independent body/row deletion, nested persistence actions, successful saving that closes its chooser, a rejected save that closes its chooser, and body deletion while a refresh is queued. Callback-created choosers retain their kind and scroll position. The maintained runner also compiles the appearance owner as a separate translation unit with warnings treated as errors.

Panel verification pending: Accent select/reselect, density changes in both directions, Large text/High contrast/Reduced transparency/Reduced motion, rapid taps, Close/reopen and retained chooser scroll after refresh. Confirm controls continue fitting after metric changes; this is lifecycle modernization before visual polish and keeps feature/profile-setup-dialogs version 7.0.0-dev. This patch follows the Reset/Time Zone/Theme preference-dialog safety batch.


### Notification-surface lifecycle safety (7.0.0-dev)

Run `python3 tools/audit/notifications_test.py` with LVGL dependencies available and `python3 tools/audit/notification_backup_test.py`. The maintained real-LVGL audit covers all five themes/densities/text sizes/viewports, existing fault priority, bounded queue/deduplication, inactive-printer identity, preferences and layouts. New callbacks cover nested Save/Toggle during storage, successful and failed saves replacing the dialog, deleted preference fields/selected controls, a retired transient timer against a persistent warning, surface deletion publishing a replacement notice, printer changes during dismissal and navigation, nested notice actions, deleted Open/body fields, and log callbacks clearing the queue or changing printer generation. The notification module compiles independently with warnings treated as errors. Backup parser tests retain notification-level migration and malformed-value validation.

Panel verification pending: Notifications Quiet/Standard/Detailed Select/Save/Cancel and reopening, fault/warning Open and Dismiss, connection loss/recovery notices for each printer, repeated notice actions and switching printers before Open. Detailed routine confirmations should still expire without dismissing a later persistent warning. This patch follows live-appearance lifecycle safety and keeps feature/profile-setup-dialogs version 7.0.0-dev, before visual polish.


### Display-setting lifecycle safety (7.0.0-dev)

Run `python3 tools/audit/display_settings_safety_test.py` with LVGL dependencies available. The audit includes the actual `ui_settings.c` callbacks and uses real LVGL widgets at 1024/640/480 widths with deterministic BSP/NVS faults. It covers hardware preview failure and slider rollback, open/set/commit failures and captured retry, nested preview/save/sleep/restore rejection, page and field replacement during hardware/storage callbacks, next-refresh reconciliation, unchanged timeout on failed save, complete timeout cycling, repeated module initialization without duplicate timer creation, stale timer rejection, backlight-off failure cleanup, wake failure retaining the shield, duplicate wake suppression, external overlay deletion and deletion during backlight-off, restoring while asleep and restoring Off to wake. It compiles `ui_settings.c` independently with warnings treated as errors. Maintained Studio component/page tests cover layout and teardown; physical display/first-touch behavior requires panel verification.

Panel verification pending: brightness drag/release and leave/reopen, brightness after reboot, sleep timeout Off/5/15/30 and persisted choice, a real timed sleep/wake cycle with the first touch waking only, and repeat sleep/wake. Hardware/NVS failure injection is host-only. This patch follows notification lifecycle safety and stays on feature/profile-setup-dialogs version 7.0.0-dev without visual polish, commit or push.


### Information-surface lifecycle safety (7.0.0-dev)

Run `python3 tools/audit/about_dialog_layout_test.py`, `python3 tools/audit/telemetry_modern_layout_test.py` and `python3 tools/audit/display_settings_safety_test.py` with LVGL dependencies available. About audits retain all five themes/densities/text sizes/viewports and three firmware channels, native pointer Close, scrolling/pinned actions, missing metadata and repeated deletion/reopening. Added hooks cover cancellation and nested Show during firmware metadata reads and popup deletion opening a replacement. The actual System Info module is exercised in the maintained telemetry audit across all five themes, including nested refresh, heap-label rebinding during an SDK query, deleting old/individual bound labels, parent deletion without explicit unbind, IDF-only queries and unbinding during version retrieval. Existing no-op refresh assertions remain. The display audit checks the updated Settings retirement integration. Changed modules compile with warnings treated as errors through their maintained host runners.

Panel verification pending: Settings memory/uptime readings through repeated leave/reopen and theme changes; About build/channel/source/SDK/LVGL fields, scroll to credits, Close and repeated reopen; leaving Settings should retire About. This patch follows display-setting lifecycle safety without visual polish, commit or push.


### Recovery-dialog safety batch (7.0.0-dev)

Run `python3 tools/audit/recovery_dialog_safety_test.py` with LVGL dependencies and `python3 tools/audit/estop_transport_test.py`. The real-LVGL recovery audit covers all five themes/densities/text sizes/viewports, rejected retry, same-name generation changes, captured sender after rebinding, nested restart/stop callbacks, accepted restart suppression across reopening, missing message fields, callbacks closing/replacing the dialog, M112 while a restart callback is active, and popup teardown opening a replacement review. Stop feedback retains its verification instructions while restart is pending. Independent owner compilation uses warnings as errors. The existing transport audit checks real WebSocket/HTTP emergency and recovery endpoints, TLS/auth/selected port, command isolation, busy-slot bypass and failures.

`calibration_dialog_layout_test.py --variant FEEDBACK` retains recovery/local-feedback checks; `studio_layout_test.py` retains native pointer E-stop and recovery layout checks. The Studio fixture now supplies printer generations and checks the current inline failure/accepted-restart feedback with explicit Close, replacing outdated toast/automatic-close assumptions.

Panel verification pending: Klipper restart review Cancel/reopen, failed-send retry and accepted review/wait/reopen behavior on the intended printer; switching between same-named profiles must prevent an old review from sending. Actual emergency stop/recovery remains an intentional physical verification step, not a routine smoke-test action. The five-second guard represents accepted dispatch, not reconnection or restart completion. No commit, push or visual-polish change is included.


### Controller restart/reset safety (7.0.0-dev)

Run `python3 tools/audit/settings_dialog_layout_test.py`, `python3 tools/audit/display_settings_safety_test.py` and `python3 tools/audit/theme_dialog_layout_test.py` with LVGL dependencies available. Settings audits cover the new Reboot confirmation across all five themes/densities/text sizes and normal/short viewports, repeated Show, Cancel, active OTA, every busy encrypted-worker phase, confirmation closing during a policy query, deleted status fields, one-shot stubbed reboot and Factory Reset remaining blocked without NVS erase. Erase/restart use SDK stubs, with a fresh-boot simulation between positive reboot and reset checks. The display audit verifies the owned reboot opener, disabled/deleted action rejection and current-page integration. Shared theme chooser regressions and independent Settings owner compilation remain covered.

Panel verification pending: Reboot Controller open/Cancel/reopen, pinned actions and active-operation feedback; Factory Reset open/Cancel while an encrypted backup is running. Actual controller reboot is an intentional verification step after background operations finish. Routine panel smoke checks should not erase settings. Branch/version remain feature/profile-setup-dialogs / 7.0.0-dev after the recovery-dialog safety batch; nothing is committed or pushed by the installer.


### Shared popup/footer and keyboard safety (7.0.0-dev)

Run `python3 tools/audit/shared_popup_safety_test.py` with LVGL dependencies. The real-LVGL fixture exercises deleted footer buttons, labels and containers; resizing and slot/footer recreation; duplicate size-callback prevention; popup, blocker and owner teardown; both keyboard constructors; input-first and keyboard-first deletion; direct replacement-input preservation; pinned-keyboard popup teardown; binding allocation failure; and repeated object-count cleanup across five themes, both text sizes and 1024/640/480 viewports. Use `control_popups_layout_test.py`, `network_backup_dialog_test.py`, `diagnostics_batch_safety_test.py` and `settings_dialog_layout_test.py` for existing form/action regressions.

Panel verification pending: temperature custom-entry Cancel/reopen, Network port-entry Cancel/reopen and a macro parameter editor Cancel/reopen; check typing, pinned keyboard/footer actions and returning to the underlying page. No printer action or controller restart is required for these checks. The installer leaves version 7.0.0-dev and does not commit or push.


### Bed Mesh profile field and action safety (7.0.0-dev)

Run `python3 tools/audit/bed_mesh_profiles_layout_test.py` with LVGL dependencies. Its real-owner fixture now covers spoofed row/action events, disabled and independently deleted actions, selected-row deletion, removed list/input/feedback/Continue controls, deleted feedback during rejected transport calls, confirmation-operation mismatch, replacement editor during Continue, replacement review during manager teardown and replacement owner during full cleanup. Existing exact script/retry, printer changes, native pointer actions, keyboard/footer bounds and repeated teardown checks run across all five themes, three densities, both text sizes and normal/narrow/short viewports. The owner also compiles separately with warnings as errors. Run `tools_action_safety_test.py` for the Bed Mesh transport bridge and `shared_popup_safety_test.py` for shared keyboard/footer cleanup.

Panel verification pending: Tools > Bed Mesh > Profiles; select a row, open Save As, type a disposable name and Cancel; repeat Save As through Continue and Back/Cancel; open Remove and Back/Cancel. Verify keyboard, selected name and pinned actions after reopening. Actual Load/Save/Remove and SAVE_CONFIG restart remain intentional printer checks rather than routine smoke checks. No commit or push is included; identity remains 7.0.0-dev.


### Setup Center operation and field safety (7.0.0-dev)

Run `python3 tools/audit/setup_center_layout_test.py` with LVGL dependencies. The actual Setup owner runs across all five themes, three densities, both text sizes and 1024/640/480 viewports. Added coverage includes nested Wi-Fi sends/navigation, repeated-Show sender capture, rejected retry, copied credentials surviving input deletion, disabled/deleted/unrelated actions, deleted feedback/content, unrelated timer rejection, closing/reopening inside Wi-Fi/default-camera/URL/completion callbacks, camera selection while testing, failed camera/default/URL saves, missing Use camera, nested completion and root DELETE reopening Setup. Existing scan, expected-SSID timeout/retry, keyboard/footer layout, printer-editor routing, discovery/test and completion-gate checks remain. Setup also compiles separately with warnings as errors. Run `shared_popup_safety_test.py` for shared keyboard/footer regression coverage.

Panel verification pending: reopen Setup from Settings, scan/select a Wi-Fi network, type and leave/reopen the form; check camera discovery/test selection and Use camera feedback where available; navigate Review and Set up later. A real reconnect, camera persistence across reboot and marking onboarding complete remain intentional checks. The installer leaves 7.0.0-dev and does not commit or push.


### Files browser control and callback safety (7.0.0-dev)

Run `python3 tools/audit/responsive_layout_test.py` and `python3 tools/audit/files_load_worker_test.py` with LVGL dependencies. The Files fixture adds unrelated/disabled toolbar rejection, nested Sort/Refresh suppression, deleted search input/Apply, shared keyboard detachment, query ownership across callback page replacement, page replacement during search teardown, copied folder/file paths after clearing rows, preview iteration clearing/replacing its row list, deleted thumbnail/list/toolbar-label cleanup and late calls. Existing native layouts, metadata touch scrolling, print confirmation and object/timer cleanup remain covered across themes, density/text/viewport and custom layout cases. Fixtures now use the actual Files Search control and reopen a page after simulated printer changes. The Macro portion likewise rebuilds after simulated printer changes and separates independent sends from its existing five-second accepted-command suppression. Production Macro behavior is unchanged. Worker checks retain off-LVGL HTTP/retries, coalesced/latest ownership, generation/page/serial fences and task/allocation failure cleanup.

Panel verification pending: Files Search typing/Cancel/reopen/apply, Sort, Refresh, folder/Up navigation and scrolling thumbnail rows. Switch printers and return to Files before repeating; old-page controls must not act on another printer. Print-start and physical-print checks remain separate. No commit or push is included; version stays 7.0.0-dev.


### Owned page-state panel safety (7.0.0-dev)

Run `python3 tools/audit/page_state_safety_test.py` with LVGL dependencies. Its real-LVGL fixture covers all four states, spinner/icon visibility, theme fonts, owned root/parent deletion, replacement owner slots, individually deleted icon/title/detail/spinner/body, missing-field Show/Hide, allocation rejection preserving the existing handle, legacy constructor behavior and metadata accounting. A controlled callback after the real text update tests nested Show, nested Hide, deleting a title, replacing the panel and deleting it while an outer Show unwinds. The matrix uses five themes, three densities, both text sizes and 1024/640/480 viewports. The helper compiles separately with warnings as errors.

`responsive_layout_test.py` includes Files integration: independently deleting its status root clears `s_files_state`, and late Loading/offline/empty updates remain safe. Existing page layout/lifecycle and print/detail regressions remain. `files_load_worker_test.py` retains worker ownership, latest-job/generation fences and task/allocation failure coverage. Panel verification pending: Files Refresh/loading, an empty search and returning to the populated list; repeat leaving/reopening Files while loading. Offline feedback can be checked when naturally disconnected. No commit or push is included; identity remains 7.0.0-dev.


### Moonraker discovery dialog/session safety (7.0.0-dev)

Run `python3 tools/audit/discovery_states_test.py` with LVGL dependencies. The real printer-scan fixture now covers repeated-Show callback/status preservation, disabled/deleted/unrelated actions, nested selection/retry, captured endpoint strings after callback-driven closure, once-only Close, deletion of status while acquiring the lock, session replacement during a public status lock wait, old workers finishing after a replacement opens, cancellation recheck during candidate-publication lock acquisition, printer changes, list deletion and replacement dialogs opened by DELETE/Close callbacks. Existing task failure/retry, native pointer actions, empty scans, layout and owner cleanup run across five themes/densities/text/viewport sizes. The Moonraker discovery module also compiles separately with warnings as errors. The camera-discovery worker fixture retains captured profile/generation and UI-only import tests; `setup_center_layout_test.py` retains Setup routing/form regressions.

Panel verification pending: open a printer profile editor, select Discover, then Retry/Close/reopen; repeat through Settings > Setup Center > Printer > Open printer setup > Discover (the launcher may say Review printer if already configured). Verify selection updates the intended draft; leave it with Cancel when no connection change is intended. Close/reopen while a scan is finishing must keep the new dialog's status/results. Discovery is in printer setup, not a direct action on the Network page. Real scan coverage/latency and endpoint-connect checks remain physical/network verification. No commit or push is included; version stays 7.0.0-dev.


### Printer-profile draft/test/authentication safety (7.0.0-dev)

Run `python3 tools/audit/profiles_layout_test.py` with LVGL dependencies. Added real-owner checks cover nested Test/Save rejection during test dispatch, duplicate pending tests, unrelated poll timers, changed host/port/API key/security/generation before result consumption, rejected test start, close/reopen during test start/result callbacks, current verified results, retired credential snapshots, disabled/unrelated Test controls, deleted authentication input/Use Key, replacement authentication during Use Key cleanup, deleted keyboard target/value/Done, shared keyboard detachment, deleted status during polling and replacement editor/manager creation during teardown. The matrix retains all five themes, three densities, both text sizes and 1024/640/480 viewports. Existing Save validation, connection/auth/security/certificate/camera controls and native keyboard/footer layout checks remain; the full profile owner also compiles separately with warnings as errors. `setup_center_layout_test.py` retains Setup's printer-editor routing and field/operation regressions.

Panel verification pending: open a printer profile editor; Test, edit the host or port draft and Test again; inspect Authentication, edit its masked key then Cancel; repeat field editor Done/Cancel and close/reopen the main editor. Keep the intended saved values by leaving the main editor with Cancel unless deliberately saving a change. Real endpoint probing/TLS and persistence/reconnect across reboot remain intentional network/device checks. No commit or push is included; version remains 7.0.0-dev.


### Printer-profile Save/Use/Remove persistence safety (7.0.0-dev)

Run `python3 tools/audit/profiles_layout_test.py` with LVGL dependencies. New persistence cases cover nested Save/Use/Remove and discovery redirection, failed base/URL/camera-name/security writes with retained drafts, active-profile notifications for accepted partial writes, successful retry, disabled/deleted/unrelated Save actions, captured data after a backend callback replaces the editor, no later writes into a replacement session, rejected/nested activation, changed profile generations at removal confirmation, last-profile recheck, deleted Remove approval, failed removal retaining the same review and nested confirmation after changing the manager selection. Repeated Show must retain its original callback; active selection and successful deletion remain model-driven. Existing draft/test/auth/keyboard, certificate/camera, native layout and teardown tests plus standalone compilation remain across all five themes/densities/text/viewports. `setup_center_layout_test.py` retains the shared profile-editor routing regression checks.

Panel verification pending: open a profile editor, review its existing values and Save/reopen; use a spare profile if changing endpoints. Select Use Printer for the intended configured slot; open Remove and Cancel, then reopen/Cancel to inspect the pinned review/actions. Actual deletion is an intentional settings change and is not required for a smoke check. Failure feedback requires a real persistence failure and remains host-injected in this environment. Confirm any changed active profile reconnects and saved values survive reopening/reboot. No commit or push is included; identity remains 7.0.0-dev.


### Printer-profile camera removal and cleanup safety (7.0.0-dev)

Run `python3 tools/audit/profiles_layout_test.py` with LVGL dependencies. Added real-owner cases cover disabled/deleted/unrelated removal controls, captured printer/slot, changed generation, selection and nested-operation suppression, pending Find/Test poll retirement, failed removal retaining its review for retry, accepted primary-slot draft clearing after feedback deletion, missing review feedback, external review deletion, replacement reviews/camera dialogs from DELETE callbacks, parent editor replacement during backend removal, deleted slot buttons, unrelated camera poll timers and complete cleanup. Existing layouts and draft/persistence/auth/security/camera regressions run across five themes, three densities, both text sizes and 1024/640/480 viewports; standalone owner compilation uses warnings as errors. Run `setup_center_layout_test.py` for shared printer-editor routing regressions.

Panel verification pending: open a printer profile editor > Camera; select a configured camera, open Remove, check its numbered name, choose Keep Camera, then repeat. Close/reopen Camera and check slot selection and pinned actions. If Find or Test was pending when review opened, run it again after Keep Camera. Actual removal is an intentional settings change and is not needed for routine smoke checks; failure/retry and injected callback races are host-tested. Build and OTA/flash validation remain required on the panel. Nothing is committed or pushed; version remains 7.0.0-dev.


### Printer-profile camera draft, apply/default and result ownership (7.0.0-dev)

Run `python3 tools/audit/profiles_layout_test.py` with LVGL dependencies. New real-owner cases cover disabled/deleted/unrelated Apply/Default/Find/Test/Cancel actions; nested writes and slot changes during backend callbacks; failed apply/default/extra-slot save and retry; invalid URL schemes; missing camera fields; captured values surviving editor deletion; replacement during catalog reads and save/default callbacks; duplicate pending Find/Test; changed URL/host/port/API key/security/name/generation before result consumption; failed starts; replacement during start/poll; URL edits during a poll; current JPEG verification; job retirement on slot changes/field deletion; current discovery import advancing its generation; primary-draft synchronization from an additional slot; no-result feedback and cleared credential snapshots. Existing removal review, partial profile persistence, authentication/security, keyboard and native layout checks remain across five themes, three densities, both text sizes and 1024/640/480 viewports. The profile owner compiles separately with warnings as errors. `setup_center_layout_test.py` retains routing and setup regressions; `discovery_states_test.py` retains worker profile/generation/import coverage.

Panel verification pending: printer profile editor > Camera; check Test for the intended stream, then change the draft URL or switch slots while a request is pending and confirm an old verification does not attach to the new draft. Run Find, close/reopen while it finishes, and repeat Find from an additional slot. Find retains its existing behavior of importing returned cameras. Check Default and Use Camera only with the intended configured camera, then reopen and verify the saved stream/default. Keep the main profile editor's intended values; persistence/reconnect across reboot and real failure paths require device/network verification. Build and OTA/flash remain required. Nothing is committed or pushed; version stays 7.0.0-dev.


### Printer-profile security and certificate picker ownership (7.0.0-dev)

Run `python3 tools/audit/profiles_layout_test.py` with LVGL dependencies. Added real-owner checks cover disabled/deleted/unrelated security actions, picker Close and rows; captured printer/path; nested import/Save/mode-change suppression; failed import retaining feedback and retry; changed generation/active printer; missing port; row/path retirement; replacement editor during import; deleted feedback; preserving a callback-edited custom port; replacement Security/picker dialogs during cleanup; external picker deletion; interrupted opendir/readdir/closedir with directory cleanup; nested listing requests; changed configuration/deleted body during enumeration; overlong paths and case-insensitive PEM names. The existing full profile/camera/draft/persistence/layout matrix still runs across five themes, three densities, both text sizes and 1024/640/480 viewports, with standalone owner compilation using warnings as errors. `setup_center_layout_test.py` retains shared profile-editor routing regressions. SDK storage and cryptographic validation are stubbed in this UI fixture; real certificate import/TLS and persistence failures remain device checks.

Panel verification pending: printer profile editor > Security; Cancel/reopen, then Select PEM > Close/reopen and scroll the SD-card list. Standard HTTP changes the draft; leave the main editor with Cancel when preserving its existing secure configuration. If intentionally importing an approved CA, select it, review feedback/port, then save the intended profile and verify TLS reconnection. Certificate import persists immediately, so selecting a PEM is an intentional configuration change rather than a routine cancel-only check. Build and OTA/flash remain required. No commit/push or visual polish is included; version stays 7.0.0-dev.


### Printer-profile remaining controls and endpoint application safety (7.0.0-dev)

Run `python3 tools/audit/profiles_layout_test.py` with LVGL dependencies. Added real-owner cases cover disabled/deleted/unrelated manager/editor/child Cancel and launcher actions; Edit/Add blocked during removal review; invalid selected slots; nested Discover/Save/Close during discovery dispatch; synchronous selected-endpoint delivery; callback-driven editor replacement; keyboard replacement and parent replacement during old-keyboard deletion; keyboard Cancel after deleting its footer action; aliased host/identity input strings; nested endpoint application and actions; missing port/generation change during field updates; replacement during profile/catalog reads; camera-name isolation between editor sessions and preservation through main Save; selection status after manager replacement; and teardown of all new handles. A fixture wrapper injects callbacks after the real textarea update, keeping LVGL's own internal event lifetime separate from owner-boundary checks. Existing security/camera/persistence/draft/native layout tests remain across five themes, three densities, both text sizes and 1024/640/480 viewports, plus standalone profile compilation with warnings as errors. `setup_center_layout_test.py` retains shared profile-editor routing regressions.

Panel verification pending: open profiles > Edit/Add, switch between name/host/port keyboard editors and Cancel/reopen; check Authentication and Camera launch/Cancel; open Discover, select an intended verified endpoint and review its host/port/name draft, then Cancel the editor if no connection change is intended. Discovery remains in the profile editor and Setup Center's printer editor, not a direct Network-page action. When intentionally saving an unrelated profile change, verify its camera name survives reopening; switch profiles and check names correspond to the selected printer. Build and OTA/flash remain required. No commit/push is included; version remains 7.0.0-dev.


### Request-bound profile discovery handoff (7.0.0-dev)

Run `profiles_layout_test.py`, `discovery_states_test.py` and `setup_center_layout_test.py` under `tools/audit` with LVGL dependencies, plus `python3 tools/audit/profile_discovery_bridge_test.py`. Profile cases now cover old tickets after editor closure/same-printer reopening/profile switching, newer-ticket preservation, one-shot selection, invalid endpoint retry, changed host/port/name/generation/active printer, missing fields, synchronous callback delivery, dispatch rejection, replacement during application, explicit scan cancellation and the synchronous helper retiring an old request. Discovery cases cover same-request Show preservation, captured selection identity, a replacement request during selection, stale/zero Close rejection, Close-driven legacy replacement preservation, external deletion notifying the captured request, an old worker finishing after request replacement and Retry recovery. Setup checks pass/cancel the captured identity and do not start after a refused Show. These fixtures retain their theme/density/text/viewport matrices and standalone owner compilations with warnings as errors.

The new bridge audit extracts main.c's actual scan/selection functions and compiles them against the real discovery header plus SDK/request mocks. It checks missing requests, disconnected/busy/start failure paths, refused/replaced Show, a retired request before task start, stale selection and closing only the original request when application opens a replacement. This does not substitute for a full ESP-IDF build or real network/panel checks.

Panel verification pending: profile editor > Discover > Close/reopen; select an intended endpoint and review its draft without Save when preserving configuration. Repeat through Settings > Setup Center > Printer > Open printer setup (or Review printer) > Discover. Selection should return to its originating draft. When switching profiles or leaving/reopening setup while scans finish, old selections/status must not change the new draft. If an old scan is still finishing, wait and use Retry. Actual scan latency, network changes and connection persistence remain device checks. Discovery is not a direct Network-page action. Version remains 7.0.0-dev; nothing is committed or pushed.


### Printer chooser action, field and refresh lifetime safety (7.0.0-dev)

Run `python3 tools/audit/chooser_refresh_test.py` with LVGL dependencies. The real-owner fixture adds native pointer taps on preview containers/images/text, card fields/body, empty cards and Manage; bubbled descendant rejection; disabled/unrelated Manage/card rejection, repeated-Show callback preservation, nested selection/management suppression, profile-read replacement during a click, callback-driven reopening, unrelated/NULL poll timers, deleted Manage and each card leaf, preview image/container deletion, revision-zero source recreation, zero-sized cached preview rejection, replacement during state/profile/health/cache reads, chained/deferred replacement and cancellation by Hide, generation changes, deleted feedback during a backend query, replacement during root DELETE, external root deletion, and real subject observers that replace the chooser or delete feedback during notification. Existing five-theme/three-density/both-text-size checks still cover geometry, active/inactive status semantics, live previews, subject/fallback updates, zero redraw/notification for unchanged polling and display-event cleanup. The chooser owner also compiles separately with warnings as errors. `preview_cache_ownership_test.py` retains bounded cache-buffer regression coverage.

Panel verification pending: reopen the printer chooser from the top bar, select an intended configured printer, return to the chooser and repeat. Open Manage Printers and Close; check an empty card's Add route and Cancel. Watch active/inactive status and preview updates while leaving/reopening the chooser. Real printer switching, network health timing, panel heap soak and frame timing remain device checks. No printer command, profile deletion or settings reset is needed for a smoke check. Build and OTA/flash remain required; identity remains 7.0.0-dev and nothing is committed or pushed.


### Fullscreen preview owner, launch and result safety (7.0.0-dev)

Run `python3 tools/audit/preview_cache_ownership_test.py` with LVGL dependencies. Its new `SAFETY` variant includes the actual lightbox owner/worker with typed allocator, queue/task and network fixtures. Real LVGL checks cover background/image/hint pointer taps, disabled and unrelated/bubbled controls, image-derived canvas sources, invalid/symbol object input with an existing overlay, aliased descriptor/object/filename input, explicit/external DELETE replacement, deleted image/hint references, replacement during rendering and host/key/port/queue/task boundaries, configuration changes, queue/task/PSRAM failure, successful publication, exhausted retries retaining fallback, display-lock failure, stale queued requests, replacement/deletion during high-resolution rendering, and replacement immediately after source detachment or transfer. Source-setter injection runs after LVGL's real setter returns to test owner boundaries without introducing deletion inside LVGL internals. Every case runs across five themes, three densities and both text sizes at the existing 1024x600 geometry, with buffer accounting returning to zero after close. The full suite retains profile-cache, Files row-buffer and bounded fallback regressions; the lightbox also compiles independently with warnings as errors. Use `--variant SAFETY` to run only the new fixture. Network, PNG decoding and scheduler timing are mocked; these tests do not establish target frame timing or real download behavior.

Panel verification pending: open fullscreen previews from Dashboard, Printer and Files. Tap the image, bottom hint and background to close, then reopen repeatedly. While a large preview loads, close it and open a different file; confirm an old result never replaces the new preview. Check loading-to-high-resolution and failed-load fallback behavior when available. Build/OTA, actual PSRAM/PNG/HTTP behavior, profile switching and longer heap soak remain device checks. No print command or configuration write is needed for the smoke check. Version remains 7.0.0-dev; nothing is committed or pushed.


### Reusable Files thumbnail component and fitting lifetime safety (7.0.0-dev)

Run `python3 tools/audit/thumbnail_component_safety_test.py` with LVGL dependencies. The actual component/renderer fixture checks independent label/image deletion and recreation, optional owner-slot binding/rebinding and replacement preservation, create/wrap teardown, native image taps, disabled/hidden/unrelated/bubbled click rejection, pinned/aliased/unscoped/overlong filenames, nested update suppression, failed rendering/PSRAM allocation, root closure during rendering/lightbox dispatch, closure after captured child deletion, root/image deletion after layout, fit/contain/cover temporary-hook cleanup and return-to-zero PSRAM accounting. Cases run across all five themes, three densities and both text sizes at 1024x600; the owner separately compiles with warnings as errors. Delete/layout/render boundary injection runs after the real LVGL/render call returns, rather than deleting a parent from inside LVGL's own child-DELETE traversal. Component buffers remain bounded to 286x215 RGB565 while alive.

`preview_fill_test.py`, `aspect_preview_pipeline_test.py` and `responsive_layout_test.py` retain exact sampling/buffer bounds, actual Dashboard/Active Print aspect/stride/reuse and Files/other page geometry/touch/lifecycle regressions. Backend calls are mocked and this does not replace ESP-IDF or panel validation.

Panel verification pending: Files > select a file, wait for its thumbnail, tap for fullscreen and close. Cancel the inspector and select a different file; check the large preview corresponds to the displayed thumbnail. Repeat while metadata/thumbnail work is pending and leave/reopen Files. Check Dashboard and Printer previews still fit their wells. Build/OTA, real decoder/download behavior and longer heap soak remain device checks. No Print action, deletion or configuration write is needed. Version remains 7.0.0-dev; nothing is committed or pushed.


### Camera page and review control ownership (7.0.0-dev)

Run `python3 tools/audit/camera_pipeline_test.py` with LVGL dependencies. New real-owner cases cover disabled/unrelated launchers and page controls, native View/picker taps, disabled/unrelated view options and camera rows, disabled/deleted Close controls, deleted options/rows/feedback, failed-save retry without feedback, nested Configure/Retry/Fullscreen/Close during saves, closure/replacement during catalog/default reads, page replacement during View application and stream retirement, selector deletion during a catalog read, DELETE-driven replacement reviews, deletion of every optional page control/status, disabled image exit and native fullscreen exit without those controls. These control cases run across all five themes, three densities and both text sizes at 1024x600. Existing dialog geometry spans 1024/640/480 widths and a short viewport; frame/mirror/rotation, redraw silence, invalid frame, lifecycle/timer balance, setup suspension and Dashboard retirement checks remain. The Camera owner now also compiles separately with warnings as errors. Catalog, stream and persistence operations are mocked.

`python3 tools/audit/offline_navigation_test.py` retains nonblocking camera retirement and connection-selection regressions. Actual streaming, persistence, profile switching, decoder behavior and heap/frame-timing soak remain device checks. Frame-poll callback boundary review is separate from this control batch.

Panel verification pending: Camera > View > Close and camera picker > Close, repeated after leaving/reopening Camera. Enter fullscreen and tap the image to exit. If unavailable, use Retry and confirm navigation stays responsive. If intentionally changing rotation/mirror/default camera, verify the intended camera and persisted choice, then restore the previous view/default if desired; those controls save immediately. Build and OTA/flash remain required. No printer command or profile deletion is needed. Version stays 7.0.0-dev; nothing is committed or pushed.


### Camera poll, frame-transfer and transform boundary safety (7.0.0-dev)

Run `python3 tools/audit/camera_pipeline_test.py` with LVGL dependencies. Added real-owner cases cover unrelated timer rejection, nested result-consumption polls, Retry/view application during consumption, page replacement during result/catalog/busy/default/start calls, generation changes during consumption and default selection, stream-start failure, zero/negative/oversized dimensions and truncated/failed frames, Hide/setup suspension during consumption, replacement during old-frame detachment, image deletion during publication, frame release/page replacement during transform reads, timeout retirement replacement and preservation of a new frame installed while old cleanup unwinds. Stream/catalog calls are mocked; image-source injection runs after LVGL's actual setter returns. Poll cases run across five themes, three densities and both text sizes at 1024x600, with allocator balance checked after each owner closes. Existing dialog/view/control geometry matrices, unchanged redraw/transform silence, source-before-free assertions, lifecycle/timer checks and standalone warnings-as-errors compilation remain.

`python3 tools/audit/offline_navigation_test.py` retains worker/connection nonblocking retirement regressions. These fixtures do not establish actual JPEG decode, HTTP timing, frame rate or long-duration heap stability.

Panel verification pending: with a live camera, repeatedly leave/reopen Camera and enter/exit fullscreen; confirm streaming resumes and frames match the selected camera. Check Setup/Configure > Cancel > Camera resumption. When unavailable, use Retry and navigate away/back; no old LIVE/unavailable feedback or frame should replace the new session. Intentional profile/camera switching should discard the previous view's frames. Build/OTA, real network/decoder behavior and a longer camera heap soak remain device checks. No printer command or deletion is needed. Version stays 7.0.0-dev; nothing is committed or pushed.


### Camera Show/Hide, catalog and setup handoff safety (7.0.0-dev)

Run `python3 tools/audit/camera_pipeline_test.py` with LVGL dependencies. New real-owner cases cover page replacement during fresh default/selector reads after complete construction; nested Show and polling during default selection; configuration changes during initial Show with a retained future poll; Hide during cached Show/fullscreen opening; reopening from review DELETE, stream-stop and frame-detach callbacks during Hide; setup resumption from dialog deletion or explicit stop; catalog refresh while hidden/suspended, replacement or selector deletion during refresh; missing persisted-default fallback; and balanced timers/allocator cleanup. The handoff cases run across five themes, three densities and both text sizes at 1024x600. Existing frame/control/poll and multi-viewport dialog cases, redraw silence, ownership assertions and standalone warnings-as-errors compilation remain. Catalog/stream operations are mocked, and source-boundary callbacks run after the real LVGL setter returns.

`python3 tools/audit/offline_navigation_test.py` retains nonblocking navigation/worker retirement regressions. Target build, real camera transport/decoder, settings persistence, profile switches and long-duration heap/frame-rate checks remain device validation.

Panel verification pending: open Camera repeatedly from other pages; enter fullscreen, tap to exit and navigate away/back. Configure > Cancel should resume the viewer, and opening/closing View or the camera picker should not disrupt navigation. Confirm the intended persisted default camera appears after reopening. If a setup operation or profile switch is in progress, check the newly opened viewer remains responsive and old feedback does not return. Build and OTA/flash remain required; no printer command, deletion or intentional settings change is needed. Version stays 7.0.0-dev; nothing is committed or pushed.


### Dashboard camera consumer closeout (7.0.0-dev)

`dashboard_camera_safety_test.py` includes the actual Dashboard owner with typed stream/catalog fixtures and a compact real-LVGL owner hierarchy. It checks disabled/unrelated controls, timer identity, nested polls, malformed/stale frames, replacement during result/busy/catalog/default/start/source boundaries, image/control/status/root deletion, replacement-frame preservation, quiescence/restoration, fullscreen routing and allocator/timer balance across five themes, three densities and both text sizes. The full Dashboard source separately compiles with warnings as errors. `camera_pipeline_test.py` also extracts its actual frame-retirement helper, including the shared detachment routine. These tests mock HTTP/decoder/backend timing; production Dashboard geometry is covered by the existing Studio/page/layout regressions during combined closeout.

Panel checks pending: Dashboard Camera/Thumbnail toggle repeatedly; confirm live camera and original print thumbnail return correctly. Open live camera fullscreen and exit; leave/reopen Dashboard and switch intended printer/camera. Observe unavailable/reconnect behavior and OTA/setup suspension/resumption when available. No printer command or configuration reset is needed. Target build/OTA and longer heap/frame-time soak remain device checks. Version remains 7.0.0-dev; nothing committed or pushed.


### Active Print canvases, worker candidates and combined closeout (7.0.0-dev)

`active_preview_safety_test.py` includes the actual Active Print and coordinator owners with real LVGL and typed allocation/lightbox fixtures. It covers native pinned-file taps, disabled/bubbled controls, independently deleted canvas/preview/metadata fields, bounded allocation requests, nested updates, closure/replacement after real label/canvas calls, older-panel deletion and coordinator replacement-slot preservation across five themes, three densities and both text sizes. It separately compiles the Active Print owner with warnings as errors. The shared buffer is intentionally retained across page deletion; the fixture explicitly releases it at shutdown and verifies zero owned bytes.

`aspect_preview_pipeline_test.py` now uses real Active Print construction across all five themes and both text sizes. It extracts actual main.c synchronous render, apply/restore, worker, current-request and render-timer functions. Checks retain aspect/stride/containment and source sampling, and add profile/file/panel changes after rendering, temporary allocation failure preserving previous pixels, synchronous staged publication and unrelated/NULL timer rejection. Render-boundary injection runs after the real renderer returns; backend persistence remains mocked. Peak temporary rendering memory adds one bounded 286x215 RGB565 PSRAM candidate; failure retains the previous preview instead of borrowing a stale source.

Combined host closeout passed Camera pipeline, Dashboard camera, Active Print/coordinator, aspect-preview pipeline, preview-cache/lightbox ownership, reusable thumbnail ownership, chooser refresh, responsive layouts, Studio pages and live appearance, plus offline navigation. Viewport/theme matrices remain those documented by each suite. This does not replace ESP-IDF compilation, target PSRAM/JPEG/HTTP behavior or panel timing/heap soak. See MODERNIZATION_CLOSEOUT.md for the ordered device acceptance and branch checkpoint. Nothing committed or pushed; identity remains feature/profile-setup-dialogs / 7.0.0-dev.

## Printer chooser visual list acceptance

Apply `apply_printer_chooser_list_polish.py` after the complete `ed86f07` modernization checkpoint on `feature/profile-setup-dialogs`, version `7.0.0-dev`. The guarded installer prints prerequisite mismatches before changing files. Build only after installer success (`python3 ~/Downloads/apply_printer_chooser_list_polish.py && ./tools/build_idf6_hosted3.sh`), then flash.

- Confirm four full-width rows, aligned name/endpoint and status/action columns, no chooser scroll, an accent edge plus Active text on the current printer, and plus symbols for empty slots.
- Open configured printers by tapping the thumbnail, name, endpoint, status, action and row background. Empty rows open their setup flow; Manage printers opens the existing profile manager.
- Change printer and reopen the chooser: only the current configured row is marked Active. Verify online/printing/paused/offline states and background verification remain accurate.
- Check cached previews and missing-preview symbols, long profile names/hosts, every theme, large text and density settings. Long names/hosts must remain one line with ellipsis rather than animated text.
- Repeat open/close and profile edits; leave idle to check stable refresh without flicker. Host `chooser_refresh_test.py` exercises the actual LVGL chooser across 5×3×2 theme/density/text combinations, native pointer taps, independent field deletion, observer/reopen lifetime, active-row styling and unchanged 500ms redraw/notification counts. Standalone chooser owner compilation treats warnings as errors.

### Chooser layout revision after list review

Apply `apply_printer_chooser_familiar_layout.py` after `apply_printer_chooser_list_polish.py`. This supersedes the full-width-row visual expectation above: expect the familiar two-by-two arrangement and larger 116px preview wells, with flat page-backed sections, thin lower separators and an active accent edge. Name/endpoint/status/action order follows the original chooser. Verify both columns and rows, long names/hosts, thumbnail fit, configured-printer taps, empty-slot setup, Manage printers and all themes/text/density settings after build and flash. The same chooser host audit covers every 5×3×2 combination and unchanged 500ms polls, including ellipsized names/hosts.

### Chooser detail polish acceptance

Apply `apply_printer_chooser_detail_polish.py` after the accepted `apply_printer_chooser_familiar_layout.py`, then build, flash and verify. Expect a muted, inset center separator, the same familiar two-column positions, aligned lower separators/actions, sentence-case live/connection status, quieter empty-slot titles/status, and consistently sized missing-preview symbols in Studio and the other themes.

Check real thumbnails remain centered and preserve aspect ratio; confirm Printing/Paused/Ready and offline/retrying/verification feedback keep their original status colors and behavior. Tap previews, names, statuses, Open, empty-slot Set up and Manage printers. Reopen after printer selection/profile edits. Repeat with all themes, large text and density settings; leave the chooser idle and check for flicker. Host `chooser_refresh_test.py` covers separator bounds/non-interaction, action alignment, aspect fit, symbol size, state presentation and all prior unchanged-refresh and lifecycle regressions across 30 combinations; firmware build/flash and panel verification remain local steps.

## Manage printers visual acceptance

Apply `apply_profile_manager_visual_polish.py` after `apply_printer_chooser_detail_polish.py` on `feature/profile-setup-dialogs`, version `7.0.0-dev`. Build only after successful installation, then flash.

- Open Manage printers from the chooser. Check the sentence-case heading, flat rows, names/endpoints on one line, subdued empty slots and four visible footer actions at the panel's normal size.
- Select another configured row: the selection accent moves but Active stays with the current printer until Use printer succeeds. Verify Edit printer opens the selected profile and Close returns normally.
- Select an empty slot: the action reads Add printer and opens its editor. Use printer/Remove retain their empty-slot guard messages.
- Verify Remove is pinned with the other footer actions, still opens its existing review and honors Cancel, last-profile protection and persistence-failure behavior.
- Check all themes, density and large text. At narrower/smaller viewports, confirm footer wrapping remains visible and scroll the profile body to reach all rows/status. Long names and endpoints must truncate on one line without animation.

Host `profiles_layout_test.py` exercises the actual manager/editor/removal and nested dialogs across 5 themes × 3 densities × 2 text sizes × 3 viewports, including flat-row bounds, Active versus selected, adaptive Add/Edit text, four pinned actions, scroll reach and prior save/discovery/removal/security/camera lifetime regressions. Standalone owner compilation treats warnings as errors. Target firmware compilation, flash and physical panel acceptance remain local.

## Edit printer scroll performance acceptance

Apply `apply_profile_editor_scroll_performance.py` after `apply_profile_manager_visual_polish.py`, then build only on installer success and flash. Open Manage printers > Edit printer and drag repeatedly through the connection/security controls in both directions. Compare responsiveness with the prior build. Repeat across themes, especially Studio and the theme where the slowdown was observed, and with large text/density options.

Verify the footer remains pinned; tap and edit name/host/port, cancel keyboard edits, open/close security and authentication, test the connection when available, and cancel/save through the existing flow. Open the same editor from Setup Center > Printer options/review to verify it appears above Setup; return/cancel without losing the launcher. Taps outside each modal must remain blocked, and nested keyboard/authentication/security/discovery dialogs must stay above it. Verify returning to the chooser and manager is normal.

Host checks: `profiles_layout_test.py` includes six real renderer scroll steps for each theme/density/text/viewport case, requires zero parent-popup draw callbacks with continuing body drawing, checks the pinned footer and native modal blocking/order, and retains persistence/discovery/security/camera regressions. `PROFILE_SCROLL_DIAGNOSTIC=1` selects a short five-theme draw-count diagnostic instead of the full matrix. `setup_center_layout_test.py` verifies the screen-local launcher and existing setup lifecycle; `shared_popup_safety_test.py` verifies the shared modal mechanisms. Host draw counts do not establish panel FPS, touch latency or transport behavior.

## Grouped profile editor visual acceptance

Apply `apply_profile_editor_visual_polish.py` after `apply_profile_editor_scroll_performance.py`, then build/flash.

- Open Edit printer and Add printer. At 1024×600, confirm the main form fits without needing to scroll: Printer name, host/IP plus Port, Discover printers, Connection security, API key and the helper/status text. Footer actions remain Cancel, Camera, Test connection and Save.
- Verify host/IP and port align, long host/name values remain editable and the five-digit maximum port remains readable. Open/cancel field keyboards; port still accepts only digits and preserves the existing validation.
- Open Connection security: current draft transport is described. Cancel, choose Standard HTTP or use the existing certificate flow; configuration changes must retain their established draft/save behavior. Open API key, edit/cancel or Use key, then test/save when appropriate.
- Check all themes, densities and large text. Narrow/small viewports may wrap security/footer actions and scroll the form, with no horizontal clipping. Preserve the improved scrolling, pinned footer and modal blocking/layering from the preceding patch.

The actual profile audit spans 90 theme/density/text/viewport cases with new form-fit, host/port/maximum-digit and action-fit checks, plus all prior persistence, discovery, camera, security, keyboard, modal blocking and redraw regressions. Native panel appearance, input feel and actual network tests remain build/flash acceptance steps.

## Profile camera visual acceptance

Apply `apply_profile_camera_visual_polish.py` after `apply_profile_editor_visual_polish.py`, then build/flash. Open Edit printer > Camera. Verify a static slot grid, an explicit Default caption, a theme-accent selection border, and no moving name marquees. Tap the current slot repeatedly: it must stay selected. Select another slot: the Default caption remains with the configured default until Make default succeeds.

Check name/URL field keyboard behavior, Find cameras, Test stream, Make default, Remove > Keep camera, Cancel and Use camera. The selected full name remains editable in the field even when its slot caption is ellipsized. Verify stream/navigation suspension and resumption remain normal, and scroll smoothly through small viewports. Confirm keyboard/removal review are above the camera modal and outside taps remain blocked. Repeat theme, density and large-text checks.

The actual profile matrix covers four-/two-column geometry, single-line name clipping, separate default markers, exclusive selected state, action text fit, zero parent redraws for opaque viewport scrolling/repainting, native repeated-slot taps and nested modal ordering, plus prior camera persistence/worker/replacement regressions. Physical camera transport, rendering speed and device acceptance remain panel steps.

## Graphics baseline before hardware acceleration

Apply `apply_graphics_baseline_diagnostics.py` after b4d4c40, build using `tools/build_idf6_hosted3.sh`, then flash the resulting firmware. Confirm unchanged boot/splash, chooser, editor scrolling, nested dialogs, page transitions and camera streaming. Capture `graphics_diag` startup, distinct buffer and active-window reports. Use the interaction sequence and timing interpretation in GRAPHICS_ACCELERATION.md. No accelerator flags are changed by this patch.

`tools/audit/graphics_diagnostics_test.py` compiles the actual observer against real LVGL 9.5 with warnings as errors and deterministic ESP clock/heap/log stubs. It checks zero-time starts, idle exclusion, exact elapsed accounting, buffer deduplication/alignment reports, duplicate attachment, second-display rejection and display-delete/timer cleanup. Host timing is synthetic; device firmware build and panel comparison remain required.

## Fill-only PPA hardware trial

Follow GRAPHICS_ACCELERATION.md. Enable only for an explicit build with `PRINTERHMI_PPA_TRIAL=1 ./tools/build_idf6_hosted3.sh`; normal/rollback builds use `PRINTERHMI_PPA_TRIAL=0`. The canonical sdkconfig.idf6 is not edited. Trial configuration is generated under the ignored build directory and verified after IDF reconfigure. PPA images and port rotation remain disabled. Apply the exact pinned-source upstream LVGL length fix before compiling the trial; never round up a PPA request beyond an existing allocation.

Host audit `tools/audit/ppa_trial_test.py` checks isolated config generation/verification, unknown-option preservation and mismatch refusal, exact-source patch apply/no-op/refusal, actual LVGL allocations at odd dimensions and indexed/alpha formats with 64-byte starts and lengths, plus software/trial/rollback wrapper order with fake IDF tools. Actual PPA driver initialization, cache coherency, panel appearance and speed require the ESP-IDF device build and panel logs; host checks do not establish those results.

## Files scroll redraw performance

Apply `apply_files_scroll_redraw_performance.py` after the PPA trial patch. Rebuild with `PRINTERHMI_PPA_TRIAL=1 ./tools/build_idf6_hosted3.sh` to compare against the same PPA-enabled firmware, then flash using the trial SDKCONFIG or install that new image by OTA. Confirm `ppa=1 draw_align=64` in diagnostics. Scroll Files repeatedly for 15–20 seconds, pause, and repeat with the same folder, theme, density and text setting used before. Return graphics_diag activity lines and perceived responsiveness. Camera performance remains untested without a camera.

Verify thumbnails still arrive, preview/detail taps work, a drag opens no file, and folder navigation/search/sort/refresh still work. Check the empty/offline status, metadata/print review and theme switching. The list stays flat and borderless; Glass now uses a solid page-colored list surface. Do not issue a print command merely to test scrolling.

Host: `python3 tools/audit/responsive_layout_test.py --variant FILES --lvgl-dir PATH --lvgl-lib ARCHIVE` runs the real Files fixture with redraw containment, fixed breadcrumb position, native scroll cancellation and lifecycle checks. Set `FILES_SCROLL_DIAGNOSTIC=1` to print the five-theme native-panel draw counts. Six scroll steps must draw the list but cause zero parent-page/viewport DRAW_MAIN callbacks. This fixture uses software LVGL; actual PPA performance and device appearance remain panel acceptance steps. The file-load worker audit also remains required.

## PPA cache handoff artifact regression

After `apply_ppa_cache_handoff_fix.py`, run the opt-in PPA build wrapper and check its new cache handoff/serialization PASS line. Flash the trial configuration. Confirm ppa=1/draw_align=64, then inspect live Dashboard speed, flow, fan, drybox, air/humidity, tool and bed updates for black blocks over 30–60 seconds. Repeat theme changes and Dashboard/Printer/Files navigation; compare Files scroll behavior. A physical Camera test remains pending until a camera is connected. No printer action or target change is required. Keep software rendering as the default; rollback and flash if artifacts recur.

Run `tools/audit/ppa_cache_coherency_test.py --lvgl-dir PATH --lvgl-lib ARCHIVE` and the existing `ppa_trial_test.py` with the same arguments. The cache fixture compiles the actual guarded backport with warnings as errors, checks cache directions/order, clipped layer-relative alignment/bounds and adjacent CPU data preservation, and proves the original code fails. The wrapper audit checks both fixes precede the trial build while normal/rollback configs remain isolated. Device cache timing, visual correctness and performance are not proven by the host stubs.

## Accepted PPA default build and fallback

Apply `apply_ppa_default_build.py` after the cache handoff fix. With no PRINTERHMI_PPA_TRIAL override, `./tools/build_idf6_hosted3.sh` must select PPA fills, verify 64-byte alignment/images-off/rotation-off, and apply/check both LVGL dependency corrections. `./tools/build_idf6_hosted3.sh flash monitor` carries the exact same generated SDKCONFIG through the build, flash and monitor actions. Check ppa=1/draw_align=64 after boot and repeat Dashboard redraw and Files scrolling checks.

`PRINTERHMI_PPA_TRIAL=0 ./tools/build_idf6_hosted3.sh flash monitor` explicitly builds/flashes software rendering and should report ppa=0/draw_align=4. Running the wrapper again with the variable unset must restore the PPA default. A value already exported as 0 intentionally keeps software selected; use `unset PRINTERHMI_PPA_TRIAL` to restore the default. Baseline sdkconfig.idf6 remains unchanged; the generated config's legacy .ppa-trial name is retained for compatibility. Do not use an explicit SDKCONFIG=sdkconfig.idf6 command to flash a PPA build.

The updated ppa_trial_test.py checks default mode, explicit fallback, explicit legacy mode 1, default restoration and combined build/flash/monitor configuration identity with fake IDF commands, plus invalid option/action refusal. Real LVGL buffer/cache regression tests remain required. Device build/boot confirmation after promotion is still part of acceptance; physical Camera testing remains pending.

## Graphics diagnostics cleanup

Apply `apply_graphics_diagnostics_cleanup.py` after the PPA default-build patch, then use the normal `./tools/build_idf6_hosted3.sh flash monitor`. Confirm the startup renderer line reports ppa=1/draw_align=64/reports=0, both framebuffer alignment lines are available, and no ten-second cycles/heap lines appear while navigating. Dashboard and Files must retain the accepted clean rendering and smooth scrolling. The new project Kconfig option defaults off; no renderer or buffer configuration changes.

For opt-in timing, enable PrinterHMI diagnostics > Periodic graphics timing and heap reports in the software seed's menuconfig, then rebuild through the wrapper as described in GRAPHICS_ACCELERATION.md. Startup must say reports=1 and active ten-second windows must report cycles. Disable and rebuild to return to default behavior.

The actual LVGL graphics_diagnostics_test.py compiles both modes with warnings as errors: default mode checks no timer, no ESP clock reads and no timing reports while preserving buffer logging, deduplication and deletion; opt-in mode retains the existing exact timing/idle/lifetime checks. Physical performance remains verified on the panel.

## Files metadata visual acceptance

Apply `apply_files_metadata_visual_polish.py` after graphics diagnostic cleanup and build/flash normally with `./tools/build_idf6_hosted3.sh flash monitor`. Inspect file rows in all themes/densities and large text: filename above a readable size/date caption, no overlaps, no repeated HOLD FOR DETAILS or DATE -- text, and small files shown in bytes/KiB instead of 0.0 MB. Known timestamps still use the existing local date formatting. Missing metadata uses G-code file. Long names/captions stay one-line ellipsized.

Verify thumbnails, preview/detail taps, drag cancellation, folder/search/sort/refresh behavior and smooth scrolling remain accepted. The updated responsive Files host fixture checks actual byte/KiB/MiB/GiB/fallback captions, theme/accessibility caption sizing and filename/metadata separation, followed by existing responsive, ownership and redraw checks. Device appearance and interaction remain build/flash acceptance.

The host pointer fixture explicitly retires a synthetic scroll-throw animation before deleting its temporary input device; AddressSanitizer exposed the previous fixture cleanup error at LVGL deinitialization. Production touch initialization/lifetime is unchanged.

## Inline Files detail acceptance

Apply `apply_inline_file_details.py` after Files metadata polish, build with `./tools/build_idf6_hosted3.sh`, then install the resulting firmware through the existing OTA server. On Studio, select a normal G-code file: the preview should be slightly shorter, common file values readable directly, and only Print/Cancel actions visible. No thumbnail storage path or duplicate print instruction should consume the inspector. With extra metadata beyond the inline height, Details should appear and expose the complete scrollable text; short updates hide it again. Inline scrolling remains available for long content.

Verify all themes, large text/density, thumbnail/fullscreen behavior, page changes, Cancel and Print > Cancel review. Do not start a print just to test layout. Readiness/profile-change safeguards must remain. The actual Files host fixture checks the standard five fields fit Studio without scroll or Details, correct two-/three-action widths, long metadata expansion, short/long transitions, unknown named-field preservation, and the existing print and lifetime regression matrix. AddressSanitizer with the full pinned LVGL source and visual inspection of the native Studio large-text inspector also pass; device/OTA appearance remains the user's acceptance step.

## Content-sized preview acceptance

Apply `apply_adaptive_file_preview.py` after inline file details, run `./tools/build_idf6_hosted3.sh`, and update through the existing OTA server. Studio should use a larger preview while preserving all normal metadata and pinned actions. Check files with shorter/more metadata, density and large-text settings. Extra overflowing details must remain accessible; no text should be hidden behind the preview or footer.

Verify both orders: metadata arrives before the thumbnail, and metadata changes after an image is already displayed. The image must remain centered, proportional and within the preview after box resizing. Fullscreen preview, Cancel, and Print > Cancel review remain unchanged. Do not start a print solely for this check.

The actual Files matrix covers common metadata without scroll, preview height greater than the old 176-pixel box, non-overlap, long-data minimum height and restored height after short updates. A real LVGL image child checks transformed image bounds across resizing. Run the normal and full-LVGL AddressSanitizer fixtures; inspect the native Studio large-text rendering as well. Actual OTA/panel rendering remains acceptance.

## Files toolbar acceptance

Apply apply_files_toolbar_polish.py after checkpoint 6dc2e79, build with ./tools/build_idf6_hosted3.sh and update through the existing OTA server. Check all themes and large text: Up, Search, sort and Refresh remain readable; an applied search highlights Search and shows its query in the breadcrumb. Open and cancel the editor: the applied search must stay active. Clear the query: the folder breadcrumb and Up availability must return to their normal state. Check sort cycling, long paths, folder navigation and unchanged smooth scrolling.

The responsive Files host matrix covers state transitions, saved folder restoration, sort captions and existing geometry/input/lifetime regressions. Host checks do not establish physical touch or OTA acceptance.

## Empty-search symbol acceptance

Apply apply_page_state_symbol_font_fix.py after Files toolbar polish, build with ./tools/build_idf6_hosted3.sh, then update through OTA. Search Files for an unmatched filename: the empty-results panel should show a folder icon instead of a rectangle, with its message readable. Clear search and verify normal browsing. Check Studio and other themes; offline/error status icons use the same corrected font. No print is required.

The page-state safety fixture checks the actual folder, Wi-Fi and warning glyphs are present and not placeholders in every theme/density/text setting, in addition to its existing ownership regressions. Run the responsive Files matrix for layout coverage.

## Camera control polish acceptance

Apply apply_camera_control_visual_polish.py after the shared status symbol fix. Build with ./tools/build_idf6_hosted3.sh and update through OTA. On Camera, inspect Configure, View, camera selection, Fullscreen and Retry captions in your theme and large text. An unavailable configured camera should show Retry; fullscreen hides it and exit restores it. A missing camera configuration should retain its existing setup feedback. Long camera names should stay still in the toolbar; open the picker to identify cameras.

Check Configure > Cancel, View > Close, picker > Close and page navigation. When a configured camera is available, verify rotation/mirroring, reset and fullscreen fit; real transport/performance needs that hardware. The host Camera matrix checks selector refresh silence, Retry visibility through fullscreen, dialog bounds and existing frame/action/ownership regressions.

## Direct Camera Configure acceptance

Apply apply_camera_configure_direct_handoff.py after Camera control polish. Build with ./tools/build_idf6_hosted3.sh and update through OTA. Camera > Configure should open only Camera setup. Cancel returns straight to Camera, with no Edit printer or Manage printers underneath. Reopen, edit a camera name/URL and choose Use camera: it saves directly to that printer; reopen to check persistence. No separate printer-editor Save is needed. A failed save must retain the current dialog and values for retry.

Check name/URL keyboards (Done and Cancel), Find/Test when available, View/selection cleanup and page navigation. Camera setup entered from Edit printer should still return to Edit printer. Cancel abandons the current unsaved fields; existing explicit Make default, Remove and slot-switch save behavior is retained. Repeat at large text/density and in your theme. Persistence across reboot remains a device check.

Run profiles_layout_test.py and camera_pipeline_test.py against LVGL 9.5.0. PROFILE_CAMERA_DIRECT_ONLY=1 selects the new direct-entry matrix across five themes, three densities, both text sizes and three viewports; the default profile matrix includes these checks too. Full-LVGL AddressSanitizer coverage checks the direct path with leak detection disabled for this host environment.

## Tools entry polish acceptance

Apply apply_tools_entry_visual_polish.py after the direct Camera Configure handoff. Build with ./tools/build_idf6_hosted3.sh and update through OTA. Tools should keep its familiar four entries, now with flat sections, lower separators, sentence-case titles and consistent icons. Inspect your theme, density and large-text settings. Open Calibration, Bed mesh, Devices and Macros, then return; no printer command is required.

Disconnected/busy explanations must remain visible as before. On narrower/large-text layouts, verify scrolling reaches every entry and stays responsive. The responsive Tools host matrix checks glyph availability/resolved size, content bounds, wrapping, offline-to-connected feedback and existing hide/reopen geometry across five themes, three densities, both text sizes, three widths and custom theme metrics.

## Tools divider and Calibration labels

Apply apply_tools_divider_calibration_labels.py after Tools entry polish, build with ./tools/build_idf6_hosted3.sh and update through OTA. Tools should show a thin center separator in its two-column layout, with unchanged entry hit areas. At narrow single-column widths the separator must disappear. Check theme/density and large text.

Open Calibration and inspect the page headings, detected-tool summaries, availability counts and actions. Capability-dependent controls and existing layouts should remain familiar. Opening and cancelling a dialog is sufficient; no heating, motion or calibration command is required for appearance verification.

The Tools responsive fixture checks the actual divider bounds, one-column hiding and deletion across themes, density, text, width and custom metrics. Existing calibration dialog/button-fit tests and standalone compilation cover the label changes.

## Shared divider consistency acceptance

Apply apply_shared_divider_consistency.py after Tools divider/Calibration labels. Build with ./tools/build_idf6_hosted3.sh and update through OTA. In each theme, compare the printer chooser center/lower separators, Tools center/lower separators, Files rows/Studio inspector divider and Calibration internal rules. Default lines should share the theme border color and brightness; their dimensions and inset positions remain page-owned. Check large text/density, one-column Tools hiding, and normal navigation/taps. Press/focus/selection feedback may intentionally differ from decorative lines.

The chooser and Tools fixtures compare actual theme-resolved color/opacity. Existing Files, Studio layout and calibration checks cover shared-helper call sites and unchanged geometry. No print, heat or motion command is needed for device visual acceptance.

## Shared Files/Macros search acceptance

Apply apply_macro_shared_search_polish.py after shared divider consistency, build with ./tools/build_idf6_hosted3.sh and update through OTA. Macros should show sentence-case page/status/action labels. Apply a search: Search gains the same active border as Files. Reopen and cancel a draft query: the existing applied search and highlight remain. Clear search: the full list and normal Search appearance return. Repeat the equivalent Files checks.

Inspect long macro names, favorites, large text/density, parameter editor and the Review dialog. Cancel before Run; no macro execution is needed for visual acceptance. Macro names and generated command text must preserve their original case and values. Host Files/Macros fixtures check active filter state, editor Cancel, Clear and existing layout, owner-generation, parameter/review and command guards.

## Macros scroll surface acceptance

Apply apply_macro_scroll_surface_cleanup.py after Macros/shared search polish, build with ./tools/build_idf6_hosted3.sh and update through OTA. Macros should have no visible list scrollbar or enclosing card border, with unchanged macro buttons. Swipe through the list and back: scrolling must still work and a swipe must not open a macro. Clear search should be disabled without a filter, enabled after applying one and disabled again after clearing. Cancelled search edits must preserve the applied filter.

Check favorites, long macro names, large text/density, Files and Tools surfaces, and the existing Review/Cancel flow. No macro execution is required. The native-pointer host matrix checks scrolling without a scrollbar and without selection/commands, along with clear-button state and existing Files/Macros/Tools regressions.

## Studio default and within-theme consistency

Apply apply_studio_default_theme.py after Macros scroll cleanup, build with ./tools/build_idf6_hosted3.sh and update through OTA. Existing valid saved themes should remain selected. New settings or missing/invalid theme data now choose Studio Dark. To use Studio on an already configured unit, choose it in Appearance; do not erase settings just to test the default. Reboot and verify your explicit selection persists.

During consistency passes, review every page against its own theme's colors, brightness, dividers, fonts, padding, icons and feedback. Repeat large text/density and relevant dialog/empty/offline states. Theme-specific layouts remain deliberate; shared items should use theme roles rather than page-local lookalikes. The source change centralizes the default and preserves existing theme IDs and preferences; it does not claim that every page has completed visual review.

Run theme_default_test.py with LVGL sources for settings/dispatcher coverage, and live_appearance_test.py for existing all-theme appearance transitions. The NVS host fixture tests persistence logic; hardware reboot/OTA acceptance remains local.

## Temporary two-theme chooser acceptance

Apply apply_two_theme_picker_scope.py after Studio default, build with ./tools/build_idf6_hosted3.sh and update through OTA. Appearance should show only Operator and Studio Dark, with no Custom themes entry. Select each, close/reopen the chooser and reboot as convenient to verify the saved choice. Studio remains the default for new/missing/invalid settings; existing valid saved choices are preserved. No theme code, custom files or saved IDs are removed.

Check both visible previews, selection feedback, Close, large text/density and narrow layouts. Current visual work focuses on these two themes. The theme-dialog host matrix checks visible entries and ID routing plus existing switch/layout/cleanup cases. Theme-default tests retain all saved-theme compatibility coverage, including hidden choices.

## Calibration visual acceptance — Studio Dark and Operator

Run `tools/audit/calibration_dialog_layout_test.py --variant PAGE` with the usual
`--lvgl-dir` / `--lvgl-lib` host arguments. This exercises the actual landing page
and its child owners in both currently visible themes, all densities and text
sizes, with discovery pending, limited capabilities and all capabilities. It
checks visible bounds, overlapping labels/actions, summary clearance, 44px action
height and flat Studio sections. `CALIBRATION_SCREENSHOTS` captures both text
sizes at native resolution and comfortable density. Existing dialog variants
retain five-theme regression coverage.

Build with `./tools/build_idf6_hosted3.sh`, update by OTA, then inspect Calibration
in Studio Dark and Operator. Studio should have shared center/horizontal rules
and no cards. Operator should retain its own card treatment with more space
above the controls. Check Motion row alignment, long capability summaries and
large text. Open and cancel available dialogs; no heating, motion or calibration
command is required for this visual check. Recheck the release size budget.

## Devices and Macros visual follow-up

Run `devices_text_layout_test.py` and `responsive_layout_test.py --variant MACROS
--variant TOOLS` under `tools/audit` with the usual host LVGL arguments. Devices
checks actual header controls for overlap and pointer routing, shared flat Studio
entry styling, six complete native entries, hidden vertical scrollbar, scroll and
pagination reach, complete value labels, and stale/offline/lifetime behavior.
Set `DEVICES_SCREENSHOTS` for Devices renders; `RESPONSIVE_SCREENSHOTS` captures
Macros/Tools fixtures. Fixtures use synthetic printer and macro data.

After build and OTA, review Devices and Macros in Studio Dark and Operator,
including large text. Confirm Studio entries are flat with consistent dividers,
Operator retains its styling, long names/readings stay legible, and category,
page and touch-scroll navigation work. Verify Macro Search/Clear and favorite
markers; open and cancel parameter review without running a command. Recheck the
firmware release-size budget before publishing.

## Devices category buttons and shared active filters

Run the Devices text-layout audit and responsive Files/Macros variants. Check
all eight category buttons, including zero-count categories, selected state,
disabled Previous, Next/Previous page changes and near-bottom-edge taps on the
Studio rail. Selected controls must use the current theme's UI_CONTROL fill
and shared accent underline/border. The eight 44px Studio targets remain
disjoint. After OTA, check these controls in Studio Dark and Operator, then
verify active Search styling in Files and Macros. No printer command is needed.

## Console visual consistency

Run `tools/audit/console_modern_layout_test.py` with the usual LVGL host paths.
Its native constructors cover supported Studio Dark/Operator plus legacy themes,
all densities/text sizes, high contrast/reduced motion and narrow viewports.
Check flat Studio history styling/shared dividers, button label bounds, shared
checked Search/temperature-filter state, Reset, history scrolling/Follow and
existing editor/send/lifetime regressions. `CONSOLE_SCREENSHOTS` exports fixture
renders at native resolution and large text.

After build and OTA, review Console in both visible themes, including large text.
Scroll history, toggle Follow and temperature visibility, apply/cancel Search,
and Reset filters. Studio history should be flat; Operator keeps its themed
history card. Confirm active filters use theme colors and labels remain readable.
Open and cancel the command editor; no printer command is required for visual
acceptance. Monitor the firmware release-size budget before publishing.

## Drybox consistency and banner padding

Run `studio_pages_test.py`, `live_widget_refresh_test.py` and
`command_surfaces_safety_test.py` under `tools/audit`, with the usual LVGL host
paths. Drybox page checks cover Studio Dark and Operator at every density/text
size, complete reading-label widths, actual child bounds and offline controls.
`STUDIO_SCREENSHOTS` now also emits `drybox-operator-native.ppm`. Run the
responsive NETWORK variant after changes to the shared simple-banner layout.

After build and OTA, inspect Drybox in both themes, including large text. Confirm
matching Studio reading dividers/title styling, complete heater/fan readings,
Operator banner content inside its shell, and unchanged program-control layout.
Review active/offline states when available; no heating command is required for
visual acceptance. Confirm Network's banner remains legible. Check release size
before publishing.

## Drybox status text size follow-up

After OTA, confirm the Studio Drybox status at upper right is at least as large
as the Drybox title, including Heating/Monitoring. Check both text sizes and
longer Unavailable/Offline states when available. Status colors and control
eligibility must retain their existing behavior.

## Settings title and action-label consistency

Run `button_text_fit_test.py` for actual Settings action-row labels in every
theme/density/text size and `display_settings_safety_test.py` for existing display
callbacks and standalone compilation. After OTA, inspect Settings in Studio Dark
and Operator: title size, section/action labels, divider appearance and scrolling
to the bottom. Open/cancel normal settings dialogs; no reset, reboot or firmware
operation is required for visual acceptance. Studio keeps flat rows and Operator
keeps its own section styling.

## Settings scrollbar and Network visual pass

After build and OTA, confirm Settings has no side scrollbar and still scrolls
through all sections by touch. Review Network in Studio Dark and Operator with
both text sizes: shared title/column hierarchy, matching divider colors, complete
SSID/signal labels and 48px scan targets. Studio result entries should be flat;
Operator retains its own button treatment. Check scan/Manage printers routing
without changing credentials. Run the responsive NETWORK audit for five-theme
scan-row bounds, action routing, scan-result ownership and empty-state teardown.

## Printer visual consistency

Run `button_text_fit_test.py`, the PRINTER fixture in `studio_pages_test.py`,
and `printer_direct_action_safety_test.py` with the usual host LVGL arguments.
The page-body fixture now covers Studio and Operator controls/readings/preview,
including all densities/text sizes; `STUDIO_SCREENSHOTS` adds
`printer-operator-body-native.ppm`. The font-fit audit checks complete labels,
minimum reading size, icon/label separation, row bounds and stable pressed frames.

After build and OTA, inspect Printer in both themes, including large text:
Toolhead/Home/Pause/Resume/Object/Stop labels and spacing, reading-card headings,
active fan contrast, the shared preview divider and the empty preview wording.
Confirm the accepted preview/temperature arrangement remains familiar. Open and
cancel Toolhead/temperature/tuning dialogs; no homing, heating or print command
is required for visual review. Check release size before publishing.

### Dashboard visual consistency

Run `studio_layout_test.py`, `button_text_fit_test.py`, `dashboard_camera_safety_test.py`, `active_preview_safety_test.py`, `dashboard_offline_test.py` and `live_widget_refresh_test.py` under `tools/audit`, supplying the host LVGL source/archive arguments when unmanaged. Native Dashboard renders cover Studio Dark and Operator at all densities and both text sizes. Check Operator timing lines fit fully below the preview and banner children remain inside the shell after status refresh.

After the IDF build, update through OTA. Review Dashboard in Studio Dark and Operator, including large text: empty model preview, Camera/Thumbnail captions, reading headings, Operator timing/footer and banner alignment. Open/close a loaded preview and navigate away/back. Camera streaming needs a configured camera and remains a separate hardware check. Visual review needs no print, heat, motion or cancellation command. Keep the 3 MiB release budget in view; this patch adds no assets. Panel acceptance remains pending until confirmed.

### Toolhead dialog visual consistency

Run `tools/audit/toolhead_dialog_safety_test.py` and `tools/audit/button_text_fit_test.py` with host LVGL source/archive arguments when unmanaged. The Toolhead suite checks complete button labels and centering, 48px minimum touch height, active panel/checked tab synchronization, no scrolling at native 1024x600, smaller-view touch fallback, pinned footer, exact commands, rejected-send retries, duplicate suppression and cleanup/printer-switch guards across themes/densities/text sizes.

After building and OTA, open Printer > Toolhead control in Studio Dark and Operator, including large text. Review Move, Filament and Z offset; all controls should fit without scrolling at native resolution, with a stationary Close button. Check flat Studio sections, shared selection outlines, readable labels and no side scrollbar. Switching tabs and selecting step sizes is sufficient for visual review; do not jog, home, extrude, retract, reset or change the actual offset for this check. No print command is needed. Retain the 3 MiB release budget; no new fonts or image assets are included.

### Thermal control dialog visual consistency

Run `tools/audit/hotend_layout_test.py`, `tools/audit/button_text_fit_test.py` and `tools/audit/printer_direct_action_safety_test.py` with the host LVGL source/archive arguments when unmanaged. Hotend fixtures cover normal/large text, all densities, all compatibility themes and 1024/640/480px widths, live no-op updates, named-heater selection, reordering/removal, active-print temperature/fan dispatch and teardown. Preset coverage includes the selected 60 C bed state and button/label bounds.

After building and OTA, review nozzle, bed, part-fan and multi-hotend dialogs in Studio Dark and Operator, including large text. Check current/target reading sections, selected preset outline, complete labels, pinned Back/Close, chooser scrolling and custom-temperature keyboard open/cancel. Studio should have flat sections and shared divider colors; Operator retains its section styling. Close controls without sending temperature/fan/activation commands for this visual check. Retain the 3 MiB release budget; no new font/image assets are included. Panel acceptance remains pending until confirmed.

### Filament sensor and cancellation dialog visual consistency

Run `filament_sensor_safety_test.py`, `sensor_status_layout_test.py`, `cancel_layout_test.py`, `object_layout_test.py` and `button_text_fit_test.py` from `tools/audit` with host LVGL source/archive arguments when unmanaged. Coverage includes long sensor/object names, failure feedback, complete controls, pinned footer/scroll behavior, native and smaller viewports, exact sensor/cancel/exclusion commands, rejected retries, pending acknowledgement/timeout, no-op live refresh and printer/owner fencing.

After the IDF build and OTA, review Filament sensors in Studio Dark and Operator, including large text. Studio rows should be flat with shared divider colors, state/action captions readable and Close fixed while scrolling. Open and back out of the Stop print and Cancel object confirmations only when an existing active job makes them available; do not start a print merely for this visual check. No sensor toggles, print cancellation or object exclusion are required. Confirm target names and warning text remain explicit. Retain the 3 MiB release budget; no new fonts/images included. Panel acceptance remains pending until confirmed.

### Filament recovery visual consistency

Run `tools/audit/filament_recovery_test.py` and `tools/audit/button_text_fit_test.py` with host LVGL source/archive arguments when unmanaged. Recovery coverage checks all actions, exact commands, selected nozzle target and limits, macro availability, paused/homed/hot/sensor gates, missing/unknown/truncated sensors, rejected retries, cooldown across reopen, reentrant callbacks, printer switching and field/root teardown. Layout coverage includes all compatibility themes, densities/text sizes, 1024/640/480px widths, native no-scroll fit and body-font minimum for action labels, touch fallback and pinned Close.

After the IDF build and OTA, review Filament recovery in Studio Dark and Operator, including large text, when the dialog is already available. Confirm the larger selected target, shared divider, complete labels, wider Resume, no side scrollbar and fixed Close. Changing the selected target alone sends no heater command; close without Heat/Unload/Load/Purge/Resume for a visual-only check. Do not create a runout or pause an active job solely for this review. Keep the 3 MiB release budget in view; no new fonts/images are included. Panel acceptance remains pending until confirmed.

### Calibration and diagnostics visual batch

Run `calibration_dialog_layout_test.py` without a variant restriction, plus `motion_layout_test.py`, `bed_mesh_profiles_layout_test.py`, `diagnostics_batch_safety_test.py` and `button_text_fit_test.py` under `tools/audit`, with host LVGL source/archive arguments when unmanaged. The full calibration suite checks PAGE, FEEDBACK, OTAFAILURE, PID, MANUAL, MOTION, PROBE, ENDSTOP, RESULTS, PA, CUSTOM and GEOMETRY and standalone translation units. Other suites cover calculator values/retained edits and keyboard bounds, live/offline diagnostics, exact bed-mesh commands, rejected retry, captured names, deletion/replacement and printer/owner fencing.

After the IDF build and OTA, review Motion limits, TMC drivers, Axis distance calculator, Probe accuracy, Live endstops and Bed-mesh profiles in Studio Dark and Operator, including large text. Check headings/actions, shared footer rules, complete labels, fixed Close/Back and touch scrolling without side scrollbars. Calculator editing/Calculate is local. Probe and bed-mesh confirmation/keyboard dialogs can be opened and cancelled when available; no probe run, profile load/save/removal, restart, heater or motion command is required for this visual review. Retain the 3 MiB release budget; this batch adds no fonts/images. Panel acceptance remains pending until confirmed.

### Monitoring visual batch

Run `telemetry_modern_layout_test.py`, `event_history_layout_test.py` and `button_text_fit_test.py` from `tools/audit` with host LVGL source/archive arguments when unmanaged. The telemetry fixture checks a duplicate-free, complete eleven-channel inventory, three visible Heat plots versus four Motion/Environment plots, synchronized reusable chart buffers, selected tab/Hold states and 48px tab height along with three view layouts, touches, ranges, held traces, named hotend identity, capability gaps, offline/reset, no-op refresh and teardown. History tests retain complete rows and pinned actions, Hold/Follow, ring replacement, clear and reentrant deletion/replacement behavior.

After building and OTA, review Telemetry Heat, Motion and Environment plus Event history in Studio Dark and Operator, including large text. Check that Drybox air appears only under Environment, with the three Heat plots and all four plots in each other view retained. Check flat Studio sections/shared divider color, selected tabs, target/held outlines, readable action labels and touch scrolling without side scrollbars. Toggle Hold/Resume live and history Hold/Follow to verify the displayed state; these are local display controls. Clear removes displayed local event history, so leave it alone during a visual-only review. No printer/heat/motion command is needed. Retain the 3 MiB release budget; no new fonts/images added. Panel acceptance remains pending until confirmed.

The graph consolidation also passed the telemetry host suite with AddressSanitizer and UndefinedBehaviorSanitizer (leak checks disabled for the host runtime). Visible traces remain complete under sampling jitter; the unused Heat slot stays hidden/empty and reusable chart buffers track range changes.

### Graph tab grouping follow-up

Run `tools/audit/telemetry_modern_layout_test.py` with the host LVGL arguments. The maintained inventory assertions require all eleven channels exactly once, printer part fan under Thermal and all four drybox measurements under Drybox. Per-view checks verify the hotend selector only in Thermal and the target control in Thermal/Drybox, together with visible chart counts, reusable point-buffer sizes, selected-state/touch behavior, held/range traces, jitter bins, missing capabilities, no-op refresh and teardown.

After build and OTA, check Thermal (nozzle, bed, part fan), Motion (velocity, volumetric flow, speed/flow overrides) and Drybox (center, air, humidity, drybox fan) in both visible themes, including large text. Scroll to inspect lower plots. Verify Targets on/Detail only works in Drybox as well as Thermal. These are local graph controls; no printer command is needed. Panel acceptance remains pending until confirmed.

### Calibration workflow label consistency

Run `tools/audit/calibration_dialog_layout_test.py` without a variant restriction and `tools/audit/button_text_fit_test.py` with host LVGL source/archive arguments when unmanaged. Existing tests cover all workflow routes, exact commands, rejected retries, captured owners, duplicate/reentrant actions, external deletion, session replacement, SAVE_CONFIG acceptance/confirmation and printer changes, alongside layout/scroll/pinned-footer checks in all themes/densities/text sizes/viewports.

After the IDF build and OTA, review PID heater/target/warning screens, pressure-advance setup, manual-probe/axis-twist controls, screws tilt/gantry confirmation and calibration results/save confirmation when available in Studio Dark and Operator, including large text. Open/back out without starting heat, motion, probing, tower setup, acceptance or SAVE_CONFIG. Do not create a calibration session only to expose later result screens; leave those for the next legitimate workflow. Check readable sentence-case headings/actions and unchanged explicit safety warnings. No new fonts/images included; retain the 3 MiB release budget. Panel acceptance remains pending until confirmed.

### Settings arrangement and dialog visual batch

Run `settings_dialog_layout_test.py`, `theme_dialog_layout_test.py`, `live_appearance_test.py`, `network_backup_dialog_test.py`, `about_dialog_layout_test.py`, `display_settings_safety_test.py` and `button_text_fit_test.py` under `tools/audit`, using the host LVGL source/archive arguments when unmanaged. Preserve fault/owner coverage for brightness/sleep, no-op/deferred theme selection, invalid port/password handling, restore review, encrypted-worker overlap, failure retries and deletion/replacement.

After build and OTA, review Settings in Studio Dark and Operator, including large text. Confirm Display & appearance is first, sleep follows brightness, Notifications & history is next, and Maintenance is lower down. Scroll to check every group and retained values. Open appearance, Wi-Fi/port, About and backup dialogs; check flat Studio sections, shared divider color, complete labels, keyboard/pinned footer bounds and touch scrolling without side scrollbars. Backup creation must occupy the first grid row, restore the second and removal its own final row. Open/cancel review dialogs without resetting, rebooting, restarting Klipper, reconnecting, writing/restoring or deleting backups during a visual-only check. Retain the 3 MiB release budget; no new font/image assets included. Panel acceptance remains pending until confirmed.


### OTA responsiveness panel check

Build with tools/build_idf6_hosted3.sh and install through the existing OTA path. In Studio Dark and Operator, open Firmware update with Camera inactive and streaming if available: the dialog should appear without waiting for camera teardown. Initial Stable/Nightly metadata still requires a network fetch; switch channels repeatedly, close/reopen and verify loaded releases reappear without a fresh request. Use Refresh to fetch newer releases, including retry after an error. Press Custom URL during loading and after results appear; the editor/keyboard should open on the next UI turn, retain the saved URL and return safely to Releases. Repeated taps must not duplicate dialogs or downloads.

Cancel an actual transfer after image bytes begin advancing, including a deliberately paused server if available. Expect Cancelling feedback immediately and worker cleanup after the active body read returns (one-second read timeout, not a hard end-to-end latency guarantee). Confirm no reboot, unchanged running firmware and that another update can start. Connection/TLS/redirect and initial image-header reads may still take longer; do not infer these are interruptible from the body-read test. Final validation/boot selection remains non-cancellable. Also test a short network stall that recovers and an established transfer with no byte progress for 15 seconds, which should fail cleanly. Keep power connected. Record build firmware size against the 3 MiB release budget. Host tests cannot establish device responsiveness or network timing.

Host validation: tools/audit/ota_transfer_test.py; calibration_dialog_layout_test.py --variant OTAFAILURE; camera_pipeline_test.py. These use actual project C with deterministic SDK/network stubs and actual LVGL for UI checks; no firmware is fetched or flashed by the host tests.


### OTA progress smoothness follow-up

Apply after apply_ota_responsiveness.py. During a known-length firmware download in Studio Dark and Operator, check that the bar moves between whole-number percentage changes and frequent updates do not cause jumps. Pause the server and confirm the bar stops at reported progress rather than advancing on its own. Verify integer percentage/bytes remain readable, validation stays below 100% until success, cancellation retains immediate feedback, and failure/reset/reopening clears the old value/animation. Unknown-length transfers retain stage-based progress. This changes presentation only, not download throughput. Host OTAFAILURE checks cover interpolation, repeated snapshots and animation cleanup; build, firmware size and OTA panel acceptance remain required.


### About credits and AI acknowledgment

In Studio Dark and Operator, open About from Settings, including large text. Verify installed firmware/version/source/build identity; scroll through Created by khelix, AI-assisted development, open-source foundations, supporting libraries, fonts/artwork, printer ecosystem, development tools and source/notices. Ensure all text is reachable, Studio stays flat with shared rules, Operator keeps its surfaces, and Close remains pinned/tappable throughout. Close/reopen repeatedly and confirm no old dialog remains. Documentation/license files do not add runtime assets, but check built firmware size against the 3 MiB release budget as usual.

Host validation: about_dialog_layout_test.py covers real LVGL layout, full credit presence, scrolling, pointer Close, delete/reopen/reentrancy and separate translation-unit compilation for all three firmware channels and five compatibility themes/densities/text sizes/viewport widths. software_credits_test.py verifies all locked components have entries and local license/notice links resolve with original author notices. It does not establish complete linked-binary license compliance or replace panel verification.


### OTA visual consistency panel check

Review Firmware releases, Stable/Nightly, release details, Custom URL with keyboard, progress and update-not-started feedback in Studio Dark and Operator, including large text. Studio release rows should be flat with the same divider color as other shared sections; Operator keeps its row surfaces. Check readable selected tabs, complete sentence-case action labels, a shared divider above each pinned footer, touch scrolling without side bars, and keyboard/action separation. Scroll release notes to the end; selected version, nightly/reinstall/downgrade warnings and retained settings guidance must remain available. Check normal/failed/cancelled progress and safe close/reopen. Use an intended firmware image only for the actual OTA check; host fixtures exercise failure/install callbacks without downloading firmware.

Host validation: calibration_dialog_layout_test.py --variant OTAFAILURE, with standalone translation-unit compilation and all prior lifetime, async, cache, smooth-progress and cancel assertions. Native captures settle checked-tab transitions before rendering; they are project-source LVGL fixtures, not panel screenshots. No asset/font additions; verify the firmware size against the 3 MiB release budget after building.


### System information readability panel check

In Studio Dark and Operator, including large text, scroll Settings to System information. Confirm Uptime appears first, Internal RAM and PSRAM have individually labelled Free now/Lowest readings aligned in columns, Internal RAM additionally shows Largest block, and ESP-IDF version appears last. Ensure values/captions remain readable within their rows and shared divider/surface styling matches surrounding Settings. Lowest is the lowest free memory since startup, not used memory; Largest block is the largest contiguous internal allocation. KiB/MiB represent 1024-based units. Values should refresh quietly while idle, remain correct after navigation/theme changes and leave no stale labels after reopening.

Host validation: telemetry_modern_layout_test.py now links the actual shared Settings metric rows and system-info owner. It checks separate values/units/zero, native column alignment, label bounds/width, no invalidations for unchanged snapshots, partial deletion, SDK-read reentrancy and unbinding alongside the existing graph suite. An instrumented LVGL archive and AddressSanitizer/UndefinedBehaviorSanitizer run exercise the same fixture (leak detection disabled). display_settings_safety_test.py and settings_dialog_layout_test.py cover unchanged brightness/sleep, reboot/reset guards and dialog lifetimes. Native full Settings page QA uses mocked SDK values; actual panel verification and the 3 MiB build-size check remain separate.


### Installer-style Setup and discovery panel check

In Studio Dark and Operator, including larger text, open Setup center and visit Welcome → Network → Time zone → Display & appearance → Printer → Camera → Review. Check the selected navigator state, center divider, pinned Continue/Finish and Back/Set up later controls, and direct step selection. Back on Welcome is disabled. Touch scrolling remains available without side bars; all four Display groups should be visible together at 1024×600.

Choose a time zone and verify the selected row, clock/event-history local time, and Settings > Time & region after closing Setup. Reopen Setup to confirm persistence. On Display, switch Studio Dark/Operator and standard/larger text; the wizard must return to Display and the shell/Settings behind it must match when closed. Existing contrast/transparency/motion preferences must survive text-size changes. Drag brightness and release to save; select a sleep timeout, then check the same values in Settings and after reopening/rebooting. Review must show the saved zone, theme, text size, brightness and timeout. Set up later retains choices already saved. Camera is optional; finishing still requires Wi-Fi plus a configured printer. No language selection is offered.

Scan Wi-Fi/cameras and discover Moonraker from printer setup. Results should fill the available width and remaining height; a long result list should scroll while actions remain pinned. Studio uses flat rows/shared rules; Operator keeps its own surfaces. Select a discovered endpoint, cancel/reopen and retry discovery to confirm no old results or overlays survive. Keyboard and password actions must remain separate and tappable.

Host checks: setup_center_layout_test.py exercises the actual Setup owner with mocked services across compatibility themes/densities/text/viewport widths, including missing catalog entries, save failure/retry, brightness release, sleep choice, preserved accessibility flags, deferred application rebuild, queue cancellation before persistence, deletion/reopen and existing Wi-Fi/camera/completion lifetime cases. discovery_states_test.py covers full native result bounds, many-result scrolling, fixed footer, real pointer selection at short viewports and request/worker lifetime fences. display_settings_safety_test.py additionally verifies Settings' time-zone label refresh alongside transactional display storage/SDK faults. live_appearance_test.py and profile_discovery_bridge_test.py cover shared appearance and profile discovery behavior. Native images are real LVGL host fixtures with simulated services, not panel captures. Build/OTA and the 3 MiB size check are still required.


### Setup Network/Printer connection summaries

In Studio Dark and Operator, including larger text, visit Setup > Network while connected. Check the actual SSID and display address against your router/Network page. Refresh status must update these facts without opening discovery or a password keyboard. Change network should open the existing scan/password flow. With Wi-Fi unavailable, expect No network connected/Not connected; while associated but awaiting DHCP, expect Waiting for an address. Back/Continue and Set up later stay pinned and usable.

With a saved profile, Setup > Printer should show the active printer name, HTTP/HTTPS endpoint and only an API-key-presence caption. Edit printer must open the active profile, including a profile other than slot 1. Use the existing editor to discover, test, cancel or save; returning to Setup must show the resulting active configuration. An unconfigured profile should offer Add printer and explain the shared editor's options. Review should say Connected for Wi-Fi and Saved for printer/camera configuration, without claiming a live printer response merely because a profile exists. Existing Finish requirements remain unchanged.

Host checks: setup_center_layout_test.py covers those summary states, address formatting, secure/plain endpoints, secret redaction, active-profile editor dispatch, native layout bounds and prior setup lifecycle/persistence cases across themes/densities/text/viewport widths. profile_discovery_bridge_test.py covers the retained request-bound main/profile bridge. Native source-based LVGL renders use simulated services, not panel captures. Check firmware size after building, then update over OTA and verify on the panel.


### Setup Camera summary and discovery states

In Studio Dark and Operator, including larger text, open Setup > Camera. With a saved camera, check its saved name (or Custom camera for a mismatched/unnamed default), printer and Discover and test action. Saved configuration is not described as a current successful stream test. Camera URLs/credentials should not appear in the summary. Continue should reach Review. Without a camera, check the optional-setup guidance; without a printer, Set up printer should navigate to the Printer step.

Discover cameras. Scanning... should be disabled until the request completes; repeated touches must not replace the owned request. With no enabled webcam, expect a visible No cameras found explanation, Scan again and Skip camera/Continue. Scan again should retry. With results, select a row and check Testing..., failed-stream retry instructions or verified dimensions with Use camera. A failed test must restore Scan again. Test a known working stream before Use camera; reopen the Camera step and check the resulting saved summary. Existing default/stream storage-failure retry feedback stays available. Switching printers while results are pending must retire old verification and offer discovery for the active printer.

Camera discovery already imports returned settings via the shared controller, including the slot-zero stream bridge. Continue navigates onward after those imports; it does not restore a pre-scan configuration. The Continue/Skip action itself must not initiate an additional camera save. Finish still requires only Wi-Fi and a saved printer profile. Close/reopen, navigate Back and return to Camera; footers remain pinned, full result lists touch-scroll without side bars, Studio stays flat and Operator keeps its surfaces.

Host checks: setup_center_layout_test.py exercises real LVGL Setup states with typed service fixtures, save/start/test failures, duplicate busy requests, default matching and credential redaction, catalog-read owner replacement, no additional save on Continue, field/poll retirement, footer/summary bounds and prior lifecycle/persistence checks. discovery_states_test.py exercises the actual camera discovery worker's captured-printer and late-result rules alongside Moonraker discovery. Native source-based fixtures simulate services; a physical camera and OTA/panel check are still needed for live stream acceptance. Verify the firmware against the 3 MiB release budget after building.


### Setup Welcome/Review/completion closeout

In Studio Dark and Operator, including larger text, open Setup center with saved configuration and without it. Welcome should show the connected SSID and active printer name at 1024×600, status at the right, and an optional camera. Long names must stay on one line with truncation in these compact rows; use Network/Printer to inspect the full value. Small viewports show the compact title/status layout. Back remains disabled on Welcome, Continue advances, and selecting a navigator step still works.

Review should show the actual network/profile, camera saved/optional status, current time zone and a readable one-line native display summary containing theme, text size, brightness and sleep. All rows/summary values should fit the native content area with Finish pinned. The text should explain that choices save as you go and Finish marks setup complete. Finishing with missing essentials must still show feedback; failed completion storage must retain Review and allow retry.

After a successful Finish, expect Setup complete, saved-choice guidance and a reference to the Printer page for live status. Done must remain pinned/tappable; close/reopen Setup and confirm saved values persist. The completion message should not imply a live verified printer connection merely because its profile is configured. Existing camera discovery imports and verified-save behavior remain as documented in the preceding check.

Host validation: setup_center_layout_test.py adds actual summary value assertions, native Welcome/Review bounds, single-line compact row cells/display summary, completion action/label bounds and close/reopen during review's network read. It retains all prior setup owner, failed save, missing-essential, camera, display/theme and discovery bridge fixtures. Instrumented LVGL can exercise the same suite under AddressSanitizer/UndefinedBehaviorSanitizer with leak detection disabled. Native captures simulate SDK/services and are not physical panel screenshots. Build with the normal script, check the 3 MiB release budget, then OTA and verify on the panel.


### Files, Camera and Network cross-page consistency

In Studio Dark and Operator, including larger text, check Files loading, no search results, empty folder/library, offline and request-error feedback. Headings should be sentence case with clear guidance and the existing shared icon/state treatment. Loading spinner colors should follow the active theme accent/divider palette. Browse and scroll long lists without a side scrollbar. Open a preview, scroll metadata to the end, and confirm the footer remains pinned. Operator uses File preview/Print details and sentence-case Cancel/Start print; both the confirmation and inspector/expanded-metadata footers use the shared rule/spacing. Studio's inline preview should retain its existing maximum size for the metadata shown. Visual verification does not require starting a print.

On Network, check connected and offline states, Scan networks/Manage printers, long SSIDs/hosts and populated/empty scan results. Values should use Connected/Disconnected/Offline/Wi-Fi with retained SSID/IP/HTTP acronyms. An empty scan gives Scan networks/range guidance. Touch scrolling remains usable without a side bar. Studio has a flat banner rule matching its section dividers; Operator retains its surfaces. Selecting a result still opens the existing connection flow.

On Camera, check unconfigured/connecting/unavailable feedback and the 48px Retry action. Open View and Select camera: footer dividers/spacing should match other forms, Close/Reset stay pinned, content scrolls without a side bar, and native picker names use normal body text rather than an unnecessarily small fitted font. Selected camera/rotation/mirror choices use the shared checked treatment. Studio camera rows remain flat; Operator keeps its surfaces. Check empty selection feedback, cancel/close/reopen, retry and fullscreen exit. Live frames require a physical configured camera and remain a separate panel acceptance check.

Host validation: responsive_layout_test.py --variant FILES --variant NETWORK retains print-confirmation guards, captured callbacks, metadata drag/reach, preview fit, page lifetimes, scroll damage and theme/profile geometry checks, and adds page-state/empty-network/hidden-sidebar fixtures. camera_pipeline_test.py retains catalog-save failure/retry, frame/cache retirement, stale/reentrant/owner fences, view transforms, fullscreen and timer cleanup, with native picker font and form body/footer checks. Native renders are actual LVGL source fixtures using simulated service data. Build normally, verify the 7 MiB CI limit, update through OTA and check both exposed themes on the panel.


### Devices, Macros and Calibration consistency panel check

After the guarded patch and local hosted build, install through OTA and check Studio Dark and Operator:

- Devices: swipe/select categories, including categories offscreen in Operator; confirm the strip has no scrollbar and readings remain legible.
- Macros: open Search, then parameter review; confirm body scrolling, keyboard fit and the shared divider above pinned Cancel/Done or review actions. Cancel safely without running a printer macro.
- Calibration: inspect Input Shaper, resonance-axis selection and custom-macro dialogs; check sentence-case titles, readable axis labels and pinned footer spacing. Close without starting motion.

Host validation: devices_text_layout_test.py, responsive_layout_test.py --variant MACROS, and every calibration_dialog_layout_test.py variant, using LVGL 9.5.0. Existing command-flow, owner guards and teardown assertions remain active. Firmware partition/budget remains 7 MiB per OTA slot.


### Theme consistency retrospective verification

Apply apply_theme_consistency_backpass_v2.py after the accepted Devices/Macros/Calibration patch. Build with tools/build_idf6_hosted3.sh, install through OTA, then sweep Studio Dark and Operator. The firmware budget remains 7 MiB per OTA slot.

Check the shared changes first:

- Compare Tools, Files and the printer chooser headings with Devices/Calibration. Confirm Files retains its preview size and now shows more of its list.
- Inspect time zones, appearance choices and bed-mesh profiles: Studio lists are flat and divided; Operator keeps its own surfaces. Selection must remain clear.
- Open profile, toolhead, temperature, hotend, filament-sensor, object and stop dialogs. Confirm the footer divider, fixed actions and scroll reach at default and larger text. Close without sending unnecessary printer commands.
- Console Search and Send command: keyboard remains usable; actions stay below it. Check history navigation and Cancel/Close without sending a command.
- Notifications: explanation and three levels are readable, selection uses the active theme color, and Cancel/Save remain pinned. Existing saved preferences and fault policy must persist.
- Compare graph and empty-camera backgrounds with the other Studio pages. Check graph Hold/Resume live and camera controls.
- Finish with Dashboard, Printer, Files, Camera, Tools/subpages, Console, Drybox, Settings/Setup and their dialogs in both themes. Check empty/offline states where practical.

Host suite sweep: studio_layout_test.py, studio_pages_test.py, responsive_layout_test.py, chooser_refresh_test.py, devices_text_layout_test.py, profiles_layout_test.py, setup_center_layout_test.py, settings_dialog_layout_test.py, theme_dialog_layout_test.py, network_backup_dialog_test.py, about_dialog_layout_test.py, control_popups_layout_test.py, motion_layout_test.py, hotend_layout_test.py, cancel_layout_test.py, object_layout_test.py, sensor_status_layout_test.py, bed_mesh_profiles_layout_test.py, event_history_layout_test.py, calibration_dialog_layout_test.py, camera_pipeline_test.py, filament_recovery_test.py, notifications_test.py, recovery_dialog_safety_test.py, live_appearance_test.py, display_settings_safety_test.py, shared_popup_safety_test.py, diagnostics_batch_safety_test.py, console_modern_layout_test.py, telemetry_modern_layout_test.py and command_surfaces_safety_test.py.

Maintained fixtures locate Console keyboards by LVGL class after reordering; existing Printer Z-offset captions and the motion-fault assertion follow current display casing. Command, ownership, retry, scrolling and teardown assertions remain active. Representative LVGL renders use simulated printer data. No ESP-IDF firmware build, OTA transfer or physical-panel verification was performed here.


### Operator Tools, printer chooser and Macros panel check

After apply_operator_tools_chooser_macros.py, build locally and update through OTA. In Operator:

- Tools: verify the banner and four cards, then open each destination. Check default and larger text.
- Printer chooser: check configured and empty slots, card alignment, active cyan outline, preview/status fit and Manage printers. Switching the active printer should not move text inside a card.
- Macros: check the panel header, search/clear, long names, favorites, scrolling and parameter/review/Cancel. No macro needs to run for this visual check.

Switch to Studio and confirm its flat sections and dividers still look correct. Host verification covers responsive Tools/Macros and chooser layout, routing, refresh and ownership checks. Real ESP-IDF build and physical OTA verification were not run here.

### Operator Tools-family header validation

For `apply_operator_tools_headers.py`, host LVGL checks passed for Tools and Macros responsive layouts, Devices layouts, Calibration PAGE layouts and separate translation units, Telemetry layouts/lifecycle/quiet refresh, and Studio pages including Operator Bed Mesh. Coverage includes theme/density/text-size and viewport combinations supported by each fixture. Native simulated LVGL renders were reviewed for Operator and Studio; these are not physical panel screenshots. Calibration retains 44px actions and at least 12px separation between summaries and actions.

After local build and OTA, check matching Operator header alignment, typography and banner styling across Tools, Calibration, Devices, Macros, Bed Mesh and Telemetry. Check large text, live telemetry status and Devices navigation, macro filtering, mesh gestures and controls, and Studio presentation. Firmware uses 7 MiB OTA partitions.

### Full two-theme audit before further visual work

The full host audit passed 42 audit entry points on the cumulative source after `apply_operator_tools_headers.py`, with the notification appearance fix included. This includes batch wrappers that also exercise constituent suites. Coverage spans Dashboard/shell, Printer controls, Files, Camera, Tools/Calibration/Devices/Macros/Bed Mesh, Console, Drybox, Settings/appearance/time zone, Network/backup, setup/discovery, OTA dialogs, About, profiles/chooser, graphs, event history, notifications, shared popups, recovery and preview lifecycles. Fixtures cover Studio and Operator plus retained compatibility themes where supported, densities, large text and viewport variations.

A source scan covered all 86 `ui_*.c` implementations plus related shared headers and application theme-rebuild routing. Native LVGL fixture renders in both themes were reviewed for representative page, dialog, recovery and monitoring states. These are simulated host renders, not physical panel screenshots or proof of hardware rendering/performance.

The new notification regression checks repeated Operator/Studio switches and density/text changes, verifies the current surface/font/colors, byte-identical queued notices, unchanged event logging, retained navigation destinations, and optional-confirmation timer cleanup. A negative run with appearance refresh disabled reproduced the old palette failure. The notification translation unit also compiles with warnings treated as errors.

Two initial legacy checks had stale expectations: Devices/Drybox uppercase status strings and popup actions fixed at exactly 48px. They now check the current sentence-case strings, at least 48px action height, complete labels within buttons, popup bounds and action non-overlap; popup coverage expands from three to all five theme IDs. Both complete suites passed on rerun.

Audit entry points (all final results PASS):

- `responsive_layout_test.py`
- `calibration_dialog_layout_test.py`
- `studio_layout_test.py`
- `studio_pages_test.py`
- `devices_text_layout_test.py`
- `chooser_refresh_test.py`
- `profiles_layout_test.py`
- `setup_center_layout_test.py`
- `theme_dialog_layout_test.py`
- `settings_dialog_layout_test.py`
- `about_dialog_layout_test.py`
- `network_backup_dialog_test.py`
- `live_appearance_test.py`
- `theme_default_test.py`
- `button_text_fit_test.py`
- `control_popups_layout_test.py`
- `motion_layout_test.py`
- `hotend_layout_test.py`
- `cancel_layout_test.py`
- `object_layout_test.py`
- `sensor_status_layout_test.py`
- `console_modern_layout_test.py`
- `telemetry_modern_layout_test.py`
- `event_history_layout_test.py`
- `bed_mesh_profiles_layout_test.py`
- `notifications_test.py`
- `filament_recovery_test.py`
- `display_settings_safety_test.py`
- `shared_popup_safety_test.py`
- `recovery_dialog_safety_test.py`
- `settings_safety_batch_test.py`
- `diagnostics_batch_safety_test.py`
- `monitoring_safety_batch_test.py`
- `page_state_safety_test.py`
- `live_widget_refresh_test.py`
- `camera_pipeline_test.py`
- `active_preview_safety_test.py`
- `dashboard_camera_safety_test.py`
- `thumbnail_component_safety_test.py`
- `lvgl95_modernization_test.py`
- `touch_navigation_test.py`
- `offline_navigation_test.py`

After applying `apply_full_theme_audit_closeout.py`, build locally, update through OTA, and switch Studio → Operator → Studio with an existing notification visible. Check that its styling follows the active theme and its text/action target are preserved. Also verify the latest Tools-family headers and large-text layouts on the panel before continuing visual changes. Each OTA partition is 7 MiB.

### Tools content visual polish validation

For `apply_tools_content_visual_polish.py`, passed Devices text/layout and pointer routes, Calibration PAGE and separate translation-unit checks, Macros responsive/lifecycle layouts, shared button text fitting (6030 checks), all four LVGL modernization owner variants, and all six native Studio-page variants. The applicable suites include compatibility themes, three densities, both text sizes, viewport variations and custom metrics/profiles. Native simulated LVGL renders were reviewed for Calibration, Devices and Macros in Studio and Operator.

New assertions verify that all Operator categories fit their grid without horizontal scrolling or overlap, four device cards fit the native viewport, and filters/list/pagination remain separate. Checked-state selection is tested in both exposed themes with native pointer input. Macro rows retain complete wrapped names, left text alignment, separate icon bounds and responsive containment. Calibration purpose descriptions fit in full when tools are available, with the existing 12px separation from 44px actions retained. Shared-button regression checks preserve centered controls and pressed/focused frames.

After local build and OTA, check all eight Operator device filters and counts, selected feedback, list scrolling and pagination. Check macro long names, favorites, filtering and review in both themes; verify Calibration descriptions and capability-based actions. Existing headers and theme surfaces are retained. Each OTA partition is 7 MiB. Nothing is committed or pushed by the patch.

### Devices button-only pagination

`apply_devices_button_paging.py` passes the complete Devices layout/pointer/lifecycle suite across compatibility themes, densities and both text sizes, and the LVGL modernization Devices owner/binding regression. New checks verify that a drag cannot scroll either exposed theme's list, every rendered entry fits, all 96 fixture devices are reachable through native Next/Previous taps, first/last controls disable correctly, page size/ranges match the visible entries, and category changes retain their reset behavior. Updated binding checks use the active theme's page size and validate final partial pages and reused cards. Native simulated Operator/Studio renders were reviewed.

After build and OTA, check four entries per Operator page or six per Studio page, no device-list swiping, category selection, live values and Previous/Next. Apply after `apply_tools_content_visual_polish.py`. Firmware build and physical panel confirmation remain local; each OTA partition is 7 MiB.

### Readable twelve-entry Devices validation

For `apply_devices_readable_twelve.py`, passed the full Devices layout/pointer/lifecycle suite across compatibility themes, densities and text sizes and the LVGL modernization Devices owner/binding regression. Native Operator/Studio simulated renders were reviewed. New checks exercise actual vertical pointer scrolling, reachability of the twelfth entry, complete readings, all 96 fixture devices through eight pages, correct boundary button states, reset-to-top after page changes and retained category reset behavior. The existing header, checked-state, bounds, live/offline/profile and teardown checks remain.

After build and OTA, check two readable columns, twelve devices in each scrollable page, Previous/Next, and current filter/header styling in both themes. Macros remains vertically scrollable. The installer validates both known delivered baselines, repeat/no-op, documentation preservation and refusal of unexpected source edits. Each OTA partition is 7 MiB; nothing is committed or pushed.

### Bed Mesh and Telemetry polish validation

For `apply_mesh_graphs_visual_polish.py`, the native page variants pass, including the populated Bed Mesh fixture in Studio and Operator across densities/text sizes. The Bed Mesh-specific rerun verifies single-line summaries above the canvas, separate height figures with no drawing overlap, empty-state clearing and readout deletion. Native populated/empty mesh and thermal graph renders were reviewed in both exposed themes.

The new renderer check exercises a 333x200 RGB565 canvas (including aligned row stride), four zoom/pan combinations and prefix/suffix memory sentinels. The Bed Mesh variant also passes AddressSanitizer and UndefinedBehaviorSanitizer with application/renderer sources instrumented and the existing host LVGL archive linked. Leak detection is disabled for that run. The populated Studio render reproduced stripes before the renderer dimension fix and renders cleanly afterward. Hardware/PPA verification remains separate.

The full Telemetry layout/lifecycle suite passes across five theme IDs, densities, text sizes and viewport sizes, preserving range/hold, hotend identity, missing data, no-op refresh and ownership checks. Connection/control regression checks pass. Other native page variants passed before the Bed Mesh-specific fixture rerun.

After build and OTA, check a populated bed mesh, pan/zoom/reset/grid, profile changes and calibration confirmation in both themes. Check the profile summary and Min/Max/Range remain readable. Verify Telemetry captions, hold/resume and tab scrolling. Each OTA partition is 7 MiB; nothing is committed or pushed.

### Bed mesh viewport and fitted view

Full-width mesh canvases (Studio 976 x 278, Operator 800 x 304), compact footer controls, and one-line height metrics reclaim the former side rail. Studio profile status shares the title row. The renderer fits the default orbit and height walls to the actual viewport with padding; opening and Reset view use this fitted baseline, while manual zoom/pan/orbit remain available. Native fixtures cover both themes, density/text settings, reset image equality, and odd-stride raster bounds. Build and OTA panel confirmation remain required. Firmware partition budget: 7 MiB.

### Taller mesh and Motion diagnostics visual pass

Bed Mesh gains twenty pixels of canvas height in each exposed theme: Studio 976 x 298, Operator 800 x 324. Its Studio stage uses eight pixels of the existing gap above the navigation shelf; the compact title row and Operator banner reclaim the remaining height. Profile and height summaries, footer actions and navigation remain separate. Default fitting and Reset view use the actual taller canvas.

Motion limits now align readings to the right with shared theme-owned row dividers and a subdued configuration note. Limits, Drivers and Distance use a matching title divider. The Distance introduction is a caption, leadscrew labels use sentence case, and the numeric editor names the measurement being edited. Printer-switch status uses sentence case. Calculation, transport, selection, scrolling and owner/lifetime behavior are unchanged. Native layout/interaction checks cover themes, densities, large text, narrow views, driver transitions and keyboard/edit retention; the Bed Mesh fixture checks navigation clearance and fitted-reset equality. Local build and OTA verification remain required; firmware partition budget is 7 MiB.
