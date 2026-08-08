# Project file catalog

This catalog reflects the v6.0.6 source list in `main/CMakeLists.txt`. Update it
when ownership or build membership changes.

## Application coordinator

| Module | Ownership |
| --- | --- |
| `main.c` | Startup, application state, shell routing and cross-module bridges |

## Theme and shared UI

| Modules | Ownership |
| --- | --- |
| `ui_theme`, `ui_theme_a`, `ui_theme_b`, `ui_theme_c` | Runtime theme contract and three implementations |
| `theme_manager` | Persistent theme, accent, density and accessibility state |
| `custom_theme` | SD-card custom-theme discovery, validation, overrides and removal |
| `ui_theme_preview`, `ui_appearance_popups` | Theme selection, custom-package management and operator previews |
| `ui_widgets`, `ui_button`, `ui_cards`, `ui_popup` | Shared controls and modal popup system |
| `ui_page_title`, `ui_page_state`, `ui_toast` | Shared page and transient-state components |

## Shell and splash

| Modules | Ownership |
| --- | --- |
| `ui_shell` | Persistent top bar, ten-destination navigation, clock and active printer; pointer state is permanently PSRAM-backed |
| `ui_console` | Console page, command entry and bounded history presentation |
| `ui_macros` | Detected public-macro page and run confirmation |
| `ui_splash` | Startup splash state and progress |
| `ui_logo_assets` | Compiled PrinterHMI logo assets |

## Dashboard

| Modules | Ownership |
| --- | --- |
| `ui_dashboard`, `ui_dashboard_page` | Dashboard page orchestration and layout |
| `ui_dashboard_status`, `ui_status_banner` | Print state and status presentation |
| `ui_active_print`, `ui_machine_status` | Active-print and machine cards |
| `ui_command_bar` | Dashboard operator actions |

## Printer

| Modules | Ownership |
| --- | --- |
| `ui_printer`, `ui_printer_layout` | Printer page orchestration and geometry |
| `ui_printer_motion`, `ui_printer_live_status` | Motion popup and live status |
| `ui_printer_info_cards`, `ui_printer_actions`, `ui_printer_banner` | Printer presentation components |
| `ui_printer_popups` | Temperature, fan, cancel and exclude-object interactions |
| `printer_controller`, `printer_ui_controller` | Printer policy and UI command routing |
| `printer_file_controller`, `printer_files` | Selected-file policy and file metadata parsing |

## Calibration, Bed Mesh, Devices, Macros and Console

| Modules | Ownership |
| --- | --- |
| `ui_calibration` | Calibration page composition and workflow routing |
| `ui_calibration_motion` | Input Shaper, Axis Twist and Z Tilt workflows |
| `ui_calibration_pressure_advance` | Pressure Advance workflow |
| `ui_calibration_manual_probe` | Shared guided Probe/Z and Axis Twist TESTZ controls, including 0.005 mm fine steps |
| `calibration_capability_controller`, `calibration_session_controller` | Calibration availability, progress and command policy |
| `ui_bed_mesh` | Bed Mesh page composition and calibration entry |
| `ui_bed_mesh_gestures`, `ui_bed_mesh_view` | Orbit, pan, pinch and shared view transform |
| `ui_bed_mesh_renderer` | RGB565 height surface, grids, axes and origin rendering |
| `ui_bed_mesh_profiles` | Profile list, Load, Save As, Remove and SAVE_CONFIG confirmations |
| `bed_mesh_controller` | Active mesh and profile snapshots |
| `ui_devices` | Devices page composition and Telemetry bridge |
| `ui_devices_catalog_view` | Filters, cards, pagination and refresh timer |
| `ui_devices_live_values` | Visible Moonraker device-value translation |
| `device_catalog_controller` | Object discovery, classification and bounded catalog state |
| `macro_controller` | Bounded public Klipper macro discovery and sorting |
| `console_controller` | Bounded command/response history and response classification |

## Files and thumbnails

| Modules | Ownership |
| --- | --- |
| `ui_files`, `files_page_controller` | Files page and behavior routing |
| `files_row_preview`, `file_detail_loader` | Row thumbnail and long-press detail work |
| `thumbnail_manager`, `thumbnail_render` | Thumbnail download/cache state and RGB565 rendering |
| `thumbnail_session`, `thumbnail_preview_coordinator` | Selected-file metadata, layer fallback and preview coordination |
| `ui_thumbnail` | Shared thumbnail UI component |

## Network and Moonraker

| Modules | Ownership |
| --- | --- |
| `ui_network`, `ui_network_tools` | Network page and Wi-Fi tools |
| `ui_printer_profiles`, `ui_printer_chooser` | Profile Add/Edit with discovery and startup chooser |
| `printer_profile_health` | Per-profile reachability state |
| `printer_preview_cache`, `printer_profile_preview_worker`, `printer_preview_store` | Per-profile preview lifecycle; the low-priority worker owns inactive-profile HTTP health and preview requests |
| `network_status_controller`, `network_wifi_scan` | Network state policy and scan service |
| `moonraker` | HTTP requests, response parsing and synchronized printer state |
| `moonraker_config_controller` | Persistent multi-printer configuration |
| `moonraker_discovery`, `moonraker_probe` | Endpoint discovery and testing |
| `moonraker_poll`, `moonraker_live_transport` | Scheduled live-state polling |
| `moonraker_live_websocket` | WebSocket identification, subscription and event merge |

## Drybox

| Modules | Ownership |
| --- | --- |
| `ui_drybox`, `ui_drybox_page` | Drybox page routing, layout and controls |

## Telemetry

| Modules | Ownership |
| --- | --- |
| `telemetry_history` | Time-series sample storage |
| `ui_telemetry_components`, `ui_telemetry_charts`, `ui_telemetry` | Telemetry controls, charts and page orchestration |

## Settings and OTA

| Modules | Ownership |
| --- | --- |
| `ui_settings`, `ui_settings_components` | Settings page and reusable settings rows |
| `settings_system_info` | Runtime firmware, memory, network and storage information |
| `timezone_config` | Persistent timezone presets and POSIX rule application |
| `ui_settings_popups`, `ui_about_popup`, `ui_ota_popup` | Settings confirmations, product/about information and cancellable OTA UI |
| `ota_manager` | Persistent OTA URL, cancellable download task and reboot |

## Build and platform files

| Path | Ownership |
| --- | --- |
| `CMakeLists.txt` | Project identity, version and local component search path |
| `main/CMakeLists.txt` | Application component source membership and requirements |
| `main/idf_component.yml` | Direct managed component constraints |
| `dependencies.lock` | Resolved component versions |
| `sdkconfig.defaults` | Reproducible target defaults |
| `sdkconfig` | Known-good resolved configuration |
| `partitions.csv` | NVS, dual-OTA and storage layout |
| `components/espressif__esp32_p4_function_ev_board/` | Local board/display/touch support |
| `common_components/bsp_extra/` | Project-specific BSP extensions |
| `tools/audit/public_tree_audit.sh` | Public-tree safety validation |
| `tools/audit/v5_feature_architecture_audit.sh` | v5 module ownership and build-membership validation |
| `tools/end_of_night_checkpoint.sh` | Build, push and nightly publication |

Files ending in `.bak_*`, generated build output and managed-component copies
are not architecture modules and must not be tracked as production source.
