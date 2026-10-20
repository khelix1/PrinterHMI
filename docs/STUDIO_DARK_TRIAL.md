# STUDIO Dark interface

STUDIO Dark is an opt-in LVGL 9.5 interface for the 1024 × 600 ESP32-P4 panel.
Select **Settings → Interface Theme → STUDIO DARK**. The saved selection survives
reboot. The operator accepted the current panel layout on October 9, 2026,
and the theme no longer carries a trial label. Operator remains the factory
default; firmware identity remains 6.5.6. STUDIO includes deferred chooser
construction and the 16 KB LVGL task stack correction.

## Composition

The theme owns its style recipes without calling Theme A, B, C or Operator style
recipes. Graphite, ivory, violet and lime combine with Inter typography, native
line icons, an inline printer selector and an eight-entry bottom navigation shelf.
The eight main pages use separately authored compositions:

- Dashboard: borderless preview, oversized progress, open thermal readouts and a
  horizontal telemetry strip. Values use real data and existing unknown/N/A states.
- Printer: large profile-owned model preview, live XYZ and telemetry strip,
  thermal column and bottom print actions. Toolhead Control opens the existing
  guarded jogging, extrusion and Z-offset popup; Tune opens speed/flow controls.
- Drybox: humidity arc, open temperature readouts and program capsules. Existing
  program commands and availability states remain authoritative; no timer is invented.
- Tools: two-by-two utility card grid with icons and descriptions.
- Files: scrolling job ledger and embedded preview/metadata inspector. Details
  opens the full metadata text; Print remains disabled until metadata is ready.
- Camera: feed surface with overlaid status and controls. Configure, view transforms,
  source selection and fullscreen retain the existing camera pipeline.
- Console: fixed command composer and six-row history pages. Older/Newer navigate
  filtered history; Follow returns to live output. The page does not scroll.
- Settings: flat editorial rows, fine rules and a scrolling settings viewport.
  Existing settings, descriptions and callbacks remain available.

Files and Settings scroll inside their content viewports. The other six main
pages occupy a fixed 976 × 424 stage between the header and dock. Tool detail
lists and long dialog content retain their own necessary scrolling. Printer
transport, preview ownership, print-action resolution, runout behavior and
cancellation confirmations continue to use the existing application contracts.
Shared dialogs receive STUDIO's independent style recipes; their underlying
control and confirmation logic is reused. The concept boards are design targets,
while these native layouts are the native implementation.

## Fonts

Subsetted Inter: regular 18 px, semibold 20/24 px, bold 28/32/48/64/96 px;
ASCII plus degree symbol, 4 bpp, uncompressed. The generated sources compile
directly. Builds do not download fonts or run font generation. The original
variable font and SIL Open Font License are in `main/assets/fonts/studio/`.
Regenerate with fonttools and lv_font_conv 1.5.3:

```sh
python3 tools/regenerate_studio_fonts.py --converter lv_font_conv
```

## Validation and limits

Real upstream LVGL v9.5.0 host checks cover Dashboard/shell bounds, all eight
routes, thumbnail hit-testing, print action availability, E-stop recreation,
unknown telemetry and repeated teardown across densities and text sizes.
The new page fixture checks Printer preview binding and fullscreen entry, Toolhead Control entry, M220 tuning,
unsupported fan isolation, popup teardown, Drybox online/offline and humidity
range, Console history paging/filter changes, and Settings component bounds.
Files checks cover inspector ownership, metadata readiness and full-detail updates.
Chooser checks cover deferred/deduplicated opening, cancellation, selection and
five-theme layouts. Camera checks cover transforms/fullscreen and frame ownership.
Full Settings and all rewritten page modules pass strict host compilation.

Native Dashboard renders: [standard](images/studio-dark-native.png) and
[large text](images/studio-dark-large-native.png). They are actual LVGL output
with fixture data and a thumbnail placeholder, not physical panel photographs.

The existing live-widget refresh fixture's Devices invalidation assertion fails
with this host archive on unchanged `52a890d` as well as the first trial; it is
recorded as a baseline failure. Host tests do not validate P4 stack headroom,
firmware link size, display performance or the complete hardware behavior.
ESP-IDF compilation and panel testing remain required.

## Panel verification

