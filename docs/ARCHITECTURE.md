# PrinterHMI architecture

## Scope

PrinterHMI v6.0.6 is an ESP-IDF application for an ESP32-P4 operator panel.
It presents an LVGL interface and connects through an ESP32-C6 hosted network
coprocessor to as many as four Klipper/Moonraker printers.

## Architectural rule

`main.c` is the application coordinator. It owns startup order, shared runtime
state and bridges between independently owned modules. It must not become the
default owner for page layout, network transport, persistence or parsing.

Page modules own page lifetime and page-local widgets. Controllers own policy
and state translation. Service modules own transport, storage, parsing and
background work. Shared UI modules own visual contracts.

## Runtime layers

```mermaid
flowchart TD
    A[Operator pages and popups] --> B[UI components and runtime theme]
    A --> C[Controllers and coordinators]
    C --> D[Moonraker, Wi-Fi, OTA, NVS and SD services]
    D --> E[ESP-IDF, LVGL and board support]
```

### Application coordination

- `main.c` initializes NVS, themes, timezone, Moonraker state, display, SD,
  Wi-Fi and the UI shell.
- It routes shell navigation, cross-page live data and compatibility bridges.
- It allocates the Moonraker HTTP capture buffer in PSRAM with an internal-RAM
  fallback.

### Shell and pages

`ui_shell` owns the persistent top status bar, navigation rail, clock and active
printer identity. It routes ten pages in operator workflow order:
Dashboard, Printer, Files, Bed Mesh, Macros, Console, Telemetry, Drybox,
Network and Settings. Its persistent runtime pointer tables are allocated once
in PSRAM after scheduler startup.

Each page has a top-level UI owner. Larger pages delegate to components such as
status banners, cards, previews, charts, action panels and popup controllers.

### Shared UI system

- `ui_theme` is the runtime style contract.
- `ui_theme_a`, `ui_theme_b` and `ui_theme_c` implement Classic, Operator and
  Dark Glass visual policies.
- `theme_manager` loads and saves the selected theme, accent, density and
  accessibility settings.
- `ui_button`, `ui_cards`, `ui_popup`, `ui_page_title` and `ui_widgets` provide
  shared primitives.
- Existing screens are rebuilt after appearance changes so existing LVGL
  objects receive the newly selected theme.
- `ui_popup` owns modal behavior; an open popup blocks interaction with the
  interface beneath it. `ui_about_popup` is a focused Settings-owned
  consumer of those shared popup primitives.

### Ownership boundaries and next seams

- `main.c` coordinates startup, lifecycle transitions and narrow adapters only. New
  page layout, parsing, persistent storage and transport policy belong to focused
  owners rather than being added there.
- Page owners may compose widgets and route operator intent. They do not perform
  blocking HTTP, SD or OTA work.
- Controllers translate state and enforce policy; services own I/O, background
  work, persistence and parsing. Background work publishes data, never LVGL
  mutations.
- The next maintenance extractions, when behavior requires change, are focused
  slices from `main.c`, `ui_calibration` and `ui_printer_popups`. They are not
  prerequisite refactors for ordinary feature work.

### Moonraker integration

- `moonraker_config_controller` owns up to four persistent printer profiles and
  a generation counter used to reject stale work.
- `moonraker_live_websocket` subscribes to live Klipper objects, including
  authoritative `virtual_sdcard` print progress, and merges updates into
  synchronized Moonraker state. It owns non-blocking recovery while a
  WebSocket is reconnecting.
- `moonraker_poll` and `moonraker_live_transport` provide scheduled HTTP state
  refresh and fallback behavior without competing with reconnect ownership.
- `moonraker_probe` and `moonraker_discovery` test and discover endpoints on
  ports 7125 through 7128; discovery is presented inside printer profile
  Add/Edit.
- File, metadata, thumbnail, G-code and print-start requests are implemented by
  `moonraker` and consumed through controllers.
- `macro_controller` derives a bounded, alphabetized public-macro catalog from
  Moonraker object discovery while excluding underscore-prefixed helpers.
- `console_controller` retains bounded command and response history and
  receives live `notify_gcode_response` WebSocket messages.

### Printer and files

- Printer UI policy is split between `printer_controller`,
  `printer_ui_controller` and focused `ui_printer_*` modules.
- File listing and selection are coordinated by `files_page_controller` and
  `printer_file_controller`.
