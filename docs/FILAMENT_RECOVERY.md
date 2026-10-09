# Filament runout recovery

A paused print's Resume action on Dashboard and Printer opens the recovery panel.
Tapping the Printer filament status while paused opens the same panel. Outside a
pause that status continues to open the sensor controls. Recovery never resumes
or extrudes automatically. Close leaves the print paused.

The panel shows live temperature and aggregate filament status, a 170–255 C
selector in 5 C steps, Heat, Unload, Load 30 mm, Purge 10 mm and Resume. The initial
selection is the current nozzle target, or 205 C if the target has been turned
off. After a cooled pause, select the correct material temperature before Heat.
Changing the selection alone does not change the heater. Heat updates the saved
resume target through HMI_FILAMENT_HEAT TEMP=<temperature>.

The four HMI_FILAMENT_* commands must appear in the active printer's macro
catalog; missing commands are disabled. No generic extrusion fallback is used.
Unload/load/purge require live paused telemetry, XYZ homing and a hot nozzle.
Resume checks enabled sensor presence and homing; the printer macro reheats and
checks again before restoring position. Repeated movement taps are briefly
blocked for the duration of the supplied short moves. The UI reports command
submission, not completion; printer errors remain visible in Console.

Closing, printer selection, theme changes and popup teardown delete the refresh
timer. The endpoint generation is checked on each refresh and click; an old panel
cannot send to a newly selected printer. Offline, stale or resumed telemetry
disables recovery actions. Standard sensor controls remain available outside a
pause; this panel does not automatically disable a sensor.

## Sermoon D1 stock direct drive configuration

`config/examples/sermoon_d1_filament_recovery.cfg` is for the supplied stock
Sermoon configuration, with `filament_switch_sensor filament_sensor`, maximum Z
300 and park X2/Y10. Replace the existing PAUSE, RESUME, STOP_PRINT and
CANCEL_PRINT sections before including the example; never define them twice.
The supplied complete revised config is an alternative, not an additional include.
It preserves the uploaded configuration's other sections and SAVE_CONFIG block.

PAUSE captures the original print state via PAUSE_BASE, retracts 2 mm if hot,
lifts by up to 10 mm within the Z limit and parks. Repeated PAUSE leaves the
original state intact. Extrusion helpers save/restore parser state with MOVE=0,
so loading does not corrupt the print's extrusion coordinates or modes.
RESUME reheats to the saved or selected temperature, checks homing and the sensor,
restores the 2 mm retract and uses RESUME_BASE to return at 30 mm/s. The dedicated
resume helper is evaluated after M109, so its temperature check is current.

After ten minutes paused, idle_timeout turns off only the nozzle. The bed and
motors remain powered to preserve adhesion and position. Outside a pause, normal
heater/motor shutdown remains in effect. Restarting Klipper, cycling power or
manually releasing motors loses the paused position; do not attempt recovery by
homing over an existing print. Cancel and STOP_PRINT skip cold extrusion and
cap their Z lift, so a cooled paused print can still be cancelled.

Install/restart only when no print is active. First test on a small sacrificial
print: Pause → park → Heat → Unload → replace spool → Load → Purge → remove strand
→ Resume. Then check a sensor-triggered pause, a cooled pause, and Cancel.
The supplied 80 mm unload is intended for the stock direct drive, not a Bowden,
multitool or filament changer configuration.

## Verification

- `filament_recovery_test.py`: actual production UI/controller events using LVGL
  9.5; all four themes, densities and large text, missing macros, temperatures,
  sensor states, offline/profile fencing, repeat taps and deletion.
- `filament_recovery_macros_test.py`: actual Jinja templates with simulated
  command/state sequencing; requires Jinja2. Covers both extrusion modes,
  repeated pause, near-maximum Z, cooling/reheat, sensor/homing rejection and
  cooled cancellation. This is not a Klipper hardware test.

Klipper references: [pause/resume](https://www.klipper3d.org/G-Codes.html#pause_resume),
[idle timeout](https://www.klipper3d.org/Config_Reference.html#idle_timeout),
[macro evaluation](https://www.klipper3d.org/Command_Templates.html).
