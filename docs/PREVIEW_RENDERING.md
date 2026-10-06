# Preview rendering

Dashboard and Printer previews fill their complete inner frame without
stretching. The shared RGB565 renderer center-crops the source to the cached
canvas proportions instead of adding black letterbox bands. The UI uses native LVGL `COVER` sizing for
that canvas to cover its well, clips overflow at the rounded
frame and removes the previous image inset. Small Files preview canvases use
the same shared fill renderer.

Different frame proportions can crop edges of the small preview. The fullscreen
high-resolution path uses the complete original thumbnail and aspect-fit
rendering and native `CONTAIN` viewport sizing, so the operator can inspect the full image. Its initial cached
fallback may show the cropped small preview until the original finishes loading.
Black pixels already present in a slicer-generated thumbnail remain part of
that image; fill removes padding added by PrinterHMI.

## Cache ownership and bounds

Profile previews hold at most four 286×215 RGB565 buffers. Active publications
reuse an equal-sized buffer; PNG publications keep a staging buffer until decode
succeeds so a failed decode cannot damage the previous preview. All profile cache
access is under the display lock. LVGL image resources are dropped before a
source descriptor or its backing pixels change. Invalidation retains storage
while old widgets may still reference it; reset requires consumers to be detached
and keeps revision numbers advancing.

Files keeps at most 24 shared-size RGB565 buffers. The worker renders into one
PSRAM staging buffer, then takes the display lock before the slot mutex and
transfers ownership into the current slot. There is no second permanent copy.
Stale generations are discarded; old storage is dropped/freed only under the
same display lock. Slot buffers remain bounded across page generations rather
than being treated as a leak when retained after navigation.

The two pools together retain at most 3,443,440 bytes of RGB565 pixel data. This
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
sources, centered sampling, every destination pixel, buffer guards, extreme
aspect ratios, fullscreen fit and preview-well coverage.

On hardware, check Dashboard and Printer for the same print, including reconnect,
profile switching and reopening each page. Confirm that previews reach their
frame edges without distortion and that tapping opens the complete high-resolution
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
