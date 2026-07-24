# PrinterHMI v3.2 Code Review

Purpose:
Track which files and blocks have been reviewed, classified, and assigned ownership.

Legend:
[ ] Not reviewed
[/] In progress
[x] Reviewed
[!] Needs action

---

# Foundation

[x] main/CMakeLists.txt
[x] main/ui_theme.c / main/ui_theme.h
[x] main/ui_widgets.c / main/ui_widgets.h

---

# Dashboard v3.2

[x] main/ui_dashboard_v32.c / main/ui_dashboard_v32.h
[x] main/ui_status_banner_v32.c / main/ui_status_banner_v32.h
[x] main/ui_machine_status_v32.c / main/ui_machine_status_v32.h
[x] main/ui_active_print_v32.c / main/ui_active_print_v32.h
[x] main/ui_command_bar_v32.c / main/ui_command_bar_v32.h
[x] main/ui_preview_v32.c / main/ui_preview_v32.h

---

# Backend / Data

[x] main/moonraker.c / main/moonraker.h
[x] main/thumbnail_manager_v32.c / main/thumbnail_manager_v32.h

---

# Existing UI Modules

[x] main/ui_settings.c / main/ui_settings.h
[x] main/ui_shell.h

---

# main.c Block Review

[x] 0000-0500 Globals, declarations, shell/system state
[x] 0500-1200 WiFi, Moonraker live polling, live state parsing
[x] 1200-1700 Graphs
[x] 1700-2100 OTA
[x] 2100-2450 v3.2 dashboard/nav bridge
[x] 2450-2900 refresh/update loop
[x] 2900-3650 dashboard/printer controls/popups
[x] 3650-4550 network
[x] 4550-5000 settings/drybox
[x] 5000-6200 files/thumbnails
[x] 6200-6650 printer page
[x] 6650-7000 legacy dashboard shell/body
[x] 7000-end SD, splash, runtime, app_main

---

# Review Notes

Add notes here as blocks are reviewed.


---

## Review 001: main/CMakeLists.txt

Status:
[x] Reviewed

Findings:
- Clean and readable.
- Current dashboard modules are registered.
- ui_preview_v32.c exists but is not registered in SRCS.
- thumbnail_manager_v32.c exists but is not registered in SRCS.

Decision:
- Keep CMakeLists.txt simple.
- Do not add preview/thumbnail manager until the next preview integration step.
- When preview work begins, add only the module needed for that step.

Owner:
- Build configuration.

Action:
[!] Update later when activating preview/thumbnail modules.


---

## Review 002: ui_theme.c / ui_theme.h

Status:
[x] Reviewed

Purpose:
Shared Theme A colors, radii, and LVGL style helpers.

Ownership:
- Belongs in ui_theme.
- Should remain independent from app logic and page layout.

Findings:
- Clean and small.
- No main.c dependency.
- Correctly centralizes colors and basic styles.
- Minor note: bottom comment in ui_theme.c says "Small UI Toolkit"; that comment probably belongs in ui_widgets.c.

Decision:
- Keep as-is.
- No code changes needed now.

Quality:
- Architecture: 5/5
- Readability: 5/5
- Ownership: 5/5
- Risk: Low


---

## Review 003: ui_widgets.c / ui_widgets.h

Status:
[x] Reviewed

Purpose:
Reusable LVGL widget constructors built on top of ui_theme.

Ownership:
- Belongs in ui_widgets.
- Should create generic reusable UI objects only.

Findings:
- Clean and small.
- No app logic.
- No main.c dependency.
- Correctly separates reusable widgets from page layout.
- Minor future note: ui_create_button() owns its internal label and does not expose it. Fine for now.

Decision:
- Keep as-is.
- No code changes needed now.

Quality:
- Architecture: 5/5
- Readability: 5/5
- Ownership: 5/5
- Risk: Low


---

## Review 004: ui_status_banner_v32.c / ui_status_banner_v32.h

Status:
[x] Reviewed

