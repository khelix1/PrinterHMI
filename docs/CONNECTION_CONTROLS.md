# Connection-aware controls

Printer, Files and Tools use a shared readiness policy based on a fresh active-printer snapshot. Moonraker must be connected, live Klipper data must be available, and the printer must not be in startup, disconnected, unknown, error or shutdown state. Idle-only actions also reject printing and paused states. E-stop and firmware recovery use their existing separate endpoints and remain outside this policy.

| Control | Available state |
| --- | --- |
| Printer Home | Ready and idle |
| Pause | Printing |
| Resume/recovery | Paused |
| Stop and object cancellation | Printing or paused, with existing capability checks |
| Toolhead control popup | Ready, including an active print |
| Files Print and confirmation | Metadata ready, same printer/configuration, and idle |
| Macro Run | Same printer/configuration and ready; active-print macros remain supported |
| Primary calibration workflows | Ready and idle, with existing capability/workflow checks |
| Bed Mesh Calibrate and profile commands | Same viewer owner, ready and idle |

Printer refresh disables action buttons while disconnected even when a cached state still says printing, and supplies the connection reason in the banner. Callbacks also take a fresh snapshot before dispatching an action. Completed/cancelled jobs restore idle Home availability. Existing hotend, bed and fan controls during an active print remain supported.

Files detail and confirmation controls track connection changes while open. Inline labels explain waiting, offline, busy and changed-printer states. Metadata readiness is retained across reconnecting to the same configuration. Changing printer/configuration requires reselecting the file. The metadata worker captures configuration generation and checks it before requesting, after the response and inside the display lock; stale results cannot populate a new printer's inspector. UI metadata publication is also generation-checked. Closing/deleting the inspector retires its timer and confirmation.

Tools remains navigable offline so operators can inspect pages and diagnostics. Tile descriptions explain unavailable commands. Macro Run is disabled/restored in place while retaining its command and parameters; a changed printer requires reopening it. Main calibration controls disable while unavailable and restore with capability refresh. Bed Mesh Calibrate, profile Load/Save/Remove and save/remove confirmations disable while unavailable. Their final callbacks check readiness and viewer ownership, so a stale confirmation cannot command another printer. Added timers and widget references are retired with their owners.

Existing Files list request serial/generation checks, live WebSocket generation fencing, calibration session ownership and tool-control popup ownership remain in use. This pass adds metadata and Bed Mesh fencing rather than replacing those pipelines.

## Validation

`connection_controls_test.py` exercises real Printer action callbacks, disabled/recovered buttons, stale printing state, Klipper waiting/error/shutdown/startup, active-print control access, metadata switches before/during a request and inside the display lock, cancellation/allocation cleanup, Bed Mesh readiness/owner guards and profile confirmation restoration/deletion. Profile controls are exercised in all five themes with large text.

The responsive page audit covers offline/reconnected Tools hints, Files detail/confirmation states and reason bounds, stale metadata rejection, Macro Run disabling/recovery, profile changes and timer cleanup across all five themes, densities and text sizes. Existing control-popup and E-stop transport tests pass. Calibration dialog and strict source compilation checks cover the modified owners.

Target firmware build and physical panel tests remain pending. On the panel, disconnect/reconnect a printer while Files confirmation or Macro Run is open, check idle versus printing controls, switch printers during file metadata loading, and inspect Bed Mesh command availability. Confirm active-print hotend/bed/fan controls continue working. Physical print/recovery tests remain pending as previously recorded.




Validation statements above record the implementation-stage checks. Current
release status and open limitations are maintained in [Known issues](KNOWN_ISSUES.md),
the [changelog](../CHANGELOG.md) and the [current audit](PROJECT_AUDIT_2026_10_30.md).
