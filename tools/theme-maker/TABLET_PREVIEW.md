# Studio tablet preview

The native Studio shell is 1024 × 600, with a 976 × 424 page stage at (24,80)
and an eight-item navigation shelf. The Linux preview embeds compiled LVGL for
Dashboard, Printer, Files, Camera, Tools, Console and Drybox. Remaining pages, color
picking and artwork projects use the SVG/HTML fallback. All machine/camera
backends use local fixtures.

| Page | Firmware recipe | Local interaction |
| --- | --- | --- |
| Dashboard | `ui_dashboard_layout_profile.c`, `ui_status_banner.c`, `ui_machine_status.c`, `ui_command_bar.c` | Native LVGL; pause/recovery, stop/object confirmation, thumbnail/camera fullscreen; telemetry stays inert |
| Printer | `ui_printer_layout.c`, `ui_printer_live_status.c`, `ui_printer_actions.c`, `ui_printer_toolhead.c` | Native LVGL; six-action row, Tune, Move/Filament/Z offset tabs, steps, extrusion and targets |
| Files | `ui_files.c` | Native LVGL; list rows, folder/up, search, sort, refresh, selection, details, fullscreen, print confirmation |
| Camera | `ui_camera.c::camera_set_viewport` | Native LVGL viewer and setup; selection, rotation/mirror, fullscreen, test/find/removal with local sample feeds |
| Tools | `ui_tools.c::studio_tools_create` | Native LVGL two-by-two page; tool subpages use representative workflows |
| Console | `ui_console.c` | Native LVGL; filter selector, temperature visibility, search/reset, output viewport, Command/Follow/Clear, command history |
| Drybox | `ui_drybox.c`, `ui_drybox_page.c::studio_drybox_create` | Native LVGL; read-only sensor/target displays, PLA/PETG/Hold/Resume/Stop, duplicate/offline protection and real error notices |
| Settings | `ui_settings.c`, `ui_settings_components.c` | SVG fallback; firmware section order, 68px rows, viewport scrolling, setting choices and navigation |
| Network | `ui_network.c` | SVG fallback; Wi-Fi/Moonraker/available-networks columns, local scan/connect/printer selection |
| Telemetry | `ui_telemetry.c`, `ui_telemetry_charts.c`, `ui_telemetry_components.c` | SVG fallback; Thermal/Motion/Drybox tabs, range, targets, Hold/Resume, scrolling two-column chart layout |

Settings starts with Display & appearance, then Notifications & history,
Connections, Time & region, Storage & backup, Firmware & updates, Setup center,
Maintenance, System information and About, matching the firmware's final reorder.

`tablet-model.js` owns sample state and pure action transitions. It has no native
bridge, network or file I/O. `studio-scene.js` draws the state and supplies action
metadata. The editor delegates clicks, keyboard input and viewport gestures,
keeping state separate from editable theme/project data. Export and Save project
still operate on the design. Reset tablet resets only the sample state.

A modal hides the navigation hit layer, supports Escape, and bounds its controls
to the panel. Keyboard editing retains cursor/selection positions. Color picking
pauses tablet interactions; comparisons share the same tablet state while using
the original palette and metrics. Native fixed pill actions retain their firmware
radii; controls that use configurable theme metrics still follow those metrics.

The SVG fallback is based on firmware recipes; its text rasterization, font
fallback and some layout details may differ from native LVGL. Secondary setup, hardware,
security, diagnostics, backup and update workflows use representative fixture
dialogs and simulated results. Live camera video, actual network discovery,
transport failures and physical machine responses require a real tablet.
No real commands, passwords, certificate files or backups are transmitted.

Validation includes source-contract checks, state/action tests, mounted editor
interaction fixtures, XML parsing, and static raster inspection of all ten pages
and representative dialogs. These do not replace a Linux Mint WebKit GUI run or
on-device validation.

Run:

```sh
node tools/theme-maker/tablet-model-test.js
node tools/theme-maker/editor-flow-test.js
node tools/theme-maker/studio-scene-test.js
```

## Parity status after the broad review

The near-perfect simulation goal is not met by the SVG renderer. Native host
checks demonstrate that shared LVGL execution is viable, but those are automated
fixtures. The packaged desktop app embeds that runtime for migrated pages;
remaining browser reconstructions are not pixel- or behavior-identical.

Preview11 replaces generic temperature/fan, custom entry, Tune, camera-view,
accessibility and confirmation recipes, and reviews 50 dialog routes. Remaining
fixture areas include profile editing, discovery, security, diagnostics, setup,
backup, update and theme selection. The physical printer and network backends
should remain simulated when the native UI is hosted on Linux.

## Shared-renderer migration: preview12

`linux/native/host.c` is the first real firmware-UI runtime. It directly compiles
`ui_printer_popups.c`, `ui_printer_toolhead.c`, the native Tune implementation,
the theme recipes, fonts, widgets and LVGL. It produces transparent ARGB frames
which the desktop app places above the page. The Python bridge owns one child
process, bounds requests/frames, and converts BGRA to PNG. The JS adapter only
forwards coordinates/keys and displays frames; it does not implement the native
dialog's actions. An in-memory printer backend records G-code and updates
sample temperatures, fan, tuning factors, positions and runtime offsets.

Native coverage: nozzle/bed presets, custom temperature editor and keyboard,
fan presets, Stop Print confirmation, Toolhead tabs/actions and Tune sliders.
Remaining work: native page shell/content, camera/settings/profile dialogs,
files workflows, backup/update/setup/diagnostics fixtures, and palette hit maps
for picking directly from native widgets. Do not describe the whole preview
as native or near-perfect until those migrations are complete.

Run native protocol tests after building:

```sh
python3 tools/theme-maker/linux/native-test.py
```

Preview13 adds the shared Tools page (`ui_tools.c`), shell (`ui_shell.c`) and
global E-stop implementation. Their callbacks supply host route events; their
widgets and click regions are unchanged. Click-contract tests compare inert
Studio readings against source builders, and native tests exercise real pointer
clicks on static regions, printer selection, navigation and a Tools tile.


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
