# Continuous integration

PrinterHMI CI runs for pull requests and pushes to `main`. It is a validation
gate only: it never creates tags, publishes GitHub releases, or uploads
firmware assets.

## Source policy job

- Installs the explicit audit dependency (`ripgrep`) before checks.
- Checks Git whitespace errors and release-script syntax.
- Runs the public-tree, version/documentation, and dependency-lock audits.

## ESP-IDF build job

- Uses the pinned ESP-IDF 6.0.2 container.
- Applies the tracked ESP-Hosted compatibility patches through the canonical
  `tools/build_idf6_hosted3.sh` entry point.
- Builds the P4 firmware and enforces the 3 MiB CI firmware-size budget.

The C6 firmware package and all GitHub release publication remain manual
release-gate work. A CI failure must be resolved before a change is merged;
passing CI does not substitute for target-panel, OTA, SD-card, or power-cycle
validation.