The local installer validates every target file before writing, accepts the
original trial or the chooser-fixed trial, and refuses unrelated edits to target
files. It performs no branch switch, commit, merge or push. It can build with
`tools/build_idf6_hosted3.sh`, then flash and monitor the same artifact using
`-B build-idf6-hosted3 -D SDKCONFIG=sdkconfig.idf6`, avoiding the earlier ELF mismatch.

Check all eight pages, scrolling in Files/Settings, long names, large text,
control availability, preview/camera changes, printer switching, chooser reopening,
sleep/wake and reboot restoration. Exercise print and runout workflows with the
same care as the accepted firmware. Changing the factory default is a separate product decision.

## Tool destinations and printer chooser width follow-up

Printer profile manager/editor dialogs now use the STUDIO screen width with
24 px outer margins. Calibration uses two 480 px columns and two 190 px rows
inside the fixed stage; its bottom actions remain visible. Devices fills the
stage, showing twelve entries per page in a vertically scrolling two-column list with Previous/Next pinned at the bottom.
Bed Mesh uses a wider 776 × 250 canvas and keeps its gesture hint and actions
inside the stage. Macros fills the stage width without the legacy inner rail inset.
Legacy themes retain their geometry. Native checks include all five themes for
profiles and Devices, and STUDIO Calibration/Bed Mesh bounds for all densities
and text sizes. The follow-up is delivered as a local apply/build script for OTA;
no Git operations are performed by it beyond reading the repository root.

## Chooser and vertical workspace correction

The topbar opens `ui_printer_chooser.c`, a separate destination from the profile
manager. Its four STUDIO cards now fill two 480 px columns and fit the stage.
Devices has a vertical filter column with 44 px touch targets and a 304 px-high
reading viewport (previously 238 px). Macros places search/reset beside the title;
its list now has at least 340 px of height. The compact controls are checked for
single-line text and bounds in normal and large text modes. Legacy theme layouts
remain unchanged. This correction follows `fix_studio_widths.py` and is provided
as another guarded local apply/build script for OTA.

The STUDIO Tools landing page uses the original two-by-two utility card layout
with Studio styling and no side index. All four cards fit the fixed stage without
scrolling; destination callbacks remain.

## Printer tuning widget lifetime correction

Tuning slider and value references now belong to the Printer page that creates
them. Deleting that page clears them in every theme; creating the next page
starts with empty widget references while pending tuning commands persist.
This fixes the stale Operator slider used by Studio refresh after a theme switch.
Host regression checks repeat Operator teardown and refresh, then Studio creation,
tuning popup close/reopen and page deletion across densities and text sizes.

## Theme chooser scroll follow-up

The built-in chooser uses an opaque scrolling viewport, with inherited preview
effects removed and nested scrolling disabled on the preview grid. Operator Shell
selects on a completed click rather than touch-down, allowing swipe gestures.
Dialog checks cover all five themes, densities, text sizes and screen widths;
physical scrolling performance remains a panel check.

The miniature built-in previews now render once into per-card RGB565 canvases
in PSRAM. Scrolling reuses those pixels instead of rendering each nested sample
widget again. Closing releases every buffer; allocation failure retains the
widget-based preview. Names, descriptions, selection and pinned actions remain
regular widgets. Tests cover repeated opens, all themes and buffer cleanup/fallback.

## Theme-dependent chooser scrolling correction

LVGL draws the active screen before top-layer modals, so scrolling the chooser
repainted the Settings page underneath. The built-in chooser now uses a
screen-local modal with the existing blocker and cleanup behavior. Its opaque
viewport can occlude the underlying page. Other modals retain their top-layer
placement. A real-LVGL draw-event regression fails with the previous top-layer
placement and verifies zero background-page draws during chooser scroll for
all five themes, densities, text sizes and tested widths. Panel timing still
requires verification.

## Acceptance and integration

The operator accepted the current STUDIO page composition and theme chooser
scrolling. The Printer overview replaces duplicate jog controls with a large
profile-owned model preview, live XYZ and print telemetry; Toolhead Control
retains the existing popup. Its runtime Z-offset reading occupies a separate
row below its caption. Devices shows up to twelve entries per page with vertical
scrolling; Tools uses a two-by-two card grid.

