# Experimental 3D G-code Viewer

Branch: `feature/gcode-viewer-3d`. Baseline: v7.1.0-dev. First delivered patch:
v7.1.1-dev. Increment the patch number for each subsequent delivered patch.
Theme Maker remains parked on its separate branch.

## Open and operate

Tools has a **3D G-code Viewer** header action. It opens the last selected file
for the active printer, if available. **Sample** displays an offline 60-layer
tapered tower without downloading anything. To choose another file, return to
Files, select it and use **3D view** on its preview. The thumbnail and ordinary
Print confirmation remain available.

File loads default to **Solid** when its depth and staging buffers are available. The tablet automatically
prepares a smoother solid from the supported extrusion paths while keeping the initial
path preview usable. **Solid** switches to **Lines**, and **Lines** switches back.
Drag the canvas to orbit; pinch to zoom. Use Top/Front/3D for standard views, -/+ to zoom, Reset to restore the view, Travel to
show/hide non-extruding moves, the slider or Layer -/+ to select an extrusion Z level, Single/Up to to choose
individual or cumulative inspection (Solid uses shaded height sections and actual
active-layer strands; Lines inspects retained/cached paths), and
All to return to the whole model. Close cancels loading. Reload retries the
selected file. Sample is always explicitly labelled and never printed.
No G-code command or print request is sent by the viewer.

## Input and resource limits

This is an orthographic toolpath file inspector, not a slicer or physical
printer simulation. Supported file motion includes G0/G1, G2/G3 arcs, G17/G18/G19
planes, relative/absolute XYZ and E, G92 origins (including no-argument reset),
G20/G21 units, G90.1/G91.1 centers, explicit SET_GCODE_OFFSET and bounded
SAVE_GCODE_STATE/RESTORE_GCODE_STATE return moves. G91 also makes E relative.
Z hops, travel/retraction and positive-E extrusion use the same path pipeline.

Arcs accept I/J/K center offsets or signed R, full circles and helical third-axis
interpolation. G18 follows the Klipper X/Z plane convention. Tessellation uses
a target of at most 1 mm of arc length and 5 degrees per segment. Arcs that
would exceed 4,096 segments are coarsened and labelled; global budgets follow. Radius/endpoint inconsistency and invalid centers
stop with a diagnostic. The final point is the exact supplied endpoint. Relative
endpoints, R arcs, absolute centers and inch units are viewer extensions; preview
support does not certify that stock Klipper accepts a file.

T0-T15 and canonical ACTIVATE_EXTRUDER names (`extruder`, `extruder1` through
`extruder15`) track independent logical E positions and color the retained paths.
T0 uses cyan shades, T1 orange, T2 green, T3 magenta, T4 blue, T5 yellow, T6 red,
T7 white; the palette repeats for higher IDs. Actual tool offsets and docking
macros are not downloaded or expanded. Each change establishes a fresh endpoint
instead of drawing an invented docking chord, and explicitly marks coverage as
partial. This model assumes per-tool E state; files relying on printer-side tool
macro state still need that macro context. Bed dimensions are not inferred.

Homing paths, START_PRINT/END_PRINT/STOP_PRINT macro-generated moves and bare G10/G11
firmware retract/recover motion are explicitly omitted and flagged. Other unknown
motion/macros stop at the retained prefix. Known geometry-neutral bookkeeping
commands, including print statistics and object markers, do not stop the preview;
there is no generic SET_* or macro-prefix skip. Long known bookkeeping payloads
(such as object polygons) are skipped without an unbounded line allocation.

Explicit offsets support XYZ/E and *_ADJUST, with optional MOVE=1 travel.
Offset command values are millimetres. Saved states support four names of up to
31 unquoted alphanumeric/underscore/dot/hyphen characters, coordinate/E modes,
base/home offsets, logical E and optional XYZ return travel. Missing states,
tool mismatches, duplicate/invalid numeric parameters and slot limits stop with
a reason. Plane, center and unit modes are not changed by Klipper state restore.

Extrusion at a new Z creates a retained level in print order, not a slicer layer
number. Helices, vertical-plane arcs and other non-planar paths may therefore
create many levels. Single/Up to refers to these retained levels. Binary BGCODE,
numbered/checksummed streams, Bézier splines, workspace selectors, macro expansion,
panning and physical tool offset/kinematics simulation remain outside this scope.

A load initially attempts to retain 262,144 main independent paths (eight times the original
capacity), falling back through 131,072 and 65,536 to 32,768 if PSRAM allocations
fail. It also keeps anchors for 2,048
extrusion Z levels, 16 tools and the final path. Generated segments are numbered;
when the main buffer fills, its sampling stride doubles and stable compaction
keeps uniformly spaced independent samples across input. Later layers remain
reachable through anchors. Discarded paths are not joined: gaps may appear,
especially in an isolated layer. Bounds and level/tool metadata include all
generated input, even discarded samples. This is labelled REDUCED DETAIL, not
silently presented as full-detail geometry. When SD caching succeeds, the fallback RAM sample allocation compacts to
32,768 after parsing; full generated paths remain in the SD cache. If caching
is unavailable, the larger path allocation remains available.

