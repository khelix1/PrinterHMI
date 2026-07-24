# Known Issues

This document tracks the remaining issues affecting release readiness, portability, security, and long-term maintenance.

Completed work is intentionally removed from this list so it remains an accurate representation of the current project state.

---

# Release Blockers

## Splash screen flashing

**Status:** Open

The display may briefly flash during startup before the main UI is presented.

The issue is cosmetic and does not affect normal firmware operation, but it should be resolved before declaring the firmware production-ready.

Areas for investigation:

* LCD initialization sequence
* Backlight enable timing
* LVGL buffer initialization
* Display flush ordering
* Panel reset timing
* Wi-Fi startup interaction

---

# Security Limitations

## Local network transport

PrinterHMI communicates with Moonraker over HTTP and WebSocket on the local network.

This is appropriate for trusted LAN environments but does not provide encrypted transport.

## OTA trust model

Firmware updates use the ESP-IDF OTA framework but do not currently implement a project-specific firmware signing policy.

---

# Build & Portability

## Absolute paths in `dependencies.lock`

The generated dependency lock file currently contains machine-specific absolute paths.

This should be made portable before introducing automated CI or distributing the repository for general development.

## Continuous Integration

GitHub Actions (or an equivalent CI system) has not yet been implemented.

Planned automated validation includes:

* Public repository safety audit
* Clean ESP-IDF build
* Version consistency checks
* Documentation consistency checks
* Size regression monitoring
* Compiler warning validation

---

# Technical Debt

## Legacy `_v32` filenames

Several modules retain historical `_v32` suffixes.

This naming no longer reflects the project version and should eventually be simplified. This is a maintenance task only and has no functional impact.

---

# Maintenance

The following items have been completed and are no longer tracked as known issues:

* Public repository migration
* Apache 2.0 licensing
* SECURITY.md
* CONTRIBUTING.md
* CODE_OF_CONDUCT.md
* Public-tree safety audit
* Removal of the global `-Wno-format` compiler suppression
* Shared Theme architecture
* Runtime theme switching
* Multi-printer support
* Exclude-object support
* Persistent timezone configuration
* Operator UI migration
* OTA update system
* Documentation restructuring

This document should remain focused on current issues rather than serving as a historical development log.
