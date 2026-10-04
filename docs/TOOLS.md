# Tools guide

Tools provides Calibration, Bed Mesh, Devices and Macros for the selected
printer. Actions that move, heat or persist configuration check printer
readiness and their workflow requirements again when dispatched.

## Calibration

Probe Accuracy offers 5, 10 or 20 samples at the current probe location after
homing. Review the command before running. Results show Klipper's final range,
average, median and standard deviation. The check does not save configuration.
Calibration sessions belong to the selected profile and time out rather than
remaining indefinitely busy after missing responses.

The Motion card provides six actions:

| Action | Behavior |
| --- | --- |
| LIMITS | Current runtime velocity, acceleration, square corner velocity and minimum cruise ratio |
| DISTANCE | Local rotation-distance calculator using belt pitch × pulley teeth or screw thread pitch × starts |
| DRIVERS | Select a discovered TMC driver to see reported run/hold current, temperature and warning/fault flags |
| SHAPER | Existing capability-aware input-shaper workflow |
| RESONANCE | Existing resonance-test workflow |
| SENSOR | Existing accelerometer-check workflow |

Limits and driver views send no movement or configuration commands. Unavailable
readings stay unavailable; unsupported temperature and null/unsampled driver
status are not represented as healthy. Open-load indications may occur on idle
motors. These views show reported samples, not a guarantee of driver health.

The distance calculator shows configured X/Y/Z reference values but changes no
settings. Examples: 2 mm belt pitch × 20 teeth = 40 mm; 2 mm screw thread pitch ×
4 starts = 8 mm. Keep configured gear ratios when using a result manually.
Do not tune XYZ rotation distance using printed-part dimensions or measured
travel; derive it from the transmission geometry.

## Bed Mesh

Profile load, save and remove actions recheck printer state. Saving a profile
and persisting it with SAVE_CONFIG use one ordered script after confirmation.
SAVE_CONFIG restarts Klipper; review the confirmation before continuing.

## Devices and endstops

Devices lists objects reported by Klipper with filters, pagination and live
values. ENDSTOPS opens a separate query view showing OPEN or TRIGGERED for each
reported limit switch. Readings refresh every two seconds while the printer is
idle. Press and hold a physical switch long enough for the next sample.
Queries pause during printing or a paused print. Disconnects, timeouts and
profile changes show an unavailable/waiting state instead of stale readings.
Sensorless endstops may only trigger during homing.

## Macros

Search filters public macros by name; underscore-prefixed helpers stay hidden.
The parameter dialog detects up to eight parameter names from macro source and
provides two extra named fields. This is a hint, not a complete Jinja parser.
Blank values are omitted so macro defaults can apply. Duplicate/invalid names
and command separators are rejected. Review the final command before Run.
Printer-profile changes invalidate pending macro actions.

For host and device checks, see [Testing](TESTING.md).
