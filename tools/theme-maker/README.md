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

## Preview20: native Settings page and shared dialogs

All eight main Studio pages now use native LVGL under the existing Panel,
no-artwork and no-Pick-color conditions. Settings compiles the actual firmware
page/components and shared appearance, theme preview, time-zone, notification,
event-history, About, backup, controller-reboot and factory-reset dialogs.
Read-only firmware/storage/memory rows remain inert; only the brightness slider
changes brightness. Sleep timeout cycles OFF/5/15/30 minutes exactly as the
firmware does. Backlight dimming and sleep are represented in native frames;
the first wake touch is consumed by the actual wake shield.

Appearance changes rebuild the shell as well as Settings and preserve the page's
scroll anchor. The actual chooser switches between Operator and Studio with
native layouts; accent, density and accessibility use the real controllers.
Simulation preferences are separate from the theme/project being edited.
Notification choices use Cancel/Save and apply to other native pages. Event
history now uses the real bounded event log rather than the previous Console
adapter. Time-zone choices use the firmware's full zone table and rules within
the child process. Hardware identity, memory and SD values are sample data.

All storage is process-local. The NVS adapter stores bounded namespaces/keys
with commit behavior. Simulated controller reboot reloads this store and returns
to Dashboard while preserving the sample printer job. Factory reset and Reset
tablet retire the fixture process and restore initial sample settings. No host
restart, actual display change, device reset or firmware flash occurs.

Backup and restore use the real UI, minimum-length/matching-password validation,
progress, confirmations and removal actions. Their backend stores only sample
preferences in memory; password-protected backup checks a sample password but
performs no encryption and writes no files. Restore stages preferences for the
simulated controller reboot. Camera/profile fixtures are not included in these
sample backups. The native custom-theme manager shows an empty SD catalog;
it does not mount or import the project being edited. Network, OTA and Setup
Center continue through the existing representative workflows. Network,
Telemetry and Tools subpages still need migration; artwork and picking retain
the SVG fallback.

The helper uses capability 9 and package preview20. Fifty native pointer/key
interaction tests include thirteen Settings checks covering inert rows, scroll,
brightness persistence/failure, sleep/wake shielding, appearance/shell rebuilds,
notification Cancel/Save, time-zone save failure, history/About cleanup, backup
validation/progress, repeated reboot and factory-reset routing. Editor/model,
artwork, desktop I/O and project regressions pass. Linux Mint WebKit visual
verification remains pending. Test-only `settings-fail` and `advance` inputs
exercise local storage/display failures and inactivity without real hardware.

## Preview21: native Network page and Wi-Fi connection workflow

Network now compiles the unchanged firmware `ui_network.c` page,
`ui_network_tools.c` password/keyboard UI, status controller, Wi-Fi scan helpers
and credential-store module. Its application assembly, scan, selection, password
save, connection dispatch and teardown callbacks are extracted from `main.c` at
build time. The Studio page exposes Scan networks, Manage printers, returned AP
rows and the normal navigation shelf. SSID, connection/IP, Moonraker host/port,
last HTTP and status readings stay inert. No legacy host/port edit buttons are
added; those belong to the profile manager in the real UI.

All Wi-Fi services are deterministic local fixtures. The initial sample is
Studio Network, with loopback IP and the existing simulation.invalid Moonraker
host. A scan reports ten sample APs and supplies the firmware's bounded eight
visible results. The actual list scrolls and long SSIDs use native marquee text.
The fixture retains scan completion through periodic refreshes. Selecting a row
opens the real password dialog; Back/Escape retires it and re-entry starts with
an empty field. Dummy passwords or blank input are accepted in the successful
sample mode; there is no real authentication, network scan, connection change,
DNS lookup or host Wi-Fi access.

Credential saving uses the real store API against the process-local NVS
adapter. Save/configuration/connect failures retain the native error and retry
flow. A simulated authentication rejection closes the accepted request and
leaves Network offline; existing machine-control connection checks then disable
controls until a successful local reconnect. SSID and connection state survive
native page changes, and the topbar uses the same fixture state. Reset tablet
or app exit restores the initial sample. Password buffers copied by the shared
connection code retain its cleanup behavior and never enter exported themes or
projects.

