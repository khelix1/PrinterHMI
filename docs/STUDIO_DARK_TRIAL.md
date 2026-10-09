# STUDIO Dark trial

STUDIO Dark is a separately authored LVGL 9.5 theme and Dashboard composition,
introduced from main `52a890d48d3f` after the accepted Settings/large-text closeout.
It is opt-in under **Settings → Interface Theme → STUDIO DARK**. The saved
selection survives reboot. Factory defaults remain Operator until panel
validation is accepted. Stable firmware identity remains 6.5.6.

## Design

Opaque graphite surfaces, warm ivory text, violet controls, lime progress,
24 px screen margins, and an eight-entry bottom dock. The theme owns its
surface/button/typography recipes independently; it does not call Theme A,
B, C or Operator Shell style recipes. Data, thumbnail ownership, observer
bindings, transport, print-action resolution and cancellation dialogs remain
shared application behavior.

The Dashboard uses a 976 × 424 workspace with a job/preview column and separate
thermal, drybox-environment and process tiles. Speed and flow are actual live
values rather than tuning percentages. Unsupported components show N/A and
unknown readings show placeholders. The mockup's drying timer is not invented.
Pause and Resume share a slot; Stop invokes the existing cancel-print flow;
Cancel Object remains available. The header retains printer selection, Wi-Fi,
clock and the immediate E-stop action. Bottom navigation preserves all eight
routes. The old footer Wi-Fi label is hidden to avoid overlap with the dock.

Auxiliary pages use the new styles in an adapted vertical viewport, preserving
existing content/control layouts during this first hardware trial. These pages
are not yet individually redesigned to the Dashboard's composition. Camera
normal/fullscreen sizing adapts to the dock; fullscreen still occupies 1024 × 600.

## Fonts and assets

Subsetted Inter glyphs: regular 18 px; semibold 20/24 px; bold 28/32/48/64 px,
ASCII plus degree symbol, 4 bpp without compression. Generated sources are
committed and compiled directly; firmware builds do not download fonts or run
font generation. The original variable font and SIL Open Font License are in
`main/assets/fonts/studio/`. Source: Google Fonts' `ofl/inter/Inter[opsz,wght].ttf`.
Regenerate with fonttools and lv_font_conv 1.5.3:

```sh
python3 tools/regenerate_studio_fonts.py --converter lv_font_conv
```

Dock icons are native LVGL line geometry, with no external icon/font dependency.

## Validation

Host LVGL source: upstream v9.5.0 (`85aa60d18b3d`). Native host checks cover:

- Dashboard, header and dock bounds, all densities, normal and large text.
- Thumbnail pointer hit-testing through the transparent status overlay.
- Pause/Resume/Stop/Cancel Object routes and availability states.
- Eight navigation routes; unknown/unsupported temperature handling.
- Stubbed M112 route, E-stop recreation, repeated shell/page teardown.
- Auxiliary-page scrolling and protection of nested small empty-state objects.
- Five-theme chooser layout/switching and custom-theme dialog behavior.
- Existing button-text, responsive Tools/Files/Macros, camera-pipeline and boot-splash regressions.
- Full Dashboard translation-unit host compilation and repository audits.

Native renders: [normal](images/studio-dark-native.png) and [large text](images/studio-dark-large-native.png).
These renders use fixture readings and a thumbnail placeholder;
they are real LVGL output, not photographs of the physical panel.

The status-observer regression also passes. The existing live-widget refresh
fixture fails its Devices repeated-refresh invalidation assertion at line 65
with this host archive on both unchanged `52a890d` and the trial. This is recorded
as a baseline host-test failure, not a passing check.

Firmware compilation, flash, display timing/performance, saved theme restoration,
long names, live camera/thumbnail changes, print lifecycle, runout recovery,
all nested dialogs and every auxiliary page still require on-device validation.
The host build does not substitute for an ESP-IDF firmware build.

## Panel trial

Build using `bash tools/build_idf6_hosted3.sh`, then flash with the established
ESP-IDF 6.0.2 workflow. Select STUDIO DARK and open Dashboard. Check the preview,
print progress/layers, current/target temperatures, Pause/Resume/Stop and Cancel
Object, all eight pages, large text, dialogs, camera fullscreen, printer switching,
reboot restoration and sleep/wake. The previous themes remain available for
returning to the accepted layout. Do not promote the default before panel acceptance.
