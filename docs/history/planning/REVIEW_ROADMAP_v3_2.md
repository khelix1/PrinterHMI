# PrinterHMI v3.2 Block Review Roadmap

## Rules
- No feature work during review.
- No deleting code during first pass.
- Move only what clearly belongs elsewhere.
- Keep main.c as app coordinator: startup, shell, nav, runtime, services.
- UI modules own layout/widgets.
- Backend modules own state, parsing, IO, cache, HTTP helpers.

## Block Table

| File | Lines | Block | Current Role | Should Belong To | Action | Notes |
|---|---:|---|---|---|---|---|