Manage printers deliberately retains the existing chooser fixture. Native
profile CRUD, endpoint testing and discovery are the next migration; their
previously stubbed services are not presented as working native management.
Network is reached through the real Settings connection row and keeps Settings
selected in the rail, matching the tablet. Telemetry, Tools subpages, OTA and
Setup Center still use existing representative workflows. Artwork and Pick
color retain the SVG fallback.

The host uses capability 10 and package preview21. Fifty-nine native tests
include nine Network checks: inert readings/navigation, scan completion and
bounded scrolling, password Back/Escape/re-entry, open sample connection and
SSID retention, failed credential commit, configuration/connect dispatch
failure, authentication rejection and recovery, empty/error/busy/timeout scan
retry, and repeated keyboard/page teardown. Editor/model, artwork, desktop I/O,
project and schema regressions pass. Linux Mint WebKit visual verification
remains pending. Test-only `network-fixture scan connect` selects these local
failure modes; it never invokes system networking.


## Preview22: native printer management and faster scrolling

The topbar printer menu now opens the firmware LVGL chooser. Network > Manage
printers opens the real four-slot manager, profile editor, text/numeric
keyboards, API-key dialog, transport/CA chooser, camera setup and removal
confirmation. Selection, Save, Cancel, final-profile guards and validation use
the shared firmware callbacks. The actual configuration and camera-catalog
controllers persist into process-local NVS; Reset tablet or app exit restores
the sample profiles. Each profile retains its own simulated job, targets,
position, Drybox state and camera catalog while switching between pages.
Profile changes refresh printer identity before Dashboard navigation, and
cannot restore online controls after Wi-Fi disconnects.

Discovery uses the actual request-bound firmware dialog, Retry and Close with
local timer-generated candidates. Endpoint checks and discovery do not scan the
network. The CA picker offers a synthetic simulation-ca.pem entry as a local
approval fixture; it does not read a host certificate, establish TLS or write SD
files. Plain sample backups omit API keys. The encrypted-backup adapter remains
an in-memory workflow fixture, with no cryptography or physical file export.

Scrolling no longer waits for a fixed 60 ms wheel interval. Input can dispatch
every 16 ms when the bridge is ready; while a frame is outstanding, wheel deltas
accumulate and idle refreshes pause. Fractional touchpad motion is retained,
large deltas are delivered in bounded chunks, and pending wheel input precedes
a following press. The native helper continues to enforce LVGL scroll bounds,
modal ownership and disabled controls. Mouse drag sampling also uses 16 ms, coalesces pending motion to the latest
position and delivers the trailing position before release.
A single desktop worker serializes native input and image work; GTK callbacks
return to the main thread. Identical idle rasters reuse PNG data, unchanged
images avoid redundant DOM updates, and changed frames use fast PNG encoding.

In a 30-request Settings benchmark in the development environment, median
wheel request time fell from 24.66 ms to 16.88 ms (about 32%); unchanged idle
frames measured 1.33 ms. These are bridge timings, not end-to-end Mint frame
rates. Smoothness and touchpad feel still need a Linux Mint desktop check.

Capability 11 and package preview22 include 73 native pointer/key tests plus
four desktop worker tests and editor flow tests for scroll backpressure,
fractional input, click ordering, profile identity and stale-page isolation.


## Preview23: native Telemetry

Telemetry now compiles the unmodified firmware page, chart components and
history buffer into the Linux helper. Under the existing Studio / Panel / no
artwork conditions, the page strip and Devices > Graphs open the actual LVGL
page. Thermal, Motion and Drybox views, 2/5/10-minute range, hotend selector,
Targets on / Detail only, Hold graph / Resume live and the scrollable chart
viewport use firmware callbacks and fonts. Values, plots and headings remain
read-only. Devices returns to the existing representative Devices workflow;
that subpage has not yet been replaced with native UI.