Purpose:
Dashboard status banner widget.

Ownership:
- Belongs in ui_status_banner_v32.
- Owns banner layout, labels, and progress bar.
- Receives already-formatted state/file/ETA/progress strings.

Findings:
- Clean create + set API.
- Correct use of user_data for child widget handles.
- Progress bar belongs inside this module.
- Minor future note: progress is parsed from a string with atoi(); later API could accept numeric progress.

Decision:
- Keep as-is.
- No code changes needed now.

Quality:
- Architecture: 5/5
- Readability: 5/5
- Ownership: 5/5
- Risk: Low


---

## Review 005: ui_machine_status_v32.c / ui_machine_status_v32.h

Status:
[x] Reviewed

Purpose:
Dashboard Machine Status card.

Ownership:
- Belongs in ui_machine_status_v32.
- Owns layout, labels, value widgets, and display updates.
- Receives already-formatted display strings.

Findings:
- Clean create + set API.
- Correct use of user_data for child widget handles.
- No main.c dependency.
- Already proven live on hardware.
- Minor future note: API accepts formatted strings; later dashboard refresh could pass structured values.

Decision:
- Keep as-is.
- No code changes needed now.

Quality:
- Architecture: 5/5
- Readability: 5/5
- Ownership: 5/5
- Risk: Low


---

## Review 006: ui_command_bar_v32.c / ui_command_bar_v32.h

Status:
[x] Reviewed

Purpose:
Dashboard command bar layout and command button event capture.

Ownership:
- Belongs in ui_command_bar_v32.
- Owns command bar layout and buttons.
- Forwards action strings to app-level handler.

Findings:
- Small and clear.
- No direct Moonraker dependency.
- Good bridge pattern: UI captures click, main.c executes action.
- ui_command_bar_v32_action() is implemented by main.c, which is acceptable because main.c currently owns app routing.

Decision:
- Keep as-is.
- No code changes needed now.
- Future: app-level command routing could move to app_controller, but not now.

Quality:
- Architecture: 4.5/5
- Readability: 5/5
- Ownership: 4.5/5
- Risk: Low


---

## Review 007: ui_active_print_v32.c / ui_active_print_v32.h

Status:
[x] Reviewed

Purpose:
Dashboard Active Print card.

Ownership:
- Belongs in ui_active_print_v32.
- Owns Active Print card layout, footer display, and preview container placement.

Findings:
- Small and readable.
- Footer API works and has been proven on hardware.
- No backend dependency.
- Correct use of user_data for child widget handles.
- Still owns its own inline preview box.
- Should eventually use ui_preview_v32.
- Header declares extern lv_obj_t *v32_active_footer_label, but .c does not define/use it. Likely stale.

Decision:
- Keep as-is for now.
- Later migrate preview container to ui_preview_v32 with one small test at a time.
- Do not add backend dependency to this module.

Quality:
- Architecture: 4/5
- Readability: 5/5
- Ownership: 4/5
- Risk: Medium


---

## Review 008: ui_preview_v32.c / ui_preview_v32.h

Status:
[x] Reviewed

Purpose:
Reusable preview widget.

Ownership:
- Belongs in ui_preview_v32.
- Should own preview box, placeholder label, image object, and image-source display only.

Findings:
- Clean basic preview widget.
- Can show placeholder text.
- Can show an LVGL image source.
- Currently includes thumbnail_manager_v32.h.
- ui_preview_v32_show_manager_status() couples UI widget to backend manager.
- This violates the intended ownership boundary.

Decision:
- Keep for now.
- Later decouple from thumbnail_manager_v32.
- ui_preview_v32 should remain UI-only.
- Manager status should be pushed into it from a higher-level owner, not pulled from manager directly.

Quality:
- Architecture: 3.5/5
- Readability: 5/5
- Ownership: 3/5
- Risk: Medium


---

## Review 009: thumbnail_manager_v32.c / thumbnail_manager_v32.h

Status:
[x] Reviewed

Purpose:
Backend thumbnail state and cache-path manager.

