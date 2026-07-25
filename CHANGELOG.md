# Changelog

This file records product-level changes. Detailed development history before
v4.0.0 is preserved under `docs/history/`.

## [Unreleased]

No changes yet.

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
