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
