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

Legacy panel previews use illustrative HTML. Migrated Studio pages use the
shared LVGL runtime. Legacy pages render blocks at the **stored package rectangles** with illustrative
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
Migrated Studio pages use the embedded LVGL runtime; remaining pages and
secondary workflows use the SVG fallback. Hardware timing and live transport
still require a physical tablet check.
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

Migrated Studio pages compile their actual firmware builders into the LVGL
host; remaining pages use the source-based SVG/HTML simulation. Secondary
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

The remaining SVG pages are not yet a near-perfect firmware
simulation: secondary hardware/setup/backup/update dialogs still use local fixture
workflows, and some settings do not reproduce a native UI rebuild. Those pages still need migration to the shared LVGL
host runtime.

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

## Preview13: click fidelity and native Tools page

The normal preview no longer treats Dashboard temperature/filament/telemetry
readings or the Drybox target readout as buttons: those are display-only in the
firmware. Printer temperature controls use their native region sizes and its
actual fan readout is clickable. Disabled print actions have no action metadata
or keyboard target. Settings brightness uses only its slider, and clipped-out
rows are excluded from keyboard focus. Pick color remains an explicit editor
mode, so display-only labels can still be selected for styling in that mode.

The Tools page now runs its actual firmware builder together with the native
top bar, eight-button navigation, clock, connection indicators and E-stop.
Clicks on the brand, clock, separators and blank regions do nothing; real
navigation and tile callbacks route back to the offline simulation. The native
UI helper is version-checked before use. Tab can leave the native preview for
the editor; typing in an editor field does not feed the native keyboard.

Native Tools is enabled in Studio's Panel preview. Pick color and artwork
projects retain the SVG Tools view while native artwork rendering/palette hit
maps are pending. Other pages remain reconstructed; this is an incremental
shared-renderer migration, not a claim that the entire tablet is native yet.


## Preview14: native hover and Console

The native overlay now asks LVGL which actual widget is under the pointer.
Tools tiles, navigation and other controls show the hand; static labels,
separators, blank space and modal-blocked controls retain the arrow. Editable
text fields show the text cursor. Hover never presses or focuses a control.
Delayed replies from an old page or a pointer that has left are ignored.

Console now compiles `main/ui_console.c` directly: filters, temperature hiding,
search, Follow/Clear, command history, command editor and its keyboard/Close
use the firmware widgets and callbacks. A bounded local console fixture records
commands and simulated responses; it has no printer or network transport.
Native Console follows the same Studio/Panel/no-artwork/no-Pick-color conditions
as native Tools. Other full pages and Tools subpages still need migration.
The helper capability is version 3; rebuild or install the preview14 package.
Pointer/key host tests and editor event tests cover this change; actual desktop
cursor appearance still needs a Linux Mint GUI run.


## Preview15: native Printer page

Studio's Panel preview now uses the actual Printer page assembler from
`main/main.c` and the shared layout, live status, temperature/fan cards and
action controller. The build extracts the complete assembler and callbacks
from this checkout; their geometry is not copied into the desktop renderer.
Native coverage includes Tune, Toolhead, nozzle/bed/fan adjustments, filament
status and recovery, Pause/Resume, stop confirmation, object selection and
exclusion confirmation. The image well and fullscreen lightbox use shared
firmware fitting, rendering and close handling with a small local sample image.
Only asynchronous image fetching is replaced with a local-image adapter.

The fixture supplies two cancellable objects, a loaded filament sensor and
recovery macros. Commands update in-memory state only. Print state and job name
are seeded from the editor, and native state changes carry across page
navigation. Disabled Home/Resume/Pause/Stop actions keep the arrow and ignore
clicks. Escape dismisses the foreground modal while keeping its native page;
physical keyboard cancellation still uses the actual editor callbacks.

Native Printer, Tools and Console currently require Studio's Panel view without
artwork or Pick color. Dashboard, Files, Camera, Drybox, Settings and Tools
subpages still need migration. The whole preview is not yet a near-perfect
simulation. Helper capability 4 and the preview15 package are required.
Fourteen host pointer/key tests cover native page/dialog behavior, alongside
editor regression checks. Linux Mint GUI verification remains a user check.


## Preview16: native Dashboard

Dashboard now compiles `ui_dashboard.c`, its page/layout profiles, Active Print,
Machine Status, command bar and live banner/telemetry writers directly from
`main/`. Native print controls use the shared stop/object confirmation and
filament recovery dialogs. Temperatures, filament status and bottom telemetry
remain display-only, matching the firmware. Disabled print actions keep the
arrow and do not dispatch commands. The actual thumbnail canvas, proportional
image packing and fullscreen lightbox use the local sample image.

Native coverage is now Dashboard, Printer, Tools and Console in Studio's Panel
view without artwork or Pick color. The Dashboard fixture has no camera
configured, so its real camera toggle stays hidden; live camera simulation and
the remaining full pages still need migration. No network transport is linked.
Job name, print state and progress carry between reconstructed/native pages;
starting a sample file at zero percent no longer restores the 64 percent fixture.
The helper uses capability 5 and the preview16 package.