It reads at most 64 MiB, with a 511-byte command line limit; semicolon comments
and long thumbnail payloads are skipped without buffering them. Values and
accumulated coordinates are bounded. The Z-level/state/path-count caps, unsupported or malformed
input and the 60-second load budget produce an explicit **PARTIAL PREVIEW**
when a usable prefix exists. If no supported paths exist, an inline error
replaces the loading status. The source text is not cached. Generated geometry can be cached on SD as described below.

The bounded path allocation ranges from about 1.34 MiB to 10.1 MiB; the
952x332 RGB565 canvas is about 617 KiB. Solid adds a lazily allocated 1.2 MiB
float depth buffer plus a 617 KiB offscreen RGB565 frame. If those allocations fail, Lines remains available and the
Solid toggle can be retried. The panel builder adds about 1.25 MiB; a Single layer uses a separate 1.34–10.1 MiB
path allocation sized for its record count. These allocations require PSRAM and are freed on dismissal. Only one
worker may load at once. Closing and immediately reopening can briefly report
Loader busy until the previous worker has retired; Reload retries afterward.
Rendering is requested by interaction, coalesced through a 33 ms idle / 5 ms active-render LVGL timer,
and uses the existing canvas buffer without per-frame allocation. Raster work
still runs on the UI thread; solid work is split across ticks. Target performance is unverified.

## Solid extrusion view

Solid draws rounded, shaded extrusion beads with a per-pixel depth test.
It builds in batches of at most 2,048 candidate paths per UI tick; controls can
interrupt/restart it, and the completed frame restores coverage diagnostics. Front
paths occlude back paths regardless of input order. Tool colors and Single/Up to,
All, travel visibility, orbit, presets and pinch share the existing model.
Travel remains thin. Solid stays shaded while dragging and pinching. Camera snapshots
render into the offscreen frame, then publish atomically; newer positions coalesce
into the next frame. Close frees the depth and staging buffers.

The nominal bead width is 0.45 mm, not inferred from filament diameter, E volume,
nozzle settings or slicer comments. It is a round visual approximation, not a
measured layer cross-section. Radius is clamped to 0.65–8 screen pixels for
visibility and bounded raster work; this exaggerates subpixel beads and caps
very close views. Gaps and infill remain visible, and no caps/walls are invented
across missing or sampled paths in the path fallback. This does not recover the original CAD mesh or
produce a watertight export. Reduced-detail warnings remain in Solid mode.
The path fallback uses round beads; merged surfaces use rectangular ribbons of
nominal width 0.45 mm and initial height 0.20 mm. The offline sample now tapers inward by 0.10 mm per level so neighboring beads
form a more continuous shell; it intentionally has no solid top/bottom skin.

## Transport and ownership

The background worker streams a GET from the selected Moonraker endpoint's
`/server/files/gcodes/` root, using the existing endpoint/TLS policy and a copied
CA/API key snapshot. Redirects are disabled. Traversal is rejected and filenames
are percent encoded. Shared network ownership is released on every exit and
exclusive operations cancel the load. HTTP failures and incomplete responses
never publish a successful model.

Request and active-printer generations reject stale results. Workers do not
use LVGL until they hold the display lock. Model ownership transfers only when
the visible viewer adopts a current result. Navigation, close, external modal
deletion and printer changes cancel the request and retire buffers/timers.
There is no printer-command callback in the viewer or loader.

## Validation and next hardware check

Host checks:

```bash
python3 tools/audit/gcode_toolpath_test.py
python3 tools/audit/gcode_viewer_loader_test.py
python3 tools/audit/gcode_viewer_ui_test.py --lvgl-dir /path/to/lvgl
bash tools/audit/version_audit.sh
```

Core/loader checks use ASan and UBSan. LeakSanitizer is disabled because this
host's process sandbox blocks its process inspection; loader allocation/free
balance is separately asserted. Cases include arbitrary chunk boundaries,
compact axis words, mode/offset semantics, malformed byte streams, hard limits,
render-buffer guards, exact URL/TLS/auth snapshots, single flight, cancellation,
profile changes, truncated HTTP, error cleanup and network exclusivity.
Native LVGL pointer checks cover five themes, sample, orbit/zoom/reset,
layer/travel controls, disabled empty actions, 100 external deletions and profile
retirement. Viewer, Tools and Files compile with warnings treated as errors.
The existing Files loading, Tools action/live-refresh and firmware identity
checks are included in this patch's regression pass.

Optional host-only raster benchmark:

```bash
cc -std=c11 -O2 -I main main/gcode_toolpath.c main/gcode_surface.c tools/gcode-viewer/render.c -lm -o /tmp/gcode-render
/tmp/gcode-render tools/gcode-viewer/sample.gcode /tmp/toolpath.ppm
/tmp/gcode-render tools/gcode-viewer/sample.gcode /tmp/solid.ppm --solid
/tmp/gcode-render tools/gcode-viewer/sample.gcode /tmp/surface.ppm --surface
```

