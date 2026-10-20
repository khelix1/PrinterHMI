# Telemetry

Open **Tools > Devices > Graphs** in Studio, or **Telemetry** in the other
built-in themes. The Devices action returns to the device catalog. Telemetry
uses the active printer's existing live stream; it sends no heater, motion,
fan or print commands.

## Views

| View | Channels |
| --- | --- |
| Heat | Selected hotend, heated bed, drybox center, drybox air |
| Motion | Velocity, volumetric flow, speed override, flow override |
| Environment | Drybox air, relative humidity, part fan, drybox fan |

The Heat selector chooses a graph source, **not** the printer's active tool.
All hotends supported by the current host model (up to four) are recorded
independently by object name. Discovery reorder and active-tool changes do not
combine their temperature traces. Volumetric flow is the active tool's reported
flow; its live readout names that tool. Drybox center and air readings come from
the existing center/environment sensor fields, not an inferred printer chamber.

Live values use C, mm/s, mm3/s, % or %RH. A discovered capability that is absent
shows N/A. Missing or invalid readings show --. Missing values are not converted
into zeros. The header distinguishes live data, offline/waiting, and a held
graph; the footer reports the latest recorded sample's age.

## Graph controls

- Choose 2, 5 or 10 minutes. Each instrument has its own scale and units.
- Targets on shows a dim historical heater-target trace and includes it in the
  scale. The colored measured trace remains visible when both coincide.
- Detail only fits measured values and hides the target trace. Live target and
  target-error readouts remain visible. A zero target is reported as heater off.
- Hold graph freezes the plotted data, scale and window statistics. It does not
  pause the printer. Live values and background history sampling continue.
  View/tool/range/scale controls are disabled while held; Resume live catches up.
- Scroll the graph area for the remaining instruments. Navigation, view/range
  controls and sample status remain outside the scrolling graph owner.

Min, Max and Span describe measured samples in the selected time window. Scale
shows the plot's lower/upper bounds. The right-edge dot identifies a current
sample; an empty current time bin has no dot. Heater targets do not affect
measured-value extrema. Unsupported sensors retain an explicit empty instrument.

## History and ownership

History is a bounded RAM ring of 300 samples, collected at most once every two
seconds even while the page is hidden. Sample timestamps place data into fixed
time bins. Missing network intervals remain blank; old samples are not stretched
over an outage or connected across an empty bin. Reboot does not preserve this
history. The main active-printer reset still clears it. A generation change
also retires any held graph, preventing the previous printer's plot from
remaining under a new identity. A backwards monotonic clock resets the ring.

The page reuses its chart/card owners on refresh and switches data bindings for
views. Graphs refresh on the two-second time grid or an explicit control change;
unchanged live labels/styles are not rewritten. The enlarged ring retains the
existing PSRAM-first allocation and internal-RAM fallback. If history allocation
is unavailable, the page explicitly reports live-values-only operation.

Native flex layouts serve all five built-in themes and custom palettes/density
settings. Two graph columns fit wide viewports; narrower viewports use one.
Legacy telemetry rectangle fields remain in the theme schema for compatibility,
but this page now derives layout from the viewport instead of fixed rectangles.

## Validation

Run `tools/audit/telemetry_modern_layout_test.py` with managed LVGL sources or
`--lvgl-dir /path/to/lvgl` and optionally `--lvgl-lib /path/to/liblvgl.a`. It tests
production code against real LVGL 9.5 across five themes, three densities, both
text sizes and 1024/640/480 widths, including a 400 px short viewport. It covers
pointer routes, native bounds/scroll reach, windows/targets/hold, independent
hotend identity, outage gaps, absent/invalid channels, history reset/expiry,
unchanged refresh, external deletion/reopen, and separate C compilation with
warnings treated as errors. Moonraker/shell/clock calls are typed host fixtures.

`telemetry_refresh_test.py` retains Dashboard/Printer unchanged-refresh checks,
chart ring equivalence, two-second sampling and bounded history. Chooser refresh
checks remain required. The subsequent Macros follow-up replaces isolated
search-action reconstruction with real-owner layout checks, including Studio
font fitting. The general button-fit audit now passes; Macros search, parameter
editing, command review and printer ownership have dedicated interaction checks.

Target build, OTA and panel checks remain pending. Check normal/large text in
each theme, all views, tool/range/target selection, Hold/Resume, graph scrolling,
Devices navigation, an offline transition and a printer switch. Existing
printing-dependent recovery/control tests remain pending. Stable stays 6.5.6.

## Continuous live traces and flat selectors

History samples and chart columns use the same wall-clock two-second bins. UI polling jitter no longer accumulates into artificial gaps. Each bin records one actual live snapshot with its original timestamp; missed intervals, disconnected printers and invalid sensor readings remain gaps. Clock rollback and printer changes still reset ownership.

The range and hotend selectors use flat fields with a small corner radius, no inherited card/pill shadow, and matching dropdown lists. Keyboard focus remains visible. The host audit covers jitter across a full 300-point window, true gaps, invalid readings and selector styling in every theme, density, text size and supported viewport.
