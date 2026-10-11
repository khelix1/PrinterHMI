# Event History

Event History retains the bounded 40-entry operator log and oldest-to-newest ordering. Each native row separates timestamp/severity from a full wrapped message. Row height follows the content in every theme and text size. The viewport-clamped popup scrolls its rows while Hold, Clear and Close remain in the footer.

The popup opens in live-follow mode. Hold freezes the displayed snapshot; new log entries still arrive in the underlying ring. Follow Live reloads that ring and scrolls to its newest entry. Reopening an already visible popup preserves Hold. Clear erases the operator log using the existing API and renders the empty state. It does not clear printer faults or notification notices. Closing or external deletion stops the refresh timer and clears popup references.

`tools/audit/event_history_layout_test.py` exercises the real owner with typed log fixtures: five themes, three densities, normal/large text, 1024/640/480-pixel widths and a 400-pixel short display, complete wrapped rows, centered 48-pixel actions, fixed footers, unchanged refresh, Hold/Follow, replacement in a full ring, Clear and deletion/reopen. The owner compiles separately with warnings as errors. Target build, panel scrolling/touch and earlier printing/recovery tests remain pending.