The host build publishes its executable atomically with explicit executable
permissions. Eighteen pointer/key tests exercise native behavior alongside the
editor and model suites. Linux Mint visual verification remains a user check.


## Preview17: native Files page

Files now compiles `main/ui_files.c` and the shared page-state component.
The local sample library supplies six root files and a Calibration folder;
firmware widgets own rows, scrolling, folder controls, search/keyboard, sorting,
thumbnail fullscreen, metadata sizing, overflow-only Details, Cancel and the
print-confirmation dialog. Refresh preserves an inspected file. Short metadata
keeps the wider Print/Cancel actions; long metadata exposes the real Details
popup and scroll body. Print follows actual idle/readiness and metadata gates.
Stop the existing sample job before trying to start another file.

Confirmed starts update the simulated filename, print state and zero progress,
then route to native Dashboard. The frame protocol now carries the job name.
There is no filesystem or printer transport: selections resolve only against
the fixed sample library, and commands update the local in-memory machine.

Mouse wheel events now scroll the actual LVGL ancestor under the pointer,
respecting modal blockers and viewport limits. They are bounded, batched and
ignored when their originating overlay is retired; hover uses the same owner
check. Native coverage is Dashboard, Printer, Files, Tools and Console under
the existing Studio/Panel/no-artwork/no-Pick-color conditions. Camera, Drybox,
Settings and Tools subpages remain to be migrated. Capability 6 and preview17
are required. Twenty-three native interaction tests and editor/model checks
cover these flows; Linux Mint visual verification remains a user check.


## Preview18: native Camera viewer and setup

Camera now compiles the actual `ui_camera.c` viewer and the Camera setup path
from `ui_printer_profiles.c`. The real widgets own camera selection/default,
rotation, independent horizontal/vertical mirroring, Reset view, fullscreen,
field keyboards, URL validation, Find/Test feedback, removal confirmation,
Cancel and Use camera. Two process-local sample feeds and synthetic discovery/
test results replace camera, profile-storage and network backends. Entering a
stream URL never contacts it. A URL containing `fail` or `unavailable` exercises
the viewer's failure/Retry path; Find restores the sample camera entries.

Normal viewer image taps have no action and retain the arrow; fullscreen taps
and Escape restore the viewer. Physical typing follows the real keyboard's
attached textarea, including setup dialogs on the screen layer. Cancelling a
test or search does not leave the fixture busy indefinitely. Dashboard's sample
frame provider keeps its Camera/Thumbnail controls above images and hands live
view to the native Camera fullscreen surface. Page teardown closes setup and
nested keyboards and releases viewer buffers/timers.

Camera settings remain in the simulation until Reset tablet or app exit.
Reset tablet now retires the native fixture process too, restoring the sample
camera catalog and machine without altering the theme/project. The host uses
capability 7 and the preview18 package. Native full pages are Dashboard, Printer,
Files, Camera, Tools and Console; Drybox, Settings, Network/Telemetry and Tools
subpages still need migration. Pick color and artwork use the existing fallback.
Thirty-one native interaction tests plus editor/model regressions pass. A Linux
Mint GUI check remains pending; these tests do not claim hardware equivalence.

## Preview19: native Drybox and command notices

Drybox now compiles the unchanged firmware `ui_drybox.c` controller and
`ui_drybox_page.c` layout. The Studio page has five program buttons: PLA, PETG,
Hold, Resume and Stop. Air/center temperature, humidity arc, target temperature,
heater and fan readings stay inert; only actual firmware actions show the hand
cursor. The native navigation shelf and emergency stop remain available.

The host macro fixture confirms selected/active programs and heater/fan state
locally. Sample PLA/PETG targets are 45/55 C. Hold suspends heating, Resume
restores the selected sample program, and Stop clears the active program.
These sample responses do not reproduce a user's Klipper macro configuration
or simulate physical heating. Drybox state survives native page changes and
resets with Reset tablet or app exit. The native initial state is idle with
sample readings 28 C air, 29 C center and 18% RH.

The real `ui_toast.c` notification queue, card layout and Dismiss callback now
replace the host's silent notification stub. The firmware default Quiet level
suppresses routine command notices; failures still show persistent error cards.
Notification preferences/logs use process-local storage and the sample Console.
The status callback is extracted unchanged from `main.c`; Studio does not expose
a status button, so the host adds none. Duplicate suppression, offline/error
button disabling, failed-send state retention/retry and teardown use real UI
controller behavior. Test-only host inputs `drybox-fail`, `drybox-sensors` and
`notify` cover failures, telemetry changes and notice levels without network I/O.

The helper uses capability 8 and package preview19. Thirty-seven native tests
cover the migrated pages, including six new Drybox interaction tests. Editor,
model, artwork, desktop I/O and project tests pass. Linux Mint WebKit visual
verification remains pending. Native full pages are Dashboard, Printer, Files,
Camera, Tools, Console and Drybox; Settings, Network/Telemetry and Tools subpages
remain on the existing fallback, as do color picking and artwork projects.
