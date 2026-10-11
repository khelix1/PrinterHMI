# Firmware update dialogs

The URL editor, release browser, install confirmation and progress dialog share native title/body/footer layouts. Dialogs clamp to the display; long content scrolls and footer actions remain pinned. Release rows wrap version/publish metadata, and long warnings/release notes remain reachable with large text in every theme.

The URL editor retains blank input and its validation prompt. Its deferred transition still deletes the editor and dispatches the update in the same callback; external deletion cancels pending callbacks instead of starting an update from a retired dialog. Camera quiescence remains part of the existing update flow.

The release browser retains Stable/Nightly selection, refresh, Custom URL and the existing install/reinstall/downgrade warnings. Install captures the selected asset URL before deleting the browser. External deletion retires its refresh timer and selection storage. Catalog errors stay in the browser and changed error text is refreshed even when the catalog state/count are unchanged.

Progress shows complete multiline status, percentage and transferred bytes. Identical values do not repeatedly invalidate labels. Cancel stays disabled while cancellation is pending; it cannot become enabled again on the next progress tick. While validation/reboot is running, disabled cancellation stays disabled. A finished failed transfer provides Close; a successful update remains on the reboot screen. Startup failure retains its separate dismissible update-error dialog.

## Validation

The real LVGL host audit covers all five themes, densities, both text sizes and 1024/640/480 pixel viewports: pinned footers, reachable scrolling content, blank URL retention, deferred callback cancellation, multiline progress, cancellation pending/finished states, release channel selection, install asset identity, inline catalog errors and external popup deletion. The three update translation units compile with warnings treated as errors. Network behavior is fixture-driven; target firmware build, physical panel, actual download/cancellation and reboot tests remain pending.

Run `python3 tools/audit/calibration_dialog_layout_test.py` with LVGL sources available; the OTAFAILURE variant includes the update owners. Notification level persistence and policy use `notifications_test.py` and `notification_backup_test.py`.
