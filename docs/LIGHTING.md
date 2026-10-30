# Case lighting

The **Tools → Lighting** button appears only when `output_pin case_led` has been
detected for the active printer. It remains hidden during initial discovery or
when that printer has no matching light. Detection updates while Tools is open.
Open **Lighting** for the active printer. The toggle switches the case
light on/off. With PWM enabled, drag the brightness slider and release to send a
single command. Turning on restores the last nonzero value observed while the
dialog is open (100% if no nonzero value has been observed). While Applying is shown, the slider holds the requested position. Live Klipper
status confirms it; failure/timeout returns to the last live value. Commands
pending confirmation temporarily disable controls.

The light must already be configured as `[output_pin case_led]` in Klipper.
Keep your working pin, inversion and shutdown settings. PrinterHMI does not edit
printer.cfg, add macros or persist a new setting. `pwm: True` enables brightness;
without PWM only on/off is enabled. Existing `scale` is honored, including values
such as 1, 100 or 255. Only this named output is controlled; RGB/LED chains,
`fan_generic` and `pwm_tool` objects are not part of this first batch.

Controls remain available during printing and pause. The live Moonraker WebSocket
must be connected and Klipper ready. Missing output, unverified configuration,
unknown values or a changed printer disable writes with local feedback. Open the
dialog again after switching printers. Other clients/macros can change the value;
live status updates the controls. A command is never retried automatically. If
confirmation is missing, check the printer and retry deliberately. Closing the
dialog does not undo an already transmitted command.

## Implementation and validation

Discovery/config use the existing object list and configfile query. Output-pin
values already flow through the shared device subscription. Klipper reports a
normalized output value; SET_PIN expects the configured scale for PWM. Commands
use a fixed `SET_PIN PIN=case_led VALUE=...` string, a profile-generation fence and
nonblocking send on the current authenticated/TLS WebSocket. No HTTP fallback or
extra polling is added. Request IDs and live echo prevent stale command errors
or unrelated status from confirming a new action.

References: [Klipper output_pin implementation](https://github.com/Klipper3d/klipper/blob/master/klippy/extras/output_pin.py)
and [SET_PIN](https://www.klipper3d.org/G-Codes.html#set_pin).

Run `case_lighting_test.py --cjson-dir PATH` for the sanitizer-backed model and
actual extracted WebSocket adapter. Run `case_lighting_ui_test.py` with
`--cjson-dir`, `--lvgl-dir` and optional `--lvgl-lib` for native modal behavior.
Run responsive Tools, offline navigation, readiness and emergency transport
regressions too. Host lock fixtures do not simulate RTOS concurrency or a panel.
Panel-check real on/off, several brightness levels, scaled/digital outputs,
external changes, printing, close/reopen, unavailable printers and reconnects.
