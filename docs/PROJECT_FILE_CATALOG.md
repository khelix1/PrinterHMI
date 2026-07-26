# Project file catalog

This catalog reflects the v4.2.1 source list in `main/CMakeLists.txt`. Update it
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
| `ui_theme_preview`, `ui_theme_lab`, `ui_appearance_popups` | Theme selection and operator preview tools |
| `ui_widgets`, `ui_button`, `ui_cards`, `ui_popup` | Shared controls and modal popup system |
| `ui_page_title`, `ui_page_state_v32`, `ui_toast_v32` | Shared page and transient-state components |

## Shell and splash

| Modules | Ownership |
| --- | --- |
| `ui_shell` | Persistent top bar, navigation, clock and active printer |
| `ui_splash_v32` | Startup splash state and progress |
| `ui_logo_assets` | Compiled PrinterHMI logo assets |

## Dashboard

| Modules | Ownership |
| --- | --- |
| `ui_dashboard_v32`, `ui_dashboard_page_v32` | Dashboard page orchestration and layout |
| `ui_dashboard_status_v32`, `ui_status_banner_v32` | Print state and status presentation |
| `ui_active_print_v32`, `ui_machine_status_v32` | Active-print and machine cards |
| `ui_command_bar_v32` | Dashboard operator actions |

## Printer

| Modules | Ownership |
| --- | --- |
| `ui_printer_v32`, `ui_printer_layout_v32` | Printer page orchestration and geometry |
| `ui_printer_motion`, `ui_printer_live_status` | Motion popup and live status |
| `ui_printer_info_cards`, `ui_printer_actions`, `ui_printer_banner` | Printer presentation components |
| `ui_printer_popups` | Temperature, fan, cancel and exclude-object interactions |
| `printer_controller`, `printer_ui_controller` | Printer policy and UI command routing |
| `printer_file_controller`, `printer_files` | Selected-file policy and file metadata parsing |

## Files and thumbnails

| Modules | Ownership |
| --- | --- |
| `ui_files_v32`, `files_page_controller` | Files page and behavior routing |
| `files_row_preview_v32`, `file_detail_loader_v32` | Row thumbnail and long-press detail work |
| `thumbnail_manager_v32`, `thumbnail_render_v32` | Thumbnail download/cache state and RGB565 rendering |
| `thumbnail_session_v32`, `thumbnail_preview_coordinator_v32` | Selected-file metadata, layer fallback and preview coordination |
| `ui_thumbnail_v32` | Shared thumbnail UI component |

## Network and Moonraker

| Modules | Ownership |
| --- | --- |
| `ui_network_v32`, `ui_network_tools` | Network page and Wi-Fi tools |
| `ui_printer_profiles`, `ui_printer_chooser_v32` | Profile Add/Edit with discovery and startup chooser |
| `printer_profile_health` | Per-profile reachability state |
| `printer_preview_cache_v32`, `printer_profile_preview_worker_v32`, `printer_preview_store_v32` | Per-profile preview lifecycle |
| `network_status_controller`, `network_wifi_scan` | Network state policy and scan service |
| `moonraker` | HTTP requests, response parsing and synchronized printer state |
| `moonraker_config_controller` | Persistent multi-printer configuration |
| `moonraker_discovery`, `moonraker_probe` | Endpoint discovery and testing |
| `moonraker_poll`, `moonraker_live_transport` | Scheduled live-state polling |
| `moonraker_live_websocket` | WebSocket identification, subscription and event merge |

## Drybox

| Modules | Ownership |
| --- | --- |
| `ui_drybox_v32`, `ui_drybox_page_v32` | Drybox page routing, layout and controls |

## Telemetry

| Modules | Ownership |
| --- | --- |
| `telemetry_history` | Time-series sample storage |
| `ui_telemetry_components`, `ui_telemetry_charts`, `ui_telemetry_v32` | Telemetry controls, charts and page orchestration |

## Settings and OTA

| Modules | Ownership |
| --- | --- |
| `ui_settings`, `ui_settings_components` | Settings page and reusable settings rows |
| `settings_system_info` | Runtime firmware, memory, network and storage information |
| `timezone_config` | Persistent timezone presets and POSIX rule application |
| `ui_settings_popups`, `ui_ota_popup` | Settings confirmations and cancellable OTA UI |
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
| `tools/end_of_night_checkpoint.sh` | Build, push and nightly publication |

Files ending in `.bak_*`, generated build output and managed-component copies
are not architecture modules and must not be tracked as production source.
