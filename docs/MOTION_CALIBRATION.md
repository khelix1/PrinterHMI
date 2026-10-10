# Motion and calibration dialog layout

Calibration uses a shared native LVGL column: one-line heading, vertically
scrolling body and a pinned action row. Modal dimensions are capped to the
current display. Instructions, measured results and lists can grow without
covering Back, Run, Abort, Accept or Apply & Restart. Standalone Close actions
remain compact. Short button labels fit on one line; manual TESTZ and PID target
choices wrap as whole buttons at narrower widths.

The layout covers Probe/Z and Axis Twist confirmations, manual probe adjustment,
Screws Tilt and Gantry leveling, PID heater selection/target/warning, Input
Shaper, Resonance and Accelerometer checks, Pressure Advance setup, custom
calibration selection/confirmation, results/save confirmation, Probe Accuracy
and Live Endstops. Native Motion diagnostics retain their runtime-limit,
axis-distance and driver views. Their numeric limit readings stay on one line,
measurement editor fields remain touch-sized, and dropdown fonts include symbol
fallback for Studio.

Command sequences and explicit confirmation steps are retained. Dialog action
buttons capture the printer generation and reject command choices after a
printer switch; Close/Back still let the operator leave the dialog. PID warning
Back preserves the chosen target and the original printer generation. Probe
Accuracy shows current homing/readiness status from the existing state snapshot
and enables Run only for an idle, live printer with XYZ homed. The operator must
still verify the probe is over the bed; homing does not establish a safe probe
position. The existing Run callback rechecks ownership, readiness and homing.

Custom calibration discovery increments the count value, rather than its
pointer. This fixes an existing out-of-bounds access when discovering matching
macros. Existing result and custom-calibration text now uses real newline
characters rather than displaying literal backslash-n sequences.

## Validation

`tools/audit/calibration_dialog_layout_test.py` builds real owner implementations
against LVGL 9.5 with typed transport, capability, catalog and session fixtures.
Nine owner variants cover all five themes, three densities, normal/large text,
1024/640/480 widths and the 480 x 400 short viewport. Checks include scrolling
to the end with stationary action rows, label bounds, PID target retention and
stale-owner rejection, all ten TESTZ choices, resonance/noise commands, probe
homing gates and sample selection, twelve endstops, sixteen custom macros,
Pressure Advance factor and results/save confirmation. It also compiles every
changed calibration owner independently with warnings treated as errors,
including the page's Probe/Z and Axis Twist constructors. Those two page
constructors are compiled; their physical workflows remain panel checks.

`tools/audit/motion_layout_test.py` covers all five themes, the distance
calculator/editor, atomic limit values, driver/offline/profile transitions and
widget deletion. Guided Motion actions are tested through their real owner in
the calibration dialog suite, replacing the old reconstructed button fixture.
The general button-fit audit still covers source-derived fixed actions and
actual card/navigation/settings controls. Native calibration actions are checked
by the dedicated real-owner suite.

Target firmware build, OTA and panel verification remain pending. Check each
workflow in normal and large text, reach the final list/result line, cancel
before sending commands, and confirm offline/unhomed/printer-switch states.
Only run physical calibration on an attended, prepared printer. Existing
printing-dependent recovery and active-print control tests remain pending.
Stable version remains 6.5.6; this update neither commits nor pushes the branch.
