# Final project review: 7.0.0-dev

> Engineering record: version, branch, PASS and pending statements below refer
> to the checkpoint described in each section, not necessarily current `main`.
> Use [Building](BUILDING.md), [Known issues](KNOWN_ISSUES.md),
> [Release checklist](RELEASE_CHECKLIST.md) and the
> [2026-10-30 audit](PROJECT_AUDIT_2026_10_30.md) for current procedures and limits.


Release preparation: current identity is 7.0.0. This report records the earlier
7.0.0-dev UI review; later security/failure-path results and remaining limitations
are included in [the v7.0.0 release notes](releases/v7.0.0.md) and
[the security review](SECURITY_REVIEW.md).

Review base: `004f6c2d27c8918a6133eef6600a128cb48f306c` on `feature/profile-setup-dialogs`, fetched into a clean integration checkout. Review date: 2026-10-19 (America/Chicago). This supersedes the earlier closeout/application-order notes. The final follow-up includes the small integration caption/test corrections, whether or not their earlier installer was applied.

## Findings and fixes

- **Keyboard actions:** Save bed mesh profile had no Ready/Cancel handlers for its on-screen keyboard. Ready now validates the current name and opens the existing save review; it does not send a command. Cancel closes only the editor. Foreign/bubbled, disabled or retired input/keyboard events cannot act. Editing clears the stale name-format warning while preserving other feedback.
- **Keyboard layout consistency:** Console command/search now use the shared form, input, keyboard and deletion-binding helpers. Keys retain the standard text layout and theme, use the shared adaptive height, occupy the form width and stay above pinned actions. Shared form finishing applies the same sizing after footer layout, preserving a reachable input on shorter viewports. Text/password/search inputs use the common text keyboard; integer/decimal inputs retain the numeric keyboard and their accepted-character restrictions. Console history, search and Send meanings are unchanged. Inline send errors scroll into view; editing brings the field back into view. Console footer actions fill their row.
- **Redraw bug:** Full status banners owned zero-padding coordinates, but repeated status styling reapplied theme padding before the caller reset it. Unchanged updates therefore invalidated/redrew the banner. The shared style dispatcher now accepts the banner's owned padding and checks it alongside the current theme/style properties. Full banners retain zero padding; compact banners retain theme padding. Printer notices/status use the same full-banner geometry. State colors, status text, observer fallback and custom-theme repair are preserved.
- **API-key guidance:** The profile caption now correctly says plain backups omit API keys and encrypted backups protect them with the passphrase. Backup contents and credential handling are unchanged.
- **Audit corrections:** The Studio Printer fixture now matches the existing Runtime Z offset (mm) caption. An unused legacy footer helper was removed so the button audit compiles with warnings treated as errors. The bed-mesh scroll-reach check now accounts for a field that is already scrolled into view. Observer tests include all five themes and the accepted opaque Studio surfaces. No compiler warning checks or safety assertions were disabled.

## Review scope and evidence

The combined integration and final review exercised 76 individual host test scripts. Affected owners/suites were rerun after changes. The host Jinja dependency was supplied for the real filament-recovery macro-template checks; it is not a firmware dependency or project source change. The two batch-wrapper scripts were not counted again because their individual suites were run directly.

Coverage includes Dashboard/Printer/Files/Camera/Tools/Console/Drybox/Settings; dialog ownership and deletion, async transitions, command readiness/duplicate/retry behavior, profile/security/discovery, input/caret/password/keyboard geometry, normal/large text and density/theme changes, notifications/event history/telemetry, backup/restore/OTA control flow, preview buffers/render handoffs, startup splash, PPA configuration/cache boundaries, offline navigation and transport adapters. Each fixture retains its own documented matrix; they are not all the same matrix. Console and bed-mesh key actions now have explicit regression cases. Status observers check that 100 identical publishes do not invalidate or notify unnecessarily, retain long-text/compatibility-writer behavior and repair external style changes.

