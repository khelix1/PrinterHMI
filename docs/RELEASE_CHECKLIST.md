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

- [ ] Clean ESP-IDF 5.4.4 build succeeds.
- [ ] `dependencies.lock`, `sdkconfig` and `partitions.csv` diffs are intentional.
- [ ] Application and bootloader size reports are saved.
- [ ] `build/PrinterHMI.bin` SHA-256 is recorded.

## Device verification

- [ ] USB flash succeeds.
- [ ] OTA from the previous known-good release succeeds.
- [ ] OTA cancellation exits without rebooting or selecting a partial image.
- [ ] First OTA boot marks the image valid.
- [ ] Warm reboot succeeds.
- [ ] Full power cycle succeeds.
- [ ] Startup has no repeated splash flashing.
- [ ] Navigation, themes and modal popups pass.
- [ ] Wi-Fi, SNTP, Moonraker HTTP/WebSocket and multi-printer switching pass.
- [ ] Printer controls, Files, previews, Drybox and Telemetry pass.
- [ ] Settings persist and factory-reset behavior is understood.
- [ ] Release-candidate soak test passes.

## Git and artifacts

```bash
git status --short
git tag -a vX.Y.Z -m "PrinterHMI vX.Y.Z"
git push origin main --follow-tags
sha256sum build/PrinterHMI.bin
```

- [ ] Annotated tag points to the tested commit.
- [ ] `main` and tags are pushed to the private remote.
- [ ] Release binary, checksum, partition table and test record are retained.
- [ ] Nightly tag and firmware/checksum assets are verified when used.
- [ ] A project-root `git archive` can be extracted and built.

## Rollback preparation

- [ ] Previous known-good tag and binary remain available.
- [ ] USB recovery port and cable are available.
- [ ] Operator understands which persistent settings and SD data survive each
      recovery method.
