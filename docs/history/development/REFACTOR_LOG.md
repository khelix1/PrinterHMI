
## 2026-07-04 — Popup Consolidation Continued (Verified)

### Result
- main.c reduced from 6981 lines to 6297 lines (-684).
- All changes built successfully.
- OTA update completed successfully after every extraction.

### Completed
- Merged ui_network_port_popup into ui_network_tools.
- Extracted OTA popup into ui_ota_popup.
- Moved Remote Builds placeholder into ui_ota_popup.
- Moved Dashboard Status popup into ui_dashboard_v32.
- Moved Moonraker Test popup into ui_network_tools.
- Moved System Information popup into ui_settings.

### Ownership Improvements
Network Tools now owns:
- WiFi Scan popup
- WiFi Password popup
- Moonraker Port popup
- Moonraker Test popup

Dashboard now owns:
- Status popup UI

Settings now owns:
- System Information popup UI

OTA now owns:
- OTA Update popup
- Remote Builds placeholder popup

### Architecture
Continuing the feature-ownership refactor:
- One feature
- One owner
- One module

Verified on hardware after each extraction before proceeding.

### Next Target
Move the Moonraker Scan subsystem into ui_network_tools:
- scan popup
- popup lifecycle
- status updates
- candidate buttons
- helper UI functions