Ownership:
- Belongs in thumbnail_manager_v32.
- Owns selected file, thumbnail state, safe cache path, status text, ready state, and error state.
- Later should own cache lookup, download, and decode.

Findings:
- Clean backend-only module.
- No LVGL dependency.
- No main.c dependency.
- Good state enum.
- Cache path builder belongs here.
- Ready/error API is useful.
- Currently Phase 1 only: no SD check, no download, no decode.

Decision:
- Keep as-is.
- Later migrate old thumbnail pipeline from main.c into this module.
- Do not allow this module to depend on UI.

Quality:
- Architecture: 5/5
- Readability: 5/5
- Ownership: 5/5
- Risk: Low


---

## Review 010: moonraker.c / moonraker.h

Status:
[x] Reviewed

Purpose:
Moonraker backend/data module.

Ownership:
- Belongs in moonraker.
- Owns Moonraker state model, JSON helpers, HTTP response capture, and thumbnail path parsing.
- Should eventually own more object parsing and state updates directly.

Findings:
- No LVGL dependency.
- Has a real moonraker_state_t bridge toward removing live globals from main.c.
- JSON helpers are already centralized here.
- HTTP capture handler belongs here.
- moonraker_state_update_from_legacy() confirms this is transitional.
- main.c still owns most live polling and parsing flow.

Decision:
- Keep as-is for now.
- Later move more Moonraker parsing/state update logic out of main.c.
- Do not add UI dependencies.

Quality:
- Architecture: 4/5
- Readability: 4.5/5
- Ownership: 4/5
- Risk: Low/Medium


---

## Review 011: ui_dashboard_v32.c / ui_dashboard_v32.h

Status:
[x] Reviewed

Purpose:
v3.2 dashboard composition module.

Ownership:
- Belongs in ui_dashboard_v32.
- Owns dashboard root, child component creation, layout, and routing dashboard update calls to child widgets.

Findings:
- Clean and small.
- No backend dependency.
- Correctly composes Status Banner, Active Print, Machine Status, and Command Bar.
- Live banner and machine data path already works.
- Active Print footer placeholder is sane.
- dash32_root x=162 overlaps the 170px nav rail by 8px.
- destroy() only clears root and banner; should also clear machine and active_print.
- ui_dashboard_v32_update() is currently placeholder/default update, not full dashboard refresh.
- Active Print live footer is not wired yet.
- Preview is not integrated yet.

Decision:
- Keep as-is for now.
- Finish dashboard carefully with one small change at a time.
- Do not add backend dependencies to this module.
- Later consider dashboard-owned refresh coordinator.

Quality:
- Architecture: 4.5/5
- Readability: 5/5
- Ownership: 4.5/5
- Risk: Medium


---

## Review 012: ui_settings.c / ui_settings.h

Status:
[x] Reviewed

Purpose:
Partial Settings module.

Ownership:
- Belongs in ui_settings for now.
- Should eventually own settings page layout, settings callbacks, reset popup, sleep setting UI.

Findings:
- Reset popup belongs here.
- Reboot callback can live here.
- settings_panel and settings_sleep_label are global externs.
- ui_settings_module_init() is scaffold only.
- system_info_popup appears at bottom of file and needs later ownership review.
- ui_shell_raise() bridge is acceptable for now.

Decision:
- Keep for now.
- Later complete settings ownership or move remaining settings UI from main.c.
- Do not expand until settings extraction phase.

Quality:
- Architecture: 3.5/5
- Readability: 4/5
- Ownership: 3.5/5
- Risk: Medium

---

## Review 013: ui_shell.h

Status:
[x] Reviewed

Purpose:
Temporary shell bridge.

Ownership:
- Header belongs as a bridge while main.c owns shell.
- Lets modules request shell raise without exposing top_bar/nav_rail globals.

Findings:
- Small and clear.
- Good temporary boundary.
- Implementation remains in main.c.

Decision:
- Keep.
- Future app shell module may replace this.

