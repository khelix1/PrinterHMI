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
