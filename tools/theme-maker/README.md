# PrinterHMI Theme Maker

The primary target is a Linux Mint desktop application. See
[Linux installation and source launch](linux/README.md). The `.deb` adds an
application-menu entry and opens the bundled editor in its own GTK/WebKit
window, with native file dialogs and persistent local drafts. It requires no
hosting, browser tab or HTTP server.

The five editor assets can also be opened directly using `index.html` during
source development, but the Linux app is the intended user-facing product.

Click the **P4 / Theme Maker button at the upper left**, or **New theme**.
Choose **Start from scratch** for an independent neutral palette, square
corners and an empty dashboard. Layout lists the printer blocks available for
the selected page: **＋** adds a block and **●** selects an existing one. Drag
to move, use its corner handle or numeric fields to resize, and remove blocks
with **Remove selected block** or Delete. Arrow keys move the focused block;
Shift moves ten pixels. Canvas tools add optional 4, 8, 16 or 32 px grids
and snapping. Hold Alt during a drag to bypass snapping. Align to the canvas
edges or center, or fill its width/height with a 16 px inset. These actions
participate in Undo. A cancelled pointer drag restores its starting geometry.
Grid preferences are saved separately from exported firmware packages.
Overlapping blocks produce an advisory notice; overlap remains allowed. Undo restores previous compositions. You can also
choose one of the six visual presets. Switch pages or open Components to inspect the result.
Project holds the package identity. Themes without artwork export as an
ordinary `.phmitheme` file. Artwork themes export a ZIP bundle; copy its
`PrinterHMI` folder to the SD card root, then rescan/select the theme in
Custom Themes. Undo/redo includes imports and preset replacement.

The browser attempts to save a local draft. Storage availability and `file://`
behavior depend on the browser; exported files are the portable saved copy.
Imports are validated before the current project is replaced. Palette defaults
and legacy layout rectangles are generated from the firmware source tables.

## Background images

Open **Design → Background image → Choose image**. Select PNG, JPEG or WebP,
then choose Cover (crop), Contain (letterbox) or Stretch. Horizontal/vertical
position controls the placement; the overlay color and strength improve text
readability. Remove background returns to the prior renderer's version 1/2
format. Undo includes image replacement, removal and settings changes.

The app accepts at most 12 MiB / 16 megapixels, applies EXIF orientation and
keeps a normalized editable PNG up to 1600 × 1600 / 4 MiB in its local artwork
cache. Export bakes placement and overlay into a 1024 × 600 RGB565 panel image.
The bundle includes the editable normalized PNG, JSON metadata, binary panel
image, and installation instructions. Import the complete ZIP to keep editing;
a standalone artwork `.phmitheme` is not enough to restore its source image.
Desktop drafts reference the artwork cache rather than storing large images in
browser local storage. Export a ZIP to move the design to another computer.

Artwork appears behind page contents. Header/navigation and opaque cards keep
their own fills. The native preview uses the same image-composition code as the
exporter; the panel additionally quantizes colors to RGB565. High-contrast
accessibility suppresses artwork. The HTML component contents are illustrative.

## Compatibility

| Base | Export | Firmware |
| --- | --- | --- |
| Operator, Foundry, Glass | Version 1, 27 colors, legacy geometry | Released v7.0.0 or this branch |
| Studio | Version 2, 58 colors, native Studio composition | This branch's updated loader |
| Any base with artwork | Version 3 metadata + prepared image, ZIP bundle | This branch's artwork firmware |

Studio exports omit legacy rectangles and require opaque surfaces. Studio
palette tokens and shared card/panel/button corner tokens are customizable;
special-purpose elements such as pills, dialogs and navigation retain their
recipe-specific shapes. Native Studio page composition remains fixed.

The panel preview is illustrative HTML, not the LVGL renderer. Legacy pages
now render blocks at the **stored package rectangles** with illustrative
contents. Hidden blocks are omitted, so a new blank dashboard really is empty.
Modern page code can adapt or override this geometry. Telemetry metric entries
retain complete schema rectangles but its legacy profile only stores their x
coordinates; the current telemetry page uses its own adaptive composition.
Use the actual panel to assess final layout and font fit.

## Direction: a visual designer

The goal is a maker capable of very different visual identities: clean light
interfaces, industrial consoles, retro terminals, playful designs, and artwork
led themes. This implementation includes an offline editor, blank block canvas, and safe
export contract. Blank themes use the Operator rendering contract for
compatibility; their neutral palette and page geometry are authored separately
from the preset themes. The header/navigation and block internals remain
firmware-owned. Settings content is initially retained so the panel can still
reach Custom Themes; telemetry remains an adaptive firmware-owned page. It does **not** yet implement custom fonts,
shadows, gradients, freeform component composition, or animation editing.
These require additional firmware rendering and asset-loading contracts.
Unsupported features must be validated explicitly, never previewed as though
an exported device package could already reproduce them.

## Development checks

