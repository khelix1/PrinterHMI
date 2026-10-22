# Custom themes

PrinterHMI supports complete, versioned custom-theme packages stored on the SD
card. The firmware applies, remembers, rescans, and removes packages; it does
not include an on-device theme editor or Theme Laboratory.

Built-in **Studio**, **Foundry**, **Operator**, and **Dark Glass** themes remain part of the
firmware and cannot be removed.

Open **Settings → Theme → Custom themes** on the panel to scan the SD card,
select an installed package, then Apply. The theme-maker firmware restores
this chooser entry alongside the built-in previews.

## Create a theme

Launch the Linux desktop Theme Maker; installation and source launch are
documented in `tools/theme-maker/linux/README.md`.
See [its README](../tools/theme-maker/README.md) for compatibility and current
preview limits. You can also create a package with a JSON-aware editor. Save it as
UTF-8 JSON with the `.phmitheme` extension. Theme packages are strict and must
be complete: partial palette, metric, or layout overrides are rejected rather
than guessed.

Every version 1 package requires these root fields:

```json
{
  "schema": "printerhmi.theme",
  "schema_version": 1,
  "id": "midnight-forge",
  "name": "Midnight Forge",
  "author": "Optional author name",
  "description": "Optional short description",
  "base_theme": "operator",
  "palette": { "...": "complete palette required" },
  "metrics": { "...": "complete metrics required" },
  "layouts": { "...": "complete layouts required" }
}
```

The identity block above is an explanation, not a complete package. A package
must also contain every item in the validation checklist below.

### Naming

- Use an ID of 2–32 characters: lowercase letters, numbers, and hyphens only.
  It must start with a letter or number. Example: `midnight-forge`.
- Use a human-readable name of up to 40 UTF-8 bytes. Example: `Midnight Forge`.
- Use `id.phmitheme` as the filename. The filename is not required to match the
  ID, but matching names make backup and recovery much clearer.
- Author is optional (up to 40 UTF-8 bytes); description is optional (up to 120
  UTF-8 bytes).
- Choose exactly one base theme: `foundry`, `operator`, or `glass`.

### Required palette

`palette` must contain `#RRGGBB` colors for all of these names:

```text
background, background_deep, popup, topbar, navigation, panel, panel_alt,
card, card_dark, control, control_alt, border, border_soft, border_control,
text, text_bright, text_dim, text_muted, accent, accent_bright, success,
warning, danger, telemetry_grid, telemetry_bed, telemetry_chamber,
telemetry_humidity
```

### Required metrics and layouts

`metrics` must provide integer values for `radius_card`, `radius_button`,
`radius_panel` (each 0–36), and `surface_opacity` (96–255).

`layouts` must define every listed card/region for `dashboard`, `drybox`,
`printer`, `files`, `network`, `settings`, and `telemetry`. Each rectangle uses
`x`, `y`, `width`, `height`, and `visible`. The application area is 854 × 528;
all rectangles must remain inside it. Required regions are:

```text
dashboard: banner, active_print, machine_status, command_bar
drybox: environment, drying_system, material_program
printer: active, status, actions
files: breadcrumb, up, search, sort, refresh, list
network: wifi, moonraker, networks, actions
settings: banner, content
telemetry: metric_1, metric_2, metric_3, metric_4, charts
```

## Save to the SD card

Save the final file here:

```text
/sdcard/PrinterHMI/themes/midnight-forge.phmitheme
```

The directory is created after the SD card is mounted. A package may be at most
48 KiB; PrinterHMI loads at most eight valid custom themes. Copy the file using
a normal computer file manager, safely eject the card, and insert it into the
controller.

## Apply, change, or remove

Open **Settings → Theme → Custom Themes**. PrinterHMI rescans the SD-card
directory, lists only valid packages, previews their key colors, and saves the
selected theme. Removing a custom theme deletes only that `.phmitheme` file
after confirmation. Built-in themes remain protected.

If a selected package or SD card is unavailable, PrinterHMI falls back to the
package's saved built-in base theme. The selected custom-theme ID is preserved
in configuration backup and restore, but the corresponding package file must
also be kept with the backup.

## Troubleshooting

