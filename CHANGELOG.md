# Changelog

This file records product-level changes. Detailed development history before
v4.0.0 is preserved under `docs/history/`.

## [Unreleased]

## [6.0.1] - 2026-08-02

### Added

- Added a reproducible full-stack builder for the ESP32-P4 application and
  matching ESP32-C6 ESP-Hosted 3.0.5 firmware.
- Added normal P4 and C6 full-flash images, component images, an internal
  manifest, flashing instructions and SHA-256 verification to one release
  archive.
- Added optional bootstrap installation of the exact ESP-IDF 6.0.2 tag.

### Changed

- Made ESP-IDF 6.0.2 the only active build toolchain for the current P4+C6
  firmware stack.
- Replaced the stable publisher's external transition-image dependency with
  the newly built and verified IDF6 stack archive.

### Validation

- Built both processors from the current repository workflow.
- Verified every packaged file and the outer archive against SHA-256 sums.
- Verified byte-for-byte placement of the P4 application at `0x20000` and
  the C6 application at `0x10000` in their full-flash images.


## [6.0.0] - 2026-08-01

### Changed

- Migrated the ESP32-P4 application to ESP-IDF 6.0.2.
- Upgraded the P4 host and ESP32-C6 co-processor to ESP-Hosted 3.0.5 with
  RPC v2.
- Upgraded ESP Wi-Fi Remote to 1.5.3.
- Established `tools/build_idf6_hosted3.sh` as the canonical build path.
- Restored the removable SD card on its native high-speed interface.

### Fixed

- Eliminated active-printer switching crashes and related OTA instability.
- Removed obsolete network settling and readiness delays while retaining
  WebSocket generation fencing and exclusive OTA ownership.
- Prevented release-catalog traffic from overlapping an active OTA download.
- Added the missing ESP-Hosted 3.0.5 host-side Wi-Fi RSSI RPC dispatch.
- Added accurate live Wi-Fi signal bars to the top status area.

### Build and migration

- Added guarded, tracked compatibility patches for ESP-IDF 6.0.2 SDIO and
  ESP-Hosted 3.0.5 RSSI dispatch.
- Added a verified C6 3.0.5 USB transition image for devices upgrading from
  the v5.1.2 C6 2.12.8 stack.
- Updated stable and nightly publishing to use the reproducible IDF6 build.

### Validation

- Verified matching ESP-Hosted 3.0.5 versions, RPC v2 and 40 MHz four-bit
  SDIO on the target panel.
- Verified repeated printer switching during prints, chooser state, OTA,
  SD-card mounting and live RSSI reporting.
- Verified reboot, OTA validation and normal Moonraker WebSocket operation.

## [5.1.2] - 2026-07-31

### Fixed

- Replaced the unstable ESP-Hosted 2.12.8 P4 host component with the verified
  2.9.3 host while retaining the installed ESP32-C6 2.12.8 firmware.
- Serialized short HTTP transactions and reserved exclusive network ownership
  for Wi-Fi scans and actual firmware downloads.
- Prevented the GitHub release catalog from blocking Custom URL OTA startup.
- Expanded the PSRAM-backed GitHub release response buffer from 128 KiB to
  256 KiB so the growing stable and nightly catalog is not discarded.
- Added visible release-catalog and OTA-start failure diagnostics.

### Validation

- Verified sustained WebSocket operation, inactive-printer health polling,
  chooser updates and repeated active-printer switching without an
  unrecoverable SDIO restart.
- Verified Devices, Stable OTA, Nightly OTA and Custom URL OTA workflows on
  the target panel.

## [5.1.1] - 2026-07-31

### Fixed

- Matched the ESP-Hosted P4 component and C6 firmware at 2.12.8.
- Stabilized Devices and OTA transitions by retaining one capability-aware
  WebSocket subscription and quiescing competing network activity before OTA.
- Preserved TX byte transfers with RX block transfers to avoid intermittent
  unrecoverable SDIO timeout resets.
- Added boot diagnostics for the ESP32-C6 co-processor firmware version.

### Build

- Added a guarded CMake hook that reapplies the tested ESP-Hosted transfer
  mode after managed-component regeneration.
- Kept the known-good ESP-IDF 5.4.4 and 40 MHz four-bit SDIO configuration.

### Validation

- Verified repeated Devices-to-OTA cycles, successful OTA completion and
  reboot with matching ESP-Hosted 2.12.8 host and co-processor versions.


## [5.1.0] - 2026-07-31

### Added

- Added a capability-aware Calibration workspace with guided PID, Input
  Shaper, Axis Twist, Z Tilt, Pressure Advance, Probe Z and custom workflows.
- Added shared manual-probe controls, calibration session tracking and
  confirmation before persistent SAVE_CONFIG operations.
- Added a capability-aware Devices catalog with category filters, pagination
  and synchronized live values.
