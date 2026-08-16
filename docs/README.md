# PrinterHMI documentation

Documents in this directory describe current v6.3.0 behavior unless they are
under `history/`.

| Document | Purpose |
| --- | --- |
| [Architecture](ARCHITECTURE.md) | Runtime structure and ownership boundaries |
| [Build](BUILDING.md) | Toolchain, dependencies and reproducible build |
| [Hardware](HARDWARE.md) | Target board, display, touch, SD and flash layout |
| [Configuration](CONFIGURATION.md) | Operator settings and persistent state |
| [Custom themes](CUSTOM_THEMES.md) | SD package format, validation and recovery |
| [Flashing and OTA](FLASHING_AND_OTA.md) | USB installation, OTA and recovery |
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
