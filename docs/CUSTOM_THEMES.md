# Custom themes

PrinterHMI supports complete, versioned custom-theme packages stored on the SD
card. The firmware applies, remembers, rescans, and removes packages; it does
not include an on-device theme editor or Theme Laboratory.

Built-in **Foundry**, **Operator**, and **Dark Glass** themes remain part of the
firmware and cannot be removed.

## Create a theme

Create the package on a computer with a JSON-aware editor, then save it as
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
- Use a human-readable name of up to 40 characters. Example: `Midnight Forge`.
- Use `id.phmitheme` as the filename. The filename is not required to match the
  ID, but matching names make backup and recovery much clearer.
- Author is optional (up to 40 characters); description is optional (up to 120
  characters).
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