```sh
python3 tools/theme-maker/generate-spec.py --check
node tools/theme-maker/model-test.js
node tools/theme-maker/editor-flow-test.js
python3 tools/theme-maker/linux/artwork-test.py
node --check tools/theme-maker/app.js
python3 tools/audit/custom_theme_package_test.py \
  --lvgl-dir /path/to/lvgl --cjson-dir /path/to/cJSON
python3 tools/audit/theme_default_test.py --lvgl-dir /path/to/lvgl
python3 tools/audit/studio_pages_test.py --lvgl-dir /path/to/lvgl
```

`custom_theme_package_test.py` compiles the actual SD package parser, actual
runtime token helpers and actual layout profiles, with real cJSON and LVGL color
functions. Only ESP heap/log calls are substituted. It runs with undefined
behavior sanitization. This does not replace an ESP-IDF target build or panel
visual review.

## Studio preview

The Studio starting point retains the firmware's original 58-color palette.
Its panel preview uses the native 1024 × 600 shell, eight navigation icons,
Inter font, open Dashboard composition, temperature column and telemetry strip.
All ten Studio preview pages follow their firmware page recipes, with an
offline tablet state model for local interaction. Camera video and chart data
are samples; printer and network operations are simulated.
RGB565 color quantization matches the native display. Font rasterization and
caption fallback can differ from LVGL. This is a layout preview, not a connected
printer or a live LVGL emulator. Clicking navigation changes the sample page;
print and machine controls never send commands.

Existing drafts are retained. Choose **P4 → Studio** to reset to the original
Studio palette; Undo restores the previous design. The preview update itself
changes only the desktop app and requires no firmware rebuild or OTA.

The bundled Inter font and its SIL Open Font License are in `assets/`.
Run `node tools/theme-maker/studio-scene-test.js` alongside the checks above.

## Editable projects

Use **Save project** (Ctrl+S) to save a `.phmiproject` file and **Open project**
(Ctrl+O) to resume it. Each save opens a native file chooser, so you can keep
multiple designs or save a new copy. Overwriting an existing file requires the
native dialog confirmation. Projects include the theme, selected page/editor
view, grid/snap settings and original normalized artwork when present. Copy
that single file to another Linux computer to continue editing. A project does
not depend on the old computer's artwork cache. Opening a project keeps the
previous theme in Undo; cancelling or rejecting a file leaves the draft intact.
Projects do not include undo history. The existing local autosaved draft remains.

**Export theme** produces panel files; `.phmiproject` files are editor documents
and are not installed on the panel. Firmware theme imports remain available
through **Import theme**. Projects use a versioned, bounded archive with fixed
entry names, validated theme metadata and a source-artwork checksum.

Studio's **Components** view now follows the native button, input, ON/OFF
settings row, progress, slider, feedback surface and confirmation dialog recipes.
These are visual specimens; they do not send printer commands. Colors and
supported corner settings update as you edit. Input/dialog fixed corners follow
the firmware; font rasterization and caption fallback can differ from LVGL.

Additional checks:

```sh
node tools/theme-maker/project-model-test.js
python3 tools/theme-maker/linux/projects-test.py
```

## Pick a Studio color

With a Studio theme open, choose **Pick color** above the preview, then click
a label, surface, border, progress track or navigation icon. The matching color
control appears at the top of Design. Button labels offer **Text / icon**,
**Fill** and **Border / line** so you can choose the part to edit. Every use of
the selected shared palette token is highlighted while picking is active.

Edit the hex value or use its color chooser. **Reset … to Studio default**
restores that one color; Undo restores your previous value. Changes apply to
all controls sharing that palette token. Clear selection closes the inspector.

Picking works in Panel preview and Components. Preview navigation is paused
while picking; page tabs still work. Toggle Pick color off or press Escape to
resume normal navigation and remove highlights. The tool is unavailable for
legacy layouts and the Package view. The existing palette controls remain
available with the keyboard. Artwork stays under Background image.

Picking state and highlights are editor-only and are never written into the
firmware theme or editable project file. No firmware update is needed.

## Compare with original Studio

Choose **Original Studio** above the preview to see the built-in Studio palette
and corner settings on the selected page. The original preview omits custom
artwork. **Back to my theme** restores your design, including its artwork.
Comparison works in Panel preview and Components; page navigation stays usable.

A visible banner identifies the original preview. Picking is disabled while it
is shown. Editing a design property or using Undo/Redo returns to your theme.
Comparison does not change the draft, theme, history or project format. Export
and Save project always save your own design, including while comparing.
Package view and legacy themes do not offer this Studio comparison.

The Linux shell no longer calls WebKit's deprecated hyperlink-auditing setting.
Offline content policy and navigation/permission restrictions remain in place.

## Interactive Studio tablet preview (preview11)

All eight navigation pages now use the Studio firmware page geometry: Dashboard,
Printer, Files, Camera, Tools, Console, Drybox and Settings. Network and Telemetry
are also accessible through their in-tablet controls or page tabs.

