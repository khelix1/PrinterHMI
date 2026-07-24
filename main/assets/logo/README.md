# PrinterHMI logo assets

Selected direction: **7F — separated HMI**.

- `printerhmi-logo-dark.svg`: full logo for dark surfaces.
- `printerhmi-logo-light.svg`: full logo for light surfaces.
- `printerhmi-mark.svg`: standalone square mark.
- `*-outlined.svg`: portable vector exports with all text converted to paths.
- `printerhmi-splash.png`: transparent raster embedded in the firmware splash.
- `printerhmi-splash.inc`: generated `static const` byte array compiled into flash.
- `printerhmi-logo-*.png` and `printerhmi-mark.png`: transparent export assets.

The non-outlined SVG files are the editable masters. The firmware compiles the
PNG byte array in `printerhmi-splash.inc`; rerun `export_logo_assets.sh` after
changing the master artwork.
