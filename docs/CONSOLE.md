# Console layout and controls

Console uses the full native page stage in all five themes. Filters wrap as needed; the command/follow/clear actions stay below the scrolling history. Studio no longer limits display to six truncated messages. All retained entries use wrapped text, with their timestamp, type prefix and type color. The existing bounded controller retains 64 messages and 16 commands. Message storage still has its existing 192-byte capacity; the UI does not recreate text already truncated by that store.

Follow scrolls to the newest retained entry. Manual scrolling turns Follow off; incoming entries preserve the visible sequence anchor where retained. If the ring evicts that entry, the view starts at the oldest remaining entry. Search, entry-type and temperature-response filters change presentation without clearing history. The counter is matched entries / retained entries. History remains session-wide; it is not a per-printer archive. The connection caption identifies the selected printer.

Command and search dialogs use native columns. The input and compact actions stay above the flexible keyboard. Prev/Next browse the existing command history. Commands remain literal, using the existing WebSocket/HTTP fallback. Sending rechecks the captured printer generation and Moonraker connection. A connected Moonraker still allows recovery commands when Klipper is in error; Console does not require homing or a ready print state.

Empty input, a changed/offline printer and rejected dispatch remain visible in the editor. A rejected send preserves the input. An accepted request closes the editor and appears in history; it is not proof the printer completed the command. There is no Console send toast. See [Notification policy](NOTIFICATIONS.md).

## Pressed button borders

Shared themed buttons suppress LVGL's default pressed surface growth and legacy one-pixel translation. Pressing changes color rather than geometry; ordinary touch focus adds no outside halo. Keyboard focus retains its two-pixel ring, and disabled controls suppress it. Studio semantic strokes retain their action color when pressed. The theme chooser and standalone Studio action primitive use the same physical press rule.

The source audit traced button creation through `ui_button.c`, `ui_widgets.c`, `ui_theme_preview.c` and `ui_studio_layout.h`. Existing custom clickable card and keyboard-item roles were checked for stable border geometry. Source-derived action tests exercise button press/focus geometry alongside text bounds; the dedicated Console suite covers every shared semantic kind, status kind, navigation, Studio actions and theme cards across all five themes.

## Validation

`console_modern_layout_test.py` covers all five themes, three densities, normal/large text, standard/narrow/short viewports, high contrast and motion settings, wrapped history, filters, retained scroll anchors, action/input/keyboard bounds, command history, generation/offline guards, rejected-input retention, error recovery, no-op refresh and lifecycle. Production Console compiles separately with warnings as errors.

`console_transport_test.py` compiles the actual main dispatch functions to check literal commands, WebSocket/HTTP fallback, host/port/auth forwarding, failure feedback ownership and retained behavior for other callers. Console filter, legacy Console row ownership, Studio pages and source-derived text/pressed-frame checks are also run.

The full older `lvgl95_modernization_test.py` has superseded popup/Macros layout expectations; its Console owner regression is run using `--variant CONSOLE`. Native dialog owners and the source-derived footer audit provide current geometry checks.

Firmware build, OTA and physical-panel verification remain pending. Check dragging history, editing commands, keyboard reach and border feedback on the panel. Existing printing/recovery tests remain pending. Stable remains 6.5.6; the feature branch stays open. Drybox retains its previous layout at the user's request.
