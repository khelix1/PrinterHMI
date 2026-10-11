# Notification policy

A toast is optional confirmation of a completed, noncritical action. Missing it must not change what the operator needs to do. It must never be the only evidence that a command failed, that the printer needs attention, or that a running operation completed.

| Situation | Presentation | Examples |
| --- | --- | --- |
| Small action completed and its result is otherwise easy to miss | Optional short toast | Copied, exported, saved a preference |
| Result is already visible | Update the control; no toast | Theme changed, favorite toggled, filter applied, target edited |
| Input is invalid or an action is unavailable | Inline feedback near the field/action; retain input | Empty command, invalid macro parameter, home axes first |
| Transport rejects a command | Persistent inline result or page notice, with retry context | Print not started, command not sent, calibration rejected |
| Established printer connection is lost | Persistent warning plus page status at every level | Printer offline; later recovery replaces the connection notice |
| Operation is ongoing | Persistent page status | Reconnecting, PID running, OTA progress |
| Operator decision is required | Explicit dialog before acting | Start print, save configuration/restart, destructive action |
| Safety or recovery needs attention | Persistent prominent fault/recovery dialog | Runout, stop not confirmed, restart failed, heater fault |

## Completion language

Sending a request and completing its effect are distinct events. WebSocket dispatch or HTTP acceptance may say **Request sent**; it cannot establish **Calibration complete**, **Printer stopped**, **Temperature reached**, or **Print started**. Those states require printer telemetry. Console keeps the command attempt and reported response/error in its bounded session history; it does not add a success toast for every command.

## Appliance-wide preference and current implementation

Settings → Display → Notifications selects one appliance-wide level:

| Level | Notifications shown |
| --- | --- |
| Quiet (default) | Connection loss/recovery, paused prints, warnings, reported printer errors and recovery instructions |
| Standard | Quiet plus reported print start/resume/completion/cancellation |
| Detailed | Standard plus routine success confirmations |

Three direct choices are available; Save persists the selection in NVS and configuration backups. Local validation, recovery instructions and page progress remain visible at every level. The former OFF preference migrates to Quiet; ON migrates to Detailed. Older boolean backups follow the same mapping; backups without a setting default to Quiet. The new numeric level takes precedence when present, and malformed or out-of-range values are rejected. All five built-in themes remain valid backup values.

The shared notification component enforces this policy for existing callers. Successful/neutral confirmations appear briefly only in Detailed. Routine informational event cards require Standard or Detailed; connection recovery is essential and remains visible in Quiet. Quiet still records reported informational events in Event History. Lowering the level removes pending routine informational cards while retaining warnings, errors and essential recovery. Warnings and errors appear in a scrollable, persistent notice with a compact Dismiss action. They have no dismissal timer and cannot be disabled by the preference. Dismiss acknowledges the message; it does not clear a printer fault. Each notice captures the selected printer name at creation, so changing printers cannot relabel an old failure.

Pending notices are bounded to four, exact duplicates are coalesced, danger notices take priority over warnings, warnings take priority over information, and overflow important notices are recorded in the existing bounded Event History ring. Optional success confirmations never displace an important notice. A queue-overflow message directs the operator to Event History. Event History is bounded to its existing 40 entries and its existing message length; this is not a durable fault archive.

Console owns empty-input, offline/stale-printer and send-failure feedback inside its editor. Rejected input remains editable. Its HTTP fallback suppresses the generic transport notice to avoid duplicate feedback. Calibration save, gantry and abort acknowledgements now stay in the workflow and describe dispatched requests. Stale-printer errors remain visible beside their blocked controls.

## Workflow migration status

Macro validation/send errors, calibration prerequisites and request results, print confirmation failures, OTA startup failures and emergency-stop/restart outcomes now use owned local feedback. General transport failures retain persistent notices with explicit Open Console navigation. Existing live result/state controllers continue to report execution outcomes after dispatch. The appliance-wide level governs informational event notices and optional transient confirmations; it does not hide local validation or faults.

