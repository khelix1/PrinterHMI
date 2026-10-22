# Studio tablet preview

The native Studio shell is 1024 × 600, with a 976 × 424 page stage at (24,80)
and an eight-item navigation shelf. The preview is an offline implementation
using SVG for layout and HTML for physical text input. It is not compiled LVGL.

| Page | Firmware recipe | Local interaction |
| --- | --- | --- |
| Dashboard | `ui_dashboard_layout_profile.c`, `ui_status_banner.c`, `ui_machine_status.c`, `ui_command_bar.c` | Pause/resume, stop confirmation, object selection, temperature targets, fan, thumbnail fullscreen |
| Printer | `ui_printer_layout.c`, `ui_printer_live_status.c`, `ui_printer_actions.c`, `ui_printer_toolhead.c` | Actual six-action row, Tune, Move/Filament/Z offset tabs, steps, extrusion and targets |
| Files | `ui_files.c` | 92px list rows, folder/up, search, sort, refresh, selection, details, fullscreen, print confirmation |
| Camera | `ui_camera.c::camera_set_viewport` | Configure/View/camera selector/Fullscreen at native bottom positions; sample viewport only |
| Tools | `ui_tools.c::studio_tools_create` | Native two-by-two Calibration/Bed mesh/Devices/Macros entries; representative local workflows |
| Console | `ui_console.c` | Filter selector, temperature visibility, search/reset, output viewport, Command/Follow/Clear, command history |
| Drybox | `ui_drybox_page.c::studio_drybox_create` | Humidity arc, target, PLA/PETG/Hold/Resume/Stop |
| Settings | `ui_settings.c`, `ui_settings_components.c` | Full native section order, 68px rows, viewport scrolling, setting choices and navigation |
| Network | `ui_network.c` | Native Wi-Fi/Moonraker/available-networks columns, local scan/connect/printer selection |
| Telemetry | `ui_telemetry.c`, `ui_telemetry_charts.c`, `ui_telemetry_components.c` | Thermal/Motion/Drybox tabs, range, targets, Hold/Resume, scrolling two-column chart layout |

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

The primary page recipes are based on native source. Text rasterization, font
fallback and some layout details may differ from LVGL. Secondary setup, hardware,
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
fixtures, not the desktop preview engine. Do not present native test success as
proof that the browser reconstruction is pixel- or behavior-identical.

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
