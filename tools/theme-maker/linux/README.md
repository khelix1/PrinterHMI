# PrinterHMI Theme Maker for Linux Mint

This is an offline Linux desktop application. It opens its own GTK window,
has an application-menu entry, and uses a native Save dialog for theme exports.
The editor is rendered by the system WebKitGTK library. It does not use a
hosted website, open a browser tab, or start a local HTTP server.

This is a development preview targeting **Linux Mint 22** (Ubuntu 24.04).
Install the package using apt so Mint installs any missing desktop libraries:

```sh
sudo apt install ./printerhmi-theme-maker_0.1.0~preview19_amd64.deb
printerhmi-theme-maker
```

Click the upper-left P4 button to open the chooser. **Start from scratch**
creates a neutral blank theme; use Layout to add, move, resize and remove
printer blocks. The canvas stores compatible region geometry, not arbitrary
LVGL widget trees. Header/navigation and block internals stay firmware-owned.

You can also launch **PrinterHMI Theme Maker** from the Graphics application
menu. Import opens the system file chooser. Export opens Save; cancellation
and successful saves are reported separately. Theme files remain ordinary
portable `.phmitheme` JSON documents; adding artwork switches export to an
editable ZIP bundle containing the theme and image.

Drafts are stored under `$XDG_DATA_HOME/printerhmi-theme-maker` (normally
`~/.local/share/printerhmi-theme-maker`). Cache uses the corresponding XDG
cache directory. Removing the package does not delete your personal drafts or
exported theme files. Export regularly to keep portable copies.

## Run from source

```sh
sudo apt install python3-gi gir1.2-gtk-3.0 gir1.2-webkit2-4.1 python3-pil
/usr/bin/python3 tools/theme-maker/linux/theme-maker.py
```

Use the system `/usr/bin/python3`; isolated Python environments may not include
Mint's GI bindings. Build a package with:

```sh
python3 tools/theme-maker/linux/build-deb.py
```

The source build requires `dpkg-deb` (provided by `dpkg`). It does not install
anything on the build computer.

## Interactive tablet preview

All ten Studio page previews now have firmware-based layouts and offline
interactions. Navigate through the dock, Settings and Devices links. Scroll
inside lists, open dialogs, type on the physical/on-screen keyboard, select
files, pause/resume the sample print, change targets, run drybox programs and
try Console and Telemetry controls. Reset tablet restores the sample state.

Tablet interactions do not contact printers or alter saved theme/project files.
Camera/video, network, machine, backup and firmware operations use simulated
results. Secondary dialogs model the interaction patterns; the preview is not
a compiled LVGL emulator.

## Compare with original Studio

**Original Studio** shows the built-in palette and corner settings without custom
artwork. Switch back with **Back to my theme**. It works across pages and in
Components, and never replaces your design. Export and Save project still save
your own theme. Editing a property or using Undo returns to the custom preview.

This version also removes the obsolete WebKit hyperlink-auditing call that
produced a startup warning.

## Edit directly from the Studio preview

Click **Pick color**, then select a Studio label, button, surface or border.
The matching color opens at the top of Design. Button labels offer separate
text, fill and border choices. Reset restores that color's original Studio
value; Undo restores your edit. Shared uses of the color are highlighted.

Toggle Pick color off or press Escape to restore normal preview navigation.
Page tabs remain available while picking. This also works in Components.

## Keep multiple projects

Save project (Ctrl+S) creates a portable `.phmiproject` file with your theme,
workspace settings and artwork source. Open project (Ctrl+O) restores it, even
on another computer. Keep separate files for separate designs. Each save uses
the native chooser with overwrite confirmation; cancellation preserves the draft.
Use Export theme to create files for the panel. Project files are for the editor.

The Studio Components view shows native-style buttons, input states, ON/OFF
rows, progress, sliders, cards, status feedback and a confirmation dialog.

## Firmware compatibility

The desktop app itself needs no panel firmware change. Operator, Foundry and
Glass version 1 exports work with released v7.0.0. Studio version 2 exports
require the loader changes on `feature/theme-maker`. The application displays
this requirement before export. Artwork requires the version 3 loader in this
patch and a local firmware rebuild/OTA. Open **Design → Background image**
to choose an image, adjust its fit/position/overlay, then export a ZIP. Copy
the ZIP's `PrinterHMI` directory to the SD card root and rescan Custom Themes. The broader design limitations in the main
Theme Maker README still apply.

## Validation status

The package structure, desktop entry, Python/JavaScript syntax, pure native
export handling, JSON model and firmware parser are checked during development.
GUI startup, WebKit integration, native dialogs and appearance still require a
run on Linux Mint. This build environment has no GTK/WebKit GI runtime, so it
cannot verify those interactions.

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

Preview11 fixes the dialog footer hit areas: navigation is hidden as a whole
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
