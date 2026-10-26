# Preview rendering

Dashboard, Printer and Files previews preserve the complete source image. The
shared RGB565 renderer packs the complete original into an aspect-aware canvas.
Dashboard/Printer use a 286×215 capacity; Files row icons use 96×96. Neither adds
synthetic letterbox pixels or
cuts off model edges. A 900×520 source produces 286×165 pixels; a portrait
520×900 source produces 124×215. Native LVGL `CONTAIN` fits that canvas into the page's
preview frame with no extra inset. Different image/frame proportions leave
unused space instead of cropping or stretching. This supersedes the earlier
center-crop/`COVER` policy after Operator previews were reported cut off.

The fullscreen high-resolution path still displays the complete original
thumbnail with aspect-fit rendering. Its initial cached fallback now preserves
the full model too. Existing black pixels in slicer thumbnails remain part of
the image. Cache bounds, loading behavior and tap-to-close are unchanged.

## Cache ownership and bounds

Profile previews hold at most four RGB565 buffers bounded by 286×215 pixels. Active publications
reuse an equal-sized buffer; PNG publications keep a staging buffer until decode
succeeds so a failed decode cannot damage the previous preview. All profile cache
access is under the display lock. LVGL image resources are dropped before a
source descriptor or its backing pixels change. Invalidation retains storage
while old widgets may still reference it; reset requires consumers to be detached
and keeps revision numbers advancing.

Files keeps at most 64 RGB565 row buffers bounded by 96×96 pixels, matching the
64-file browser bound. Its queue also covers all 64 entries. The worker renders into one
PSRAM staging buffer, then takes the display lock before the slot mutex and
transfers ownership into the current slot. There is no second permanent copy.
Stale generations are discarded; old storage is dropped/freed only under the
same display lock. Slot buffers remain bounded across page generations rather
than being treated as a leak when retained after navigation.

The two pools together retain at most 1,671,568 bytes of RGB565 pixel data. This
excludes descriptor metadata, active page canvases, staging, compressed PNGs,
transient decoder work and the fullscreen buffers. Pixel buffers in these pools
use PSRAM only; allocation failure preserves the previous valid profile preview
or leaves a Files placeholder instead of consuming large internal-RAM blocks.

Fullscreen owns a small, aspect-preserving fallback snapshot. Changing or deleting
its source page/cache cannot change that snapshot. Once the original 900×520
fullscreen canvas is installed, the fallback is released. Closing the overlay
invalidates image resources and releases both owned buffers. Loading/retry,
native sizing and tap-to-close behavior remain in place.

The shared decoder already uses `no_cache` for temporary image descriptors and
closes its decode session on success/failure; this behavior is retained. SD cache
retention policy and other download-buffer lifetimes remain separate follow-ups.

## Verification

Run `python3 tools/audit/preview_cache_ownership_test.py` for profile cache bounds,
PSRAM failure, Files publication/ownership and fullscreen lifetime checks.
Run `python3 tools/audit/preview_fill_test.py` with managed LVGL sources, or supply
`--lvgl-dir /path/to/lvgl`. Tests build real LVGL and check wide/portrait/square
sources, fit sampling, buffer guards, extreme aspect ratios, fullscreen fit and
preview-well containment. The retained fill utility has separate crop checks.

On hardware, check Dashboard and Printer for the same print, including reconnect,
profile switching and reopening each page. Confirm that previews retain the complete model
without distortion and that tapping opens the complete high-resolution
thumbnail. Check the Files preview and its popup, normal/larger text themes,
loading placeholders, unavailable previews and tap-to-close behavior.

## SD cache retention and transfer buffers

File-preview PNG directories (`/sdcard/hmi/thumbs` and the legacy `thumbs32`)
retain at most 64 PNG files and 16 MiB each during normal writes. Cleanup evicts
older modification times, keeps the newly written PNG, and resolves equal times
by filename. This is write-age retention, not an access-based LRU. Only immediate
regular `.png` files participate; profile startup previews, metadata, nested
directories and user files are excluded. An existing large backlog drains by up
to 64 deletions per successful write. Cleanup is best effort on SD errors and
runs during cache writes, never as a new boot scan.

`thumbnail_cache_io` owns the portable write/retention policy. The manager
serializes cache reads and writes with a lazily initialized mutex. Writes stage
to a sibling `.tmp`, close it and rename it into place; failed write/close/rename
removes staging and preserves the preceding cache when replacement rollback
succeeds. For filesystems rejecting overwrite with EEXIST, replacement uses a
sibling `.bak` and restores it if installation fails. Filesystem replacement
semantics and power-loss durability still depend on the SD filesystem. Individual
PNG writes remain capped at 512 KiB. Cache failure remains optional: downloaded
previews can display without SD storage.

Moonraker downloads retain the existing 512 KiB receive capacity, timeout and
network ownership. Successful transfers attempt a PSRAM realloc to the actual
PNG size before handing ownership to the caller. Allocation failure preserves
the complete original buffer; existing decode/publication/release paths remain
responsible for freeing it. Small previews and 900×520 fullscreen sizing are
unchanged. Version remains 6.5.6 on the LVGL modernization branch.

## Aspect-aware pipeline checks

`python3 tools/audit/aspect_preview_pipeline_test.py` exercises the production
Dashboard restore/apply/render-worker functions and Active Print canvas reuse
against real LVGL. It checks wide/portrait/square dimension changes, packed stride,
source-header invalidation, profile cache restore, stale worker rejection and
containment in all four themes/text sizes. Renderer tests verify each packed
pixel, output dimensions and untouched buffer tails; cache tests cover variable
Files descriptor dimensions and publication ownership.

On hardware, compare wide, tall and square previews on Dashboard, Printer and
Files, including the ready-to-print popup and fullscreen fallback. Images should
use more of the frame where old cache padding limited them, retain the whole
model and stay stable across profile/page/theme switches. Source PNG black
borders remain. The full-resolution lightbox path is unchanged.


## v7.1.29-dev Files row loading

Row wells display at most 76 pixels, so full 286×215 intermediate canvases wasted
sampling work and PSRAM. Rows now use a packed aspect-preserving 96×96 capacity:
96×55 for a 900×520 image, or 55×96 for portrait. The per-slot pixel capacity falls
from 122,980 to 18,432 bytes (6.7x smaller), while the full row pool supports all
64 browser files instead of only 24. Maximum row-pool pixels fall from 2,951,520
to 1,179,648 bytes. Profiles and detail/fullscreen rendering keep their capacities.

On a cache miss, rows choose the smallest-area metadata thumbnail whose long
edge is at least 96 pixels. If none is adequate, they choose the largest available;
missing dimensions retain a valid path fallback. A large-only slicer source still
requires a full PNG download/decode, but the row samples only its small destination.
When a smaller suitable source exists, network and decoding work can also drop.
Existing SD PNG caches are reusable. New row sources are stored at `.png.row.png`
paths, so they never replace the full preview's `.png` cache. Tapping a row keeps
its small fallback and the existing asynchronous original-resolution lightbox.
No new slicer setup is required. Actual cold/warm and scrolling load times still
need checking on the tablet.

`preview_cache_ownership_test.py` accepts `--cjson-dir /path/to/cJSON` when the
ESP-IDF/managed cJSON source is not discoverable. It uses real cJSON and LVGL for
source selection, invalid/missing metadata, portrait/large-only fallback, SD and
no-SD fetches, separate writes, failure cleanup, descriptor replacement and stale
publication. The fullscreen worker/tap/profile/lifetime checks also pass.
