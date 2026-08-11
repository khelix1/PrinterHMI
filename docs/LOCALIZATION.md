# Localization

## Status

Spanish is the first non-English PrinterHMI language pack. The localization
foundation keeps the selected language in NVS and rebuilds the shell after a
selection changes. English remains the complete fallback at all times.

## Language picker rule

Every selector entry uses its own native name (an autonym): `English`,
`Español`, and future entries such as `Français` or `Deutsch`. This makes a
language easy to find again after the interface switches.

Only a complete, tested language pack may be selectable. A future language
must cover its operational pages and shared popups, fit the 1024×600 layout,
and have verified glyph support before it is exposed.

## Scope gates

Latin and Cyrillic languages can use the current typography once glyph coverage
is verified. RTL languages additionally require bidirectional text and layout
mirroring. CJK languages require an external glyph font, plus PSRAM, flash and
layout validation. These are separate gated changes; no unsupported language
is presented to an operator.
