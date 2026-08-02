# Test plan

PrinterHMI changes are not complete when they compile. The target device is the
acceptance environment.

## Build gate

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
- Public macros appear alphabetically; underscore-prefixed helpers do not.
- Running a safe macro requires confirmation and is recorded in Console
  history.
- Known commands show normal responses, warnings remain amber and unknown
  commands render red.

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
