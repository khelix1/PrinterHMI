# Scrolling and refresh performance

Telemetry refresh reads each history sample once for all four displayed channels, rather than once per chart. It uses the existing chart arrays and one sample object; no second history buffer or extra chart widgets are allocated. Time bins, missing intervals, named hotend selection, target scaling, Hold and two-second sampling remain unchanged. Existing changed-value label updates and plot tick gating remain in use.

Settings system information compares formatted text with each live label before setting it. Repeated identical values therefore avoid redraw and label allocation. An unbound Settings page does not query heap or uptime. Values are compared against actual widgets so reopening a page always populates it correctly.

Files header and metadata labels similarly skip identical text. Submitting the same normalized search, reopening the current folder or pressing Up at root preserves existing rows and scroll position. Changed searches, folder navigation, sorting and explicit network refresh still render the appropriate result. Loading and error states remain protected.

## Validation

Host LVGL checks cover all five themes, density and text-size variants. Instrumented telemetry refresh asserts one history read per stored sample. Settings repeated identical refresh asserts no display invalidation. Repeated Telemetry and Files open/close and Settings dialog cycles check object and timer counts return to baseline. Files worker tests check equivalent searches do not clear rows while changed queries still rebuild them. Existing graph time-gap, named-tool, Hold, offline, reset and range tests pass.

These checks measure host behavior and widget/timer lifetime, not ESP32 heap usage or hardware frame rate. Firmware build, physical scrolling with live printers and long-run heap behavior remain pending. Physical print/recovery tests remain pending as previously recorded.

The pass stays on feature/profile-setup-dialogs. Stable version remains 6.5.6; no commit, push or release is performed by the installer.
