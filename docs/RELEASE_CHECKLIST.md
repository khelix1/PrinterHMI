# Release checklist

## Source and security

- [ ] Working tree is clean.
- [ ] Release branch contains only reviewed changes.
- [ ] No credentials, tokens, private keys or unintended private endpoints are
      present in tracked files or reachable history.
- [ ] Known exposed credentials have been rotated.
- [ ] `README.md`, `CHANGELOG.md` and relevant technical documents are current.
- [ ] Project license status is explicit.
- [ ] Dependency and component-license changes are reviewed.

## Versioning

- [ ] `version.txt` contains the intended semantic release version.
- [ ] CMake derives `PROJECT_VER` from `version.txt`.
- [ ] Runtime version labels read the running image's `esp_app_desc_t`.
- [ ] `tools/audit/version_audit.sh` passes.
- [ ] Changelog and release notes contain the release date and summary.

## Build

- [ ] Canonical ESP-IDF 6.0.2/ESP-Hosted 3.0.5 build succeeds.
- [ ] `dependencies.lock`, `sdkconfig.idf6` and `partitions.csv` diffs are intentional.
- [ ] Application and bootloader size reports are saved.
- [ ] `build-idf6-hosted3/PrinterHMI.bin` SHA-256 is recorded.

## Device verification

- [ ] USB flash succeeds.
- [ ] OTA from the previous known-good release succeeds.
- [ ] OTA cancellation exits without rebooting or selecting a partial image.
- [ ] First OTA boot marks the image valid.
- [ ] Warm reboot succeeds.
- [ ] Full power cycle succeeds.
- [ ] Startup has no repeated splash flashing.
- [ ] Startup passes `vApplicationGetTimerTaskMemory()` and reaches
      `main_task: Calling app_main()`.
- [ ] All ten sidebar destinations fit, highlight and open.
- [ ] Navigation, themes and modal popups pass.
- [ ] Calibration workflows, guided probing and SAVE_CONFIG confirmations pass.
- [ ] Bed Mesh rotation, pinch, two-finger pan, grid, origins, calibration and
      profile management pass.
- [ ] Devices filters, pagination, live values and Telemetry navigation pass.
- [ ] `tools/audit/v5_feature_architecture_audit.sh` passes.
- [ ] Public macros are detected, helper macros are hidden and execution
      requires confirmation.
- [ ] Console command history, live responses and severity colors pass.
- [ ] Wi-Fi, SNTP, Moonraker HTTP/WebSocket and multi-printer switching pass.
- [ ] Host and C6 both report ESP-Hosted 3.0.5 with RPC v2.
- [ ] Live Wi-Fi RSSI bars and native high-speed SD-card mounting pass.
- [ ] Printer controls, Files, previews, Drybox and Telemetry pass.
- [ ] Settings persist and factory-reset behavior is understood.
- [ ] Release-candidate soak test passes.

## Git and artifacts

```bash
git status --short
sha256sum build-idf6-hosted3/PrinterHMI.bin
./tools/release_stable.sh
```

- [ ] Annotated tag points to the tested commit.
- [ ] `main` and the annotated release tag are pushed to `origin`.
- [ ] OTA binary, checksum, required C6 transition asset and test record are retained.
- [ ] Nightly tag and firmware/checksum assets are verified when used.
- [ ] A project-root `git archive` can be extracted and built.

## Rollback preparation

- [ ] Previous known-good tag and binary remain available.
- [ ] USB recovery port and cable are available.
- [ ] Operator understands which persistent settings and SD data survive each
      recovery method.