- Added printer-action resolution and a repeatable v5 feature-architecture
  ownership audit.

### Changed

- Split Calibration into page composition, motion, Pressure Advance and
  manual-probe modules.
- Split Bed Mesh into page composition, gestures, rendering and profile
  controls.
- Split Devices into page composition, catalog view, live-value adapter and
  controller layers.
- Preserved the existing Console and Macros page/controller boundaries.

### Validation

- Verified incremental and final ESP-IDF 5.4.4 builds on the target firmware
  tree.
- Verified Calibration, Bed Mesh, Devices, Console and Macros on the target
  panel.
- Verified architecture, version, public-tree and firmware identity audits.

## [5.0.0] - 2026-07-28

### Added

- Added Bed Mesh as a dedicated sidebar page with a solid height-colored 3D
  surface, optional surface grid, rear X/Y/Z reference planes, origin markers
  and minimum, maximum and range statistics.
- Added drag rotation, responsive pinch zoom and two-finger graph panning.
- Added Bed Mesh calibration and detected profile management.
- Added a dedicated Console page with bounded command/response history,
  JSON-safe transport and response severity classification.
- Added automatic discovery of public Klipper macros with helper-macro
  filtering, execution confirmation and Console history integration.

### Changed

- Reordered the sidebar into Dashboard, Printer, Files, Bed Mesh, Macros,
  Console, Telemetry, Drybox, Network and Settings.
- Replaced the BED-card long-press entry point with a first-class Bed Mesh
  destination.
- Routed the new pages through shared themes, semantic buttons, modal popups,
  standard page geometry and existing Moonraker transport.
- Moved persistent Macros page and shell state into PSRAM-backed contexts
  allocated after scheduler startup.

### Fixed

- Corrected Bed Mesh pinch direction, sensitivity, redraw pacing and gesture
  competition.
- Corrected mesh origin placement, surface-line visibility and rear reference
  grid placement.
- Prevented overlapping profile-name copies during profile sorting.
- Protected FreeRTOS timer-task creation from the internal startup-RAM
  regression introduced by static Macros and sidebar state.

### Validation

- Added v5-specific build, startup, sidebar, Bed Mesh, Macros and Console
  release gates.

## [4.2.2] - 2026-07-26

### Changed

- Unified active-print filename presentation across the Dashboard and Printer
  page.
- Made `virtual_sdcard.progress` the primary live progress source, with
  `display_status.progress` retained as a compatibility fallback.
- Unified Remaining and ETA calculations under the shared printer controller.
- Made the top bar show the actual estimated completion clock instead of
  labeling the remaining-duration value as ETA.

### Fixed

- Corrected volumetric-flow presentation by treating retraction velocity as a
  magnitude and suppressing insignificant idle noise.
- Kept Dashboard, Printer page and top-bar timing values synchronized.
- Made completed jobs report `0:00` remaining while unknown timing remains
  `--:--`.


## [4.2.1] - 2026-07-25

### Added

- Added an on-device browser for stable releases and identity-aware nightly
  builds published by the official PrinterHMI GitHub repository.
- Added separate `STABLE` and `NIGHTLY` release channels with development-build
  warnings and exact installed-nightly detection.
- Added release status, publication date and time, firmware size and
  release-note details for each compatible OTA asset.
- Added explicit installation, downgrade and same-version reinstall
  confirmation paths.
- Added a secondary custom OTA URL path for local and development servers.

### Security

- Added trusted certificate-bundle verification to the GitHub Releases API
  request and HTTPS release-asset OTA download.
- Restricted stable entries to non-draft semantic-version releases containing
  the exact expected PrinterHMI OTA asset.
- Restricted nightly entries to prereleases with strict nightly tags, exact
  firmware assets and matching embedded-identity release metadata.

### Changed

- Made the official remote release browser the primary Settings OTA entry.
- Kept GitHub catalog parsing off the LVGL thread.
- Moved release-catalog buffers, task stack, mutex and selected-release state
  to PSRAM-backed runtime allocation to protect startup internal RAM.
- Increased OTA HTTP transmit buffering for GitHub signed-asset redirects.
- Updated nightly publishing so each firmware image embeds its exact nightly
  tag while restoring the stable version file after the build.

## [4.2.0] - 2026-07-25

### Added

- Added versioned custom-theme packages loaded from the SD card.
- Added custom-theme discovery, validation, preview, selection, persistence,
  configuration backup/restore and protected removal.
- Added per-page custom layout geometry, palette, radius and surface-opacity
  overrides with built-in theme fallbacks.

### Changed

- Upgraded LVGL from 9.2.2 to 9.5.0.
- Kept Foundry, Operator and Dark Glass compiled into firmware as protected
  fallback themes.

### Fixed

- Added missing standard I/O declarations found by the LVGL clean rebuild.

### Validation