- **Theme does not appear:** confirm the filename ends in `.phmitheme`, the
  file is UTF-8 JSON, it is under the exact `themes` directory, and it is no
  larger than 48 KiB.
- **Theme is rejected:** check the complete required palette, metrics, and
  layouts; every color must use `#RRGGBB` and all rectangles must fit within
  854 × 528.
- **Theme disappears after restore:** copy the matching `.phmitheme` package
  back to the SD card, then rescan Custom Themes.

## Version 2 / Studio (theme-maker branch firmware)

Released v7.0.0 accepts version 1. The updated loader on this branch also accepts
version 2. A Studio package must set `base_theme` to `studio`, provide all 58
palette entries listed by `tools/theme-maker/theme-spec.js`, and include the
same four metrics. Studio requires `surface_opacity: 255` and must **omit**
`layouts`; its native composition is not compatible with the legacy 854 × 528
rectangle format. The maker exports this format automatically for Studio.

Version 2 colors cover the shared Studio color tokens, including popup,
telemetry and bed-mesh colors. Common card, panel and button corner tokens use
the three radius metrics. Special-purpose recipe shapes remain built in.
Version 1 packages keep their existing 27-color contract and legacy geometry.
A version 2 package using a legacy base still requires its complete layouts.
The file-size limit remains 48 KiB and the registry remains limited to eight
loaded themes. Package identity/name limits and SD installation are unchanged.

Font files, image assets, arbitrary shader/effect data and executable content
are not part of version 1 or 2. Version 3 adds prepared background assets. The editor rejects unsupported imported fields
rather than quietly dropping them on export.

## Version 3 / background artwork (artwork branch firmware)

Use **Design → Background image** in the Linux app. Artwork themes use
`schema_version: 3` plus a `background` object containing `asset`, `fit`
(`cover`, `contain`, `stretch`), `position_x`, `position_y`, `overlay_color`
and `overlay_opacity`. Position/opacity are integers 0–100. Studio retains its
58-color palette and native composition; other bases retain the 27-color
palette and complete legacy layouts. Studio surfaces remain opaque.

The app bakes crop/position/readability overlay into a fixed 1024 × 600 RGB565
image. The `asset` value must be a basename matching
`bg-<64 lowercase hex digits>.phmibg`, located next to its `.phmitheme` file.
Its 20-byte header is `PHMIBG01`, then little-endian width/height (16-bit),
pixel-byte count (32-bit), and CRC32 of the following little-endian RGB565
pixels (32-bit). Pixel count is exactly 1,228,800 bytes. JSON remains limited
to 48 KiB. Filesystem path traversal and oversized/mismatched headers are
rejected; a CRC failure or unavailable PSRAM uses the solid theme background.

The app's export ZIP includes `PrinterHMI/themes/<id>.phmitheme`, its prepared
`.phmibg`, `ThemeMaker/source.png` (an editable normalized source copy) and
`INSTALL.txt`. Copy the `PrinterHMI` folder to the SD card root. Keep the whole
ZIP for desktop re-import; firmware does not open ZIPs or decode source photos.
Removing a theme does not delete shared/content-addressed asset files.

The firmware shares one pixel allocation between the screen and cropped page
views, retaining references until each LVGL owner is destroyed or restyled.
A padded scanline meets LVGL's stride requirements for bottom-edge crops.
At most two distinct image allocations (about 2.46 MiB total) are retained
while previous-page owners retire. Larger allocations use PSRAM only.
Backgrounds do not create interactive image objects. Native headers, navigation,
dialogs, cards, and emergency controls keep their existing rendering and input
ownership. High-contrast mode suppresses artwork.

Version 3 is **not supported by released v7.0.0**. Apply the source patch,
build with `./tools/build_idf6_hosted3.sh`, then OTA the resulting
`build-idf6-hosted3/PrinterHMI.bin` using the local Custom URL flow in
[Flashing and OTA](FLASHING_AND_OTA.md).

Host checks cover the actual LVGL RGB565 render, full-screen/page crop
alignment, parser and binary bounds, lifetime after registry changes,
deallocation, bad CRC and allocation failure, high-contrast fallback,
ZIP traversal/size rejection and editable bundle round trips. ESP-IDF target
build and physical panel touch/appearance still require local validation.