The process starts with ten minutes of explicitly synthetic history to make
chart styling visible immediately. New samples pass through the real two-second
history bins, including while Telemetry is hidden. Hold freezes plots and their
configuration while live values and sampling continue. Offline periods are not
backfilled; unsupported sensors use the firmware unavailable state. Changing
printer or endpoint clears the shared history so machines cannot be combined.
Returning to a previous printer begins a fresh history, as the firmware does.
Reset tablet or app exit restores the initial sample history. No printer data
is fetched or written by this preview.

The scrolling and worker improvements from preview22 remain in place.
Capability 12 / package preview23 adds native tests for tabs, range and target
controls, chart scrolling, Hold/live-value separation, hidden sampling, offline
gaps and reconnect, missing sensors, multiple hotends, profile resets, dropdown
cleanup, emergency-stop isolation and repeated teardown. Desktop flow checks
cover native routing, Devices return and stale-frame rejection. Linux Mint
visual/touch verification remains a user check.


## Preview24: native Devices and live Endstops

Tools > Devices now opens the unmodified firmware Devices page, category rail,
read-only cards, pagination and scrolling list. Graphs opens native Telemetry,
and its Devices button returns to native Devices. The firmware catalog and
live-value controllers consume local sample object discovery and status data;
core readings follow the active simulated machine and target changes. Generic
values update without rebuilding the catalog. Unsupported/read-only devices
remain informational, and disabled Previous/Next buttons keep the default
cursor. Eighteen sample objects exercise all eight filters and two pages; the
second sample printer has a smaller independent catalog.

Endstops uses the real dialog and request controller, with bounded local timer
replies. It queries only while idle and connected, refreshes every two seconds,
and retains the firmware waiting, error, timeout and retry behavior. Closing a
viewer, switching printer or changing pages retires its ownership; late replies
cannot repopulate a closed dialog. Modal scrolling and navigation remain
isolated. No WebSocket, printer command or hardware endstop request is sent.

The desktop build now uses libc for LVGL string formatting, including floating
point readings. This fixes native device values that previously displayed the
literal letter f, without changing the existing integer-coordinate LVGL ABI.
The real catalog and Endstops parsers use vendored cJSON; its MIT license is
included with source and the installed package.

Capability 13 / package preview24 retains the scrolling improvements and adds
native tests for all filters, paging, scrolling, numeric/live readings, generic
value updates, empty/truncated catalogs, offline/reconnect and profile changes,
Graphs navigation, Endstops rate limits, print-time guards, retries, timeouts,
close/reopen, modal isolation and pending-response retirement. Visual/touch
verification on Linux Mint remains a user check. Calibration, Bed mesh and
Macros still use representative workflows.


## Preview25: native Macros and parameter review

Tools > Macros now opens the unmodified firmware page, search keyboard,
parameter fields and nested editor, Review and Run confirmations. Macro discovery,
parameter detection, command construction, favorites and duplicate protection
use the real controllers. The local object/config adapter supplies sixteen public
sample macros plus a private helper; the real catalog excludes the private one.
The second printer has a smaller independent catalog. Empty and bounded/truncated
catalogs retain the firmware states. Calibration and Bed mesh still use
representative workflows.

Blank values are omitted; additional named parameters are supported, whitespace
is quoted by the real builder, and invalid names or forbidden value characters
keep the edit form open. Run sends only a reviewed command to the local machine
fixture. A rejected send retains the reviewed command for retry, repeated accepted
commands are guarded for five seconds, and offline/profile ownership checks remain
firmware-owned. No macro runs on a physical printer. Search and dialog Cancel,
Done, Escape, page teardown and profile switching use the native lifecycle.

Long-press favorites use the real controller and a 512-byte process-local NVS
blob, persisting across native page changes. Reset tablet or app exit starts a
new process with sample defaults. Favorite storage is panel-local, as in the
firmware. The native bridge now accepts the firmware's full 639-byte command
payload, with explicit byte-length and UTF-8 protocol tests; shorter legacy
frames and PNG caching keep their existing behavior.

