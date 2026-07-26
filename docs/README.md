# PrinterHMI documentation

Documents in this directory describe current v4.2.2 behavior unless they are
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
| [Troubleshooting](TROUBLESHOOTING.md) | Diagnostic and recovery procedures |
| [Known issues](KNOWN_ISSUES.md) | Open product and repository risks |
| [Release checklist](RELEASE_CHECKLIST.md) | Release-quality gate |
| [Git workflow](GIT_WORKFLOW.md) | Branches, known-good tags and nightly backup |
| [Project file catalog](PROJECT_FILE_CATALOG.md) | Current source ownership map |
| [History](history/README.md) | Preserved v3.x engineering record |

Top-level project documents:

- [`README.md`](../README.md) — project entry point
- [`CHANGELOG.md`](../CHANGELOG.md) — product-level changes
- [`CONTRIBUTING.md`](../CONTRIBUTING.md) — development policy
- [`SECURITY.md`](../SECURITY.md) — security and credential policy

When implementation and documentation disagree, treat the implementation as
evidence of a defect in one or the other. Resolve the mismatch in the same
reviewed change rather than silently accepting drift.
