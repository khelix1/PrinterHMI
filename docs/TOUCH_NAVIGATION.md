# Touch and navigation consistency

The current pass keeps modal dialogs above their touch-blocking backdrop while tying cleanup to the originating page. Discovery stays on the top layer; removing its owner closes the dialog and cancels discovery. Removing a modal blocker also closes its popup. Nested Back/Close returns to the existing parent dialog without making the page underneath actionable.

Devices Telemetry and Endstops, Network Scan and Manage Printers, and Calibration PID Tune now have visible heights of at least 44 pixels. Existing shared touch slop is unchanged. Drybox layout is preserved.

Tapping a Files preview before its thumbnail arrives opens file details. Once an image exists, the tap opens the preview lightbox. Nested preview events cannot also select the row. Duplicate Files search, Macro editor and Profile field editor opens preserve the existing dialog and draft input.

## Validation

Host tests use LVGL 9.5 pointer input to check label and edge taps, adjacent actions, disabled/hidden controls, scrolling without accidental activation, modal backdrop blocking, nested navigation and owner deletion. Discovery tests exercise actual popup Retry, selection and Close from a page owner. Layout and action-fit checks cover all five themes, three densities, normal/large text and smaller viewports. Source-derived button checks include minimum dimensions.

ESP-IDF firmware compilation and physical panel testing remain pending. Verify discovery Retry/Close, nested Back, Files thumbnail taps while loading, repeated editor opens and scrolling on the panel. Physical print/recovery tests remain pending as previously recorded.




Validation statements above record the implementation-stage checks. Current
release status and open limitations are maintained in [Known issues](KNOWN_ISSUES.md),
the [changelog](../CHANGELOG.md) and the [current audit](PROJECT_AUDIT_2026_10_30.md).