Quality:
- Architecture: 4/5
- Readability: 5/5
- Ownership: 4/5
- Risk: Low


---

## Review 014: main.c lines 0000-0520

Status:
[x] Reviewed

Purpose:
Top-level includes, early declarations, shell globals, service globals, live state globals, and early helper functions.

Classification:
- KEEP:
  - Includes needed by app_main/runtime/services.
  - top_bar/nav_rail shell globals.
  - ui_shell_raise().
  - SNTP startup/task helpers.
  - OTA validity confirmation.
  - simple helper functions like safe_copy(), printer_state_is(), set_btn_enabled(), dump_heap_caps().

- TEMPORARY BRIDGE:
  - Live Moonraker globals.
  - Printer state globals.
  - Dashboard v32 push source globals.
  - Settings/shell bridge globals.
  - Existing printer button guard helpers.

- MOVE LATER:
  - WiFi credentials/config constants into config/service layer.
  - SD state into storage/sd manager.
  - Thumbnail globals into thumbnail_manager_v32.
  - Graph globals into ui_graphs_v32 / graph history manager.
  - Network globals into ui_network_v32.
  - Printer page globals into ui_printer_v32.
  - Drybox globals into ui_drybox_v32.
  - Legacy dashboard globals into retired/removed old dashboard path.

- RETIRE EVENTUALLY:
  - Old dashboard thumbnail globals.
  - Duplicate preview state.
  - Obsolete wifi_label if unused.
  - Legacy dashboard card globals once v32 dashboard fully owns dashboard.

Findings:
- main.c is not broken, but this block mixes shell, backend state, UI page globals, thumbnail state, graph state, network state, and printer state.
- Shell ownership is acceptable in main.c.
- Thumbnail and graph globals are the biggest early extraction candidates.
- The existing architecture is visible: modules already exist, but main.c still owns large legacy state.
- Do not move anything yet; continue mapping first.

Decision:
- Keep this block stable during review.
- No code changes now.
- Use this classification later when creating extraction tasks.

Quality:
- Architecture: 2.5/5
- Readability: 3/5
- Ownership: 2/5
- Risk: High
- Action: Review complete, extraction later.

---

## Review 015: main.c lines 0520-1220

Status:
[x] Reviewed

Purpose:
WiFi startup, NVS configuration, Moonraker live polling, live object parsing, dashboard live update bridge.

Classification:

KEEP
----
- wifi_event_handler()
- wifi_init_sta()
- load_wifi_credentials_from_nvs()
- save_wifi_credentials_to_nvs()
- moonraker_live_poll_tasklet()

These are application/service responsibilities.

TEMPORARY BRIDGE
----------------
- moonraker_get_live_objects()

Reason:
Correct functionality, but currently performs:
    HTTP
    JSON parsing
    state updates
    UI globals

Eventually HTTP + parsing should live inside moonraker.c while
main.c simply schedules polling.

MOVE LATER
----------
update_live_cards()

Reason:
This function is almost entirely dashboard presentation.

It currently:
- formats strings
- computes ETA
- computes remaining
- updates dashboard widgets
- manages dashboard thumbnail lifecycle
- updates dashboard cards

Almost all of this belongs inside ui_dashboard_v32_refresh().

KEEP
----
temp_value_color()
progress_value_color()
fan_value_color()

These are presentation helpers.
Later they can move beside the widgets that use them.

MOVE LATER
----------
Dashboard thumbnail management.

This section:

- dash_live_preview_key
- selected_print_file
- printer_thumb_target
- printer_thumb_free()
- printer_thumb_start_delayed()

belongs with the preview/thumbnail pipeline rather than
inside dashboard refresh.

KEEP
----
moonraker_send_gcode()

Application command transport.
Correct location for now.

Findings:

This chunk contains the largest architectural win discovered so far.

Today the flow is roughly:

main.c
    poll
        ↓
HTTP
        ↓
JSON
        ↓
Globals
        ↓
update_live_cards()
        ↓
