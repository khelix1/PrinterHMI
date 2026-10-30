# Keyboard and input dialogs

Native editors keep their focused field in the scrolling content and place the keyboard above a stationary action row. The keyboard adapts to the available form height; compact spacing preserves field visibility in small dialogs. The shared focus helper resolves layout before bringing the field into view. Text inputs use the body font and retain their requested minimum height after LVGL's one-line mode updates their style.

| Editor | Keyboard completion | Keyboard cancel | Validation/ownership |
| --- | --- | --- | --- |
| Files search | Applies search, matching Search | Discards, matching Cancel | Existing query limit and deferred keyboard teardown |
| Macros search/parameter | Copies edited value, matching Done | Discards editor value | Macro review and printer guards remain separate |
| Network password | Requests connection, matching Connect | Returns to scan, matching Back | Mask/length limit; failed save/config/start retains input |
| Network port | Saves, matching Save | Discards | Digits, five-character limit, 1–65535 |
| Profile field editor | Copies field, matching Done | Discards | Numeric port is checked before copying; API keys remain masked |
| Backup passphrase | Saves/verifies, matching footer | Clears staged secret and cancels | Existing minimum length, matching confirmation and worker checks |
| OTA URL | Uses the existing Start Update flow | Closes editor | Empty URL remains editable; existing updater owns confirmation/progress |
| Motion measurement | Validates and copies, matching Done | Discards | Pitch 0.000001–1000 mm; whole count 1–10000; stale printer rejected |
| Custom temperature | Sends, matching Set | Returns to temperature presets | Digits and the control's existing temperature limit; 0 means Off |
| Setup Wi-Fi | Starts connection verification | Returns to network scanning | Masked password; duplicate connection requests are blocked during verification |
| Console command/search | Existing Send/Apply behavior | Existing discard | Literal command dispatch, connection/generation guards and rejected input retention |

Files search now uses a viewport-clamped native form. Macros, OTA, motion, profile field editing and custom temperature no longer put their keyboard inside the scrolling body. Setup moves its network context into the scrolling stage so the keyboard and Connect remain visible. Backup focus continues to select the intended passphrase field. Arbitrary macro parameters remain text because their printer-defined meaning is not a numeric schema. Fan presets retain their existing 0–100 percent mapping.

Temperature and fan controls use a checked dispatch callback in the application. An offline/stale-printer or rejected request retains the control and entered temperature with local feedback. Successful dispatch follows the existing close behavior; it does not claim the target temperature has been reached. Closing a temperature owner also retires its nested custom editor. Existing legacy void callbacks remain supported by the popup API, while the application registers its checked sender during startup.

## Validation

Real LVGL owner suites exercise keyboard completion/cancel, field and keyboard/footer bounds, full focused-field visibility after scrolling, port and motion rejection/correction, masked fields, accepted-character and length limits, temperature/fan rejection/retry, printer-generation fencing, nested deletion and duplicate Setup connection prevention. Coverage spans all five themes, densities and normal/large text; individual owner suites include standard/narrow/short viewports. Console, hotend, notification and theme-preview regressions pass, including separate production compilation with warnings treated as errors where supported. The Console test driver links the dashboard-profile and small font used by the theme previews.

Host renders were inspected. Firmware build, touch/keyboard entry, actual network failures, real temperature/fan requests and earlier printing/recovery tests remain pending.


Validation statements above record the implementation-stage checks. Current
release status and open limitations are maintained in [Known issues](KNOWN_ISSUES.md),
the [changelog](../CHANGELOG.md) and the [current audit](PROJECT_AUDIT_2026_10_30.md).