- Verified a clean ESP-IDF 5.4.4 build with LVGL 9.5.0.
- Verified startup, navigation, built-in themes, popups, previews, telemetry,
  scrolling and display sleep/wake on target hardware.

## [4.1.0] - 2026-07-24

### Added

- Added capability-driven discovery and controls for as many as four hotends,
  including live temperatures, independent targets and active-tool selection.
- Added live Moonraker switch and motion filament-sensor status, with compact
  single-sensor state and detailed multi-sensor reporting.
- Added operator event history with chronological live updates and clear
  control.
- Added configuration backup and restore.
- Added Moonraker discovery directly to printer profile Add/Edit.
- Added an active-print layer fallback based on file metadata when the slicer
  does not publish `SET_PRINT_STATS_INFO`.
- Added nightly firmware and SHA-256 publication through the end-of-night
  checkpoint workflow.

### Changed

- Made printer controls reflect discovered Moonraker capabilities.
- Added auto-scaling telemetry ranges and removed unnecessary chart sliders.
- Hardened WebSocket profile rebinding, stale-client event handling and
  capability rediscovery.
- Expanded Dashboard status cards for filament state while preserving preview
  proportions.
- Enabled the two-framebuffer, avoid-tearing LVGL direct-mode display path.
- Separated the fast OTA UI timer from the general application refresh timer.
- Moved ESP-Hosted/C6 transport initialization ahead of visible display
  startup.
- Normalized OTA keyboard, editor and progress-popup transitions.

### Fixed

- Fixed delayed OTA keyboard and progress-popup closure.
- Fixed OTA cancellation responsiveness and final image validation handling.
- Fixed Add Printer host/IP and hostname text entry.
- Fixed active-printer connection failure and recovery states.
- Fixed live capability state after switching between Moonraker profiles.
- Reduced startup flashing to a single initial panel appearance.
- Removed obsolete UI compatibility symbols reported by the compiler.

### Validation

- Verified clean ESP-IDF 5.4.4 builds and target operation.
- Verified cold boot, warm reboot, power-cycle and display sleep/wake behavior.
- Verified multi-printer switching, Moonraker/Klipper restart recovery,
  previews, telemetry, layer reporting and printer controls.
- Verified configuration backup/restore and OTA update behavior.

## [4.0.0] - 2026-07-22

### Added

- Multi-printer profile management for up to four Moonraker endpoints.
- Runtime Classic, Operator and Dark Glass themes.
- Accent, density and accessibility appearance controls.
- Theme Lab and theme previews.
- Persistent timezone, brightness and display-sleep settings.
- Modal shared popups with normalized footers.
- Exclude-object visualization and selection.
- Files search, row thumbnails, long-press preview and metadata display.
- Printer-profile preview caching on SD storage.
- PrinterHMI logo assets and splash integration.

### Changed

- Normalized operator pages, status bars, page geometry and popup controls.
- Routed current UI styling through shared theme and component modules.
- Reduced routine serial output after startup stability testing.
- Moved suitable large buffers and preview data toward PSRAM-backed storage.

### Fixed

- Protected LVGL updates with the display lock during startup and profile
  identity changes.
- Corrected OTA first-boot validation and rollback cancellation.
- Corrected clock timezone handling after SNTP synchronization.
- Corrected several popup, file-preview and cancel-object build/runtime defects.

## Historical releases

The v3.0 through v3.3 transition was an iterative hardware bring-up and UI


## Development Update — Public Repository Hardening

### Public repository audit modularization

Extracted the embedded public-tree safety audit from `tools/end_of_night_checkpoint.sh` into the new reusable script:

* `tools/audit/public_tree_audit.sh`

`end_of_night_checkpoint.sh` now delegates repository validation to the standalone audit tool, making the audit reusable for future GitHub Actions workflows and manual validation while preserving existing checkpoint behavior.

### Compiler format-warning cleanup

Removed the global compiler suppression:

```cmake
add_compile_options(-Wno-format)
```

and replaced it with:

```cmake
add_compile_options(-Wformat=2)
```

Resolved every format warning originating from PrinterHMI-owned source code.

Changes included:

* Safe truncation handling for dashboard print filenames.
* Safer folder path construction in the Files controller.
* Increased buffer size for custom temperature popup initialization.
* Replaced unnecessary empty `snprintf()` usage with direct buffer clearing.

### Validation

Completed successfully:

* Clean project build.
* No remaining PrinterHMI-owned format warnings.
* Hardware smoke testing passed.
* Public repository safety audit continues to pass after modularization.

Remaining format warnings originate only from third-party components (ESP-IDF and LVGL) and are outside PrinterHMI source ownership.

### Project impact

This work improves overall code quality and prepares the project for future continuous integration by:

* Eliminating a project-wide compiler warning suppression.
* Making repository safety validation reusable.
* Reducing technical debt without changing runtime behavior.

ownership refactor. Its original plans, crash records and step-by-step logs are
retained verbatim under `docs/history/`.
