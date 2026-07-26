# Custom themes

PrinterHMI supports versioned custom-theme packages stored on the SD card.
Built-in Foundry, Operator, and Dark Glass themes remain compiled into the
firmware and cannot be removed.

## SD-card layout

```text
/sdcard/PrinterHMI/themes/
├── midnight-forge.phmitheme
└── another-theme.phmitheme
```

The firmware creates the directory after mounting the SD card. Copy packages
into that directory with PrinterHMI Theme Studio or a normal file manager.

Open **Settings → Theme → Custom Themes** to rescan the directory. Tap a theme
to apply and save it. The Remove action requires confirmation and deletes only
that custom package.

## Safety behavior

- Maximum eight discovered custom themes.
- Maximum package size: 48 KiB.
- Schema version must be supported.
- IDs must be unique and contain only lowercase letters, numbers, and hyphens.
- Colors, opacity, and every rectangle are range checked.
- Layout rectangles must stay inside the 854 × 528 application area.
- Invalid packages are logged and omitted from the theme list.
- A custom theme always names Foundry, Operator, or Dark Glass as its fallback.
- If the selected file or SD card is unavailable, the saved built-in base
  remains usable.

## Package identity

Packages are UTF-8 JSON with a `.phmitheme` extension:

```json
{
  "schema": "printerhmi.theme",
  "schema_version": 1,
  "id": "midnight-forge",
  "name": "Midnight Forge",
  "base_theme": "operator"
}
```

The complete version 1 schema and an example package are distributed with
PrinterHMI Theme Studio.

## Persistence and backup

The selected custom-theme ID is stored in NVS. Configuration backup and restore
also preserve it. The corresponding `.phmitheme` file must still exist on the
SD card when restoring or booting.