This benchmark reports host CPU time, not ESP32-P4 performance. The current sample has
443 retained segments, 60 extrusion Z levels, three arc commands and T0/T1 paths. The user confirmed the v7.1.1-dev firmware built with the verified wrapper
and opened on the tablet. Actual-file coverage and target performance remain
pending. Build every firmware patch with `./tools/build_idf6_hosted3.sh`; use
`build-idf6-hosted3/PrinterHMI.bin` for OTA. On the tablet, test Sample first, then a small linear
file, rapid Close/Reload, printer switching during a load, and a large file's
partial-preview label. Serial `gcode_view` lines report raster microseconds on
the first and every 30th requested model frame. Check scrolling/navigation,
network responsiveness, PSRAM reclamation and E-stop visibility before deciding
the next geometry budget. Do not infer target frame rate from the host tests.

## v7.1.2-dev follow-up

Horizontal orbit is reversed following tablet feedback. Native LVGL pointer
checks assert that rightward drag decreases yaw and leftward drag increases it
in all five themes. Vertical orbit is unchanged. Firmware work uses
`./tools/build_idf6_hosted3.sh`, not a bare `idf.py build`.

This batch also adds Top, Front and 3D orientation presets. Presets fit zoom to
1x while retaining the selected layer and Travel setting; Reset continues to
restore the whole default view. Related viewer changes are grouped into one
patch/build/flash cycle.

## v7.1.3-dev layer inspection batch

The slider jumps directly to a layer and displays its index/Z height. Single
shows that layer; Up to shows the retained prefix through it. All restores the
whole model; Reset additionally restores Single mode and the default view.
Top/Front/3D preserve the chosen layer, cumulative mode and Travel setting.
Layer - from All selects the final layer; Layer + from All selects the first.
Impossible boundary steps disable. Single-layer models retain useful mode and
step controls while their slider disables; travel-only models disable all layer
controls. The slider/captions synchronize after presets, fine steps and reloads.

Travel between extrusion layers now belongs to the layer it approaches, so
Up to excludes the next layer's transition travel. Layer numbers remain retained
extrusion Z levels, not slicer metadata numbers. Existing parser/resource limits
and partial-preview notices remain in force. The user accepted the previous
view-control batch; this inspection batch needs the next tablet pass.

Pinch zoom uses the same scaled two-point touch path as Bed Mesh. Its 60% scale
sensitivity, 45% smoothing and lift-all-fingers guard follow the Bed Mesh
reference, with the viewer's existing 0.25x to 4x zoom limits. Both fingers must
be over the visible canvas; controls, an overlay, tiny spans and printer changes
cannot start a zoom. Pinching blocks orbit until all fingers lift. Redraws stay
coalesced through the existing 33 ms timer. This patch adds a second optional
observer to the local LVGL9 touch port without changing the Bed Mesh observer,
LVGL's first pointer, or gesture-recognizer settings. Two-finger pan is not added.

Host validation exercises the actual touch-reader body with fake GT911 data and
checks scaled 0/1/2/5-point delivery to both observers, either observer absent,
and unchanged pointer state. Native viewer tests cover spread/contract, zoom
limits, pinch/orbit exclusion, the one-finger remainder, re-entry after lift,
controls/overlay rejection and close/profile retirement. Test actual GT911 feel
on the tablet in the same build/flash pass as the layer controls.

## v7.1.4-dev motion and coverage batch

The motion support above, two-tool arc Sample, and a scrollable Details popup
are delivered together. Details shows consumed bytes, physical line counts,
retained paths/levels/tools, arc/tool-change counts, omitted motion and the exact
stop command/reason. Diagnostics store command names rather than parameter values.
CR, LF and split CRLF boundaries preserve line numbers, including comments and
blank lines. Empty results remain inspectable through Details with geometry
controls disabled. Started reloads clear old geometry; a failed selected-file
request cannot leave the sample displayed under that file's name.

Host checks include arbitrary chunk boundaries; CW/CCW, full, helical, three-plane,
IJK/R, relative and absolute-center arcs; invalid/radius/budget cases; tool E state;
units, offsets and state return moves; exact command skips and lookalike rejection;
long object metadata; diagnostic privacy/line numbers; guided malformed motion;
loader timeout/allocation errors; native Details scrolling/pinned Close; empty
results and reload identity. The layer/pinch/preset lifecycle checks remain active.