LVGL

Target architecture:

main.c
    scheduler
        ↓
moonraker.c
        ↓
moonraker_state_t
        ↓
ui_dashboard_v32_refresh(state)
        ↓
Banner
Machine
Active Print
Preview

Decision:

Do not refactor yet.

First finish reviewing the entire application.

Quality:

Architecture: 3/5
Readability: 3.5/5
Ownership: 2.5/5
Risk: Medium
Action: Review complete.

---

## Review 017: main.c lines 1700-2100

Status:
[x] Reviewed

Purpose:
OTA update system, OTA progress UI, OTA task, OTA URL persistence, OTA configuration popup.

Classification:

KEEP
----
- ota_update_task()
- ota_http_event_handler()
- OTA task startup
- esp_https_ota integration
- NVS load/save for OTA URL

These are application/backend responsibilities.

MOVE LATER
----------
OTA progress popup.

Functions:

    ota_progress_show_popup()
    ota_progress_ui_pump()

These belong in a future ui_ota module.

MOVE LATER
----------
OTA configuration popup.

Functions:

    ota_open_popup_cb()
    ota_popup_start_cb()
    ota_popup_close_cb()

These are entirely UI.

KEEP
----
ota_load_url_from_nvs()
ota_save_url_to_nvs()

Configuration persistence belongs with OTA.

TEMPORARY BRIDGE
----------------
ota_test_btn_cb()

Currently bridges:

UI
    ↓
OTA backend

Eventually ui_ota should call an ota_manager API instead.

FUTURE MODULES

ota_manager.c

    download
    task
    HTTP
    NVS
    reboot
    progress state

ui_ota.c

    popup
    URL editor
    progress dialog
    remote build browser

Findings:

The OTA implementation is already nicely layered.

Current flow:

Button
    ↓
Popup
    ↓
Task
    ↓
esp_https_ota

Very little redesign is needed.

Most work is simply separating backend from UI.

Decision:

Keep implementation.

Later split into:

    ota_manager
    ui_ota

No logic changes required.

Quality:

Architecture: 4/5
Readability: 4.5/5
Ownership: 3.5/5
Risk: Low
Action: Review complete.

---

## Review 018: main.c lines 2100-2450

Status:
[x] Reviewed

Purpose:
Dashboard v3.2 integration, shell navigation, dashboard bridge.

Classification:

KEEP
----
- nav_btn_event_cb()
- set_active_nav()
- open_v32_dashboard_cb()
- ui_command_bar_v32_action()
- hide_files_tab()

TEMPORARY BRIDGE
----------------
- ui_dashboard_v32_push_live_banner_data()
- ui_dashboard_v32_push_live_machine_data()

These should eventually collapse into:

    ui_dashboard_v32_refresh(state)

MOVE LATER
----------
- printer_thumb_cleanup_for_popup_close()

Should migrate with the thumbnail/preview pipeline.

Findings:
- Shell/navigation ownership is clear.
- Dashboard composition already lives in ui_dashboard_v32.
- Remaining dashboard bridge is very small.
- Final dashboard API should become ui_dashboard_v32_refresh(state).

Decision:
- Keep navigation in main.c.
- Finish dashboard migration by replacing push helpers with one refresh entry point.

Quality:
- Architecture: 4.5/5
- Readability: 5/5
- Ownership: 4.5/5
- Risk: Low


---

## Review 019: main.c lines 2450-2900

Status:
[x] Reviewed

Purpose:
Main UI refresh timer and repeated page/status updates.

Classification:

KEEP:
- ui_refresh_timer_cb() as the scheduler/timer entry point.
- module update sequencing.

MOVE LATER:
- Dashboard refresh into ui_dashboard_v32_refresh()
- Printer page refresh into ui_printer_v32_refresh()
- Drybox page refresh into ui_drybox_v32_refresh()
- Network page refresh into ui_network_v32_refresh()
- Graph refresh into ui_graphs_v32_refresh()
- Status popup refresh into its owning UI module.

