# PrinterHMI documentation

Operator/build guides describe current v7.2.4-dev behavior. `releases/`,
`history/`, the changelog and the engineering records identified below describe
their own checkpoints; they are not automatically current procedures.

| Document | Purpose |
| --- | --- |
| [Architecture](ARCHITECTURE.md) | Runtime structure and ownership boundaries |
| [Build](BUILDING.md) | Toolchain, dependencies and reproducible build |
| [Hardware](HARDWARE.md) | Target board, display, touch, SD and flash layout |
| [Configuration](CONFIGURATION.md) | Operator settings and persistent state |
| [Custom themes](CUSTOM_THEMES.md) | SD package format, validation and recovery |
| [Flashing and OTA](FLASHING_AND_OTA.md) | USB installation, OTA and recovery |
| [LVGL modernization](LVGL_MODERNIZATION.md) | Completed modernization, live refresh, aspect previews, offline navigation and responsive page work; remaining follow-up queue |
| [Preview rendering](PREVIEW_RENDERING.md) | Preview fill/cropping and fullscreen behavior |
| [Filament recovery](FILAMENT_RECOVERY.md) | Paused-print replacement, temperature control and Sermoon macros |
| [Tools](TOOLS.md) | Calibration, live endstops, motion diagnostics and macro parameters |
| [Testing](TESTING.md) | Required host and target validation |
| [Continuous integration](CI.md) | Automated source policy and clean IDF6 build gate |
| [Troubleshooting](TROUBLESHOOTING.md) | Diagnostic and recovery procedures |
| [Known issues](KNOWN_ISSUES.md) | Open product and repository risks |
| [Release checklist](RELEASE_CHECKLIST.md) | Release-quality gate |
| [Git workflow](GIT_WORKFLOW.md) | Branches, known-good tags and nightly backup |
| [Project file catalog](PROJECT_FILE_CATALOG.md) | Current source ownership map |
| [Secure Moonraker transport](SECURE_MOONRAKER_TRANSPORT.md) | Per-profile HTTPS/WSS policy and trust boundaries |
| [Secure Moonraker setup](SECURE_MOONRAKER_SETUP.md) | Public Nginx installer and panel setup |
| [History](history/README.md) | Preserved v3.x engineering record |

Top-level project documents:

- [`README.md`](../README.md) — project entry point
- [`CHANGELOG.md`](../CHANGELOG.md) — product-level changes
- [`CONTRIBUTING.md`](../CONTRIBUTING.md) — development policy
- [`SECURITY.md`](../SECURITY.md) — security and credential policy

When implementation and documentation disagree, treat the implementation as
evidence of a defect in one or the other. Resolve the mismatch in the same
reviewed change rather than silently accepting drift.

- [`FULL_STACK_BUILD.md`](FULL_STACK_BUILD.md) — reproducible ESP-IDF 6.0.2 P4 and C6 firmware build

## Encrypted configuration backups

See [Encrypted Configuration Backups](ENCRYPTED_CONFIGURATION_BACKUPS.md) for
the portable encrypted-backup format, restore verification, and recovery rules.

Historical control-dialog closeout: [Nightly validation and pending printing checks](NIGHTLY_VALIDATION.md).

Historical Settings and large-text source closeout: [validation and pending follow-ups](SETTINGS_LAYOUT_VALIDATION.md).

- [STUDIO Dark](STUDIO_DARK_TRIAL.md): independent dark theme, native LVGL Dashboard and panel validation.

- [Telemetry views, graph controls and validation](TELEMETRY.md)

- [Motion and calibration dialog layouts and validation](MOTION_CALIBRATION.md)

- [Console layouts, command feedback and pressed button frames](CONSOLE.md)
- [Notification policy and remaining migration](NOTIFICATIONS.md)

See [v7.2.4-dev release notes](releases/v7.2.4-dev.md) for the current development scope and [Code cleanup](CODE_CLEANUP.md) for development history.

- [Lighting](LIGHTING.md) — Tools case-light control and PWM brightness.

See [Project audit — 2026-10-30](PROJECT_AUDIT_2026_10_30.md) for current input-hardening findings, validation and remaining trust boundaries.

## Documentation types

- Current runbooks: Building, Full-stack build, Flashing and OTA, Configuration,
  Hardware, Troubleshooting, Git workflow, Release checklist and Security Policy.
- Operator guides: the feature guides linked above, including Lighting,
  notifications, previews, themes, calibration, Console and firmware-update UI.
- Engineering records: TESTING, CODE_CLEANUP, LVGL_MODERNIZATION,
  MODERNIZATION_CLOSEOUT, SECURITY_REVIEW, SETTINGS_LAYOUT_VALIDATION and
  NIGHTLY_VALIDATION. Their checkpoint-specific PASS/pending/release statements
  are historical evidence; consult the current audit and known issues as well.
- Release notes and CHANGELOG preserve each release's facts. `history/` and
  third-party license notices are retained as historical/upstream records.

The 2026-10-30 documentation pass covers all 128 project-authored/upstream-notice
Markdown/RST files tracked outside components on the reviewed main checkpoint,
plus all 11 local-component documentation files and the new audit/release note.
Upstream component documentation is labeled separately from PrinterHMI runbooks.
See the [audit](PROJECT_AUDIT_2026_10_30.md) for scope and corrections.