Click the tablet controls to try pause/resume, confirmations, temperature targets,
file selection/search/print, camera setup, macros, console commands and Settings.
Scroll Settings, file lists, console history and graphs with the mouse wheel;
Settings, files and graphs also support dragging. Input dialogs accept physical
keys and the on-screen keyboard. Escape closes dialogs or fullscreen. Reset tablet
restores the sample machine without changing the theme. Pick color temporarily
pauses tablet interaction.

The simulation is isolated from project data and never contacts printers, cameras,
Wi-Fi or update services. Settings, calibration, backup and transport operations
use local demonstration results. Page layouts follow native builders; dialogs
currently share a preview recipe rather than reproducing every native dialog.
This SVG simulation is not an embedded LVGL runtime: panel font rendering, live
video, timing and hardware behavior still require a physical tablet check.
Legacy bases retain their existing illustrative canvas previews. No firmware
files are changed by this app update.

## Interactive tablet preview

Studio Panel preview now has local tablet interactions across Dashboard, Printer,
Files, Camera, Tools, Console, Drybox, Settings, Network and Telemetry. Use the
eight-item dock, page tabs, or Settings/Devices links to navigate. Scroll within
Settings, Files, Console and Telemetry with the wheel; Settings/Files/Telemetry
also support touch or mouse drag. Forms accept physical keyboard input and the
on-screen keyboard; Enter submits and Escape closes the current overlay.

Pause/resume, print confirmation, toolhead tabs, temperature targets, drybox
programs, file selection/search/sorting, console commands/filters, graph tabs,
range/hold/targets, camera controls and settings choices change only local
sample state. Use **Reset tablet** to restore the initial scenario. Tablet state
is not saved into themes, projects or firmware. Changing a theme color still
updates every preview page and any open tablet dialog.

This is a source-based SVG/HTML simulation, not the compiled LVGL application.
Primary Studio page layout and ordering come from firmware builders. Secondary
configuration and machine workflows use representative local dialogs. Live
camera frames, discovery, history, firmware/backup operations and physical machine
behavior are simulated. Fonts and rasterization can differ from LVGL.
See [TABLET_PREVIEW.md](TABLET_PREVIEW.md) for source mapping and validation.

```sh
node tools/theme-maker/tablet-model-test.js
```

Preview10 fixes the dialog footer hit areas: navigation is hidden as a whole
while dialogs/fullscreen are open, and outlined buttons accept clicks across
their full interiors.

## Preview11 popup and interaction review

Temperature and fan controls now use the firmware preset lists and 660 × 460
dialogs. Custom temperature entry uses a separate 600 × 500 window with Back/Set
and a numeric keypad. Tune follows the 680 × 280 two-slider native layout.
Stop/start/object confirmations use dedicated dimensions; object cancellation
requires confirmation. Camera View uses rotation and independent horizontal and
vertical mirroring with Reset. Accessibility options toggle independently.

Choice lists no longer use the generic two-column action grid. There is one
footer, without duplicate Close buttons. Keyboard focus skips obscured tablet
actions. Console scrolling is clamped after filter changes, and sorting files
returns to the top. A 50-route dialog regression checks nested flows and native
source dimensions.

This remains a reconstructed SVG preview. It is not a near-perfect firmware
simulator: secondary hardware/setup/backup/update dialogs still use local fixture
workflows, and some settings do not reproduce a native UI rebuild. A shared LVGL
host runtime is required to remove those remaining implementation differences.

## Preview12: shared native LVGL dialogs

In the Linux app, Studio's nozzle, bed, fan, Stop Print, Toolhead and Tune
dialogs now run the actual firmware C implementations and bundled Inter fonts
in a local LVGL child process. Pointer gestures, the on-screen keyboard and
physical keys go through LVGL. Custom colors and corners are supplied using
the firmware color-route table. The backend holds simulated machine state and
never sends commands to a printer.

The native overlay appears directly inside the panel preview. Its footer
buttons are the actual firmware widgets. Escape closes a dialog, or goes back
from a nested temperature editor. Page changes and design edits retire the
old overlay; delayed frames cannot overwrite a newer preview.

The .deb now contains a compiled helper and therefore uses the host architecture
suffix (`amd64` on the development/Mint desktop), rather than `all`. No firmware
build or OTA is needed. Running from source uses the installed helper when
available. To rebuild the helper from the checkout instead:

```sh
python3 tools/theme-maker/linux/native/build.py --lvgl-dir managed_components/lvgl__lvgl
```

This needs a C compiler, `ar`, and the LVGL source dependency used by PrinterHMI.
The build compiles the shared UI sources; no UI code is copied into the helper.
The small Tune bridge includes its native page implementation to expose its
private dialog. Only platform and printer transport are replaced by host fixtures.

This is the first shared-renderer stage. Main pages and the remaining secondary
dialogs still use the SVG preview. Native dialogs are identified by the
**Native LVGL popup** note below the panel. Without a native helper, source-only
runs retain the existing SVG behavior. Linux Mint GTK/WebKit integration still
needs a desktop run; host rendering, pointer/keyboard callbacks, native palette
overrides, nested dialogs and cleanup are tested here.