- Thumbnail sessions, download, decode, RGB565 rendering and preview caching
  are separate modules.
- Large image/message buffers prefer PSRAM. Rendered profile previews can be
  persisted on SD storage.

### Calibration, Bed Mesh, Devices, Macros and Console

- `ui_calibration` composes calibration workflows; `ui_calibration_layout` owns its reusable cards, labels and capability summaries.
  `ui_calibration_geometry`, `ui_calibration_motion`, `ui_calibration_pressure_advance` and
  `ui_calibration_manual_probe`, `ui_calibration_custom`, `ui_calibration_pid` and `ui_calibration_results` own their focused workflows. The shared
  manual-probe surface provides coarse and 0.005 mm fine TESTZ steps for
  both Probe/Z and Axis Twist, while `calibration_capability_controller`
  and `calibration_session_controller`
  retain capability and session policy.
- `ui_bed_mesh` composes the 3D mesh page. Gesture recognition, rendering
  and profile dialogs belong to `ui_bed_mesh_gestures`,
  `ui_bed_mesh_renderer` and `ui_bed_mesh_profiles`; `bed_mesh_controller`
  remains the snapshot source.
- `ui_devices` composes the Devices page and Telemetry bridge.
  `ui_devices_catalog_view` owns filters, cards, pagination and refresh timing;
  `ui_devices_live_values` translates synchronized Moonraker state for visible
  labels; `device_catalog_controller` owns discovery and classification.
- `ui_macros` and `macro_controller` own public-macro presentation and
  policy. `ui_console` and `console_controller` own command entry and
  bounded response history. These already had clean page/controller
  boundaries and were intentionally left intact.
- Long-lived feature contexts and bounded catalogs prefer PSRAM with an
  internal-RAM fallback.

### Settings and persistence

`wifi_credentials_store` owns the NVS read/write boundary for Wi-Fi credentials.

NVS stores network configuration, printer profiles, theme selection,
accessibility settings, brightness, display sleep, timezone, OTA URL and the
last selected file. Factory reset erases NVS and reboots; it does not erase SD
card content.

### OTA lifecycle

`ota_ui_controller` owns release-catalog, custom-URL and popup dispatch; `ota_manager` owns the update transaction.

The OTA manager downloads to the inactive application slot. The progress
popup can request cancellation; the worker aborts the OTA handle and returns
without rebooting or selecting the incomplete image. On successful installation
the device reboots. When rollback marks the new image as pending verification,
`ota_boot_validation` marks the running image valid and cancels rollback after the splash lifecycle completes.

## Startup sequence

1. Initialize NVS, recovering from incompatible or exhausted storage.
2. Load theme and timezone state.
3. Initialize synchronized Moonraker state.
4. Hold the backlight off and initialize the ESP-Hosted/C6 transport.
5. Allocate PSRAM-backed capture storage.
6. Start the BSP display/touch path with the LVGL task on CPU 1.
7. Build the dashboard, printer chooser and splash with the backlight off.
8. Restore brightness and start Wi-Fi association.
9. Start SNTP, delayed SD mounting and Moonraker runtime services.
10. Mark a pending OTA image valid when rollback is enabled.

## Concurrency rules

- LVGL objects may be accessed only from LVGL callbacks or while holding the
  BSP display lock.
- Background HTTP, WebSocket, discovery, preview and OTA work must publish
  results without directly mutating LVGL objects. Inactive-profile preview
  and health HTTP work belongs to its low-priority worker, never the shell
  runtime task.
- Active-printer configuration changes increment a generation. Results from a
  retired generation must be discarded.
- DMA buffers must use DMA-capable memory; general large buffers should prefer
  PSRAM when latency permits.
- Avoid adding sizeable static internal-RAM state. FreeRTOS creates its
  statically configured timer task before `app_main()`, so long-lived UI and
  controller contexts should be allocated permanently from PSRAM after the
  scheduler starts.

## Source of truth

- Build membership: `main/CMakeLists.txt`
- Dependency versions: `dependencies.lock`
- Flash layout: `partitions.csv`
- Resolved configuration: `sdkconfig`
- Reproducible defaults: `sdkconfig.defaults`
- Current module ownership: `docs/PROJECT_FILE_CATALOG.md`

The v3.x architecture documents are historical and live under
`docs/history/architecture/`.
