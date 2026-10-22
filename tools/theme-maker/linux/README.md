# PrinterHMI Theme Maker for Linux Mint

This is an offline Linux desktop application. It opens its own GTK window,
has an application-menu entry, and uses a native Save dialog for theme exports.
The editor is rendered by the system WebKitGTK library. It does not use a
hosted website, open a browser tab, or start a local HTTP server.

This is a development preview targeting **Linux Mint 22** (Ubuntu 24.04).
Install the package using apt so Mint installs any missing desktop libraries:

```sh
sudo apt install ./printerhmi-theme-maker_0.1.0~preview12_amd64.deb
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
This SVG simulation is not an embedded LVGL runtime: panel font rendering, live
video, timing and hardware behavior still require a physical tablet check.
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
