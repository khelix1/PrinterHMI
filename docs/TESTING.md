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
