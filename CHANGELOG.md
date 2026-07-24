# Changelog

This file records product-level changes. Detailed development history before
v4.0.0 is preserved under `docs/history/`.

## [Unreleased]

### Documentation

- Replaced the inherited LVGL demo README with a PrinterHMI project README.
- Established authoritative architecture, build, hardware, configuration,
  test, release, security and troubleshooting documentation.
- Moved v3.x design and refactor records into a clearly historical area.

### Known work

- Remove embedded development credentials and rewrite affected Git history.
- Remove tracked mechanical backups and accidental root artifacts.
- Complete a repository-wide theme-token coverage audit.
- Continue investigation of intermittent splash-frame flashing.

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
ownership refactor. Its original plans, crash records and step-by-step logs are
retained verbatim under `docs/history/`.
