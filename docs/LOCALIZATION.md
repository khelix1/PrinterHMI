# Localization

## Status

Spanish is the first non-English PrinterHMI language pack. The localization
foundation keeps the selected language in NVS and rebuilds the shell after a
selection changes. English remains the complete fallback at all times.

Catalogs are owned by feature: common popup actions, shell, settings, and each
operator page have their own small module. A language is selectable only when
the registry marks every required module complete and target-tested. Spanish
currently has common, shell, and language-settings catalogs, so it remains a
development pack and is intentionally not offered in the operator picker yet.

To add a language, add its enum/native name, implement each catalog module,
mark each target-tested module complete in `ui_i18n_registry.c`, then verify
glyph coverage and layout on the target panel before the registry exposes it.

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
