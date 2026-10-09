# Control-dialog nightly validation

Stable version: 6.5.6. This is a development nightly, not a new stable release.

Included: paused filament recovery with temperature selection; responsive
nozzle/bed/fan, Motion, sensor/status, hotend, object-exclusion and whole-print
cancellation dialogs. Settings Time Zone/Reset changes have not started.

Host checks cover real LVGL layouts across all four themes, densities/text
sizes and representative widths, command identity, endpoint/state guards and
modal teardown. The incremental layout work was accepted by the operator.
The closeout runs repository audits, host checks and the canonical ESP-IDF
6.0.2 build through tools/end_of_night_checkpoint.sh.

Printing and hardware validation remains pending for the next test session:

- Sensor-triggered runout pause, parking, loading/purging, resume and cooled pause.
- Hotend/bed temperature and fan adjustments during printing and pause.
- Single-object exclusion while other objects continue printing.
- Whole-print cancellation and recovery afterward.

The example Sermoon D1 recovery macros need printer-specific validation;
publishing firmware does not install those macros on the printer.
No completed printing test, soak or known-good designation is claimed.

Feature branch: `feature/filament-recovery`
