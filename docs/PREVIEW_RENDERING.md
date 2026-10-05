# Preview rendering

Dashboard and Printer previews fill their complete inner frame without
stretching. The shared RGB565 renderer center-crops the source to the cached
canvas proportions instead of adding black letterbox bands. The UI then scales
that canvas proportionally to cover its well, clips overflow at the rounded
frame and removes the previous image inset. Small Files preview canvases use
the same shared fill renderer.

Different frame proportions can crop edges of the small preview. The fullscreen
high-resolution path uses the complete original thumbnail and aspect-fit
rendering, so the operator can inspect the full image. Its initial cached
fallback may show the cropped small preview until the original finishes loading.
Black pixels already present in a slicer-generated thumbnail remain part of
that image; fill removes padding added by PrinterHMI.

## Verification

Run `python3 tools/audit/preview_fill_test.py` with managed LVGL sources, or supply
`--lvgl-dir /path/to/lvgl`. Tests build real LVGL and check wide/portrait/square
sources, centered sampling, every destination pixel, buffer guards, extreme
aspect ratios, fullscreen fit and preview-well coverage.

On hardware, check Dashboard and Printer for the same print, including reconnect,
profile switching and reopening each page. Confirm that previews reach their
frame edges without distortion and that tapping opens the complete high-resolution
thumbnail. Check the Files preview and its popup, normal/larger text themes,
loading placeholders, unavailable previews and tap-to-close behavior.