Physical printing, configuration-save/reconnection and recovery tests remain pending.

## Validation

`notifications_test.py` uses real LVGL to check global preference behavior, NVS persistence/failure, notice persistence, profile identity, priority, deduplication, bounded queues and Event History forwarding, layout/scroll/footer bounds and lifecycle in all five themes, three densities, normal/large text and wide/narrow/short displays. The production notification unit compiles separately with warnings as errors. `notification_backup_test.py` checks the actual preference parser against real libcJSON, including older-backup defaults and malformed values. Firmware/panel tests remain pending.

## Feedback at the action and explicit notice navigation

| Action | Feedback location |
| --- | --- |
| Macro validation, stale printer, failed dispatch | Parameter/review dialog; entered values retained for correction or retry |
| Macro sent/favorite change | Macro page status and favorite marker |
| Calibration readiness, command failure and leveling request | Current calibration dialog; a dismissible calibration status dialog when no workflow is open |
| Configuration save | Save dialog stays open with request/reconnect instructions; dispatch failure retains results and retry controls |
| Print readiness, printer change, HTTP start failure | Print confirmation remains open with inline explanation and Cancel |
| Emergency stop/restart | Recovery dialog displays dispatch outcome; M112 dispatch still happens immediately before rendering |
| Empty custom update URL | Editor remains open with a URL-required prompt |
| Update startup failure | Dismissible Update Not Started dialog; running update progress remains owned by the updater |
| Generic command failure | Persistent notice with explicit Open Console and Dismiss |

Tapping the notification body does not navigate. Open Console is explicit, never sends a printer command, and only opens when the captured configuration generation still owns the active printer. A stale notice remains visible with an explanation instead of navigating into another printer's context. Dismiss only acknowledges the notice. Optional confirmations stay non-interactive and follow the appliance-wide preference.

Local feedback is owned by its dialog and does not create a second toast. Existing calibration result/session updates, OTA download progress, live recovery state and command resolution remain responsible for actual execution outcomes; accepted dispatch is described as a request, not completed motion or a verified restart.

Host validation covers all five themes, densities, large text and narrow viewports; macro field retention/retry, stale command rejection, immediate M112 failure, restart feedback, nested calibration deletion, SAVE_CONFIG controls, print confirmation failures, OTA startup/active-update cases, notification navigation guards and actual WS/HTTP routing. Firmware build, panel and physical printing/recovery tests remain pending.

## Event ownership

Printer connection and print-state events come from observed telemetry transitions for the active configuration generation. Opening the application or selecting a printer establishes a baseline, without generating a fabricated print start. The initial connection must remain live for three seconds before connection monitoring arms; chooser activation and bootstrap gaps produce neither Offline nor Reconnected notices. Later connection changes must persist for three seconds, filtering brief transport freshness gaps. Repeated telemetry polls do not emit repeated notices. A disconnect preserves the last reported job state; reconnecting alone does not invent a print transition. Offline is a persistent warning at every level; recovery replaces this owner’s old connection card and remains visible in Quiet. Paused transitions require attention at every level. After connection monitoring is armed, reported error/shutdown states produce a persistent fault with Open Console, including when Moonraker remains reachable but live Klipper telemetry is unavailable. Ordinary job events remain governed by the notification level and are recorded in Event History even when Quiet hides their cards. Notices use the existing bounded queue and explicit Open Printer or Open Console action. Monitoring covers the active configuration; it does not add background connections to every configured printer. Live progress remains in its owning page/dialog, including firmware downloads.

## Attention regression checks

The real-owner suite exercises Offline/Recovery at all three levels, the full three-second debounce, repeated unacknowledged loss/recovery cycles, paused/error/shutdown attention, duplicate suppression after dismissal, full informational queues receiving warnings, fault priority, routine-card filtering on a switch to Quiet, and bootstrap fencing. These run across all five themes, densities, both text sizes and the existing viewport matrix. Existing NVS/backup preference behavior and navigation ownership remain covered. Host checks pass; firmware/panel connection-loss and physical recovery tests remain pending.