Findings:
- This timer is currently the UI update engine for the whole app.
- main.c should keep the timer, but not directly manipulate every page.
- Final architecture should make this timer a dispatcher that calls module refresh functions.
- This block explains why main.c feels oversized: it is currently both conductor and instrument.

Decision:
- Keep stable for now.
- Do not refactor during review.
- Later shrink this block into module refresh calls.

Quality:
- Future Architecture: 5/5
- Current Ownership: 2/5
- Readability: 3/5
- Risk: Medium


---

## Review 020: main.c lines 2900-3650

Status:
[x] Reviewed

Purpose:
Dashboard popups, printer UI interactions, printer controls, motion controls, temperature controls, printer widget builders.

Classification:

KEEP:
- dash_cmd_event_cb()
- printer_cmd_event_cb()
- format_hhmm()

MOVE LATER:
- Dashboard status popup into ui_dashboard_v32.
- Printer status popup into ui_printer_v32.
- Cancel print dialog into ui_printer_v32.
- Motion popup into ui_printer_v32 (possible later split to ui_motion_popup).
- Temperature popups into ui_printer_v32.
- Printer widget builders into ui_printer_v32.

Findings:
- This block is almost entirely printer UI behavior.
- The printer page is composed of several cohesive sub-features that naturally belong together.
- Routing callbacks correctly remain application-level.

Decision:
- Keep routing in main.c.
- Move printer UI behavior and widget creation into ui_printer_v32 during refactor.

Quality:
- Architecture: 4/5
- Readability: 4/5
- Ownership: 2.5/5
- Risk: Medium


---

## Review 021: main.c lines 3650-4550

Status:
[x] Reviewed

Purpose:
Network page, WiFi configuration, Moonraker configuration, WiFi scanning, Moonraker discovery, network popups.

Classification:

KEEP:
- load_moonraker_config_from_nvs()
- save_moonraker_config_to_nvs()
- connect_selected_wifi()
- network_wifi_scan_now() (later split)
- scan_moonraker_now() (later split)
- test_moonraker_now() (later split)

MOVE LATER:
- WiFi scan popup into ui_network_v32.
- Password popup into ui_network_v32.
- Moonraker discovery popup into ui_network_v32.
- Test popup into ui_network_v32.
- hide_network_tab() into ui_network_v32.

FUTURE:
- network_manager module owns NVS, WiFi connection, scanning and Moonraker probing.
- ui_network_v32 owns all presentation and popups.

Findings:
- The Network page is already highly cohesive.
- Refactor will mostly involve relocating code rather than redesigning it.
- Clear future split exists between UI and networking services.

Quality:
- Architecture: 4.5/5
- Ownership: 3/5
- Readability: 4/5
- Risk: Low


---

## Review 022: main.c lines 4550-5000

Status:
[x] Reviewed

Purpose:
Settings page, System Information popup, Drybox page, Drybox controls.

Classification:

KEEP:
- esp_restart()
- NVS erase/reset
- OTA callbacks
- moonraker_send_gcode()

MOVE LATER:
- show_settings_tab() into ui_settings.c
- System Information popup UI into ui_settings.c
- show_drybox_tab()/hide_drybox_tab() into ui_drybox_v32
- Drybox layout, widgets and buttons into ui_drybox_v32

Findings:
- ui_settings.c currently exists but most page construction is still in main.c.
- Drybox page is already a cohesive module waiting to be extracted.
- Application services remain separate from UI responsibilities.

Quality:
- Architecture: 5/5
- Ownership: 3/5
- Readability: 4/5
- Risk: Low


---

## Review 023: main.c lines 5000-6200

Status:
[x] Reviewed

Purpose:
Files page, print confirmation popup, metadata fetch, thumbnail download/cache/render, dashboard/printer preview.

Classification:

KEEP:
- moonraker_start_print_file()