Closeout removes the trial display labels, builds the accepted sources, merges
the feature branch into main and runs the existing end-of-night checkpoint.
Stable identity stays 6.5.6. Printing, runout recovery and extended hardware
soak checks remain pending; acceptance does not substitute for those checks.

## CI whitespace cleanup

Commit `9b92f796ecfa` (October 9, 2026 in America/Chicago) removes trailing spaces from the Studio font license and layout audit, and extra blank lines at EOF from the generated Inter font C files. This is a formatting change; font data, license wording and runtime behavior are unchanged. It addresses the CI whitespace-check failure. No new nightly is needed for this cleanup.

## Printer setup dialog follow-up

The nested authentication, certificate, camera and shared value-entry dialogs now use the responsive profile helpers. See [LVGL modernization](LVGL_MODERNIZATION.md) and [Testing](TESTING.md) for the scope and pending panel validation.

## Printer preview separator

The Studio Printer page places a 1 px theme-border divider in the 20 px gap between the preview/telemetry area and thermal column, inset 8 px at the top and bottom. The divider is non-interactive and does not change either content area or preview geometry.

## Network stage and Files inspector follow-up

Network now uses the full 976 × 424 Studio stage: Wi-Fi and Moonraker status columns, separators, a 188 px tall scrolling network list, and Scan networks / Manage printers actions. Connection and scan status messages stay stationary with ellipsis for overflow; the scan status area allows two lines for “Connecting to:” and the SSID. Long Wi-Fi/Moonraker values and SSIDs in scan rows scroll on one line. The Files inspector keeps its existing 352 × 228 preview; its filename scrolls on one line and Print / Details / Cancel share the action row. File details uses a scrolling body with a pinned Close action.

## Modern Setup Center

Setup Center now uses a responsive modal shell with Studio typography, pill navigation, active-step borders, readiness indicators and connection review cards. Wi-Fi scan results and password entry use a scrolling stage with pinned actions. The printer step opens the shared profile editor for discovery, security, authentication, cameras, testing and saving. Camera discovery/test/save and final completion use the same layout language.

## Optional camera continuation

Camera discovery and verification keep a pinned Skip camera action. An empty result, unavailable printer, discovery failure or camera-test failure can continue to Review without a camera. A verified camera offers Use camera and Skip camera; selecting another camera resets the save action until verification succeeds.

## Files Details button-label correction

Studio actions now refit and center their labels when the button width changes. Temporary widths cannot produce negative label widths. This corrects the blank Close label in the Files Details popup and keeps Studio action text stationary on one line. The Files inspector metadata now scrolls within its own viewport, preserving the 352 × 228 preview and pinned actions. Details and the other themes’ metadata panes show a scrollbar when content overflows.

## Files print confirmation and refresh

Print opens a themed confirmation showing the filename and active printer. Cancel returns to the preview; Start print alone calls the existing print-start bridge. A failed start now shows a visible toast. Studio file rows refresh in place while the embedded inspector remains open. Actual Details/confirmation modals defer row publication until closed.

## E-stop follow-up

The global E-stop now uses the immediate Moonraker emergency endpoint. Stop and recovery failures show visible feedback; successful transmission is labeled STOP REQUEST SENT. Real LVGL pointer checks pass across all built-in themes. Verify shutdown on the selected printer before attempting recovery.

## Details Close sizing and recovery layouts

The File details Close button is compact and right-aligned rather than stretched across the modal. E-stop and restart dialogs keep their actions visible independently of scrolling text, show the printer name separately, and adapt to large text. Panel validation remains pending.

## Visible Devices actions

Tools > Devices now shows GRAPHS and ENDSTOPS in the upper-right header. Graphs opens existing temperature/humidity telemetry; Endstops opens existing live limit-switch status. The header anchor fix retains the earlier catalog layout. Panel validation remains pending.

## Modern telemetry page

Tools > Devices > Graphs opens the native telemetry instruments. Heat, Motion and Environment controls share the range/hold toolbar; Heat adds the hotend and target/detail choices. Graph cards show live values, scale/extrema and current-sample dots; scroll for additional channels. Holding freezes graphs while values stay live. Studio fills its 976 x 424 stage, and the same responsive layout serves the other themes. Pending panel checks are listed in [Telemetry](TELEMETRY.md).
