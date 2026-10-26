# G-code viewer bug and security review — v7.1.23-dev

Reviewed this experimental branch's G-code parsing, HTTP/session ownership, source
refinement, SD/PHM decoding, raster bounds, modal/gesture lifecycle, Bed Mesh live
view guards and Dashboard filename changes. Live print following is still deferred.

## Fixed findings

- Capture the printer generation before constructing the URL/certificate/key
  snapshot and reject a changed generation afterwards. Apply the same post-capture
  fence to refinement, so capture cannot silently adopt a different printer owner.
- Use explicit volatile stores to erase per-job API-key copies before releasing
  storage. Cover all endpoint/worker/task-start exits, including the refinement
  task-start failure which previously released its job without erasing its key.
- During performance development, the new owned-RAM path's task-start failure test
  exposed missing source-copy cleanup. It is fixed before release and now covered
  along with cancellation and retirement. This copy path was not in v7.1.22-dev.

## Checked behavior

- Read-only GETs use the existing transport policy, captured key/certificate, no
  redirects, profile/request fences, single-flight workers and network exclusivity.
- Source replay still verifies consumed bytes, fingerprint and generated path count
  when no complete owned RAM source or usable SD cache is available.
- Cache/mesh readers enforce dimensions/counts, checksums, finite coordinates,
  record/triangle bounds and safe names before rasterization. Parsing remains
  bounded by source, line, motion, arc, layer, allocation and time limits.
- RAM-source refinement owns its copy, caps it at 32,768 paths, and uses the existing
  fallback on copy OOM. UI close, request cancellation, profile changes, network
  exclusivity, task failure and rejected adoption retire owned resources.
- Cancellation is checked at each probe and after timed yields. The contact-band
  mask stays inside the section, preserves holes, and has an optional-memory fallback.
- One-finger orbit, pinch, complete-frame publication, cached layer adoption, modal
  cleanup and read-only Bed Mesh viewing during prints retain their existing guards.

## Validation

Host AddressSanitizer/UndefinedBehaviorSanitizer parser, motion/retention, cache,
volume, raster and loader audits; 5,000 malformed input streams; 400,000 extrusion /
1,000 indexed-layer cache case; actual supplied clip with geometry/pixel hash
comparisons; explicit source-copy OOM, task failure and retirement cases; API-key
marker checks at deallocation; profile change during endpoint capture; five-theme
native viewer/modal tests; touch forwarding; standalone mesh worker audits.
GCC `-fanalyzer -Wall -Wextra -Werror` reports no findings in parser, cache and
volume source. Existing public-tree and Bed Mesh guards are checked separately.

This review found no further confirmed issue in the paths inspected. It is a
focused source and host-test pass, not a hardware or network penetration test.
Actual ESP-IDF build, tablet timing/visual checks, watchdog scheduling under real
traffic and SD-removal behavior remain target validation items. Host sleep/timing
benchmarks do not establish panel frame rate or real network throughput.


## v7.1.24-dev follow-up

The v23 upfront triangle viewport checks were rolled back after slower movement
was reported on the tablet. This restores the preceding renderer without reverting
the reviewed ownership, profile/key or preparation changes. Host timing gains were
not sufficient evidence of target performance. Tablet movement verification remains
pending; compare after background mesh preparation completes.


## v7.1.25-dev rendering-route review

The mode matrix now exercises movement and release rather than only settled mode
selection. It found mesh-interaction bookkeeping could apply while Lines was active;
that flag now requires actual Solid rendering. The new Single material-only fast
path therefore cannot write missing Solid buffers. Tests explicitly remove depth
and staging buffers with a prepared mesh present, simulate failed allocation, and
exercise Lines gestures before re-enabling Solid.

Reference-pixel checks cover Solid/Lines, all three layer scopes, Travel off/on,
all camera presets, drag/pinch and full-detail release in five themes. Additional
cases cover no gesture mesh, no section mask, merged surfaces and no-mesh paths.
Target performance and transient coarse-cut appearance remain device checks.
