# Known Issues

This document tracks the remaining issues affecting release readiness, portability, security, and long-term maintenance.

Completed work is intentionally removed from this list so it remains an accurate representation of the current project state.

---


# Network Transport

## ESP-Hosted 3.0.5 compatibility pin

**Status:** Mitigated and release-gated

The v6 platform is pinned to ESP-IDF 6.0.2, ESP-Hosted 3.0.5 on both the
ESP32-P4 and ESP32-C6, and ESP Wi-Fi Remote 1.5.3. The link negotiates RPC v2
over four-bit 40 MHz SDIO.

Two upstream compatibility corrections are part of the reproducible build:
the official ESP-Hosted SDIO patch for ESP-IDF 6.0.2 and four missing host-side
RSSI RPC dispatch entries in ESP-Hosted 3.0.5. Both patches are tracked and
validated by `tools/build_idf6_hosted3.sh`.

Do not refresh ESP-IDF, ESP-Hosted, ESP Wi-Fi Remote or C6 firmware
independently. Treat a transport change as a platform migration and repeat the
idle, multi-printer, Devices, WebSocket, Network, OTA, SD-card, reboot and
power-cycle test matrix.

Active-printer changes retire the previous WebSocket immediately while
generation fencing rejects stale events. Firmware downloads retain exclusive
network ownership, and release-catalog traffic cannot overlap an active OTA.

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

PrinterHMI defaults to HTTP and WebSocket on a trusted local network. This behavior remains available for every profile, with or without an optional Moonraker API key.

v6.1.1 also provides opt-in, per-profile HTTPS/WSS using a selected local CA.
It requires a separately configured local TLS proxy; the public Nginx installer
and operator steps are in `docs/SECURE_MOONRAKER_SETUP.md`. Secure profiles do
not silently downgrade to HTTP after a certificate or endpoint failure.

## OTA trust model

Firmware updates use the ESP-IDF OTA framework but do not currently implement a project-specific firmware signing policy.

---

# Build & Portability

## Internal startup-RAM margin

FreeRTOS creates the statically configured timer task before `app_main()`.
Small increases in internal `.bss` can therefore expose an early
`vApplicationGetTimerTaskMemory` allocation failure. v5 moves the new shell
and Macros page contexts to permanent post-scheduler PSRAM allocations, but
release builds must continue checking the memory map and cold boot.

## Continuous Integration

**Status:** Implemented and required for changes pushed to `main`.

GitHub Actions validates the public-tree and version audits, portable locked
dependencies, a clean ESP-IDF 6.0.2 P4 build, firmware-size budget, and
PrinterHMI application compiler warnings. See [CI validation](CI.md).

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