Reference semantics were checked against the official
[Klipper command reference](https://www.klipper3d.org/G-Codes.html),
[Klipper arc implementation](https://github.com/Klipper3d/klipper/blob/master/klippy/extras/gcode_arcs.py),
[Klipper coordinate-state implementation](https://github.com/Klipper3d/klipper/blob/master/klippy/extras/gcode_move.py),
and [Marlin arc forms](https://marlinfw.org/docs/gcode/G002-G003.html).
The viewer's bounded approximation is not copied printer execution logic.

Tablet pass: open Sample, choose All and inspect the orange T1 rings, use
Top/Front/3D and pinch, then open/scroll/close Details. Load a small real file with
arcs/tools, check its coverage reason, and verify a failed Reload leaves an inline
error rather than a mislabelled old model. Build with
`./tools/build_idf6_hosted3.sh`; OTA `build-idf6-hosted3/PrinterHMI.bin`.

## v7.1.5-dev large-file retention

The user reported `Line 17942 G3: Segment limit at arc` after 32,759 retained
paths and 11 layers. Removing that early stop does not mean unlimited input:
geometry now reduces display detail progressively, while unsupported input,
Z-level/state caps and the larger read/time budgets still report true partial
coverage. An arc that exceeds the remaining main-buffer space can finish and
subsequent layers continue. Very large arcs coarsen within their per-arc budget.

REDUCED DETAIL and Details expose sampling stride and retained/generated counts.
Layer/tool/final anchors prevent sparse late features from vanishing completely;
they do not reconstruct discarded paths. For exact layer inspection of a very
large file, a future on-demand detail pass is still needed. Current geometry
requires approximately 1,123,160 bytes on the host (target struct padding may
differ), plus the unchanged 952x332 RGB565 canvas.

Loading progress shows consumed KiB, generated paths and extrusion levels,
updated at most four times per second through a request-fenced snapshot. The
worker never updates LVGL labels. Close/profile/OTA cancellation remains active.

Regression tests fill the original 32,759 paths, parse the next G3, then retain
metadata/anchors through 901 layers. Smaller budgets force repeated compaction,
late sparse tool/level coverage, full bounds, final-path preservation, travel
ownership after skipped extrusion and nonempty selected-layer rasters. Native
checks verify reduced-detail notices, level selection, Details, progress updates
and existing modal/input cleanup. Re-test the original file on the tablet; target
load/render performance has not been measured here.

## v7.1.7-dev detail and solid batch

Expand the primary path buffer with bounded allocation fallbacks and add Solid/Lines.
Host regression checks 200,000 paths retained at stride 1, both smaller-memory
fallbacks, depth-order independence, shading/thickness, layer/travel filtering,
clipping and pixel/depth guards. Five-theme native pointer checks cover failed
Solid allocation/retry, toggle, orbit/pinch and cleanup. Target memory pressure
and full-file rendering speed still need verification on the tablet.

## v7.1.8-dev compact surfaces and SD layer detail

The goal is detail approaching the high-resolution previews while keeping all
processing on the tablet. This batch establishes the first pipeline; it does not
claim a CAD-quality reconstruction for arbitrary files.

Every generated segment is offered to the surface builder and SD cache before
RAM sampling. The builder merges contiguous, same-tool, same-level collinear
extrusions (0.0001 mm line tolerance), then joins matching XY runs on consecutive
levels into wall panels (0.001 mm endpoint quantization, positive Z steps up to
0.5 mm). It does not match arbitrary curved/tapered contours across levels or
extract an exterior shell from infill. Each panel is a rectangular ribbon with
0.45 mm nominal width and a 0.20 mm initial base thickness; intermediate adjacent
levels extend it upward. No top skin or infill is fabricated. Curves keep their
existing tessellation. Surface storage caps at 32,768 panels; non-planar input or
capacity exhaustion causes the entire overview to use the path fallback,
rather than silently showing an incomplete surface.

Solid/All and Solid/Up to use eligible surfaces with depth-tested, shaded
triangles, in 128-panel UI batches. Travel on uses the path view so travel remains
available. Lines remains the path inspection mode. Details reports panels,
merged runs, surface eligibility and SD status. Sampling warnings refer to the
active representation; a complete merged overview is not labelled reduced
merely because its fallback path samples were compacted. Other coverage reasons
(omitted macros, unsupported input, coarse arcs) remain visible.

Mounted SD storage enables a session cache under
`/sdcard/PrinterHMI/gcode-preview/`. It stores geometry records, not source text,
authentication or reusable printer metadata. It retains every generated path
within the existing parse limits and a 128 MiB cache limit. The per-layer index
assigns transition travel to the approached level, including moves preceding its
first extrusion. Each 36-byte record has a checksum, expected sequence, bounded
coordinates, and validated tool/flags. File headers are versioned and checked.
Write failures/card removal/cache exhaustion disable the cache and preserve the
path view. Temporary files are removed when the last owning preview/worker
retires; the first successful cache opening after startup removes only known
cache-file names left by interrupted sessions. Other SD files are untouched.
The cache is rebuilt on Reload, not reused across preview sessions.

In Single mode, moving to a level waits 180 ms for scrubbing to settle, then
reads its cache range in a background task without HTTP. Until it arrives the
RAM fallback remains visible; `Reading SD layer` and the final SD path count/
stride distinguish the two. Layer detail uses up to 262,144 paths with the same
bounded allocation fallback/sampling policy. A layer exceeding its capacity
still reports its actual stride, and the existing coarse-arc/parse limits apply.
This is detailed retained toolpath geometry, not unlimited exact source geometry.
Changing level/mode, Close and printer switching retire stale requests. Worker
references preserve the file until an in-flight read has closed it. Missing or
corrupt cache data leaves the fallback visible and suggests Reload.

Host checks cover a deliberately repetitive 400,000-extrusion/1,000-level model
merged to four panels while all 400 extrusion paths on a late layer remain
readable. This synthetic reduction is not a typical-file compression guarantee.
Additional tests cover holes/Up to clipping, raster/depth guards, incremental
completion, cache checksums, travel ownership, cache capacity/failure cleanup,
real loader cancellation/profile/display/task failure, and five-theme UI detail
scheduling/adoption/cancellation. Actual SD speed, panel compression, visual
fidelity and interactive rendering still need the user's real file on the tablet.

Run `python3 tools/audit/gcode_surface_cache_test.py` in addition to the existing
core, loader and native UI checks. Build with `./tools/build_idf6_hosted3.sh`.

## Parked v7.1.9-dev standalone model mesh experiment

This section records the previous experiment. Its Mesh entry and separate loader/UI
are excluded from v7.1.10-dev firmware. Use ordinary Files > 3D view instead.

### Previous workflow

The Mesh header action opens a separate SD solid-model viewer. It uses prepared
STL-derived geometry rather than reconstructing surfaces from printer commands.
The desktop converter, PHM format, demo, limits and tablet workflow are documented
in [tools/mesh-preview/README.md](../tools/mesh-preview/README.md). No PrusaSlicer
change, custom build or Pi service is required.

Mesh is independent of the selected G-code. It does not infer supports, infill,
placement or live print correspondence. The G-code viewer remains the path/layer
inspector. Its selected-layer background work is cancelled and its timer pauses
while Mesh is open; normal rendering resumes when the model popup closes.
Parent/profile cleanup retires both views. The existing touch observer delegates
to Mesh only while its modal is active; Bed Mesh's hook remains unchanged.


## v7.1.10-dev automatic solid from normal G-code

Choose a normal file in Files and tap **3D view**. No slicer changes, companion
service, conversion or separate model file are required. The initial preview stays
usable while automatic preparation runs. **Solid**, **All**, and hidden **Travel**
select the prepared mesh when ready. Layer inspection, Up to, Lines and Travel keep
the existing path/panel views. Orbit and pinch keep the shaded solid using complete-frame publication. Details reports triangle count,
cell size and whether automatic preparation succeeded.

The worker consumes all supported generated extrusion paths in the parsed prefix,
not just retained RAM samples. A complete session SD cache is read locally. Without
that cache (or if it cannot be read), the worker rereads the normal file through
the existing read-only HTTP endpoint. Endpoint/TLS/key settings are snapshotted;
redirects are disabled. Replayed consumed-byte count, 64-bit FNV-1a fingerprint and
generated path count must match the initial load before adoption. This detects
ordinary changes; it is not a cryptographic integrity guarantee. Unsupported motion,
omitted macros and partial-prefix limits remain visible in the existing diagnostics.

Each extrusion carries width and height. Full-line `;WIDTH:0.45` and `;HEIGHT:0.2`
comments update subsequent paths, including arcs. Only bounded finite values and
trailing whitespace are accepted; inline or oversized comments do not set dimensions.
Without these literal tags, nominal 0.45 mm width and 0.2 mm height are used. This
batch does not infer bead dimensions from E or cover every slicer's comment syntax.
The session cache uses PHGC002 with 44-byte checked records including these dimensions.

The bounded occupancy volume unions swept XY extrusion capsules and their vertical
height, fills sealed internal air, and retains air connected to the exterior, including
through holes. Surface Nets extracts shared vertices and triangles; eight paired
smoothing passes move each vertex by at most 0.75 cell along each axis from its
initial extracted position. Area-weighted normals provide smooth lighting. This
is an approximate deposition envelope, not recovered CAD: small gaps/features can
merge, closed cavities are hidden, and supports remain part of the preview.

Allocation tries grids with longest dimensions near 512, 384, 256, 192 and 128 cells.
Two occupancy bitsets, flood-fill runs and vertex indexing use bounded scratch;
the mesh allows at most 65,535 vertices and 100,000 triangles. If extraction exceeds
its budgets the occupancy coarsens automatically and reports the resulting mm cell
size. Scratch is freed before compacting the published mesh. Preparation has a
120-second worker deadline and a bounded cell-test budget. Failure leaves the original
path preview available. Selecting a level cancels automatic work so layer reads can
proceed; All can restart it. Close, reload, printer changes and exclusive network
operations fence or cancel background work. No printer commands are sent.

Host validation covers full-source geometry despite an eight-path RAM sample, open
holes, raster guards, incremental frame equivalence, cache and network replay,
changed-file rejection, cancellation and real LVGL adoption/lifecycle. Target build,
PSRAM headroom, timing and appearance remain to be measured with actual sliced files.
The high-resolution thumbnail remains the quality reference; this batch does not
claim to match its surface fidelity.

Host-only visual benchmark (CPU timings do not predict ESP32-P4 timings):

```bash
cc -std=c11 -O2 -I main main/gcode_toolpath.c main/gcode_volume.c main/mesh_preview.c tools/gcode-viewer/auto-render.c -lm -o /tmp/auto-render
/tmp/auto-render input.gcode /tmp/automatic.ppm 384
```


## v7.1.11-dev solid frame presentation

The previous UI intentionally switched to Lines/wireframe while touching the canvas
and exposed incremental raster batches after release. A stationary LVGL PRESSING
sample also requested another redraw even with no coordinate change. This batch
keeps the selected Solid renderer throughout gestures, ignores unchanged pointer
positions and saturated pinch zoom, and avoids exposing an incomplete solid frame.

A lazy 617 KiB RGB565 staging buffer accompanies the depth buffer. Both are required
before entering Solid; failure preserves Lines and allows retry. Each solid frame
captures the camera and source model. Incoming camera changes queue the latest view
without restarting the in-flight frame. Up to eight bounded renderer batches run
per timer callback, checking an 8 ms elapsed budget between batches; one batch can
exceed that target. Only a completed frame is copied to the visible canvas. New
models/details, renderer toggles, reload and close cancel stale raster work before
freeing its geometry. Dense models may rotate at a lower frame rate; tablet timing
still needs measurement. This changes presentation, not reconstruction quality.

Status begins with LINES, SOLID MESH, SOLID SURFACES or SOLID PATHS. Preparing mesh
percentage or a preparation failure appears before coverage details. A path fallback
can still resemble thin lines at whole-model scale; SOLID MESH specifically confirms
that the automatic volume result is being displayed. Details includes the preparation
error. The Lines/Solid button continues to name the current selected mode.


## v7.1.12-dev rendering performance

The mesh triangle rasterizer now computes barycentric, depth and light gradients
once per triangle and steps them across each scanline. It no longer divides twice
for each candidate pixel. A 256-entry RGB565 shade table replaces per-pixel color
channel conversion, with slight brightness quantization. Generated Surface Nets
meshes are explicitly outward-wound, so rear-facing triangles can be skipped.
Imported research meshes retain the prior two-sided behavior. The actual vertices,
triangles, cell size, normals and canvas resolution are unchanged.

Unfinished frames now schedule the next timer callback after 5 ms rather than the
33 ms idle interval. The existing eight-batch / approximately 8 ms work budget per
callback remains; a single batch can exceed the target. The timer returns to 33 ms
when idle. Complete-frame publication, camera coalescing, shaded gestures and
cancellation ownership remain in place. Frame logs now measure start-to-publication
time, including the waits between batches, instead of only the final batch.

A host CPU comparison over 60 views of the same 46,368-triangle, 23,184-vertex
G-code ring at 952x332 measured 8.83 ms/frame before and 4.30 ms/frame after.
The final-view silhouette matched exactly; shade-table quantization changes some
colors slightly. These are host timings, not an ESP32-P4 frame-rate prediction.
The active timer change is additional to that CPU-only raster comparison.

To benchmark your own file, the host tool accepts an optional view count:

```bash
/tmp/auto-render input.gcode /tmp/automatic.ppm 384 60
```

Host tests check culling against two-sided renders from eight camera directions,
open-hole preservation, incremental/full-frame equivalence and raster guards.
The real LVGL audit verifies progress after a 6 ms tick, visible mode differences,
stationary-touch stability and atomic frame publication. Tablet timing remains to
be measured; initial automatic volume preparation is unchanged by this batch.


## v7.1.13-dev adaptive shaded interaction

Automatic solid preparation now also attempts a separate gesture mesh. Bounded
vertex clustering uses successively coarser grids, removes collapsed/duplicate
triangles, averages positions/normals and caps the result at 4,096 vertices and
4,096 triangles. Its camera bounds are copied from the full mesh, so the camera
framing stays consistent. Clustering can change local topology and winding; the
small gesture mesh renders two-sided. The original mesh is never simplified or
modified. Meshes already below 4,096 triangles reuse the original geometry.

During orbit or pinch, the UI selects that child and renders at 476x166 instead
of 952x332, then scales the finished shaded frame by 2x into the visible canvas.
This uses a quarter of the raster pixels and reuses the existing staging/depth
buffers; it allocates nothing during a gesture. A new gesture can cancel a stale
full-resolution frame to begin the lighter frame promptly. Other camera updates
coalesce as before. Complete-frame publication remains in place.

When all fingers lift, the timer automatically requests the full mesh at full
canvas resolution, even if there was no final movement event. A one-finger tail
after pinch remains guarded until every finger lifts. Lines, layer inspection,
Travel and surfaces/path fallbacks retain their existing renderers. Small features
may soften or briefly merge in the gesture view; release restores the detailed
result. Details reports the gesture triangle count.

The optional child retains about 185 KiB at its fixed maximum capacity and uses
up to 264 KiB of temporary clustering scratch. It is generated on the existing
preparation worker with cancellation checks, then adopted with the root mesh.
Closing/reloading/replacing the root releases its child as well. If allocation,
clustering or its remaining preparation budget fails, the full mesh remains usable
and gestures still use the lower pixel resolution. No slicer or file setup changes
are required.

A host 60-view run of the same ring measured 46,368 full-detail triangles at
2.98 ms/frame versus a 2,258-triangle gesture mesh at half resolution at
0.51 ms/frame; optional clustering took 1.3 ms. These CPU raster timings exclude
UI upscaling, LVGL/LCD transfer and ESP32-P4 scheduling. Target speedup needs
measurement. Full-detail generation and rendering after release are unchanged.


## v7.1.14-dev display work, Up to and input response

Normal Solid Up to now keeps the automatic shaded mesh rather than returning to
thin paths. It clips triangles at the selected retained Z height, and a bounded
scanline parity mask shades the cut plane on the closed full mesh. Through holes
remain open. A section is a height cut through the approximate extrusion envelope,
not an exact replay of cumulative moves for non-planar/non-monotonic layer order.
Single and Travel keep their path-based inspection. Preparation can continue in
Up to while the full mesh is still being built; Single still prioritizes layer work.

Cut intersections use a consistent endpoint order so shared triangle edges meet at
the same coordinates. The cap mask costs about 39 KiB of optional PSRAM at full
canvas size and is reused at gesture resolution. Cap rows are drawn incrementally
with depth testing. Capping is restricted to generated closed meshes; the clustered
gesture mesh may be topologically altered, so its cut is two-sided and uncapped.
Release restores the capped, full-detail section. A cap allocation failure preserves
an open shaded cut and reports that fallback. No mesh reconstruction is repeated
when selecting a height.

Every changed one-pixel orbit sample now wakes the renderer immediately instead of
waiting for the idle timer. Horizontal sensitivity is 0.012 radians/pixel, matching
the Bed Mesh reference. The viewer adds no movement-distance threshold; physical
touch controller/polling and display latency are still outside that guarantee.
Stationary samples remain ignored. Pinch and release also wake pending work.

The visible canvas still has the same size, but subsequent mesh frames invalidate
only the union of their old/new projected bounds, including gesture expansion
padding. Mode transitions conservatively invalidate the whole canvas. This avoids
repainting the wide unchanged background. Mesh frames also stop rewriting status
and layer widgets every time they finish. Driver, LCD timing and PPA settings are
unchanged. A five-theme native fixture measured 143,045 damaged canvas pixels and
155,973 pixels flushed for a held-finger redraw, versus 316,064 full-canvas pixels.
This checks display work, not target FPS; the savings depend on the model and zoom.

Details now snapshots frame-ready latency, accumulated raster work, publish cost,
last LVGL display cycle, flush-callback time and canvas damage. A display observer
is attached only while the viewer exists and removed on close. Display-cycle timing
can include synchronous flush waits; these measurements are not physical panel FPS
or an isolated end-to-end touch-latency measurement. Use them to distinguish CPU
raster costs from presentation costs if the tablet remains slow.

The same batch corrects Bed Mesh read-only availability; see MOTION_CALIBRATION.md.


## v7.1.15-dev orbit without automatic zoom

The mesh camera previously refit its scale to the rotated bounding box for every
frame. A one-finger drag did not change the zoom value, but the angle-dependent fit
made the model expand or shrink as it rotated. The camera now uses a fixed bounding
sphere about the mesh's bounds center, computed once from its vertices during
preparation. The gesture mesh inherits the same radius and bounds; drawing at half
resolution keeps the same normalized scale. If a mesh has no cached radius, a
rotation-independent bounding-box diagonal provides the fallback.

Orbit changes yaw/pitch only. Pinch and +/- still change the user zoom; Reset and
camera presets retain their explicit reset behavior. Initial fit uses the stable
sphere rather than the old angle-specific rectangle fit, so its framing can differ
slightly from the preceding patch. Radius preparation adds no per-frame vertex scan
or gesture allocation. Section caps and projected invalidation bounds use the same
camera transform. Native tests keep zoom unchanged over one-pixel orbit, still allow
pinch, and verify angle-independent projection scale across 80 camera/zoom settings.


## v7.1.16-dev solid modes and visible active-layer infill

With a prepared mesh, selecting Single or enabling Travel no longer sends the
whole view back to the thin path fallback. All retains the exterior solid. Up to
clips that exterior at the selected retained Z height; Single keeps only an
exterior height interval. Both expose the selected layer's actual extruding paths
as shaded material strands. This includes infill, walls, supports and other positive
extrusion; no slicer-role classification or invented interior pattern is applied.
The earlier artificial filled cut cap is removed from the product view because it
concealed the infill and its gaps. Portable section/cap APIs remain for audits.

Single's lower height uses the previous positive level gap when reasonable, or the
reported maximum/nominal layer height for its first/ambiguous level. These exterior
cuts remain approximate height sections, not exact replay of non-planar build order.
The displayed active-layer paths come from actual supported generated motion. The
view does not reconstruct every earlier internal infill layer below that selection.
An individual layer viewed edge-on can naturally look thin; its material is shaded,
not a wireframe. Tool colors are retained for the active strands.

The SD detail reader now serves Solid Up to as well as Single. A matching cached
layer replaces sampled active-layer paths after a short debounce without changing
the exterior mesh or camera. Until it arrives, or without a usable SD cache, the
retained RAM paths and layer anchor are used. Reduced coverage remains labelled;
there is no promise of complete infill when the RAM sample is reduced. Mesh
preparation can continue in Solid Single, so selecting a layer before preparation
ends no longer prevents the full solid from becoming available. Lines Single still
prioritizes its existing detail read.

Travel overlays retained/cached non-extruding paths in gray, with depth testing,
layer filtering and clipping in the same mesh camera. It does not replace the mesh
or zoom out to include distant park moves. Travel coverage is labelled separately.
All-travel overlays use retained whole-file paths; Single can use exact cached-layer
paths. Root geometry, active strands and travels are drawn in separate bounded
phases into the existing staging/depth buffers, then published together. No frame
or stencil allocation is added; the unused 39 KiB product cut-cap scratch is removed.

All-view gestures keep the lighter mesh. Height-section gestures use the full
exterior at half pixel resolution so the section boundary remains consistent,
then restore full resolution after release. Dirty bounds include active extrusion
strands even where they extend beyond the approximate exterior. Travel frames use
conservative full-canvas invalidation so toggling them cannot leave stale paths.
Default All/Travel-off retains its reduced-damage fast path.

Host tests cover the All/Single/Up to x Travel off/on x Top/Front/3D mode matrix in
five themes, actual cached infill adoption in Solid Up to, shaded strands and empty
gaps, travel depth/layer filtering, slab clipping and incremental frame equivalence.
Target appearance and responsiveness still require checking with a real sliced file.


## v7.1.17-dev extrusion faces and inspection zoom

Solid selected-layer material now uses closed octagonal extrusions rather than
screen-space capsule strokes. Each path has two elliptical eight-vertex rings:
width spans the XY perpendicular, height spans Z, with its top at the path Z.
Shaded side triangles and flat filled ends share the exterior's depth buffer and
camera. Tool colors and real gaps are retained. Nominal width/height are used when
source dimensions are unavailable. Flat caps, overlaps and joins are approximate;
this is not a physical deposition simulation. No persistent layer mesh or per-frame
heap allocation is added. Batches contain at most 64 extrusions. More triangle work
may affect selected-layer speed; tablet measurements are still required. The All
exterior fast path and cached/reduced source coverage remain unchanged.

Maximum zoom is now 16x. Thin physical strands can still resemble lines at whole
object scale; tilt and zoom to inspect their side faces and height. Pinch anchors
zoom under the two-finger centroid using normalized camera offsets shared by mesh,
path, surface, section caps and damage bounds. Plus/minus anchor at canvas center;
camera presets and Reset clear offsets and restore framing. One-finger orbit does
not change zoom. Screen line clipping bounds work when highly zoomed or offset.


## v7.1.18-dev exposed interior readability

The selected-layer material beyond the exterior shell now shares the body's gold
palette in Solid Up to/Single. Explicit Lines retains flat tool colors. Geometry
under the pixel grid previously aliased to the same hairline appearance as Lines,
even with actual extrusion faces. The solid interior now has a minimum displayed
cross-section of 3 pixels wide by 2 pixels high. Source path tops stay at their Z;
only unresolved display dimensions are enlarged, so narrow gaps may close visually.
Close zoom uses physical dimensions once each exceeds the screen threshold.

The UI snapshots its staging/display ratio for the interior raster; half-resolution
gestures halve the pixel threshold before frame expansion. Source geometry/cache,
exterior surface, source coverage, All fast path and explicit Lines are unchanged.
This is a display approximation for interior readability, not exact deposition or
all previous layers of internal infill. Details explains the display enlargement.

Already-loaded matching SD layer detail is retained when comparing with Lines,
so returning to Solid restores the same full interior without another detail read.
Changing the layer, file or printer still retires it through existing ownership.


## v7.1.19-dev connected-looking top skin

Selected-layer material uses flattened bevelled beads instead of elliptical tubes.
The broad top spans 95% of width; the short bevels and vertical sides use planar
face lighting so a dense skin does not inherit a highlight/valley for every path.
Source top Z stays fixed. Source/nominal width and height, the minimum display size,
shared depth camera, gold palette and 28 candidate triangles per segment remain.

Path endpoints overlap slightly in XY, bounded by the minimum of one quarter of
segment XY length, one quarter of display width, and half of display height.
Endpoints keep their Z, including sloped motion; normals use the extended tangent.
Vertical-only motion receives no XY extension. This is a local display approximation;
it does not bridge wide infill bays or fill a whole section. Very narrow gaps can
still be obscured by the existing screen-thickness aid. Sparse source coverage and
coarse exterior-shell errors retain their existing limits. No exterior mesh, source
cache or All fast-path change is made.


## v7.1.20-dev top-layer to shell contact

The reported clip reproduced a separate issue from rounded strand seams: at Z8.40,
exact active paths left a narrow open ledge inside the smoothed 0.19935 mm shell.
Solid Up to and Single now add a narrow inside band at their upper cut. It is
computed from an even/odd section mask minus its horizontal/vertical erosion,
using projected extents based on 1.5 shell cells. The final band intersects the
original mask, preserving its holes and outside boundary. Interior pixels away
from that boundary remain open for the actual selected-layer material.

The optional 39,840-byte PSRAM mask at the 952 by 332 canvas is cleared at cursor
zero and retained across bounded rendering calls. Half-resolution gesture frames
reuse it at their raster dimensions. Allocation failure leaves the old open cut;
close and nested deletion free it with the other solid buffers. There is no filled
interior cap or new All-view raster phase. The band is a visual join to approximate
shell geometry, not an additional printed path. Section rendering adds bounded
mask scanline work; hardware performance still needs checking.
