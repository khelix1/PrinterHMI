# Known Issues

This document tracks the remaining issues affecting release readiness, portability, security, and long-term maintenance.

Completed work is intentionally removed from this list so it remains an accurate representation of the current project state.

---


# Network Transport

## ESP-Hosted compatibility pin

**Status:** Mitigated and release-gated

The ESP32-P4 host component is pinned to ESP-Hosted 2.9.3 while the installed
ESP32-C6 co-processor firmware remains at 2.12.8. ESP-Hosted 2.12.8 on the P4
reproduced intermittent `sdmmc_send_cmd` timeout 0x107 failures followed by an
`Unrecoverable host sdio state` restart. The 2.9.3 P4 host restored stable
operation with the existing C6 firmware.

The upstream transport defect remains open. Do not update the P4 host pin,
ESP-IDF or C6 firmware as a routine dependency refresh. Treat any transport
change as a platform migration and repeat the idle, multi-printer, Devices,
WebSocket, Network, OTA, reboot and power-cycle test matrix.

Short HTTP operations are serialized. Wi-Fi scans and firmware downloads use
exclusive network ownership; GitHub release browsing remains shared so it can
drain before a firmware update begins.

# Display Limitations

## Initial panel appearance

**Status:** Accepted cosmetic limitation

ESP-Hosted/C6 initialization now completes while the backlight is off, removing
the repeated flashes previously seen during splash progress. The display may
still flash once when the panel and first splash frame become visible.

Further work may replace the fixed startup delay with an explicit first-frame
completion signal if that does not destabilize startup.

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

## Internal startup-RAM margin

FreeRTOS creates the statically configured timer task before `app_main()`.
Small increases in internal `.bss` can therefore expose an early
`vApplicationGetTimerTaskMemory` allocation failure. v5 moves the new shell
and Macros page contexts to permanent post-scheduler PSRAM allocations, but
release builds must continue checking the memory map and cold boot.

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
