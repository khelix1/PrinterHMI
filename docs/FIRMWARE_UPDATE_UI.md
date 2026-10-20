# Firmware update dialogs

The URL editor, release browser, install confirmation and progress dialog share native title/body/footer layouts. Dialogs clamp to the display; long content scrolls and footer actions remain pinned. Release rows wrap version/publish metadata, and long warnings/release notes remain reachable with large text in every theme.

The URL editor retains blank input and its validation prompt. Its deferred transition still deletes the editor and dispatches the update in the same callback; external deletion cancels pending callbacks instead of starting an update from a retired dialog. Camera quiescence remains part of the existing update flow.

The release browser retains Stable/Nightly selection, refresh, Custom URL and the existing install/reinstall/downgrade warnings. Install captures the selected asset URL before deleting the browser. External deletion retires its refresh timer and selection storage. Catalog errors stay in the browser and changed error text is refreshed even when the catalog state/count are unchanged.

Progress shows complete multiline status, percentage and transferred bytes. Identical values do not repeatedly invalidate labels. Cancel stays disabled while cancellation is pending; it cannot become enabled again on the next progress tick. While validation/reboot is running, disabled cancellation stays disabled. A finished failed transfer provides Close; a successful update remains on the reboot screen. Startup failure retains its separate dismissible update-error dialog.

## Validation

The real LVGL host audit covers all five themes, densities, both text sizes and 1024/640/480 pixel viewports: pinned footers, reachable scrolling content, blank URL retention, deferred callback cancellation, multiline progress, cancellation pending/finished states, release channel selection, install asset identity, inline catalog errors and external popup deletion. The three update translation units compile with warnings treated as errors. Network behavior is fixture-driven; target firmware build, physical panel, actual download/cancellation and reboot tests remain pending.

Run `python3 tools/audit/calibration_dialog_layout_test.py` with LVGL sources available; the OTAFAILURE variant includes the update owners. Notification level persistence and policy use `notifications_test.py` and `notification_backup_test.py`.


## Running firmware identity and available releases

The browser shows the running image's embedded build channel, version and source
revision. Regular local builds are **Custom**, even when their base version equals
a published stable release. `tools/release_stable.sh` supplies the Stable build
channel through the full-stack build. Generated `nightly-YYYY-MM-DD-commit` versions
identify Nightly builds. The channel describes the firmware build, not whether it
was installed using a custom URL; downloading an official build by URL does not
turn it into a custom build.

| Candidate versus installed build | Label |
| --- | --- |
| Matching stable channel and semantic version | Installed |
| Matching nightly channel and complete nightly identity | Installed |
| Later stable semantic version or later nightly date in its channel | Update available |
| Earlier comparable version/date | Older |
| Stable release at the same base version as a custom build | Switch to Stable |
| Changing between stable and nightly channels | Switch to Stable/Nightly |
| Different nightly commit on the same date | Different Build |

An installed row has an accent border; the browser opens the installed nightly
channel when applicable. Different same-day commit hashes do not establish build
order. A nightly and stable version are not ranked against one another. Version
comparison does not verify binary checksums or authenticity, and older firmware
without these UI/metadata changes will continue to use its older behavior.

## URL dialog lifetime

Custom URL and return-to-releases transitions are deferred until the current input
event finishes. Camera quiescence is retained between the dialogs rather than
resuming and stopping the camera between them. Repeated taps queue one transition;
closing or externally deleting the owner cancels pending work. Failed async
reservation retains the dialog and URL. Start Update reserves its callback before
transferring ownership, and the action-size handler ignores bubbled child events
and missing labels. These changes harden the intermittent Custom URL path;
confirming the reported crash is resolved requires a panel test or matching panic
trace.

The host audit repeatedly exercises both transitions, external deletion, rapid
taps, keyboard submission/cancellation, pending-start cancellation and rejected
async allocation across all themes, density, text sizes and viewport fixtures.
`tools/audit/firmware_identity_test.py` compiles all three build channels and tests
comparison cases. CMake channel execution is optional when CMake is available;
this environment lacks CMake and the ESP-IDF target toolchain. A target build,
physical panel test and actual OTA/reboot remain pending.


### Settings firmware identity and update review

Settings now shows the installed build channel and source revision beside the image version and compilation time. OTA confirmation separates installed and selected builds, download information, reboot/power guidance and scrollable release notes; actions remain pinned. The active chooser card shows Checking before its first health result and Connecting when initial health is online but transport is not ready. Confirmed failures and post-connection losses remain offline. Host checks cover these transitions and responsive OTA review; target/panel reboot and real installation remain pending.


### OTA transfer measurement and bounded receive tuning

The common GitHub/custom-URL worker requests a 4 KiB receive buffer while retaining the 2 KiB redirect request buffer, certificate verification, keep-alive, existing cancellation/validation boundaries and sequential flash erasure. Firmware progress uses the ESP HTTPS OTA image counters after each perform call, excluding redirect bodies from transferred-image readings. A short critical section publishes coherent byte, state and clock snapshots; UI-thread formatting replaces per-data-event formatting. The progress dialog reports average effective download speed in KiB/s and elapsed time since the update started, including network preparation; readings freeze when transfer ends, before validation. Unknown image lengths remain supported. Host fault tests cover worker success/restart, cancellation, incomplete images, begin/perform/finish and task failures, lane timeouts, known/unknown lengths, clock snapshots and configuration. Real GitHub/custom download speed, target memory/cancel responsiveness and reboot remain pending; no speedup is claimed without panel measurement.

Telemetry history status now has room for the compiler-reported worst-case text. Its host compile check enables format-truncation warnings as errors.

The stricter Telemetry owner compilation also checks hotend-option assembly: bounded copies retain all four maximum-length object names without a potentially truncating append. Host selector tests cover those names and active-tool selection.


## Development identity

The current branch uses `7.0.0-dev` and local builds remain Custom. Settings and
OTA display the embedded version and source revision. A final Stable `7.0.0` is
newer than `7.0.0-dev`; Stable `6.5.6` is older. Stable publishing rejects `-dev`.
Latest published stable remains `6.5.6`. The intermittent OTA failure that cleared
after reboot remains unresolved.
