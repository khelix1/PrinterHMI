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