MOVE LATER:
- File browser and file selection into ui_files_v32.
- Print confirmation popup into ui_files_v32.
- Metadata display into ui_files_v32.
- Metadata HTTP/parsing into moonraker.c or a file metadata service.
- Thumbnail download/cache/SD/PNG byte ownership into thumbnail_manager_v32.
- Thumbnail render/decode/canvas/aspect-fit into ui_preview_v32.

Findings:
- This block is the largest architectural knot reviewed so far.
- It contains several responsibilities interleaved together.
- Correct future split appears to be:
    ui_files_v32
        -> thumbnail_manager_v32
        -> ui_preview_v32
- thumbnail_manager_v32 should eventually own the full thumbnail pipeline except visual rendering.
- ui_preview_v32 should own visual decode/render/canvas, not download/cache.
- main.c should not own thumbnail state long term.

Quality:
- Architecture: 5/5 future direction
- Current Ownership: 2/5
- Readability: 3.5/5
- Risk: Medium


---

## Review 024: main.c lines 6200-6650

Status:
[x] Reviewed

Purpose:
Legacy printer page construction, printer widgets, tuning panel, preview box, cross-page refreshes.

Classification:

KEEP:
- Printer state synchronization.
- Thumbnail selection/state coordination (later may move toward thumbnail_manager_v32).

MOVE LATER:
- Entire printer page construction into ui_printer_v32.
- Printer callbacks (nozzle, bed, fan, motion) into ui_printer_v32.
- Printer preview widget into ui_preview_v32.

REMOVE LATER:
- Cross-page Drybox and Network updates from the printer section.
- Replace with ui_drybox_v32_refresh() and ui_network_v32_refresh() from the main scheduler.

Findings:
- show_printer_tab() is effectively ui_printer_v32_create().
- Preview should be a shared component, not printer-specific.
- Cross-page ownership is the remaining architectural issue in this block.

Quality:
- Architecture: 4.5/5
- Ownership: 2.5/5
- Readability: 4/5
- Risk: Low


---

## Review 025: main.c lines 6650-7000

Status:
[x] Reviewed

Purpose:
Application shell construction (top bar, navigation rail, shell timers) plus legacy dashboard creation.

Classification:

KEEP:
- clock_timer_cb()
- Shell timer creation.
- Shell creation responsibilities.

MOVE LATER:
- Top bar into ui_shell.
- Navigation rail into ui_shell.
- Clock and status icons into ui_shell.
- Shell timers into ui_shell.

REMOVE LATER:
- Legacy dashboard construction.
- Replace with ui_dashboard_v32_create().

Findings:
- build_drybox_dashboard() is now actually an application shell builder.
- Two dashboard implementations currently coexist (legacy and v3.2).
- A dedicated ui_shell module naturally sits between the application and page modules.

Quality:
- Architecture: 5/5
- Ownership: 2/5
- Readability: 3/5
- Risk: Low


---

## Review 026: main.c lines 7000-end

Status:
[x] Reviewed

Purpose:
SD storage init, boot splash, runtime task, and app_main lifecycle.

Classification:

KEEP:
- app_main()
- hmi_runtime_task()
- NVS init
- BSP display/touch startup
- WiFi startup sequence
- OTA running-app confirmation
- Runtime task creation

MOVE LATER:
- init_sd_card_storage() into storage_manager.c
- splash_create(), splash_set_progress(), splash_destroy() into ui_splash.c

REMOVE/SIMPLIFY LATER:
- build_drybox_dashboard() should become shell-only or be replaced by ui_shell_create().
- app_main() should create shell + v3.2 dashboard only, not legacy dashboard plus v3.2 dashboard.

Findings:
- app_main confirms the duplicate-dashboard issue.
- Runtime layer is clear and should remain mostly in main.c.
- Splash and SD are clean extraction candidates.
- Final architecture should be:
    app_main()
        -> init services
        -> ui_shell_create()
        -> ui_dashboard_v32_create()
        -> start runtime task

Quality:
- Architecture: 4.5/5
- Ownership: 3.5/5
- Readability: 4/5
- Risk: Low