Capability 14 / package preview25 adds native coverage for search, clear,
parameter keyboards, additional fields, validation, complete review/run,
Console logging, Cancel/Escape, failed-send retry, duplicate guards, favorites,
catalog bounds, offline/reconnect, profile changes and modal ownership. The
three bridge protocol tests exercise maximum-length commands, UTF-8 lengths
and oversize rejection. Linux Mint visual/touch verification remains a user
check. Scrolling retains the verified preview22 behavior.


## Preview26: native Bed mesh viewer and profiles

Tools > Bed mesh now compiles the unmodified firmware viewer, canvas renderer,
gesture implementation, mesh parser and profile dialogs. Mouse drag rotates the
surface; + / - zoom, Reset view restores the view, and Grid toggles the surface
grid. An interactive canvas reports grab/grabbing cursors; status and metric
captions remain read-only. Two-finger tablet touch gestures are compiled with
the shared implementation but are not connected to desktop multitouch input.

The local service supplies a seven-by-seven synthetic mesh and saved profiles.
Load changes the sample surface through the real parser. Save as uses the native
name keyboard and confirmation, and Remove uses the native review and Back
flow. The exact reviewed SAVE/REMOVE script, including SAVE_CONFIG, is retained
in the local response and Console. These actions update only process-local
sample profiles; they do not write to SD, restart a real Klipper instance or
perform physical probing. Calibrate presents the real warning and replaces the
sample surface after confirmation. Each printer retains its own sample profiles
across page changes; Reset tablet or app exit restores defaults.

Idle/connection/profile guards, failed profile-request feedback and retry,
selection validation and nested dialog retirement stay firmware-owned. Empty
meshes retain the firmware recovery state. The real controller clamps large
matrices to 33 by 33. The host publishes only changed mesh snapshots, avoiding
an expensive canvas redraw on every idle poll. The host's single UI thread uses
a no-op FreeRTOS mutex adapter; it introduces no concurrent controller calls.
Desktop Escape closes the active calibration review through its native close
handler and respects newer top-layer modals.

Capability 15 / package preview26 adds thirteen native checks covering viewer
controls/cursors, rotation, profile load/save/remove, complete reviewed scripts,
name validation and cancellation, rejected-request retry, calibration Back and
Escape, busy/offline guards, profile isolation, empty/large meshes, modal
ownership and repeated teardown. Editor checks cover native routing, cursor
responses and stale-frame isolation. Linux Mint visual/touch verification
remains a user check. Calibration is added in preview27 below.


## Preview27: native Calibration and Motion workflows

Tools > Calibration now compiles the firmware Studio page, modal layouts,
capability/session controllers, manual probe controls, PID, pressure advance,
results and motion diagnostics. All four Tools destinations now use native
LVGL. The primary sample exposes 24 objects and 13 detected calibration tools;
the second printer keeps its limited capability set.

The native workflows include screws tilt, gantry alignment, three-sample axis
twist, PID heater/target selection and final heat-cycle review, manual probe/Z
adjustment, probe accuracy, input shaper, resonance axis selection,
accelerometer noise, pressure advance and reviewed custom macros. Motion limits,
TMC currents/temperature/warnings and the editable axis-distance calculator use
the real diagnostics controller and widgets. Captions and disabled controls do
not advertise clicks. Desktop Escape invokes the visible native Back/Close
callback where available, preserving private dialog ownership across reopen.

Commands and delayed Console responses are process-local fixtures. Probe steps
change sample Z; pressure advance changes sample runtime limits. Session results
use the real parser: Apply & restart appears only after successful completion
with SAVE_CONFIG evidence. The final save review, rejected-send retry and
duplicate guard use the firmware handlers. No physical motion, heating, probing,
configuration write, resonance file or printer restart occurs. Reset tablet or
app exit restores sample state. Profile changes discard stale pending results.

Capability 16 / package preview27 adds nineteen native tests covering the
workflows above, repeated dismissal/reopen, successful and failed results,
missing save evidence, save retry, homing/busy/offline guards, limited profiles
and stale reply ownership. The complete native suite contains 137 tests. Editor
checks cover Calibration routing, Bed mesh navigation and stale-frame isolation.
Linux Mint GUI/touch verification remains a user check; desktop multitouch and
remaining Settings service workflows still have the documented limitations.
