# STUDIO Dark redesign trial

STUDIO Dark is an opt-in LVGL 9.5 interface for the 1024 × 600 ESP32-P4 panel.
Select **Settings → Interface Theme → STUDIO DARK**. The saved selection survives
reboot. Factory defaults remain Operator until the panel trial is accepted;
firmware identity remains 6.5.6. This update builds on the first local STUDIO trial
and includes its deferred chooser construction and 16 KB LVGL task stack fix.

## Composition

The theme owns its style recipes without calling Theme A, B, C or Operator style
recipes. Graphite, ivory, violet and lime combine with Inter typography, native
line icons, an inline printer selector and an eight-entry bottom navigation shelf.
The eight main pages use separately authored compositions:

- Dashboard: borderless preview, oversized progress, open thermal readouts and a
  horizontal telemetry strip. Values use real data and existing unknown/N/A states.
- Printer: XYZ jog workspace, step selection, thermal column and bottom action
  strip. Fresh online/homed/printing checks guard jog commands. Tune opens the
  existing speed/flow controls; Motion retains extrusion and Z-offset controls.
- Drybox: humidity arc, open temperature readouts and program capsules. Existing
  program commands and availability states remain authoritative; no timer is invented.
- Tools: typographic instrument index with full-width tool entries.
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
while these native layouts are the concrete trial implementation.

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
The new page fixture checks Printer jog guards, step selection, M220 tuning,
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

## Local panel trial

The local installer validates every target file before writing, accepts the
original trial or the chooser-fixed trial, and refuses unrelated edits to target
files. It performs no branch switch, commit, merge or push. It can build with
`tools/build_idf6_hosted3.sh`, then flash and monitor the same artifact using
`-B build-idf6-hosted3 -D SDKCONFIG=sdkconfig.idf6`, avoiding the earlier ELF mismatch.

Check all eight pages, scrolling in Files/Settings, long names, large text,
control availability, preview/camera changes, printer switching, chooser reopening,
sleep/wake and reboot restoration. Exercise print and runout workflows with the
same care as the accepted firmware. Promote the default only after panel acceptance.

## Tool destinations and printer chooser width follow-up

Printer profile manager/editor dialogs now use the STUDIO screen width with
24 px outer margins. Calibration uses two 480 px columns and two 190 px rows
inside the fixed stage; its bottom actions remain visible. Devices fills the
stage, showing four entries per page with Previous/Next pinned at the bottom.
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

The STUDIO Tools landing page now uses four full-width rows beneath its title.
Its redundant side index has been removed; all four destination callbacks remain.