| Host suite | Result |
| --- | --- |
| `about_dialog_layout_test.py` | PASS |
| `active_preview_safety_test.py` | PASS |
| `aspect_preview_pipeline_test.py` | PASS |
| `bed_mesh_profiles_layout_test.py` | PASS |
| `boot_splash_freeze_test.py` | PASS |
| `button_text_fit_test.py` | PASS |
| `calibration_dialog_layout_test.py` | PASS |
| `camera_pipeline_test.py` | PASS |
| `cancel_layout_test.py` | PASS |
| `chooser_refresh_test.py` | PASS |
| `command_surfaces_safety_test.py` | PASS |
| `connection_controls_test.py` | PASS |
| `console_filters_test.py` | PASS |
| `console_modern_layout_test.py` | PASS |
| `console_transport_test.py` | PASS |
| `control_popups_layout_test.py` | PASS |
| `dashboard_camera_safety_test.py` | PASS |
| `dashboard_offline_test.py` | PASS |
| `development_version_test.py` | PASS |
| `devices_text_layout_test.py` | PASS |
| `diagnostics_batch_safety_test.py` | PASS |
| `discovery_states_test.py` | PASS |
| `display_settings_safety_test.py` | PASS |
| `estop_transport_test.py` | PASS |
| `event_history_layout_test.py` | PASS |
| `filament_recovery_macros_test.py` | PASS |
| `filament_recovery_test.py` | PASS |
| `filament_sensor_safety_test.py` | PASS |
| `files_load_worker_test.py` | PASS |
| `firmware_identity_test.py` | PASS |
| `graphics_diagnostics_test.py` | PASS |
| `hotend_layout_test.py` | PASS |
| `klipper_readiness_test.py` | PASS |
| `live_appearance_test.py` | PASS |
| `live_widget_refresh_test.py` | PASS |
| `lvgl95_modernization_test.py` | PASS |
| `motion_layout_test.py` | PASS |
| `network_backup_dialog_test.py` | PASS |
| `notification_backup_test.py` | PASS |
| `notifications_test.py` | PASS |
| `object_layout_test.py` | PASS |
| `offline_navigation_test.py` | PASS |
| `ota_transfer_test.py` | PASS |
| `page_state_safety_test.py` | PASS |
| `ppa_cache_coherency_test.py` | PASS |
| `ppa_trial_test.py` | PASS |
| `preview_cache_ownership_test.py` | PASS |
| `preview_fill_test.py` | PASS |
| `printer_action_lifetime_test.py` | PASS |
| `printer_direct_action_safety_test.py` | PASS |
| `printer_tuning_safety_test.py` | PASS |
| `profile_discovery_bridge_test.py` | PASS |
| `profiles_layout_test.py` | PASS |
| `recovery_dialog_safety_test.py` | PASS |
| `responsive_layout_test.py` | PASS |
| `sensor_status_layout_test.py` | PASS |
| `settings_dialog_layout_test.py` | PASS |
| `setup_center_layout_test.py` | PASS |
| `shared_popup_safety_test.py` | PASS |
| `software_credits_test.py` | PASS |
| `status_observers_test.py` | PASS |
| `studio_layout_test.py` | PASS |
| `studio_pages_compile_test.py` | PASS |
| `studio_pages_test.py` | PASS |
| `telemetry_modern_layout_test.py` | PASS |
| `telemetry_refresh_test.py` | PASS |
| `theme_default_test.py` | PASS |
| `theme_dialog_layout_test.py` | PASS |
| `thumbnail_component_safety_test.py` | PASS |
| `thumbnail_storage_test.py` | PASS |
| `toolhead_dialog_safety_test.py` | PASS |
| `tools_action_safety_test.py` | PASS |
| `tools_features_test.py` | PASS |
| `tools_live_refresh_test.py` | PASS |
| `touch_navigation_test.py` | PASS |
| `wifi_connect_dispatch_test.py` | PASS |


Eight additional source audits passed: dependency-lock portability, public-tree hygiene, version consistency, button icon ownership, theme style ownership, theme palette ownership, feature architecture/build membership and repository-relative documentation links. Theme/icon audits are source reports; they do not establish device performance. Python tool syntax compilation and `git diff --check` passed. Native Console large-text renders were inspected in Studio and Operator.

An additional cross-module host check linked the real backup and OTA dialog owners, completed encrypted backup, closed its persistent result and backup page, then opened/closed OTA repeatedly with balanced overlay/timer cleanup. It used mocked backup/catalog workers. PPA tests use deterministic cache/DMA models. Storage, network, SDK, crypto-loader and scheduler boundaries are mocked where documented by their suites. Host results do not establish FreeRTOS stack headroom, real SD/crypto timing, hardware cache behavior or long-duration stability. No ESP-IDF build, flash, physical print/stop/recovery or real restore was performed by this workspace.

## Existing behavior retained

Version remains `7.0.0-dev`; LVGL remains 9.5.0. The build wrapper keeps the accepted corrected PPA fill-only default, with image acceleration/rotation disabled and the existing explicit software fallback. OTA application slots remain 7 MiB. No commands, thermal/motion limits, persistence schema, backup contents, transport routing, preview sizes or fonts changed in this follow-up. See [GRAPHICS_ACCELERATION.md](GRAPHICS_ACCELERATION.md) for renderer details.

## Open intermittent OTA/rollback report

The user reported Settings -> OTA update crashes after encrypted backup, including repeats after reboot, then observed rollback. Updating successfully from the local build folder restored normal behavior; the sequence could not subsequently be reproduced. The encrypted-backup success-confirmation patch was not installed when the crashes were first reported. No matching panic/backtrace is available. The host passes do not establish the cause or resolve that report.

`ota_boot_validation_confirm_running_image()` confirms a pending image after startup reaches the Wi-Fi/display baseline and logs the partition/state and result. If the crash returns during normal use, the useful evidence is the matching firmware identity/ELF, running slot/state, full panic/backtrace/reset output and any mark-valid error. An ELF checksum mismatch requires the matching build for decoding.

The user chose not to repeat a multi-cycle panel checklist. No repeated panel sequence is required by this final pass. Continue with the normal build/OTA workflow and report an unexpected result if it recurs. Physical restore, printing-dependent recovery and long-duration hardware behavior retain their existing evidence limits; no new device acceptance is claimed.

## Checkpoint and release status

The final installer validates known source/test baselines before writing, preserves unrelated documentation and recognizes repeat/partial application. It accepts the reviewed 004f6c2 baseline or the optional preceding integration patch. It does not commit, push, merge, bump the version or publish firmware. Merge/release decisions remain with the project owner. Only integrated `main` produces nightly firmware; this feature checkpoint is not a nightly release. Earlier subsystem evidence remains in [TESTING.md](TESTING.md) and [LVGL_MODERNIZATION.md](LVGL_MODERNIZATION.md).
