# Settings and large-text closeout

Stable version remains 6.5.6. This closeout publishes source only: no nightly
firmware, tag, checksum or release is created.

Included: responsive Time Zone/Reset and theme dialogs; full-label button and
row sizing; single-line navigation and compact values; navigation rail inset;
Motion/Calibration and Devices text fit; chooser card spacing; scrolling
profile manager, primary add/edit form and removal confirmation.

The operator accepted the incremental panel changes. Real-LVGL host checks
cover all four built-in themes, densities and both text sizes, with layout,
scrolling, command/lifecycle checks appropriate to each component. The closeout
runs version, architecture, portable-dependency, documentation-link and public
tree audits before merging/pushing, then uses the existing end-of-night script
with non-interactive input to skip nightly. No new firmware build is performed.

Pending: physical printing and recovery tests already recorded in
[Testing notes](TESTING.md), including filament runout,
load/purge/resume, cooled pause, active/paused temperature/fan changes, object
exclusion and whole-print cancellation. Profile authentication, camera,
certificate and keyboard layout modernization is a follow-up.

## Subsequent printer setup work

The follow-up branch implements the nested authentication, certificate, camera and keyboard layouts previously listed as pending above. Host validation is recorded in [Testing](TESTING.md); target build and touch testing remain pending. The prior source closeout and printing-test status are unchanged.

## Next page checks: Network and Files

Pending panel checks: Studio Network fills the area between header and dock; long SSIDs/hostnames remain readable, scan results scroll and both top actions work. In Files, check long filenames, preview/fullscreen taps, metadata Details/Close, Cancel and Print readiness with normal/large text. Check the ready-to-print modal in the other themes. Existing printing and recovery tests remain pending.

## Setup Center redesign

Pending panel checks: open Setup Center from Settings; inspect welcome/review cards, active/ready navigation and all text sizes/themes. Scan/select a Wi-Fi network, verify password entry/Connect, open the shared printer editor and its discovery/security/camera options, discover/test/use a camera, then check Finish, Set up later and reopening. Confirm scrolling keeps the step actions reachable. No commit, merge, push or nightly accompanies this patch.

## Files follow-up panel checks

Select a file and tap Print: verify printer/filename in the confirmation, Cancel returns to its preview, and Start print starts only after acceptance. Verify an unavailable printer reports a visible failure. With a Studio preview selected, Refresh should reload file rows while preserving the preview. Check automatic file-change refresh, Details/confirmation modal deferral and close/reopen. Previous printing/recovery tests remain pending.
